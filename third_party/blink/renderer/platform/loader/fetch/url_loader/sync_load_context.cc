// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/sync_load_context.h"

#include <optional>
#include <string>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/sync_load_response.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "url/origin.h"

namespace blink {

// An inner helper class to manage the SyncLoadContext's events and timeouts,
// so that we can stop or resumse all of them at once.
class SyncLoadContext::SignalHelper final {
 public:
  SignalHelper(SyncLoadContext* context,
               base::WaitableEvent* redirect_or_response_event,
               base::WaitableEvent* abort_event,
               base::TimeDelta timeout)
      : context_(context),
        redirect_or_response_event_(redirect_or_response_event),
        abort_event_(abort_event) {
    // base::TimeDelta::Max() means no timeout.
    if (timeout != base::TimeDelta::Max()) {
      // Instantiate a base::OneShotTimer instance.
      timeout_timer_.emplace();
    }
    Start(timeout);
  }

  void SignalRedirectOrResponseComplete() {
    abort_watcher_.StopWatching();
    if (timeout_timer_)
      timeout_timer_->AbandonAndStop();
    redirect_or_response_event_->Signal();
  }

  bool RestartAfterRedirect() {
    if (abort_event_ && abort_event_->IsSignaled())
      return false;

    base::TimeDelta timeout_remainder = base::TimeDelta::Max();
    if (timeout_timer_) {
      timeout_remainder =
          timeout_timer_->desired_run_time() - base::TimeTicks::Now();
      if (timeout_remainder <= base::TimeDelta())
        return false;
    }
    Start(timeout_remainder);
    return true;
  }

 private:
  void Start(base::TimeDelta timeout) {
    DCHECK(!redirect_or_response_event_->IsSignaled());
    if (abort_event_) {
      abort_watcher_.StartWatching(
          abort_event_,
          base::BindOnce(&SyncLoadContext::OnAbort, base::Unretained(context_)),
          context_->task_runner_);
    }
    if (timeout_timer_) {
      DCHECK_NE(base::TimeDelta::Max(), timeout);
      timeout_timer_->Start(FROM_HERE, timeout, context_.get(),
                            &SyncLoadContext::OnTimeout);
    }
  }

  raw_ptr<SyncLoadContext> context_;
  raw_ptr<base::WaitableEvent> redirect_or_response_event_;
  raw_ptr<base::WaitableEvent> abort_event_;
  base::WaitableEventWatcher abort_watcher_;
  std::optional<base::OneShotTimer> timeout_timer_;
};

// static
void SyncLoadContext::StartAsyncWithWaitableEvent(
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    uint32_t loader_options,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    WebVector<std::unique_ptr<URLLoaderThrottle>> throttles,
    SyncLoadResponse* response,
    SyncLoadContext** context_for_redirect,
    base::WaitableEvent* redirect_or_response_event,
    base::WaitableEvent* abort_event,
    base::TimeDelta timeout,
    mojo::PendingRemote<mojom::blink::BlobRegistry> download_to_blob_registry,
    const Vector<String>& cors_exempt_header_list,
    std::unique_ptr<ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper) {
  scoped_refptr<SyncLoadContext> context(base::AdoptRef(new SyncLoadContext(
      request.get(), std::move(pending_url_loader_factory), response,
      context_for_redirect, redirect_or_response_event, abort_event, timeout,
      std::move(download_to_blob_registry), loading_task_runner)));
  context->resource_request_sender_->SendAsync(
      std::move(request), std::move(loading_task_runner), traffic_annotation,
      loader_options, cors_exempt_header_list, context,
      context->url_loader_factory_, std::move(throttles),
      std::move(resource_load_info_notifier_wrapper),
      /*code_cache_host=*/nullptr,
      /*evict_from_bfcache_callback=*/
      base::OnceCallback<void(mojom::blink::RendererEvictionReason)>(),
      /*did_buffer_load_while_in_bfcache_callback=*/
      base::RepeatingCallback<void(size_t)>());
}

SyncLoadContext::SyncLoadContext(
    network::ResourceRequest* request,
    std::unique_ptr<network::PendingSharedURLLoaderFactory> url_loader_factory,
    SyncLoadResponse* response,
    SyncLoadContext** context_for_redirect,
    base::WaitableEvent* redirect_or_response_event,
    base::WaitableEvent* abort_event,
    base::TimeDelta timeout,
    mojo::PendingRemote<mojom::blink::BlobRegistry> download_to_blob_registry,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : response_(response),
      context_for_redirect_(context_for_redirect),
      body_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      download_to_blob_registry_(std::move(download_to_blob_registry)),
      task_runner_(std::move(task_runner)),
      signals_(std::make_unique<SignalHelper>(this,
                                              redirect_or_response_event,
                                              abort_event,
                                              timeout)) {
  if (download_to_blob_registry_)
    mode_ = Mode::kBlob;

  url_loader_factory_ =
      network::SharedURLLoaderFactory::Create(std::move(url_loader_factory));

  // Constructs a new ResourceRequestSender specifically for this request.
  resource_request_sender_ = std::make_unique<ResourceRequestSender>();

  // Initialize the final URL with the original request URL. It will be
  // overwritten on redirects.
  response_->url = request->url;

  has_authorization_header_ =
      request->headers.HasHeader(net::HttpRequestHeaders::kAuthorization);
}

SyncLoadContext::~SyncLoadContext() {}

void SyncLoadContext::OnUploadProgress(uint64_t position, uint64_t size) {}

void SyncLoadContext::OnReceivedRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head,
    FollowRedirectCallback follow_redirect_callback) {
  DCHECK(!Completed());

  if (has_authorization_header_ &&
      !url::IsSameOriginWith(response_->url, redirect_info.new_url)) {
    response_->has_authorization_header_between_cross_origin_redirect_ = true;
  }

  response_->url = redirect_info.new_url;
  response_->head = std::move(head);
  response_->redirect_info = redirect_info;
  *context_for_redirect_ = this;

  follow_redirect_callback_ = std::move(follow_redirect_callback);
  signals_->SignalRedirectOrResponseComplete();
}

void SyncLoadContext::FollowRedirect(std::vector<std::string> removed_headers,
                                     net::HttpRequestHeaders modified_headers) {
  CHECK(follow_redirect_callback_);
  if (!signals_->RestartAfterRedirect()) {
    CancelRedirect();
    return;
  }

  response_->redirect_info = net::RedirectInfo();
  *context_for_redirect_ = nullptr;
  std::move(follow_redirect_callback_)
      .Run(std::move(removed_headers), std::move(modified_headers));
}

void SyncLoadContext::CancelRedirect() {
  response_->redirect_info = net::RedirectInfo();
  *context_for_redirect_ = nullptr;

  response_->error_code = net::ERR_ABORTED;
  CompleteRequest();
}

void SyncLoadContext::OnReceivedResponse(
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  DCHECK(!Completed());
  response_->head = std::move(head);

  if (!body) {
    return;
  }

  if (mode_ == Mode::kBlob) {
    DCHECK(download_to_blob_registry_);
    DCHECK(!blob_response_started_);

    blob_response_started_ = true;

    download_to_blob_registry_->RegisterFromStream(
        String(response_->head->mime_type), "",
        std::max<int64_t>(0, response_->head->content_length), std::move(body),
        mojo::NullAssociatedRemote(),
        base::BindOnce(&SyncLoadContext::OnFinishCreatingBlob,
                       base::Unretained(this)));
    return;
  }
  DCHECK_EQ(Mode::kInitial, mode_);
  mode_ = Mode::kDataPipe;
  // setup datapipe to read.
  body_handle_ = std::move(body);
  body_watcher_.Watch(
      body_handle_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&SyncLoadContext::OnBodyReadable,
                          base::Unretained(this)));
  body_watcher_.ArmOrNotify();
}

