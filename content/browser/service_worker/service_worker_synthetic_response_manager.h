// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SYNTHETIC_RESPONSE_MANAGER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SYNTHETIC_RESPONSE_MANAGER_H_

#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/browser/service_worker/service_worker_synthetic_response_data_pipe_connector.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/race_network_request_simple_buffer_manager.h"
#include "content/common/service_worker/race_network_request_write_buffer_manager.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_fetch_response_callback.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_stream_handle.mojom-forward.h"

namespace content {
class ServiceWorkerClient;
class StoragePartitionImpl;

// (crbug.com/352578800): `ServiceWorkerSyntheticResponseManager` handles
// requests and responses for SyntheticResponse.
// This class is responsible for 1) initiating a network request, 2) sending
// response headers to `ServiceWorkerVersion` which is persistent across
// navigations, 3) returning the response locally, 4) transferring the response
// data between data pipes.
class CONTENT_EXPORT ServiceWorkerSyntheticResponseManager {
 public:
  // Indicates the current status to dispatch SyntheticResponse.
  // `kNotReady`: required data pipes are not created, or there is no local
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
  using OnReceiveRedirectCallback = base::OnceCallback<void(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head)>;
  using OnCompleteCallback = base::OnceCallback<void(
      const network::URLLoaderCompletionStatus& status)>;
  using FetchCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode,
                              ServiceWorkerFetchDispatcher::FetchEventResult,
                              blink::mojom::FetchAPIResponsePtr,
                              blink::mojom::ServiceWorkerStreamHandlePtr,
                              blink::mojom::ServiceWorkerFetchEventTimingPtr,
                              scoped_refptr<ServiceWorkerVersion>)>;

  explicit ServiceWorkerSyntheticResponseManager(
      scoped_refptr<ServiceWorkerVersion> version);
  ServiceWorkerSyntheticResponseManager(
      const ServiceWorkerSyntheticResponseManager&) = delete;
  ServiceWorkerSyntheticResponseManager& operator=(
      const ServiceWorkerSyntheticResponseManager&) = delete;
  ~ServiceWorkerSyntheticResponseManager();

  // Starts the network request.
  //
  // If `IsServiceWorkerSyntheticResponseNetworkService()` is true and the
  // manager is `kReady`, this method modifies the `request` object by
  // populating its `trusted_params`. Specifically:
  // 1. `expected_response_headers_for_synthetic_response` is set to the
  //    cached synthetic response headers.
  // 2. `response_body_stream` is set to a data pipe producer handle for
  //    the synthetic response body.
  // These changes allow the network service to serve the synthetic response
  // without additional copies in the browser process.
  void InitiateRequest(ServiceWorkerClient* service_worker_client,
                       StoragePartitionImpl* storage_partition,
                       network::ResourceRequest& request,
                       OnReceiveResponseCallback receive_response_callback,
                       OnReceiveRedirectCallback receive_redirect_callback,
                       OnCompleteCallback complete_callback);
  // Tries to start the synthetic response. Returns true if the synthetic
  // response is started, otherwise returns false.
  bool MaybeStartSyntheticResponse(FetchCallback callback);
  SyntheticResponseStatus Status() const { return status_; }

  // The static function to override the dry run mode.
  static void SetDryRunMode(bool enabled);
  static bool IsDryRunModeEnabledForTesting();

 private:
  friend class ServiceWorkerSyntheticResponseManagerTest;

  class SyntheticResponseURLLoaderClient;

  void StartRequest(int request_id,
                    uint32_t options,
                    network::ResourceRequest& request,
                    OnReceiveResponseCallback receive_response_callback,
                    OnReceiveRedirectCallback receive_redirect_callback,
                    OnCompleteCallback complete_callback);

  void OnReceiveResponse(network::mojom::URLResponseHeadPtr response_head,
                         mojo::ScopedDataPipeConsumerHandle body);
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr response_head);
  void OnComplete(const network::URLLoaderCompletionStatus& status);

  void MaybeSetResponseHead(
      const network::mojom::URLResponseHead& response_head);

  void TransferResponseBody(mojo::ScopedDataPipeConsumerHandle body);

  // Read response data from the data pipe which has the actual response from
  // the network, and keep it in buffer.
  void Read(MojoResult result, const mojo::HandleSignalsState& state);

  // Write the buffered data to the data pipe, those one endpoint is already
  // passed to the client side.
  void Write(MojoResult result, const mojo::HandleSignalsState& state);

  // Check whether the response headers are consistent between the locally
  // stored header and the header from the network.
  bool CheckHeaderConsistency(scoped_refptr<net::HttpResponseHeaders> headers);

  // Notify the browser to reload the page by passing the <meta> tag to the
  // response body stream.
  void NotifyReloading(mojo::ScopedDataPipeProducerHandle producer);

  // Callback executed after copying data in `simple_buffer_manager_` or
  // `data_pipe_connector_`. This calls `stream_callback_->OnCompleted()`.
  void OnCloneCompleted();

  // These are helpers for thread offloading to clone the response body data to
  // the other data pipe.
  static void CloneBufferInBackground(
      mojo::ScopedDataPipeConsumerHandle consumer,
      mojo::ScopedDataPipeProducerHandle producer,
      base::OnceCallback<void()> callback);

  SyntheticResponseStatus status_ = SyntheticResponseStatus::kNotReady;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  mojo::PendingRemote<network::mojom::URLLoader> url_loader_;
  std::unique_ptr<SyntheticResponseURLLoaderClient> client_;
  scoped_refptr<ServiceWorkerVersion> version_;
  OnReceiveResponseCallback response_callback_;
  OnReceiveRedirectCallback redirect_callback_;
  OnCompleteCallback complete_callback_;
  std::optional<RaceNetworkRequestWriteBufferManager> write_buffer_manager_;
  // This is used to store the producer handle when it is passed to the network
  // service in the network service delegation mode.
  // Storing it here allows us to reclaim the handle and write a fallback body
  // if the request is intercepted by an embedder.
  scoped_refptr<network::SharedDataPipeProducerHandle> shared_producer_;
  mojo::Remote<blink::mojom::ServiceWorkerStreamCallback> stream_callback_;
  // TODO(crbug.com/447039330): Remove this after confirming
  // `ServiceWorkerSyntheticResponseDataPipeConnector` performs better.
  std::optional<RaceNetworkRequestSimpleBufferManager> simple_buffer_manager_;
  std::optional<ServiceWorkerSyntheticResponseDataPipeConnector>
      data_pipe_connector_;
  bool did_start_synthetic_response_ = false;

  base::TimeTicks request_start_time_;
  base::TimeTicks response_received_time_;

  static bool dry_run_mode_for_testing_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ServiceWorkerSyntheticResponseManager> weak_factory_{
      this};
};
}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SYNTHETIC_RESPONSE_MANAGER_H_
