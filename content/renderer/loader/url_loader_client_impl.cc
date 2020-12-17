// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/url_loader_client_impl.h"

#include <iterator>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "content/public/common/url_utils.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/loader/resource_dispatcher.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/mojom/frame/back_forward_cache_controller.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/features.h"

namespace content {
namespace {

constexpr size_t kDefaultMaxBufferedBodyBytes = 100 * 1000;
constexpr base::TimeDelta kGracePeriodToFinishLoadingWhileInBackForwardCache =
    base::TimeDelta::FromSeconds(15);

// Determines whether it is safe to redirect from |from_url| to |to_url|.
bool IsRedirectSafe(const GURL& from_url, const GURL& to_url) {
  return IsSafeRedirectTarget(from_url, to_url) &&
         (!GetContentClient()->renderer() ||  // null in unit tests.
          GetContentClient()->renderer()->IsSafeRedirectTarget(to_url));
}

}  // namespace

class URLLoaderClientImpl::DeferredMessage {
 public:
  DeferredMessage() = default;
  virtual void HandleMessage(ResourceDispatcher* dispatcher,
                             int request_id) = 0;
  virtual bool IsCompletionMessage() const = 0;
  virtual ~DeferredMessage() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeferredMessage);
};

class URLLoaderClientImpl::DeferredOnReceiveResponse final
    : public DeferredMessage {
 public:
  explicit DeferredOnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head)
      : response_head_(std::move(response_head)) {}

  void HandleMessage(ResourceDispatcher* dispatcher, int request_id) override {
    dispatcher->OnReceivedResponse(request_id, std::move(response_head_));
  }
  bool IsCompletionMessage() const override { return false; }

 private:
  network::mojom::URLResponseHeadPtr response_head_;
};

class URLLoaderClientImpl::DeferredOnReceiveRedirect final
    : public DeferredMessage {
 public:
  DeferredOnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : redirect_info_(redirect_info),
        response_head_(std::move(response_head)),
        task_runner_(std::move(task_runner)) {}

  void HandleMessage(ResourceDispatcher* dispatcher, int request_id) override {
    dispatcher->OnReceivedRedirect(request_id, redirect_info_,
                                   std::move(response_head_), task_runner_);
  }
  bool IsCompletionMessage() const override { return false; }

 private:
  const net::RedirectInfo redirect_info_;
  network::mojom::URLResponseHeadPtr response_head_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

class URLLoaderClientImpl::DeferredOnUploadProgress final
    : public DeferredMessage {
 public:
  DeferredOnUploadProgress(int64_t current, int64_t total)
      : current_(current), total_(total) {}

  void HandleMessage(ResourceDispatcher* dispatcher, int request_id) override {
    dispatcher->OnUploadProgress(request_id, current_, total_);
  }
  bool IsCompletionMessage() const override { return false; }

 private:
  const int64_t current_;
  const int64_t total_;
};

class URLLoaderClientImpl::DeferredOnReceiveCachedMetadata final
    : public DeferredMessage {
 public:
  explicit DeferredOnReceiveCachedMetadata(mojo_base::BigBuffer data)
      : data_(std::move(data)) {}

  void HandleMessage(ResourceDispatcher* dispatcher, int request_id) override {
    dispatcher->OnReceivedCachedMetadata(request_id, std::move(data_));
  }
  bool IsCompletionMessage() const override { return false; }

 private:
  mojo_base::BigBuffer data_;
};

class URLLoaderClientImpl::DeferredOnStartLoadingResponseBody final
    : public DeferredMessage {
 public:
  explicit DeferredOnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body)
      : body_(std::move(body)) {}

  void HandleMessage(ResourceDispatcher* dispatcher, int request_id) override {
    dispatcher->OnStartLoadingResponseBody(request_id, std::move(body_));
  }
  bool IsCompletionMessage() const override { return false; }

 private:
  mojo::ScopedDataPipeConsumerHandle body_;
};

class URLLoaderClientImpl::DeferredOnComplete final : public DeferredMessage {
 public:
  explicit DeferredOnComplete(const network::URLLoaderCompletionStatus& status)
      : status_(status) {}

  void HandleMessage(ResourceDispatcher* dispatcher, int request_id) override {
    dispatcher->OnRequestComplete(request_id, status_);
  }
  bool IsCompletionMessage() const override { return true; }

