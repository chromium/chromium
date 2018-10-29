// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SINGLE_SCRIPT_UPDATE_CHECKER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SINGLE_SCRIPT_UPDATE_CHECKER_H_

#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace network {
class MojoToNetPendingBuffer;
class SharedURLLoaderFactory;
}  // namespace network

namespace content {

struct HttpResponseInfoIOBuffer;
class ServiceWorkerCacheWriter;

// Executes byte-for-byte update check of one script. This loads the script from
// the network and compares it with the stored counterpart read from
// |compare_reader|. The result will be passed as an argument of |callback|:
// true when they are identical and false otherwise. When |callback| is
// triggered, |cache_writer_| owned by |this| should be paused if the scripts
// were not identical.
class CONTENT_EXPORT ServiceWorkerSingleScriptUpdateChecker
    : public network::mojom::URLLoaderClient {
 public:
  using ResultCallback = base::OnceCallback<void(bool)>;

  // Both |compare_reader| and |copy_reader| should be created from the same
  // resource ID, and this ID should locate where the script specified with
  // |url| is stored. |writer| should be created with a new resource ID.
  ServiceWorkerSingleScriptUpdateChecker(
      const GURL& url,
      bool is_main_script,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      std::unique_ptr<ServiceWorkerResponseReader> compare_reader,
      std::unique_ptr<ServiceWorkerResponseReader> copy_reader,
      std::unique_ptr<ServiceWorkerResponseWriter> writer,
      ResultCallback callback);

  ~ServiceWorkerSingleScriptUpdateChecker() override;

  // network::mojom::URLLoaderClient override:
  void OnReceiveResponse(
      const network::ResourceResponseHead& response_head) override;
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      const network::ResourceResponseHead& response_head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle consumer) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

 private:
  enum class NetworkLoaderState {
    kNotStarted,
    kLoadingHeader,
    kWaitingForBody,
    kLoadingBody,
    kCompleted
  };

  enum class CacheWriterState { kNotStarted, kWriting, kCompleted };

  void WriteHeaders(scoped_refptr<HttpResponseInfoIOBuffer> info_buffer);
  void OnWriteHeadersComplete(net::Error error);

  void MaybeStartNetworkConsumerHandleWatcher();
  void OnNetworkDataAvailable(MojoResult,
                              const mojo::HandleSignalsState& state);
  void CompareData(
      scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer,
      uint32_t bytes_available);
  void OnCompareDataComplete(
      scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer,
      uint32_t bytes_written,
      net::Error error);
  void Finish(bool is_script_changed);

  network::mojom::URLLoaderPtr network_loader_;
  mojo::Binding<network::mojom::URLLoaderClient> network_client_binding_;
  mojo::ScopedDataPipeConsumerHandle network_consumer_;
  mojo::SimpleWatcher network_watcher_;

  std::unique_ptr<ServiceWorkerCacheWriter> cache_writer_;
  ResultCallback callback_;

  // Represents the state of |network_loader_|.
  // Corresponds to the steps described in the class comments.
  //
  // When response body exists:
  // CreateLoaderAndStart(): kNotStarted -> kLoadingHeader
  // OnReceiveResponse(): kLoadingHeader -> kWaitingForBody
  // OnStartLoadingResponseBody(): kWaitingForBody -> kLoadingBody
  // OnComplete(): kLoadingBody -> kCompleted
  //
  // When response body is empty:
  // CreateLoaderAndStart(): kNotStarted -> kLoadingHeader
  // OnReceiveResponse(): kLoadingHeader -> kWaitingForBody
  // OnComplete(): kWaitingForBody -> kCompleted
  NetworkLoaderState network_loader_state_ = NetworkLoaderState::kNotStarted;

  // Represents the state of |cache_writer_|.
  // Set to kWriting when it starts to send the header to |cache_writer_|, and
  // set to kCompleted when the header has been sent.
  //
  // OnReceiveResponse(): kNotStarted -> kWriting (in WriteHeaders())
  // OnWriteHeadersComplete(): kWriting -> kCompleted
  CacheWriterState header_writer_state_ = CacheWriterState::kNotStarted;

  // Represents the state of |cache_writer_| and |network_consumer_|.
  // Set to kWriting when |this| starts watching |network_consumer_|, and set to
  // kCompleted when |cache_writer_| reports any difference between the stored
  // body and the network body, or the entire body is compared without any
  // difference.
  //
  // When response body exists:
  // OnStartLoadingResponseBody() && OnWriteHeadersComplete():
  //     kNotStarted -> kWriting
  // OnNetworkDataAvailable() && MOJO_RESULT_FAILED_PRECONDITION:
  //     kWriting -> kCompleted
  //
  // When response body is empty:
  // OnComplete(): kNotStarted -> kCompleted
  CacheWriterState body_writer_state_ = CacheWriterState::kNotStarted;

  base::WeakPtrFactory<ServiceWorkerSingleScriptUpdateChecker> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SINGLE_SCRIPT_UPDATE_CHECKER_H_
