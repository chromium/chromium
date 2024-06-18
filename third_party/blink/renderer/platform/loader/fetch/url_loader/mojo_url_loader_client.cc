// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/mojo_url_loader_client.h"

#include <iterator>

#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/navigation/renderer_eviction_reason.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/back_forward_cache_buffer_limit_tracker.h"
#include "third_party/blink/renderer/platform/back_forward_cache_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/loader_freeze_mode.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/resource_request_sender.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {
namespace {

constexpr base::TimeDelta kGracePeriodToFinishLoadingWhileInBackForwardCache =
    base::Seconds(60);

}  // namespace

class MojoURLLoaderClient::DeferredMessage {
 public:
  DeferredMessage() = default;
  DeferredMessage(const DeferredMessage&) = delete;
  DeferredMessage& operator=(const DeferredMessage&) = delete;
  virtual ~DeferredMessage() = default;

  virtual void HandleMessage(
      ResourceRequestSender* resource_request_sender) = 0;
  virtual bool IsCompletionMessage() const = 0;
};

class MojoURLLoaderClient::DeferredOnReceiveResponse final
    : public DeferredMessage {
 public:
  explicit DeferredOnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata,
      base::TimeTicks response_ipc_arrival_time)
      : response_head_(std::move(response_head)),
        body_(std::move(body)),
        cached_metadata_(std::move(cached_metadata)),
        response_ipc_arrival_time_(response_ipc_arrival_time) {}

  void HandleMessage(ResourceRequestSender* resource_request_sender) override {
    resource_request_sender->OnReceivedResponse(
        std::move(response_head_), std::move(body_),
        std::move(cached_metadata_), response_ipc_arrival_time_);
  }
  bool IsCompletionMessage() const override { return false; }

 private:
  network::mojom::URLResponseHeadPtr response_head_;
  mojo::ScopedDataPipeConsumerHandle body_;
  std::optional<mojo_base::BigBuffer> cached_metadata_;
  const base::TimeTicks response_ipc_arrival_time_;
};

class MojoURLLoaderClient::DeferredOnReceiveRedirect final
    : public DeferredMessage {
 public:
  DeferredOnReceiveRedirect(const net::RedirectInfo& redirect_info,
                            network::mojom::URLResponseHeadPtr response_head,
                            base::TimeTicks redirect_ipc_arrival_time)
      : redirect_info_(redirect_info),
        response_head_(std::move(response_head)),
        redirect_ipc_arrival_time_(redirect_ipc_arrival_time) {}

  void HandleMessage(ResourceRequestSender* resource_request_sender) override {
    resource_request_sender->OnReceivedRedirect(
        redirect_info_, std::move(response_head_), redirect_ipc_arrival_time_);
  }
  bool IsCompletionMessage() const override { return false; }

 private:
  const net::RedirectInfo redirect_info_;
  network::mojom::URLResponseHeadPtr response_head_;
  const base::TimeTicks redirect_ipc_arrival_time_;
};

class MojoURLLoaderClient::DeferredOnUploadProgress final
    : public DeferredMessage {
 public:
  DeferredOnUploadProgress(int64_t current, int64_t total)
      : current_(current), total_(total) {}

  void HandleMessage(ResourceRequestSender* resource_request_sender) override {
    resource_request_sender->OnUploadProgress(current_, total_);
  }
  bool IsCompletionMessage() const override { return false; }

 private:
  const int64_t current_;
  const int64_t total_;
};


class MojoURLLoaderClient::DeferredOnComplete final : public DeferredMessage {
 public:
  explicit DeferredOnComplete(const network::URLLoaderCompletionStatus& status,
                              base::TimeTicks complete_ipc_arrival_time)
      : status_(status),
        complete_ipc_arrival_time_(complete_ipc_arrival_time) {}

  void HandleMessage(ResourceRequestSender* resource_request_sender) override {
    resource_request_sender->OnRequestComplete(status_,
                                               complete_ipc_arrival_time_);
  }
  bool IsCompletionMessage() const override { return true; }

 private:
  const network::URLLoaderCompletionStatus status_;
  const base::TimeTicks complete_ipc_arrival_time_;
};

