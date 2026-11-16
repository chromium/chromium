// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_SYNC_LOAD_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_SYNC_LOAD_CONTEXT_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/byte_size.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom-blink.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/resource_request_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/resource_request_sender.h"

namespace base {
class WaitableEvent;
}

namespace network {
struct ResourceRequest;
}

namespace blink {
class ResourceLoadInfoNotifierWrapper;
class URLLoaderThrottle;
struct SyncLoadResponse;

// This class owns the context necessary to perform an asynchronous request on a
// background `loading_task_runner` while the calling thread is blocked so that
// it appears to be synchronous.
//
// Ownership model: Created on the calling thread via `Create()` (with initial
// refcount 1 and detached sequence checker), bound and used on
// `loading_task_runner`, and released on `loading_task_runner` via
// `SequencedTaskRunner::ReleaseSoon()` once the blocked caller finishes.
//
// There are a couple of modes to load a request:
//   1) kDataPipe: body is received on a data pipe passed on
//      OnReceivedResponse(), and the body is set to response_.data.
//   2) kBlob: body is received on a data pipe passed on
//      OnReceivedResponse(), and wraps the data pipe with a BlobDataHandle.
class BLINK_PLATFORM_EXPORT SyncLoadContext : public ResourceRequestClient {
 public:
  // Creates a `SyncLoadContext` bound to `loading_task_runner`. The caller must
  // not copy the returned `scoped_refptr` on the calling thread and must
  // release it on `loading_task_runner` via `ReleaseSoon()`.
  static scoped_refptr<SyncLoadContext> Create(
      scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner);

  SyncLoadContext(const SyncLoadContext&) = delete;
  SyncLoadContext& operator=(const SyncLoadContext&) = delete;
  ~SyncLoadContext() override;

  // Begins a new asynchronous request on `loading_task_runner`.
  // `completed_event` will be signalled when each redirect or the final
  // response completes, and `response` will be populated with redirect or
  // response data (`response->redirect_info` is engaged iff a redirect awaits
  // `FollowRedirect()` or `CancelRedirect()`). `abort_event` will be signalled
  // from the main thread to abort the sync request on a worker thread when the
  // worker thread is being terminated.
  // If `download_to_blob_registry` is not null, it is used to redirect the
  // download to a blob, with the resulting blob populated in `response`.
  void StartAsyncWithWaitableEvent(
      std::unique_ptr<network::ResourceRequest> request,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      uint32_t loader_options,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory,
      std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
      SyncLoadResponse* response,
      base::WaitableEvent* completed_event,
      base::WaitableEvent* abort_event,
      base::TimeDelta timeout,
      mojo::PendingRemote<mojom::blink::BlobRegistry> download_to_blob_registry,
      const Vector<String>& cors_exempt_header_list,
      std::unique_ptr<ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper);

  void FollowRedirect(std::vector<std::string> removed_headers,
                      net::HttpRequestHeaders modified_headers);
  void CancelRedirect();

 private:
  friend class SyncLoadContextTest;
  class SignalHelper;

  explicit SyncLoadContext(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  void InitializeRequest(network::ResourceRequest* request,
                         std::unique_ptr<network::PendingSharedURLLoaderFactory>
                             url_loader_factory,
                         SyncLoadResponse* response,
                         base::WaitableEvent* completed_event,
                         base::WaitableEvent* abort_event,
                         base::TimeDelta timeout,
                         mojo::PendingRemote<mojom::blink::BlobRegistry>
                             download_to_blob_registry);

  // ResourceRequestClient implementation:
  void OnUploadProgress(uint64_t position, uint64_t size) override;
  void OnReceivedRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr head,
      FollowRedirectCallback follow_redirect_callback) override;
  void OnReceivedResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnTransferSizeUpdated(base::ByteSize transfer_size_diff) override;
  void OnCompletedRequest(
      const network::URLLoaderCompletionStatus& status) override;
  void OnFinishCreatingBlob(const scoped_refptr<BlobDataHandle>& blob);

  void OnBodyReadable(MojoResult, const mojo::HandleSignalsState&);

  void OnAbort(base::WaitableEvent* event);
  void OnTimeout();

  void CompleteRequest();
  bool Completed() const;

  // This raw pointer will remain valid until CompleteRequest() signals the
  // completion event because it remains on the caller's stack.
  // Set to null before CompleteRequest() signals the final event.
  raw_ptr<SyncLoadResponse> response_ = nullptr;

  // Used when handling a redirect. It is set in OnReceivedRedirect(), and
  // called when FollowRedirect() is called from the original thread.
  FollowRedirectCallback follow_redirect_callback_;

  // Stores the completion status if OnCompletedRequest() arrives while a
  // redirect is being handled on the calling thread.
  std::optional<network::URLLoaderCompletionStatus> deferred_completion_status_;

  enum class Mode { kInitial, kDataPipe, kBlob };
  Mode mode_ = Mode::kInitial;

  // Used when Mode::kDataPipe.
  mojo::ScopedDataPipeConsumerHandle body_handle_;
  std::unique_ptr<mojo::SimpleWatcher> body_watcher_;

  // State necessary to run a request on `task_runner_`.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<ResourceRequestSender> resource_request_sender_;

  // State for downloading to a blob.
  mojo::Remote<mojom::blink::BlobRegistry> download_to_blob_registry_;
  bool blob_response_started_ = false;
  bool blob_finished_ = false;
  bool request_completed_ = false;

  // True when the request contains Authorization header.
  // TODO(https://crbug.com/1393520): Remove this field once we get enough
  // stats to make a decision.
  bool has_authorization_header_ = false;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  std::unique_ptr<SignalHelper> signals_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_SYNC_LOAD_CONTEXT_H_