 private:
  const network::URLLoaderCompletionStatus status_;
};

class URLLoaderClientImpl::BodyBuffer final
    : public mojo::DataPipeDrainer::Client {
 public:
  BodyBuffer(URLLoaderClientImpl* owner,
             mojo::ScopedDataPipeConsumerHandle readable,
             mojo::ScopedDataPipeProducerHandle writable,
             scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : owner_(owner),
        writable_(std::move(writable)),
        writable_watcher_(FROM_HERE,
                          mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                          std::move(task_runner)),
        max_bytes_drained_(base::GetFieldTrialParamByFeatureAsInt(
            blink::features::kLoadingTasksUnfreezable,
            "max_buffered_bytes",
            kDefaultMaxBufferedBodyBytes)) {
    pipe_drainer_ =
        std::make_unique<mojo::DataPipeDrainer>(this, std::move(readable));
    writable_watcher_.Watch(
        writable_.get(),
        MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        base::BindRepeating(&BodyBuffer::WriteBufferedBody,
                            base::Unretained(this)));
  }

  bool active() const { return writable_watcher_.IsWatching(); }

  // mojo::DataPipeDrainer::Client
  void OnDataAvailable(const void* data, size_t num_bytes) override {
    DCHECK(draining_);
    SCOPED_CRASH_KEY_NUMBER(OnDataAvailable, buffered_body_size,
                            buffered_body_.size());
    SCOPED_CRASH_KEY_NUMBER(OnDataAvailable, data_bytes, num_bytes);
    SCOPED_CRASH_KEY_STRING256(
        OnDataAvailable, last_loaded_url,
        owner_->last_loaded_url().possibly_invalid_spec());
    total_bytes_drained_ += num_bytes;

    if (total_bytes_drained_ > max_bytes_drained_ &&
        owner_->IsDeferredWithBackForwardCache()) {
      owner_->EvictFromBackForwardCache(
          blink::mojom::RendererEvictionReason::kNetworkExceedsBufferLimit);
      return;
    }
    buffered_body_.emplace(static_cast<const char*>(data),
                           static_cast<const char*>(data) + num_bytes);
    WriteBufferedBody(MOJO_RESULT_OK);
  }

  void OnDataComplete() override {
    DCHECK(draining_);
    draining_ = false;
    WriteBufferedBody(MOJO_RESULT_OK);
  }

 private:
  void WriteBufferedBody(MojoResult) {
    // Try to write all the remaining chunks in |buffered_body_|.
    while (!buffered_body_.empty()) {
      // Write the chunk at the front of |buffered_body_|.
      const std::vector<char>& current_chunk = buffered_body_.front();
      DCHECK_LE(offset_in_current_chunk_, current_chunk.size());
      uint32_t bytes_sent = base::saturated_cast<uint32_t>(
          current_chunk.size() - offset_in_current_chunk_);
      MojoResult result =
          writable_->WriteData(current_chunk.data() + offset_in_current_chunk_,
                               &bytes_sent, MOJO_WRITE_DATA_FLAG_NONE);
      switch (result) {
        case MOJO_RESULT_OK:
          break;
        case MOJO_RESULT_FAILED_PRECONDITION:
          // The pipe is closed unexpectedly, finish writing now.
          draining_ = false;
          Finish();
          return;
        case MOJO_RESULT_SHOULD_WAIT:
          writable_watcher_.ArmOrNotify();
          return;
        default:
          NOTREACHED();
          return;
      }
      // We've sent |bytes_sent| bytes, update the current offset in the
      // frontmost chunk.
      offset_in_current_chunk_ += bytes_sent;
      DCHECK_LE(offset_in_current_chunk_, current_chunk.size());
      if (offset_in_current_chunk_ == current_chunk.size()) {
        // We've finished writing the chunk at the front of the queue, pop it so
        // that we'll write the next chunk next time.
        buffered_body_.pop();
        offset_in_current_chunk_ = 0;
      }
    }
    // We're finished if we've drained the original pipe and sent all the
    // buffered body.
    if (!draining_)
      Finish();
  }

  void Finish() {
    DCHECK(!draining_);
    // We've read and written all the data from the original pipe.
    writable_watcher_.Cancel();
    writable_.reset();
    // There might be a deferred OnComplete message waiting for us to finish
    // draining the response body, so flush the deferred messages in
    // the owner URLLoaderClientImpl.
    owner_->FlushDeferredMessages();
  }

  URLLoaderClientImpl* const owner_;
  mojo::ScopedDataPipeProducerHandle writable_;
  mojo::SimpleWatcher writable_watcher_;
  std::unique_ptr<mojo::DataPipeDrainer> pipe_drainer_;
  // We save the received response body as a queue of chunks so that we can free
  // memory as soon as we finish sending a chunk completely.
  base::queue<std::vector<char>> buffered_body_;
  uint32_t offset_in_current_chunk_ = 0;
  size_t total_bytes_drained_ = 0;
  const size_t max_bytes_drained_;
  bool draining_ = true;
};

URLLoaderClientImpl::URLLoaderClientImpl(
    int request_id,
    ResourceDispatcher* resource_dispatcher,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    bool bypass_redirect_checks,
    const GURL& request_url)
    : request_id_(request_id),
      resource_dispatcher_(resource_dispatcher),
      task_runner_(std::move(task_runner)),
      bypass_redirect_checks_(bypass_redirect_checks),
      last_loaded_url_(request_url) {
  back_forward_cache_timeout_ =
      base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kLoadingTasksUnfreezable,
          "grace_period_to_finish_loading_in_seconds",
          static_cast<int>(
              kGracePeriodToFinishLoadingWhileInBackForwardCache.InSeconds())));
 }