void SyncLoadContext::OnTransferSizeUpdated(int transfer_size_diff) {}

void SyncLoadContext::OnCompletedRequest(
    const network::URLLoaderCompletionStatus& status) {
  if (Completed()) {
    // It means the response has been aborted due to an error before finishing
    // the response.
    return;
  }
  request_completed_ = true;
  response_->error_code = status.error_code;
  response_->extended_error_code = status.extended_error_code;
  response_->resolve_error_info = status.resolve_error_info;
  response_->should_collapse_initiator = status.should_collapse_initiator;
  response_->cors_error = status.cors_error_status;
  response_->head->encoded_data_length = status.encoded_data_length;
  DCHECK_GE(status.encoded_body_length, 0);
  response_->head->encoded_body_length =
      network::mojom::EncodedBodyLength::New(status.encoded_body_length);
  if ((blob_response_started_ && !blob_finished_) || body_handle_.is_valid()) {
    // The body is still begin downloaded as a Blob, or being read through the
    // handle. Wait until it's completed.
    return;
  }
  CompleteRequest();
}

void SyncLoadContext::OnFinishCreatingBlob(
    const scoped_refptr<BlobDataHandle>& blob) {
  DCHECK(!Completed());
  blob_finished_ = true;
  response_->downloaded_blob = blob;
  if (request_completed_)
    CompleteRequest();
}

void SyncLoadContext::OnBodyReadable(MojoResult,
                                     const mojo::HandleSignalsState&) {
  DCHECK_EQ(Mode::kDataPipe, mode_);
  DCHECK(body_handle_.is_valid());
  base::span<const uint8_t> buffer;
  MojoResult result =
      body_handle_->BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    body_watcher_.ArmOrNotify();
    return;
  }
  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    // Whole body has been read.
    body_handle_.reset();
    body_watcher_.Cancel();
    if (request_completed_)
      CompleteRequest();
    return;
  }
  if (result != MOJO_RESULT_OK) {
    // Something went wrong.
    body_handle_.reset();
    body_watcher_.Cancel();
    response_->error_code = net::ERR_FAILED;
    CompleteRequest();
    return;
  }

  base::span<const char> chars = base::as_chars(buffer);
  if (!response_->data) {
    response_->data = SharedBuffer::Create(chars.data(), chars.size());
  } else {
    response_->data->Append(chars.data(), chars.size());
  }
  body_handle_->EndReadData(chars.size());
  body_watcher_.ArmOrNotify();
}

void SyncLoadContext::OnAbort(base::WaitableEvent* event) {
  DCHECK(!Completed());
  body_handle_.reset();
  body_watcher_.Cancel();
  response_->error_code = net::ERR_ABORTED;
  CompleteRequest();
}

void SyncLoadContext::OnTimeout() {
  // OnTimeout() must not be called after CompleteRequest() was called, because
  // the OneShotTimer must have been stopped.
  DCHECK(!Completed());
  body_handle_.reset();
  body_watcher_.Cancel();
  response_->error_code = net::ERR_TIMED_OUT;
  CompleteRequest();
}

void SyncLoadContext::CompleteRequest() {
  DCHECK(blob_finished_ || (mode_ != Mode::kBlob));
  DCHECK(!body_handle_.is_valid());
  body_watcher_.Cancel();
  signals_->SignalRedirectOrResponseComplete();
  signals_ = nullptr;
  response_ = nullptr;

  // This will indirectly cause this object to be deleted.
  resource_request_sender_->DeletePendingRequest(task_runner_);
}

bool SyncLoadContext::Completed() const {
  DCHECK_EQ(!signals_, !response_);
  return !response_;
}

}  // namespace blink
