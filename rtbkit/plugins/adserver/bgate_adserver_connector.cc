/** mock_adserver_connector.cc                                 -*- C++ -*-
    Eric Robert, 03 April 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Example of a simple ad server connector.

*/

#include "bgate_adserver_connector.h"


using namespace RTBKIT;

BgateAdServerConnector::
BgateAdServerConnector(std::string const & serviceName, std::shared_ptr<ServiceProxies> const & proxies, Json::Value const & json) :
    HttpAdServerConnector(serviceName, proxies),
    publisher(getServices()->zmqContext) {
}

void BgateAdServerConnector::init(int winPort, int eventPort) {
    auto services = getServices();

    // Initialize our base class
    HttpAdServerConnector::init(services->config);

    // Prepare a simple JSON handler that already parsed the incoming HTTP payload so that it can
    // create the requied post bidRequest object.
    auto handleEvent = [&](const Datacratic::HttpHeader & header,
                           const Json::Value & json,
                           const std::string & text) {
        return this->handleEvent(PostAuctionEvent(json));
    };
    registerEndpoint(winPort, handleEvent);
    registerEndpoint(eventPort, handleEvent);

    // Publish the endpoint now that it exists.
    HttpAdServerConnector::bindTcp();
    
    // And initialize the generic publisher on a predefined range of ports to try avoiding that
    // collision between different kind of service occurs.
    publisher.init(services->config, serviceName() + "/logger");
    publisher.bindTcp(services->ports->getRange("adServer.logger"));
}

void BgateAdServerConnector::start() {
    recordLevel(1.0, "up");
    HttpAdServerConnector::start();
    publisher.start();
}


void BgateAdServerConnector::shutdown() {
    HttpAdServerConnector::shutdown();
    publisher.shutdown();
}


HttpAdServerResponse BgateAdServerConnector::handleEvent(PostAuctionEvent const & event) {
    HttpAdServerResponse response;

    if(event.type == PAE_WIN) {
        publishWin(event.auctionId,
                   event.adSpotId,
                   event.winPrice,
                   event.timestamp,
                   Json::Value(),
                   event.uids,
                   event.account,
                   Date::now());

        Date now = Date::now();
        publisher.publish("WIN", now.print(3), event.auctionId.toString(), event.winPrice.toString(), "0");
    }

    if (event.type == PAE_CAMPAIGN_EVENT) {
        publishCampaignEvent(event.label,
                             event.auctionId,
                             event.adSpotId,
                             event.timestamp,
                             Json::Value(),
                             event.uids);
    }
    
    return response;
}

namespace {

struct AtInit {
    AtInit()
    {
      PluginInterface<AdServerConnector>::registerPlugin("bgate",
					   [](std::string const & serviceName,
					      std::shared_ptr<ServiceProxies> const & proxies,
					      Json::Value const & json) {
					     
            auto server = new BgateAdServerConnector(serviceName, proxies, json);
            int winPort = json.get("winPort", "12340").asInt();
            int eventPort = json.get("eventPort", "12341").asInt();
            server->init(winPort, eventPort);
            return server;
        });
    }
} atInit;

}