URLLoaderClientImpl::~URLLoaderClientImpl() = default;

void URLLoaderClientImpl::SetDefersLoading(
    blink::WebURLLoader::DeferType value) {
  deferred_state_ = value;
  if (value == blink::WebURLLoader::DeferType::kNotDeferred) {
    StopBackForwardCacheEvictionTimer();
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&URLLoaderClientImpl::FlushDeferredMessages,
                       weak_factory_.GetWeakPtr()));
  } else if (IsDeferredWithBackForwardCache() && !has_received_complete_ &&
             !back_forward_cache_eviction_timer_.IsRunning()) {
    // We should evict the page associated with this load if the connection
    // takes too long until it either finished or failed.
    back_forward_cache_eviction_timer_.SetTaskRunner(task_runner_);
    back_forward_cache_eviction_timer_.Start(
        FROM_HERE, back_forward_cache_timeout_,
        base::BindOnce(
            &URLLoaderClientImpl::EvictFromBackForwardCacheDueToTimeout,
            weak_factory_.GetWeakPtr()));
  }
}

void URLLoaderClientImpl::FlushDeferredMessages() {
  if (deferred_state_ != blink::WebURLLoader::DeferType::kNotDeferred)
    return;
  std::vector<std::unique_ptr<DeferredMessage>> messages;
  messages.swap(deferred_messages_);
  bool has_completion_message = false;
  base::WeakPtr<URLLoaderClientImpl> weak_this = weak_factory_.GetWeakPtr();
  // First, dispatch all messages excluding the followings:
  //  - transfer size change
  //  - completion
  // These two types of messages are dispatched later.
  for (size_t index = 0; index < messages.size(); ++index) {
    if (messages[index]->IsCompletionMessage()) {
      // The completion message arrives at the end of the message queue.
      DCHECK(!has_completion_message);
      DCHECK_EQ(index, messages.size() - 1);
      has_completion_message = true;
      break;
    }

    messages[index]->HandleMessage(resource_dispatcher_, request_id_);
    if (!weak_this)
      return;
    if (deferred_state_ != blink::WebURLLoader::DeferType::kNotDeferred) {
      deferred_messages_.insert(
          deferred_messages_.begin(),
          std::make_move_iterator(messages.begin()) + index + 1,
          std::make_move_iterator(messages.end()));
      return;
    }
  }

  // Dispatch the transfer size update.
  if (accumulated_transfer_size_diff_during_deferred_ > 0) {
    auto transfer_size_diff = accumulated_transfer_size_diff_during_deferred_;
    accumulated_transfer_size_diff_during_deferred_ = 0;
    resource_dispatcher_->OnTransferSizeUpdated(request_id_,
                                                transfer_size_diff);
    if (!weak_this)
      return;
    if (deferred_state_ != blink::WebURLLoader::DeferType::kNotDeferred) {
      if (has_completion_message) {
        DCHECK_GT(messages.size(), 0u);
        DCHECK(messages.back()->IsCompletionMessage());
        deferred_messages_.emplace_back(std::move(messages.back()));
      }
      return;
    }
  }

  // Dispatch the completion message.
  if (has_completion_message) {
    DCHECK_GT(messages.size(), 0u);
    DCHECK(messages.back()->IsCompletionMessage());
    if (body_buffer_ && body_buffer_->active()) {
      // If we still have an active body buffer, it means we haven't drained all
      // of the contents of the response body yet. We shouldn't dispatch the
      // completion message now, so
      // put the message back into |deferred_messages_| to be sent later after
      // the body buffer is no longer active.
      deferred_messages_.emplace_back(std::move(messages.back()));
      return;
    }
    messages.back()->HandleMessage(resource_dispatcher_, request_id_);
  }
}

