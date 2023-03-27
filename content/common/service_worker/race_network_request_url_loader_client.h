// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_RACE_NETWORK_REQUEST_URL_LOADER_CLIENT_H_
#define CONTENT_COMMON_SERVICE_WORKER_RACE_NETWORK_REQUEST_URL_LOADER_CLIENT_H_

#include "content/common/service_worker/service_worker_resource_loader.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {
// RaceNetworkRequestURLLoaderClient handles the response when the request is
// triggered in the RaceNetworkRequest mode.
// If the response from the RaceNetworkRequest mode is faster than the one from
// the fetch handler, this client handles the response and commit it via owner's
// CommitResponse methods.
// If the response from the fetch handler is faster, this class doesn't do
// anything, and discards the response.
class ServiceWorkerRaceNetworkRequestURLLoaderClient
    : public network::mojom::URLLoaderClient {
 public:
  explicit ServiceWorkerRaceNetworkRequestURLLoaderClient(
      const network::ResourceRequest& request,
      base::WeakPtr<ServiceWorkerResourceLoader> owner);
  ServiceWorkerRaceNetworkRequestURLLoaderClient(
      const ServiceWorkerRaceNetworkRequestURLLoaderClient&) = delete;
  ServiceWorkerRaceNetworkRequestURLLoaderClient& operator=(
      const ServiceWorkerRaceNetworkRequestURLLoaderClient&) = delete;
  ~ServiceWorkerRaceNetworkRequestURLLoaderClient() override;

  void Bind(mojo::PendingRemote<network::mojom::URLLoaderClient>* remote);
  const net::LoadTimingInfo& GetLoadTimingInfo() { return head_->load_timing; }

  static net::NetworkTrafficAnnotationTag NetworkTrafficAnnotationTag();

  // network::mojom::URLLoaderClient overrides:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      absl::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        base::OnceCallback<void()> callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

 private:
  mojo::Receiver<network::mojom::URLLoaderClient> receiver_{this};
  const network::ResourceRequest request_;
  base::WeakPtr<ServiceWorkerResourceLoader> owner_;
  network::mojom::URLResponseHeadPtr head_;
};
}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_RACE_NETWORK_REQUEST_URL_LOADER_CLIENT_H_