class MojoURLLoaderClient::BodyBuffer final
    : public mojo::DataPipeDrainer::Client {
 public:
  BodyBuffer(MojoURLLoaderClient* owner,
             mojo::ScopedDataPipeConsumerHandle readable,
             mojo::ScopedDataPipeProducerHandle writable,
             scoped_refptr<base::SequencedTaskRunner> task_runner)
      : owner_(owner),
        writable_(std::move(writable)),
        writable_watcher_(FROM_HERE,
                          mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                          std::move(task_runner)) {
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
  void OnDataAvailable(base::span<const uint8_t> data) override {
    std::string_view chars = base::as_string_view(data);
    DCHECK(draining_);
    if (owner_->freeze_mode() == LoaderFreezeMode::kBufferIncoming) {
      owner_->DidBufferLoadWhileInBackForwardCache(chars.size());
      if (!owner_->CanContinueBufferingWhileInBackForwardCache()) {
        owner_->EvictFromBackForwardCache(
            mojom::blink::RendererEvictionReason::kNetworkExceedsBufferLimit);
        return;
      }
    }
    buffered_body_.push(std::vector<char>(chars.begin(), chars.end()));
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
      base::span<const uint8_t> bytes =
          base::as_byte_span(current_chunk).subspan(offset_in_current_chunk_);

      size_t actually_written_bytes = 0;
      MojoResult result = writable_->WriteData(bytes, MOJO_WRITE_DATA_FLAG_NONE,
                                               actually_written_bytes);
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
          NOTREACHED_IN_MIGRATION();
          return;
      }
      // We've sent |bytes_sent| bytes, update the current offset in the
      // frontmost chunk.
      offset_in_current_chunk_ += actually_written_bytes;
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
    // the owner MojoURLLoaderClient.
    owner_->FlushDeferredMessages();
  }

  const raw_ptr<MojoURLLoaderClient> owner_;
  mojo::ScopedDataPipeProducerHandle writable_;
  mojo::SimpleWatcher writable_watcher_;
  std::unique_ptr<mojo::DataPipeDrainer> pipe_drainer_;
  // We save the received response body as a queue of chunks so that we can free
  // memory as soon as we finish sending a chunk completely.
  base::queue<std::vector<char>> buffered_body_;
  size_t offset_in_current_chunk_ = 0;
  bool draining_ = true;
};

MojoURLLoaderClient::MojoURLLoaderClient(
    ResourceRequestSender* resource_request_sender,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    bool bypass_redirect_checks,
    const GURL& request_url,
    base::OnceCallback<void(mojom::blink::RendererEvictionReason)>
        evict_from_bfcache_callback,
    base::RepeatingCallback<void(size_t)>
        did_buffer_load_while_in_bfcache_callback)
    : back_forward_cache_timeout_(
          base::Seconds(GetLoadingTasksUnfreezableParamAsInt(
              "grace_period_to_finish_loading_in_seconds",
              static_cast<int>(
                  kGracePeriodToFinishLoadingWhileInBackForwardCache
                      .InSeconds())))),
      resource_request_sender_(resource_request_sender),
      task_runner_(std::move(task_runner)),
      bypass_redirect_checks_(bypass_redirect_checks),
      last_loaded_url_(request_url),
      evict_from_bfcache_callback_(std::move(evict_from_bfcache_callback)),
      did_buffer_load_while_in_bfcache_callback_(
          std::move(did_buffer_load_while_in_bfcache_callback)) {}

MojoURLLoaderClient::~MojoURLLoaderClient() = default;

void MojoURLLoaderClient::Freeze(LoaderFreezeMode mode) {
  freeze_mode_ = mode;
  if (mode != LoaderFreezeMode::kBufferIncoming) {
    // Back/forward cache eviction should only be triggered when `freeze_mode_`
    // is kBufferIncoming.
    StopBackForwardCacheEvictionTimer();
  }
  if (mode == LoaderFreezeMode::kNone) {
    task_runner_->PostTask(
        FROM_HERE, WTF::BindOnce(&MojoURLLoaderClient::FlushDeferredMessages,
                                 weak_factory_.GetWeakPtr()));
  } else if (mode == LoaderFreezeMode::kBufferIncoming &&
             !has_received_complete_ &&
             !back_forward_cache_eviction_timer_.IsRunning()) {
    // We should evict the page associated with this load if the connection
    // takes too long until it either finished or failed.
    back_forward_cache_eviction_timer_.SetTaskRunner(task_runner_);
    back_forward_cache_eviction_timer_.Start(
        FROM_HERE, back_forward_cache_timeout_,
        WTF::BindOnce(
            &MojoURLLoaderClient::EvictFromBackForwardCacheDueToTimeout,
            weak_factory_.GetWeakPtr()));
  }
}