void URLLoaderClientImpl::Bind(
    network::mojom::URLLoaderClientEndpointsPtr endpoints) {
  url_loader_.Bind(std::move(endpoints->url_loader), task_runner_);
  url_loader_client_receiver_.Bind(std::move(endpoints->url_loader_client),
                                   task_runner_);
  url_loader_client_receiver_.set_disconnect_handler(base::BindOnce(
      &URLLoaderClientImpl::OnConnectionClosed, weak_factory_.GetWeakPtr()));
}

void URLLoaderClientImpl::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head) {
  TRACE_EVENT1("loading", "URLLoaderClientImpl::OnReceiveResponse", "url",
               last_loaded_url_.possibly_invalid_spec());

  has_received_response_head_ = true;
  on_receive_response_time_ = base::TimeTicks::Now();

  if (NeedsStoringMessage()) {
    StoreAndDispatch(
        std::make_unique<DeferredOnReceiveResponse>(std::move(response_head)));
  } else {
    resource_dispatcher_->OnReceivedResponse(request_id_,
                                             std::move(response_head));
  }
}

void URLLoaderClientImpl::EvictFromBackForwardCache(
    blink::mojom::RendererEvictionReason reason) {
  StopBackForwardCacheEvictionTimer();
  resource_dispatcher_->EvictFromBackForwardCache(reason, request_id_);
}

void URLLoaderClientImpl::EvictFromBackForwardCacheDueToTimeout() {
  EvictFromBackForwardCache(
      blink::mojom::RendererEvictionReason::kNetworkRequestTimeout);
}

void URLLoaderClientImpl::StopBackForwardCacheEvictionTimer() {
  back_forward_cache_eviction_timer_.Stop();
}

void URLLoaderClientImpl::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head) {
  DCHECK(!has_received_response_head_);
  if (deferred_state_ ==
      blink::WebURLLoader::DeferType::kDeferredWithBackForwardCache) {
    // Close the connections and dispatch and OnComplete message.
    EvictFromBackForwardCache(blink::mojom::RendererEvictionReason::kNetworkRequestRedirected);
    url_loader_.reset();
    url_loader_client_receiver_.reset();
    OnComplete(network::URLLoaderCompletionStatus(net::ERR_ABORTED));
    return;
  }
  if (!bypass_redirect_checks_ &&
      !IsRedirectSafe(last_loaded_url_, redirect_info.new_url)) {
    OnComplete(network::URLLoaderCompletionStatus(net::ERR_UNSAFE_REDIRECT));
    return;
  }

  last_loaded_url_ = redirect_info.new_url;
  if (NeedsStoringMessage()) {
    StoreAndDispatch(std::make_unique<DeferredOnReceiveRedirect>(
        redirect_info, std::move(response_head), task_runner_));
  } else {
    resource_dispatcher_->OnReceivedRedirect(
        request_id_, redirect_info, std::move(response_head), task_runner_);
  }
}

void URLLoaderClientImpl::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  if (NeedsStoringMessage()) {
    StoreAndDispatch(std::make_unique<DeferredOnUploadProgress>(
        current_position, total_size));
  } else {
    resource_dispatcher_->OnUploadProgress(request_id_, current_position,
                                           total_size);
  }
  std::move(ack_callback).Run();
}

