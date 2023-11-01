// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_FORWARDED_RACE_NETWORK_REQUEST_URL_LOADER_FACTORY_H_
#define CONTENT_COMMON_SERVICE_WORKER_FORWARDED_RACE_NETWORK_REQUEST_URL_LOADER_FACTORY_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {
// A URLLoaderFactory for BestEffortServiceWorker (crbug.com/1420517).
// RaceNetworkRequest is initiated outside of ServiceWorker, but the response
// will be reused as a corresponding fetch event result in ServiceWorker in
// order to avoid sending duplicated requests.
// This URLLoaderFactory fuses two different message pipes into a single pipe by
// passing |client_receiver| in the constructor and calling
// CreateLoaderAndStart(). This behavior only works on the first
// CreateLoaderAndStart() call.
//
// When CreateLoaderAndStart() is called, there is a case that the message pipes
// are already fused to the pipes for the prior request. In that case this
// URLLoaderFactory creates a new regular URLLoader and dispatches the request
// with using |fallback_factory|. Specifically, this happens when the request is
// proxied by the extension then respond with the internal redirect, and the new
// request url starts with chrome-extension://.
class ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory
    : network::mojom::URLLoaderFactory {
 public:
  ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderClient> client_receiver,
      scoped_refptr<network::SharedURLLoaderFactory> fallback_factory);
  ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory(
      const ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory&) = delete;
  ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory& operator=(
      const ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory&) = delete;
  ~ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory() override;

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

  mojo::PendingReceiver<network::mojom::URLLoader>
  InitURLLoaderNewPipeAndPassReceiver();

 private:
  mojo::Receiver<network::mojom::URLLoaderFactory> receiver_{this};
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client_receiver_;
  mojo::PendingRemote<network::mojom::URLLoader> loader_;
  scoped_refptr<network::SharedURLLoaderFactory> fallback_factory_;
  bool is_data_pipe_fused_ = false;
};
}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_FORWARDED_RACE_NETWORK_REQUEST_URL_LOADER_FACTORY_H_
