// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_SYNC_LOAD_CONTEXT_H_
#define CONTENT_RENDERER_LOADER_SYNC_LOAD_CONTEXT_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/public/renderer/request_peer.h"
#include "content/renderer/loader/resource_dispatcher.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom.h"

namespace base {
class WaitableEvent;
}

namespace network {
struct ResourceRequest;
}

namespace blink {
class URLLoaderThrottle;
}

namespace content {

struct SyncLoadResponse;

// This class owns the context necessary to perform an asynchronous request
// while the main thread is blocked so that it appears to be synchronous.
// There are a couple of modes to load a request:
//   1) kDataPipe; body is received on a data pipe passed on
//      OnStartLoadingResponseBody(), and the body is set to response_.data.
//   2) kBlob: body is received on a data pipe passed on
//      OnStartLoadingResponseBody(), and wraps the data pipe with a
//      SerializedBlobPtr.
class CONTENT_EXPORT SyncLoadContext : public RequestPeer {
 public:
  // Begins a new asynchronous request on whatever sequence this method is
  // called on. |completed_event| will be signalled when the request is complete
  // and |response| will be populated with the response data. |abort_event|
  // will be signalled from the main thread to abort the sync request on a
  // worker thread when the worker thread is being terminated.
  // If |download_to_blob_registry| is not null, it is used to redirect the
  // download to a blob, with the resulting blob populated in |response|.
  static void StartAsyncWithWaitableEvent(
      std::unique_ptr<network::ResourceRequest> request,
      int routing_id,
      scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo>
          url_loader_factory_info,
      std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles,
      SyncLoadResponse* response,
      base::WaitableEvent* completed_event,
      base::WaitableEvent* abort_event,
      base::TimeDelta timeout,
      mojo::PendingRemote<blink::mojom::BlobRegistry>
          download_to_blob_registry);

  ~SyncLoadContext() override;

  void FollowRedirect();
  void CancelRedirect();

 private:
  friend class SyncLoadContextTest;

  SyncLoadContext(
      network::ResourceRequest* request,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo> url_loader_factory,
      SyncLoadResponse* response,
      base::WaitableEvent* completed_event,
      base::WaitableEvent* abort_event,
      base::TimeDelta timeout,
      mojo::PendingRemote<blink::mojom::BlobRegistry> download_to_blob_registry,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  // RequestPeer implementation:
  void OnUploadProgress(uint64_t position, uint64_t size) override;
  bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          network::mojom::URLResponseHeadPtr head) override;
  void OnReceivedResponse(network::mojom::URLResponseHeadPtr head) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnTransferSizeUpdated(int transfer_size_diff) override;
  void OnCompletedRequest(
      const network::URLLoaderCompletionStatus& status) override;
  scoped_refptr<base::TaskRunner> GetTaskRunner() override;

  void OnFinishCreatingBlob(blink::mojom::SerializedBlobPtr blob);

  void OnBodyReadable(MojoResult, const mojo::HandleSignalsState&);

  void OnAbort(base::WaitableEvent* event);
  void OnTimeout();

  void CompleteRequest();
  bool Completed() const;

  // This raw pointer will remain valid for the lifetime of this object because
  // it remains on the stack until |event_| is signaled.
  // Set to null after CompleteRequest() is called.
  SyncLoadResponse* response_;

  enum class Mode { kInitial, kDataPipe, kBlob };
  Mode mode_ = Mode::kInitial;

  // Used when Mode::kDataPipe.
  mojo::ScopedDataPipeConsumerHandle body_handle_;
  mojo::SimpleWatcher body_watcher_;

  // State necessary to run a request on an independent thread.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<ResourceDispatcher> resource_dispatcher_;

  // State for downloading to a blob.
  mojo::Remote<blink::mojom::BlobRegistry> download_to_blob_registry_;
  bool blob_response_started_ = false;
  bool blob_finished_ = false;
  bool request_completed_ = false;

  int request_id_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  class SignalHelper;
  std::unique_ptr<SignalHelper> signals_;

  DISALLOW_COPY_AND_ASSIGN(SyncLoadContext);
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_SYNC_LOAD_CONTEXT_H_