void MojoURLLoaderClient::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {}

void MojoURLLoaderClient::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  TRACE_EVENT1("loading", "MojoURLLoaderClient::OnReceiveResponse", "url",
               last_loaded_url_.GetString().Utf8());

  has_received_response_head_ = true;
  has_received_response_body_ = !!body;
  base::TimeTicks response_ipc_arrival_time = base::TimeTicks::Now();

  base::WeakPtr<MojoURLLoaderClient> weak_this = weak_factory_.GetWeakPtr();
  if (!NeedsStoringMessage()) {
    resource_request_sender_->OnReceivedResponse(
        std::move(response_head), std::move(body), std::move(cached_metadata),
        response_ipc_arrival_time);
    return;
  }

  if (body && (freeze_mode_ == LoaderFreezeMode::kBufferIncoming)) {
    DCHECK(IsInflightNetworkRequestBackForwardCacheSupportEnabled());
    // We want to run loading tasks while deferred (but without dispatching the
    // messages). Drain the original pipe containing the response body into a
    // new pipe so that we won't block the network service if we're deferred for
    // a long time.
    mojo::ScopedDataPipeProducerHandle new_body_producer;
    mojo::ScopedDataPipeConsumerHandle new_body_consumer;
    MojoResult result =
        mojo::CreateDataPipe(nullptr, new_body_producer, new_body_consumer);
    if (result != MOJO_RESULT_OK) {
      OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
      return;
    }
    body_buffer_ = std::make_unique<BodyBuffer>(
        this, std::move(body), std::move(new_body_producer), task_runner_);
    body = std::move(new_body_consumer);
  }
  StoreAndDispatch(std::make_unique<DeferredOnReceiveResponse>(
      std::move(response_head), std::move(body), std::move(cached_metadata),
      response_ipc_arrival_time));
}

void MojoURLLoaderClient::EvictFromBackForwardCache(
    mojom::blink::RendererEvictionReason reason) {
  DCHECK_EQ(freeze_mode_, LoaderFreezeMode::kBufferIncoming);
  StopBackForwardCacheEvictionTimer();
  if (!evict_from_bfcache_callback_) {
    return;
  }
  std::move(evict_from_bfcache_callback_).Run(reason);
}

void MojoURLLoaderClient::DidBufferLoadWhileInBackForwardCache(
    size_t num_bytes) {
  if (!did_buffer_load_while_in_bfcache_callback_) {
    return;
  }
  did_buffer_load_while_in_bfcache_callback_.Run(num_bytes);
}

bool MojoURLLoaderClient::CanContinueBufferingWhileInBackForwardCache() {
  return BackForwardCacheBufferLimitTracker::Get()
      .IsUnderPerProcessBufferLimit();
}

void MojoURLLoaderClient::EvictFromBackForwardCacheDueToTimeout() {
  EvictFromBackForwardCache(
      mojom::blink::RendererEvictionReason::kNetworkRequestTimeout);
}

void MojoURLLoaderClient::StopBackForwardCacheEvictionTimer() {
  back_forward_cache_eviction_timer_.Stop();
}

void MojoURLLoaderClient::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head) {
  base::TimeTicks redirect_ipc_arrival_time = base::TimeTicks::Now();
  DCHECK(!has_received_response_head_);
  if (freeze_mode_ == LoaderFreezeMode::kBufferIncoming) {
    // Evicting a page from the bfcache and aborting the request is not good for
    // a request with keepalive set, which is why we block bfcache when we find
    // such a request.
    // TODO(crbug.com/1356128): This will not be a problem when we move the
    // keepalive request infrastructure to the browser process.

    EvictFromBackForwardCache(
        mojom::blink::RendererEvictionReason::kNetworkRequestRedirected);

    OnComplete(network::URLLoaderCompletionStatus(net::ERR_ABORTED));
    return;
  }
  if (!bypass_redirect_checks_ &&
      !Platform::Current()->IsRedirectSafe(GURL(last_loaded_url_),
                                           redirect_info.new_url)) {
    OnComplete(network::URLLoaderCompletionStatus(net::ERR_UNSAFE_REDIRECT));
    return;
  }

  last_loaded_url_ = KURL(redirect_info.new_url);
  if (NeedsStoringMessage()) {
    StoreAndDispatch(std::make_unique<DeferredOnReceiveRedirect>(
        redirect_info, std::move(response_head), redirect_ipc_arrival_time));
  } else {
    resource_request_sender_->OnReceivedRedirect(
        redirect_info, std::move(response_head), redirect_ipc_arrival_time);
  }
}