void URLLoaderClientImpl::OnReceiveCachedMetadata(mojo_base::BigBuffer data) {
  if (NeedsStoringMessage()) {
    StoreAndDispatch(
        std::make_unique<DeferredOnReceiveCachedMetadata>(std::move(data)));
  } else {
    resource_dispatcher_->OnReceivedCachedMetadata(request_id_,
                                                   std::move(data));
  }
}

void URLLoaderClientImpl::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  if (NeedsStoringMessage()) {
    accumulated_transfer_size_diff_during_deferred_ += transfer_size_diff;
  } else {
    resource_dispatcher_->OnTransferSizeUpdated(request_id_,
                                                transfer_size_diff);
  }
}

void URLLoaderClientImpl::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  TRACE_EVENT1("loading", "URLLoaderClientImpl::OnStartLoadingResponseBody",
               "url", last_loaded_url_.possibly_invalid_spec());

  DCHECK(has_received_response_head_);
  DCHECK(!has_received_response_body_);
  has_received_response_body_ = true;

  if (!on_receive_response_time_.is_null()) {
    UMA_HISTOGRAM_TIMES(
        "Renderer.OnReceiveResponseToOnStartLoadingResponseBody",
        base::TimeTicks::Now() - on_receive_response_time_);
  }

  if (!NeedsStoringMessage()) {
    // Send the message immediately.
    resource_dispatcher_->OnStartLoadingResponseBody(request_id_,
                                                     std::move(body));
    return;
  }

  if (deferred_state_ !=
      blink::WebURLLoader::DeferType::kDeferredWithBackForwardCache) {
    // Defer the message, storing the original body pipe.
    StoreAndDispatch(
        std::make_unique<DeferredOnStartLoadingResponseBody>(std::move(body)));
    return;
  }

  DCHECK(
      base::FeatureList::IsEnabled(blink::features::kLoadingTasksUnfreezable));
  // We want to run loading tasks while deferred (but without dispatching the
  // messages). Drain the original pipe containing the response body into a
  // new pipe so that we won't block the network service if we're deferred for
  // a long time.
  mojo::ScopedDataPipeProducerHandle new_body_producer;
  mojo::ScopedDataPipeConsumerHandle new_body_consumer;
  MojoResult result =
      mojo::CreateDataPipe(nullptr, &new_body_producer, &new_body_consumer);
  if (result != MOJO_RESULT_OK) {
    // We failed to make a new pipe, close the connections and dispatch an
    // OnComplete message instead.
    url_loader_.reset();
    url_loader_client_receiver_.reset();
    OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
    return;
  }
  body_buffer_ = std::make_unique<BodyBuffer>(
      this, std::move(body), std::move(new_body_producer), task_runner_);

  StoreAndDispatch(std::make_unique<DeferredOnStartLoadingResponseBody>(
      std::move(new_body_consumer)));
}

void URLLoaderClientImpl::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  has_received_complete_ = true;
  StopBackForwardCacheEvictionTimer();

  // Dispatch completion status to the ResourceDispatcher.
  // Except for errors, there must always be a response's body.
  DCHECK(has_received_response_body_ || status.error_code != net::OK);
  if (NeedsStoringMessage()) {
    StoreAndDispatch(std::make_unique<DeferredOnComplete>(status));
  } else {
    resource_dispatcher_->OnRequestComplete(request_id_, status);
  }
}

bool URLLoaderClientImpl::NeedsStoringMessage() const {
  return deferred_state_ != blink::WebURLLoader::DeferType::kNotDeferred ||
         deferred_messages_.size() > 0 ||
         accumulated_transfer_size_diff_during_deferred_ > 0;
}

void URLLoaderClientImpl::StoreAndDispatch(
    std::unique_ptr<DeferredMessage> message) {
  DCHECK(NeedsStoringMessage());
  if (deferred_state_ != blink::WebURLLoader::DeferType::kNotDeferred) {
    deferred_messages_.push_back(std::move(message));
  } else if (deferred_messages_.size() > 0 ||
             accumulated_transfer_size_diff_during_deferred_ > 0) {
    deferred_messages_.push_back(std::move(message));
    FlushDeferredMessages();
  } else {
    NOTREACHED();
  }
}

void URLLoaderClientImpl::OnConnectionClosed() {
  // If the connection aborts before the load completes, mark it as aborted.
  if (!has_received_complete_) {
    OnComplete(network::URLLoaderCompletionStatus(net::ERR_ABORTED));
    return;
  }
}

}  // namespace content
