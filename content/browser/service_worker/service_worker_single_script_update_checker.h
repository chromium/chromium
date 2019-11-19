// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SINGLE_SCRIPT_UPDATE_CHECKER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SINGLE_SCRIPT_UPDATE_CHECKER_H_

#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_updated_script_loader.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

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
  // Result of the comparison of a single script.
  enum class Result {
    kNotCompared,
    kFailed,
    kIdentical,
    kDifferent,
  };

  // This contains detailed error info of update check when it failed.
  struct CONTENT_EXPORT FailureInfo {
    FailureInfo(blink::ServiceWorkerStatusCode status,
                const std::string& error_message,
                network::URLLoaderCompletionStatus network_status);
    ~FailureInfo();

    const blink::ServiceWorkerStatusCode status;
    const std::string error_message;
    const network::URLLoaderCompletionStatus network_status;
  };

  // The paused state consists of Mojo endpoints and a cache writer
  // detached/paused in the middle of loading script body and would be used in
  // the left steps of the update process.
  struct CONTENT_EXPORT PausedState {
    PausedState(
        std::unique_ptr<ServiceWorkerCacheWriter> cache_writer,
        std::unique_ptr<
            ServiceWorkerUpdatedScriptLoader::ThrottlingURLLoaderCoreWrapper>
            network_loader,
        mojo::PendingReceiver<network::mojom::URLLoaderClient>
            network_client_receiver,
        mojo::ScopedDataPipeConsumerHandle network_consumer,
        ServiceWorkerUpdatedScriptLoader::LoaderState network_loader_state,
        ServiceWorkerUpdatedScriptLoader::WriterState body_writer_state);
    PausedState(const PausedState& other) = delete;
    PausedState& operator=(const PausedState& other) = delete;
    ~PausedState();

    std::unique_ptr<ServiceWorkerCacheWriter> cache_writer;
    std::unique_ptr<
        ServiceWorkerUpdatedScriptLoader::ThrottlingURLLoaderCoreWrapper>
        network_loader;
    mojo::PendingReceiver<network::mojom::URLLoaderClient>
        network_client_receiver;
    mojo::ScopedDataPipeConsumerHandle network_consumer;
    ServiceWorkerUpdatedScriptLoader::LoaderState network_loader_state;
    ServiceWorkerUpdatedScriptLoader::WriterState body_writer_state;
  };

  // This callback is only called after all of the work is done by the checker.
  // It notifies the check result to the callback and the ownership of
  // internal state variables (the cache writer and Mojo endpoints for loading)
  // is transferred to the callback in the PausedState only when the result is
  // Result::kDifferent. Otherwise it's set to nullptr. FailureInfo is set to a
  // valid value if the result is Result::kFailed, otherwise it'll be nullptr.
  using ResultCallback = base::OnceCallback<void(const GURL&,
                                                 Result,
                                                 std::unique_ptr<FailureInfo>,
                                                 std::unique_ptr<PausedState>)>;

  // Both |compare_reader| and |copy_reader| should be created from the same
  // resource ID, and this ID should locate where the script specified with
  // |script_url| is stored. |writer| should be created with a new resource ID.
  // |main_script_url| could be different from |script_url| when the script is
  // imported.
  ServiceWorkerSingleScriptUpdateChecker(
      const GURL& script_url,
      const GURL& referrer,
      bool is_main_script,
      const GURL& main_script_url,
      const GURL& scope,
      bool force_bypass_cache,
      blink::mojom::ServiceWorkerUpdateViaCache update_via_cache,
      base::TimeDelta time_since_last_check,
      const net::HttpRequestHeaders& default_headers,
      ServiceWorkerUpdatedScriptLoader::BrowserContextGetter
          browser_context_getter,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      std::unique_ptr<ServiceWorkerResponseReader> compare_reader,
      std::unique_ptr<ServiceWorkerResponseReader> copy_reader,
      std::unique_ptr<ServiceWorkerResponseWriter> writer,
      ResultCallback callback);

  ~ServiceWorkerSingleScriptUpdateChecker() override;

  // network::mojom::URLLoaderClient override:
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head) override;
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle consumer) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  bool network_accessed() const { return network_accessed_; }

  static const char* ResultToString(Result result);

 private:
  class WrappedIOBuffer;

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

  // Called when the update check for this script failed. It calls Finish().
  void Fail(blink::ServiceWorkerStatusCode status,
            const std::string& error_message,
            network::URLLoaderCompletionStatus network_status);

  // Called when the update check for this script succeeded. It calls Finish().
  void Succeed(Result result);
  void Finish(Result result, std::unique_ptr<FailureInfo> failure_info);

  const GURL script_url_;
  const bool is_main_script_;
  const GURL scope_;
  const bool force_bypass_cache_;
  const blink::mojom::ServiceWorkerUpdateViaCache update_via_cache_;
  const base::TimeDelta time_since_last_check_;
  bool network_accessed_ = false;

  std::unique_ptr<
      ServiceWorkerUpdatedScriptLoader::ThrottlingURLLoaderCoreWrapper>
      network_loader_;
  mojo::Receiver<network::mojom::URLLoaderClient> network_client_receiver_{
      this};
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
  ServiceWorkerUpdatedScriptLoader::LoaderState network_loader_state_ =
      ServiceWorkerUpdatedScriptLoader::LoaderState::kNotStarted;

  // Represents the state of |cache_writer_|.
  // Set to kWriting when it starts to send the header to |cache_writer_|, and
  // set to kCompleted when the header has been sent.
  //
  // OnReceiveResponse(): kNotStarted -> kWriting (in WriteHeaders())
  // OnWriteHeadersComplete(): kWriting -> kCompleted
  ServiceWorkerUpdatedScriptLoader::WriterState header_writer_state_ =
      ServiceWorkerUpdatedScriptLoader::WriterState::kNotStarted;

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
  ServiceWorkerUpdatedScriptLoader::WriterState body_writer_state_ =
      ServiceWorkerUpdatedScriptLoader::WriterState::kNotStarted;

  base::WeakPtrFactory<ServiceWorkerSingleScriptUpdateChecker> weak_factory_{
      this};

  DISALLOW_IMPLICIT_CONSTRUCTORS(ServiceWorkerSingleScriptUpdateChecker);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SINGLE_SCRIPT_UPDATE_CHECKER_H_
