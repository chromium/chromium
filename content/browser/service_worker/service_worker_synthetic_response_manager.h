// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SYNTHETIC_RESPONSE_MANAGER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SYNTHETIC_RESPONSE_MANAGER_H_

#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/race_network_request_read_buffer_manager.h"
#include "content/common/service_worker/race_network_request_write_buffer_manager.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_fetch_response_callback.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_stream_handle.mojom-forward.h"

namespace content {
// (crbug.com/352578800): `ServiceWorkerSyntheticResponseManager` handles
// requests and responses for SyntheticResponse.
// This class is responsible for 1) initiating a network request, 2) sending
// response headers to `ServiceWorkerVersion` which is persistent across
// navigations, 3) returning the response locally, 4) transferring the response
// data between data pipes.
class CONTENT_EXPORT ServiceWorkerSyntheticResponseManager {
 public:
  // Indicates the current status to dispatch SyntheticResponse.
  // `kNotReady`: required data pipes are not clreated, or there is no local
  // response header in `ServiceWorkerVersion`.
  // `kReady`: required data pipes are all created, and there is a local
  // response header in `ServiceWorkerVersion` already.
  enum class SyntheticResponseStatus {
    kNotReady,
    kReady,
  };

  using OnReceiveResponseCallback = base::RepeatingCallback<void(
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle body)>;
  using OnCompleteCallback = base::OnceCallback<void(
      const network::URLLoaderCompletionStatus& status)>;
  using FetchCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode,
                              ServiceWorkerFetchDispatcher::FetchEventResult,
                              blink::mojom::FetchAPIResponsePtr,
                              blink::mojom::ServiceWorkerStreamHandlePtr,
                              blink::mojom::ServiceWorkerFetchEventTimingPtr,
                              scoped_refptr<ServiceWorkerVersion>)>;

  ServiceWorkerSyntheticResponseManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      scoped_refptr<ServiceWorkerVersion> version);
  ServiceWorkerSyntheticResponseManager(
      const ServiceWorkerSyntheticResponseManager&) = delete;
  ServiceWorkerSyntheticResponseManager& operator=(
      const ServiceWorkerSyntheticResponseManager&) = delete;
  ~ServiceWorkerSyntheticResponseManager();

  void StartRequest(int request_id,
                    uint32_t options,
                    const network::ResourceRequest& request,
                    OnReceiveResponseCallback receive_response_callback,
                    OnCompleteCallback complete_callback);
  void StartSyntheticResponse(FetchCallback callback);
  SyntheticResponseStatus Status() const { return status_; }
  void SetResponseHead(network::mojom::URLResponseHeadPtr response_head);

 private:
  class SyntheticResponseURLLoaderClient;

  void OnReceiveResponse(network::mojom::URLResponseHeadPtr response_head,
                         mojo::ScopedDataPipeConsumerHandle body);
  void OnComplete(const network::URLLoaderCompletionStatus& status);

  // Read response data from the data pipe which has the actual response from
  // the network, and keep it in buffer.
  void Read(MojoResult result, const mojo::HandleSignalsState& state);

  // Write the buffered data to the data pipe, those one endpoint is already
  // passed to the client side.
  void Write(MojoResult result, const mojo::HandleSignalsState& state);

  SyntheticResponseStatus status_ = SyntheticResponseStatus::kNotReady;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  mojo::PendingRemote<network::mojom::URLLoader> url_loader_;
  std::unique_ptr<SyntheticResponseURLLoaderClient> client_;
  scoped_refptr<ServiceWorkerVersion> version_;
  OnReceiveResponseCallback response_callback_;
  OnCompleteCallback complete_callback_;
  std::optional<RaceNetworkRequestReadBufferManager> read_buffer_manager_;
  std::optional<RaceNetworkRequestWriteBufferManager> write_buffer_manager_;
  mojo::Remote<blink::mojom::ServiceWorkerStreamCallback> stream_callback_;

  base::WeakPtrFactory<ServiceWorkerSyntheticResponseManager> weak_factory_{
      this};
};
}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SYNTHETIC_RESPONSE_MANAGER_H_
