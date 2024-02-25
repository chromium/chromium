// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SINGLE_SCRIPT_UPDATE_CHECKER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SINGLE_SCRIPT_UPDATE_CHECKER_H_

#include "base/time/time.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/browser/service_worker/service_worker_updated_script_loader.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/cpp/cross_origin_resource_policy.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom-forward.h"

namespace network {
class MojoToNetPendingBuffer;
class SharedURLLoaderFactory;
}  // namespace network

namespace content {

class BrowserContext;
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

  // These values indicate if the update check returns the updated sha256
  // checksum from |cache_writer_|.
  //
  // kDefault: By default, this class doesn't handle the script sha256
  // checksum. The checksum is updated only when there is a cache mismatch but
  // this class never checks it.
  // kForceUpdate: If this value is passed to the ctor of this class,
  // the checksum is updated even when there is no cache mismatch, and the
  // updated checksum is passed to the |ResultCallback| param only if the check
  // result is |kIdentical|. If the result is |kDifferent| or others, the
  // checksum wouldn't be passed.
  enum class ScriptChecksumUpdateOption {
    kDefault,
    kForceUpdate,
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
        std::unique_ptr<blink::ThrottlingURLLoader> network_loader,
        mojo::Remote<network::mojom::URLLoaderClient> network_client_remote,
        mojo::PendingReceiver<network::mojom::URLLoaderClient>
            network_client_receiver,
        scoped_refptr<network::MojoToNetPendingBuffer> pending_network_buffer,
        uint32_t consumed_bytes,
        ServiceWorkerUpdatedScriptLoader::LoaderState network_loader_state,
        ServiceWorkerUpdatedScriptLoader::WriterState body_writer_state);
    PausedState(const PausedState& other) = delete;
    PausedState& operator=(const PausedState& other) = delete;
    ~PausedState();

    std::unique_ptr<ServiceWorkerCacheWriter> cache_writer;
    std::unique_ptr<blink::ThrottlingURLLoader> network_loader;
    // The endpoint called by `network_loader`. That needs to be alive while
    // `network_loader` is alive.
    //
    // We need to keep the mojo::Remote<network::mojom::URLLoaderClient> because
    // `network_loader` keeps a pointer to the mojo::Remote. This means
    // when ServiceWorkerSingleScriptUpdateChecker pauses loading the script
    // and passes the state to SWUpdatedScriptLoader, the Mojo's pipe works as
    // a queue that stores incoming method calls by ThrottlingURLLoader.
    // Once the Mojo's receiver for network::mojom::URLLoaderClient is re-bound
    // to SWUpdatedScriptLoader, the queued method calls are invoked.
    // Note that this can't be mojo::PendingRemote because `network_loader`
    // holds a raw pointer to the instance of `network_client_remote`.
    mojo::Remote<network::mojom::URLLoaderClient> network_client_remote;
    // The other side of endpoint for `network_loader`. This is connected to
    // `network_client_remote`.
    mojo::PendingReceiver<network::mojom::URLLoaderClient>
        network_client_receiver;

    // The buffer which has a part of the body from the network which had a
    // diff. This could be nullptr when the data from the network is smaller
    // than the data in the disk.
    scoped_refptr<network::MojoToNetPendingBuffer> pending_network_buffer;
    // The number of bytes in |pending_network_buffer| that have already been
    // processed by the cache writer.
    uint32_t consumed_bytes;