void MojoURLLoaderClient::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  if (NeedsStoringMessage()) {
    StoreAndDispatch(std::make_unique<DeferredOnUploadProgress>(
        current_position, total_size));
  } else {
    resource_request_sender_->OnUploadProgress(current_position, total_size);
  }
  std::move(ack_callback).Run();
}

void MojoURLLoaderClient::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kMojoURLLoaderClient);

  if (NeedsStoringMessage()) {
    accumulated_transfer_size_diff_during_deferred_ += transfer_size_diff;
  } else {
    resource_request_sender_->OnTransferSizeUpdated(transfer_size_diff);
  }
}

void MojoURLLoaderClient::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  base::TimeTicks complete_ipc_arrival_time = base::TimeTicks::Now();
  has_received_complete_ = true;
  StopBackForwardCacheEvictionTimer();

  // Dispatch completion status to the ResourceRequestSender.
  // Except for errors, there must always be a response's body.
  DCHECK(has_received_response_body_ || status.error_code != net::OK);
  if (NeedsStoringMessage()) {
    StoreAndDispatch(std::make_unique<DeferredOnComplete>(
        status, complete_ipc_arrival_time));
  } else {
    resource_request_sender_->OnRequestComplete(status,
                                                complete_ipc_arrival_time);
  }
}

bool MojoURLLoaderClient::NeedsStoringMessage() const {
  return freeze_mode_ != LoaderFreezeMode::kNone ||
         deferred_messages_.size() > 0 ||
         accumulated_transfer_size_diff_during_deferred_ > 0;
}

void MojoURLLoaderClient::StoreAndDispatch(
    std::unique_ptr<DeferredMessage> message) {
  DCHECK(NeedsStoringMessage());
  if (freeze_mode_ != LoaderFreezeMode::kNone) {
    deferred_messages_.emplace_back(std::move(message));
  } else if (deferred_messages_.size() > 0 ||
             accumulated_transfer_size_diff_during_deferred_ > 0) {
    deferred_messages_.emplace_back(std::move(message));
    FlushDeferredMessages();
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

void MojoURLLoaderClient::OnConnectionClosed() {
  // If the connection aborts before the load completes, mark it as aborted.
  if (!has_received_complete_) {
    OnComplete(network::URLLoaderCompletionStatus(net::ERR_ABORTED));
    return;
  }
}

void MojoURLLoaderClient::FlushDeferredMessages() {
  if (freeze_mode_ != LoaderFreezeMode::kNone) {
    return;
  }
  WebVector<std::unique_ptr<DeferredMessage>> messages;
  messages.swap(deferred_messages_);
  bool has_completion_message = false;
  base::WeakPtr<MojoURLLoaderClient> weak_this = weak_factory_.GetWeakPtr();
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

    messages[index]->HandleMessage(resource_request_sender_);
    if (!weak_this)
      return;
    if (freeze_mode_ != LoaderFreezeMode::kNone) {
      deferred_messages_.reserve(messages.size() - index - 1);
      for (size_t i = index + 1; i < messages.size(); ++i)
        deferred_messages_.emplace_back(std::move(messages[i]));
      return;
    }
  }

  // Dispatch the transfer size update.
  if (accumulated_transfer_size_diff_during_deferred_ > 0) {
    auto transfer_size_diff = accumulated_transfer_size_diff_during_deferred_;
    accumulated_transfer_size_diff_during_deferred_ = 0;
    resource_request_sender_->OnTransferSizeUpdated(transfer_size_diff);
    if (!weak_this)
      return;
    if (freeze_mode_ != LoaderFreezeMode::kNone) {
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
    messages.back()->HandleMessage(resource_request_sender_);
  }
}

}  // namespace blink
