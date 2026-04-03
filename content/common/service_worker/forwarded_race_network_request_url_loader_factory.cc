// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/forwarded_race_network_request_url_loader_factory.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace content {

namespace {
// Kill switch for multiple CreateLoaderAndStart calls.
BASE_FEATURE(kKillSwitchForRaceNetworkRequestMultipleCreateLoaderAndStartCalls,
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace

ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory::
    ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory(
        mojo::PendingReceiver<network::mojom::URLLoaderClient> client_receiver,
        scoped_refptr<network::SharedURLLoaderFactory> fallback_factory,
        bool is_main_resource)
    : client_receiver_(std::move(client_receiver)),
      fallback_factory_(fallback_factory),
      is_main_resource_(is_main_resource) {}

ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory::
    ~ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory() = default;

void ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory::
    CreateLoaderAndStart(
        mojo::PendingReceiver<network::mojom::URLLoader> receiver,
        int32_t request_id,
        uint32_t options,
        const network::ResourceRequest& resource_request,
        mojo::PendingRemote<network::mojom::URLLoaderClient> client,
        const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  base::UmaHistogramBoolean(
      base::StrCat({"ServiceWorker.FetchEvent.",
                    is_main_resource_ ? "MainResource" : "Subresource",
                    ".RaceNetworkRequest.ForwardedFactory.CreateLoaderAndStart."
                    "MultipleCalls"}),
      is_data_pipe_fused_);
  if (!is_data_pipe_fused_) {
    // If the member data pipes are still not fused to mojo endpoints, fuse them
    // to reuse the response.
    bool result =
        mojo::FusePipes(std::move(client_receiver_), std::move(client));
    CHECK(result) << resource_request.url;
    result = mojo::FusePipes(std::move(receiver), std::move(loader_));
    CHECK(result) << resource_request.url;
    is_data_pipe_fused_ = true;
  } else {
    // A legitimate renderer will never hit this branch.
    // If we are here, the renderer is compromised or severely buggy.
    if (base::FeatureList::IsEnabled(
            kKillSwitchForRaceNetworkRequestMultipleCreateLoaderAndStartCalls)) {
      receiver_.ReportBadMessage(
          "ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory: "
          "CreateLoaderAndStart called multiple times.");
    } else {
      // If already fused, create a new URLLoader and start the new request.
      // TODO(crbug.com/497437113): Remove this once the kill switch is removed.
      fallback_factory_->CreateLoaderAndStart(
          std::move(receiver), request_id, options, resource_request,
          std::move(client), traffic_annotation);
    }
  }
}

void ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  receiver_.Bind(std::move(receiver));
}

mojo::PendingReceiver<network::mojom::URLLoader>
ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory::
    InitURLLoaderNewPipeAndPassReceiver() {
  return loader_.InitWithNewPipeAndPassReceiver();
}
}  // namespace content