    ServiceWorkerUpdatedScriptLoader::LoaderState network_loader_state;
    ServiceWorkerUpdatedScriptLoader::WriterState body_writer_state;
  };

  // This callback is only called after all of the work is done by the checker.
  // It notifies the check result to the callback and the ownership of
  // internal state variables (the cache writer and Mojo endpoints for loading)
  // is transferred to the callback in the PausedState only when the result is
  // Result::kDifferent. Otherwise it's set to nullptr. FailureInfo is set to a
  // valid value if the result is Result::kFailed, otherwise it'll be nullptr.
  using ResultCallback = base::OnceCallback<void(
      const GURL&,
      Result,
      std::unique_ptr<FailureInfo>,
      std::unique_ptr<PausedState>,
      const std::optional<std::string>& sha256_checksum)>;

  ServiceWorkerSingleScriptUpdateChecker() = delete;

  // Both |compare_reader| and |copy_reader| should be created from the same
  // resource ID, and this ID should locate where the script specified with
  // |script_url| is stored. |writer| should be created with a new resource ID.
  // |main_script_url| could be different from |script_url| when the script is
  // imported.
  ServiceWorkerSingleScriptUpdateChecker(
      const GURL& script_url,
      bool is_main_script,
      const GURL& main_script_url,
      const GURL& scope,
      bool force_bypass_cache,
      blink::mojom::ScriptType worker_script_type,
      blink::mojom::ServiceWorkerUpdateViaCache update_via_cache,
      const blink::mojom::FetchClientSettingsObjectPtr&
          fetch_client_settings_object,
      base::TimeDelta time_since_last_check,
      BrowserContext* browser_context,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      mojo::Remote<storage::mojom::ServiceWorkerResourceReader> compare_reader,
      mojo::Remote<storage::mojom::ServiceWorkerResourceReader> copy_reader,
      mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> writer,
      int64_t write_resource_id,
      ScriptChecksumUpdateOption script_checksum_update_option,
      const blink::StorageKey& storage_key,
      ResultCallback callback);

  ServiceWorkerSingleScriptUpdateChecker(
      const ServiceWorkerSingleScriptUpdateChecker&) = delete;
  ServiceWorkerSingleScriptUpdateChecker& operator=(
      const ServiceWorkerSingleScriptUpdateChecker&) = delete;

  ~ServiceWorkerSingleScriptUpdateChecker() override;

  // network::mojom::URLLoaderClient override:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle consumer,
      std::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  bool network_accessed() const { return network_accessed_; }
  const scoped_refptr<PolicyContainerHost> policy_container_host() const {
    return policy_container_host_;
  }

  static const char* ResultToString(Result result);

 private:
  class WrappedIOBuffer;

  void WriteHeaders(network::mojom::URLResponseHeadPtr response_head);
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
  // |paused_state| should be set when result is kDifferent.
  void Succeed(Result result, std::unique_ptr<PausedState> paused_state);
  void Finish(Result result,
              std::unique_ptr<PausedState> paused_state,
              std::unique_ptr<FailureInfo> failure_info,
              const std::optional<std::string>& sha256_checksum);

  const GURL script_url_;
  const bool is_main_script_;
  const GURL scope_;
  const bool force_bypass_cache_;
  const blink::mojom::ServiceWorkerUpdateViaCache update_via_cache_;
  const base::TimeDelta time_since_last_check_;
  bool network_accessed_ = false;
  const ScriptChecksumUpdateOption script_checksum_update_option_ =
      ScriptChecksumUpdateOption::kDefault;
  scoped_refptr<PolicyContainerHost> policy_container_host_;

  // The endpoint called by `network_loader_`. That needs to be alive while
  // `network_loader_` is alive.
  mojo::Remote<network::mojom::URLLoaderClient> network_client_remote_;
  mojo::Receiver<network::mojom::URLLoaderClient> network_client_receiver_{
      this};
  mojo::ScopedDataPipeConsumerHandle network_consumer_;
  mojo::SimpleWatcher network_watcher_;

  // `network_loader_` needs to be declared after `network_client_remote_`
  // because the former holds a `raw_ptr` on the latter, and thus it needs to
  // be destroyed first to avoid holding a dangling pointer.
  std::unique_ptr<blink::ThrottlingURLLoader> network_loader_;

  std::unique_ptr<ServiceWorkerCacheWriter> cache_writer_;
  ResultCallback callback_;

  // Represents the state of |network_loader_|.
  // Corresponds to the steps described in the class comments.
  //
  // CreateLoaderAndStart(): kNotStarted -> kLoadingHeader
  // OnReceiveResponse(): kLoadingHeader -> kLoadingBody
  // OnComplete(): kLoadingBody -> kCompleted
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
  // OnReceiveResponsey() && OnWriteHeadersComplete():
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
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SINGLE_SCRIPT_UPDATE_CHECKER_H_
