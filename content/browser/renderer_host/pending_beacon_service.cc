// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pending_beacon_service.h"
#include "base/bind.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/pending_beacon_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/blink/public/mojom/frame/pending_beacon.mojom.h"

constexpr net::NetworkTrafficAnnotationTag kPendingBeaconNetworkTag =
    net::DefineNetworkTrafficAnnotation("pending_beacon_api",
                                        R"(
        semantics {
          sender: "Pending Beacon API"
          description:
            "This request sends out a pending beacon data as single HTTP POST "
            " or GET request. This is used similarly to javascript "
            "`navigator.sendBeacon`, but can be sent either manually by a "
            "developer using the `sendNow` method, or automatically by the "
            "browser when a document is being discarded."
          trigger:
            "On document destruction or `PendingBeacon.sendNow` is called."
          data:
            "Data sent by the beacon is set by javascript on the page."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "This feature cannot be fully disabled. Only for the beacon "
            "requests sent on document discarded, they can be disabled via the "
            "`Background sync` section under the `Privacy and security` tab "
            "in chrome://settings. This feature is enabled by default."
          policy_exception_justification: "The policy for Background sync is "
            "not yet implemented."
        }
      )");

namespace content {

class Beacon;

PendingBeaconService* PendingBeaconService::GetInstance() {
  return base::Singleton<PendingBeaconService>::get();
}

PendingBeaconService::PendingBeaconService() = default;
PendingBeaconService::~PendingBeaconService() = default;

void PendingBeaconService::SendBeacons(
    const std::vector<std::unique_ptr<Beacon>>& beacons,
    network::SharedURLLoaderFactory* shared_url_loader_factory) {
  for (const auto& beacon : beacons) {
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = beacon->url();
    std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
        network::SimpleURLLoader::Create(std::move(resource_request),
                                         kPendingBeaconNetworkTag);
    network::SimpleURLLoader* simple_url_loader_ptr = simple_url_loader.get();

    // Send out the |beacon|.
    // The PendingBeaconService is a singleton with a lifetime the same as the
    // browser process', so it's safe to capture it here.
    simple_url_loader_ptr->DownloadHeadersOnly(
        shared_url_loader_factory,
        base::BindOnce(
            [](std::unique_ptr<network::SimpleURLLoader> loader,
               scoped_refptr<net::HttpResponseHeaders> headers) {
              // Intentionally left empty, this callback captures the |loader|
              // so it stays alive until the beacon request, i.e.
              // |DownloadHeadersOnly|, completes.
            },
            std::move(simple_url_loader)));
  }
}

}  // namespace content
