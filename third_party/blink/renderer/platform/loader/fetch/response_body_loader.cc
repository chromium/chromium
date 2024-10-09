// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/loader/fetch/response_body_loader.h"

#include <algorithm>
#include <utility>

#include "base/auto_reset.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/navigation/renderer_eviction_reason.mojom-blink.h"
#include "third_party/blink/renderer/platform/back_forward_cache_buffer_limit_tracker.h"
#include "third_party/blink/renderer/platform/back_forward_cache_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/back_forward_cache_loader_helper.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/navigation_body_loader.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class ResponseBodyLoader::DelegatingBytesConsumer final
    : public BytesConsumer,
      public BytesConsumer::Client {
 public:
  DelegatingBytesConsumer(
      BytesConsumer& bytes_consumer,
      ResponseBodyLoader& loader,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : bytes_consumer_(bytes_consumer),
        loader_(loader),
        task_runner_(std::move(task_runner)) {}

  Result BeginRead(base::span<const char>& buffer) override {
    buffer = {};
    if (loader_->IsAborted()) {
      return Result::kError;
    }
    // When the loader is suspended for non back/forward cache reason, return
    // with kShouldWait.
    if (IsSuspendedButNotForBackForwardCache()) {
      return Result::kShouldWait;
    }
    if (state_ == State::kCancelled) {
      return Result::kDone;
    }
    auto result = bytes_consumer_->BeginRead(buffer);
    if (result == Result::kOk) {
      buffer = buffer.first(std::min(buffer.size(), lookahead_bytes_));
      if (buffer.empty()) {
        result = bytes_consumer_->EndRead(0);
        buffer = {};
        if (result == Result::kOk) {
          result = Result::kShouldWait;
          if (in_on_state_change_) {
            waiting_for_lookahead_bytes_ = true;
          } else {
            task_runner_->PostTask(
                FROM_HERE,
                base::BindOnce(&DelegatingBytesConsumer::OnStateChange,
                               WrapPersistent(this)));
          }
        }
      }
    }
    HandleResult(result);
    return result;
  }
  Result EndRead(size_t read_size) override {
    DCHECK_LE(read_size, lookahead_bytes_);
    lookahead_bytes_ -= read_size;
    auto result = bytes_consumer_->EndRead(read_size);
    if (loader_->IsAborted()) {
      return Result::kError;
    }
    HandleResult(result);
    return result;
  }
  scoped_refptr<BlobDataHandle> DrainAsBlobDataHandle(
      BlobSizePolicy policy) override {
    if (loader_->IsAborted()) {
      return nullptr;
    }
    auto handle = bytes_consumer_->DrainAsBlobDataHandle(policy);
    if (handle) {
      HandleResult(Result::kDone);
    }
    return handle;
  }
  scoped_refptr<EncodedFormData> DrainAsFormData() override {
    if (loader_->IsAborted()) {
      return nullptr;
    }
    auto form_data = bytes_consumer_->DrainAsFormData();
    if (form_data) {
      HandleResult(Result::kDone);
    }
    return form_data;
  }
  mojo::ScopedDataPipeConsumerHandle DrainAsDataPipe() override {
    if (loader_->IsAborted()) {
      return {};
    }
    auto handle = bytes_consumer_->DrainAsDataPipe();
    if (handle && bytes_consumer_->GetPublicState() == PublicState::kClosed) {
      HandleResult(Result::kDone);
    }
    return handle;
  }
  void SetClient(BytesConsumer::Client* client) override {
    DCHECK(!bytes_consumer_client_);
    DCHECK(client);
    if (state_ != State::kLoading) {
      return;
    }
    bytes_consumer_client_ = client;
  }
  void ClearClient() override { bytes_consumer_client_ = nullptr; }
  void Cancel() override {
    if (state_ != State::kLoading) {
      return;
    }

    state_ = State::kCancelled;

    if (in_on_state_change_) {
      has_pending_state_change_signal_ = true;
      return;
    }
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&DelegatingBytesConsumer::CancelSync,
                                          WrapWeakPersistent(this)));
  }
  PublicState GetPublicState() const override {
    if (loader_->IsAborted())
      return PublicState::kErrored;
    return bytes_consumer_->GetPublicState();
  }
  Error GetError() const override {
    if (bytes_consumer_->GetPublicState() == PublicState::kErrored) {
      return bytes_consumer_->GetError();
    }
    DCHECK(loader_->IsAborted());
    return Error{"Response body loading was aborted"};
  }
  String DebugName() const override {
    StringBuilder builder;
    builder.Append("DelegatingBytesConsumer(");
    builder.Append(bytes_consumer_->DebugName());
    builder.Append(")");
    return builder.ToString();
  }

  void Abort() {
    if (state_ != State::kLoading) {
      return;
    }
    if (bytes_consumer_client_) {
      bytes_consumer_client_->OnStateChange();
    }
  }

  void OnStateChange() override {
    DCHECK(!in_on_state_change_);
    DCHECK(!has_pending_state_change_signal_);
    DCHECK(!waiting_for_lookahead_bytes_);
    base::AutoReset<bool> auto_reset_for_in_on_state_change(
        &in_on_state_change_, true);
    base::AutoReset<bool> auto_reset_for_has_pending_state_change_signal(
        &has_pending_state_change_signal_, false);
    base::AutoReset<bool> auto_reset_for_waiting_for_lookahead_bytes(
        &waiting_for_lookahead_bytes_, false);

    // Do not proceed to read the data if loader is aborted, suspended for non
    // back/forward cache reason, or the state is cancelled.
    if (loader_->IsAborted() || IsSuspendedButNotForBackForwardCache() ||
        state_ == State::kCancelled) {
      return;
    }

    // Proceed to read the data, even if in back/forward cache.
    while (state_ == State::kLoading) {
      // Peek available bytes from |bytes_consumer_| and report them to
      // |loader_|.
      base::span<const char> buffer;
      // Possible state change caused by BeginRead will be realized by the
      // following logic, so we don't need to worry about it here.
      auto result = bytes_consumer_->BeginRead(buffer);
      if (result == Result::kOk) {
        if (lookahead_bytes_ < buffer.size()) {
          loader_->DidReceiveData(buffer.subspan(lookahead_bytes_));
          lookahead_bytes_ = buffer.size();
        }
        // Possible state change caused by EndRead will be realized by the
        // following logic, so we don't need to worry about it here.
        result = bytes_consumer_->EndRead(0);
      }
      waiting_for_lookahead_bytes_ = false;
      if ((result == Result::kOk || result == Result::kShouldWait) &&
          lookahead_bytes_ == 0) {
        // We have no information to notify the client.
        break;
      }
      if (bytes_consumer_client_) {
        bytes_consumer_client_->OnStateChange();
      }
      if (!waiting_for_lookahead_bytes_) {
        break;
      }
    }

    switch (GetPublicState()) {
      case PublicState::kReadableOrWaiting:
        break;
      case PublicState::kClosed:
        HandleResult(Result::kDone);
        break;
      case PublicState::kErrored:
        HandleResult(Result::kError);
        break;
    }

    if (has_pending_state_change_signal_) {
      switch (state_) {
        case State::kLoading:
          NOTREACHED_IN_MIGRATION();
          break;
        case State::kDone:
          loader_->DidFinishLoadingBody();
          break;
        case State::kErrored:
          loader_->DidFailLoadingBody();
          break;
        case State::kCancelled:
          CancelSync();
          break;
      }
    }
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(bytes_consumer_);
    visitor->Trace(loader_);
    visitor->Trace(bytes_consumer_client_);
    BytesConsumer::Trace(visitor);
  }

 private:
  enum class State {
    kLoading,
    kDone,
    kErrored,
    kCancelled,
  };

  void CancelSync() {
    bytes_consumer_->Cancel();
    loader_->DidCancelLoadingBody();
  }

  void HandleResult(Result result) {
    if (state_ != State::kLoading) {
      return;
    }

    if (result == Result::kDone) {
      state_ = State::kDone;
      if (in_on_state_change_) {
        has_pending_state_change_signal_ = true;
      } else {
        task_runner_->PostTask(
            FROM_HERE, base::BindOnce(&ResponseBodyLoader::DidFinishLoadingBody,
                                      WrapWeakPersistent(loader_.Get())));
      }
    }

    if (result == Result::kError) {
      state_ = State::kErrored;
      if (in_on_state_change_) {
        has_pending_state_change_signal_ = true;
      } else {
        task_runner_->PostTask(
            FROM_HERE, base::BindOnce(&ResponseBodyLoader::DidFailLoadingBody,
                                      WrapWeakPersistent(loader_.Get())));
      }
    }
  }

  bool IsSuspendedButNotForBackForwardCache() {
    return loader_->IsSuspended() && !loader_->IsSuspendedForBackForwardCache();
  }

  const Member<BytesConsumer> bytes_consumer_;
  const Member<ResponseBodyLoader> loader_;
  Member<BytesConsumer::Client> bytes_consumer_client_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // The size of body which has been reported to |loader_|.
  size_t lookahead_bytes_ = 0;
  State state_ = State::kLoading;
  bool in_on_state_change_ = false;
  // Set when |state_| changes in OnStateChange.
  bool has_pending_state_change_signal_ = false;
  // Set when BeginRead returns kShouldWait due to |lookahead_bytes_| in
  // OnStateChange.
  bool waiting_for_lookahead_bytes_ = false;
};

class ResponseBodyLoader::Buffer final
    : public GarbageCollected<ResponseBodyLoader::Buffer> {
 public:
  explicit Buffer(ResponseBodyLoader* owner) : owner_(owner) {}

  bool IsEmpty() const { return buffered_data_.empty(); }

  // Add |buffer| to |buffered_data_|.
  void AddChunk(const char* buffer, size_t available) {
    TRACE_EVENT2("loading", "ResponseBodyLoader::Buffer::AddChunk",
                 "total_bytes_read", static_cast<int>(total_bytes_read_),
                 "added_bytes", static_cast<int>(available));
    Vector<char> new_chunk;
    new_chunk.Append(buffer, base::checked_cast<wtf_size_t>(available));
    buffered_data_.emplace_back(std::move(new_chunk));
  }

  // Dispatches the frontmost chunk in |buffered_data_|. Returns the size of
  // the data that got dispatched.
  size_t DispatchChunk(size_t max_chunk_size) {
    // Dispatch the chunk at the front of the queue.
    const Vector<char>& current_chunk = buffered_data_.front();
    DCHECK_LT(offset_in_current_chunk_, current_chunk.size());
    // Send as much of the chunk as possible without exceeding |max_chunk_size|.
    base::span<const char> span(current_chunk);
    span = span.subspan(offset_in_current_chunk_);
    span = span.subspan(0, std::min(span.size(), max_chunk_size));
    owner_->DidReceiveData(span);

    size_t sent_size = span.size();
    offset_in_current_chunk_ += sent_size;
    if (offset_in_current_chunk_ == current_chunk.size()) {
      // We've finished sending the chunk at the front of the queue, pop it so
      // that we'll send the next chunk next time.
      offset_in_current_chunk_ = 0;
      buffered_data_.pop_front();
    }

    return sent_size;
  }

  void Trace(Visitor* visitor) const { visitor->Trace(owner_); }

 private:
  const Member<ResponseBodyLoader> owner_;
  // We save the response body read when suspended as a queue of chunks so that
  // we can free memory as soon as we finish sending a chunk completely.
  Deque<Vector<char>> buffered_data_;
  size_t offset_in_current_chunk_ = 0;
  size_t total_bytes_read_ = 0;
};

ResponseBodyLoader::ResponseBodyLoader(
    BytesConsumer& bytes_consumer,
    ResponseBodyLoaderClient& client,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    BackForwardCacheLoaderHelper* back_forward_cache_loader_helper)
    : task_runner_(std::move(task_runner)),
      bytes_consumer_(bytes_consumer),
      client_(client),
      back_forward_cache_loader_helper_(back_forward_cache_loader_helper) {
  bytes_consumer_->SetClient(this);
  body_buffer_ = MakeGarbageCollected<Buffer>(this);
}

mojo::ScopedDataPipeConsumerHandle ResponseBodyLoader::DrainAsDataPipe(
    ResponseBodyLoaderClient** client) {
  DCHECK(!started_);
  DCHECK(!drained_as_datapipe_);
  DCHECK(!drained_as_bytes_consumer_);
  DCHECK(!aborted_);

  *client = nullptr;
  DCHECK(bytes_consumer_);
  auto data_pipe = bytes_consumer_->DrainAsDataPipe();
  if (!data_pipe) {
    return data_pipe;
  }

  drained_as_datapipe_ = true;
  bytes_consumer_ = nullptr;
  *client = this;
  return data_pipe;
}

BytesConsumer& ResponseBodyLoader::DrainAsBytesConsumer() {
  DCHECK(!started_);
  DCHECK(!drained_as_datapipe_);
  DCHECK(!drained_as_bytes_consumer_);
  DCHECK(!aborted_);
  DCHECK(bytes_consumer_);
  DCHECK(!delegating_bytes_consumer_);

  delegating_bytes_consumer_ = MakeGarbageCollected<DelegatingBytesConsumer>(
      *bytes_consumer_, *this, task_runner_);
  bytes_consumer_->ClearClient();
  bytes_consumer_->SetClient(delegating_bytes_consumer_);
  bytes_consumer_ = nullptr;
  drained_as_bytes_consumer_ = true;
  return *delegating_bytes_consumer_;
}

void ResponseBodyLoader::DidReceiveData(base::span<const char> data) {
  if (aborted_)
    return;

  if (IsSuspendedForBackForwardCache()) {
    // Track the data size for both total per-process bytes and per-request
    // bytes.
    DidBufferLoadWhileInBackForwardCache(data.size());
    if (!BackForwardCacheBufferLimitTracker::Get()
             .IsUnderPerProcessBufferLimit()) {
      EvictFromBackForwardCache(
          mojom::blink::RendererEvictionReason::kNetworkExceedsBufferLimit);
    }
  }

  client_->DidReceiveData(data);
}

void ResponseBodyLoader::DidReceiveDecodedData(
    const String& data,
    std::unique_ptr<ParkableStringImpl::SecureDigest> digest) {
  if (aborted_)
    return;

  client_->DidReceiveDecodedData(data, std::move(digest));
}

void ResponseBodyLoader::DidFinishLoadingBody() {
  if (aborted_) {
    return;
  }

  TRACE_EVENT0("blink", "ResponseBodyLoader::DidFinishLoadingBody");

  if (IsSuspended()) {
    finish_signal_is_pending_ = true;
    return;
  }

  finish_signal_is_pending_ = false;
  client_->DidFinishLoadingBody();
}

void ResponseBodyLoader::DidFailLoadingBody() {
  if (aborted_) {
    return;
  }

  TRACE_EVENT0("blink", "ResponseBodyLoader::DidFailLoadingBody");

  if (IsSuspended()) {
    fail_signal_is_pending_ = true;
    return;
  }

  fail_signal_is_pending_ = false;
  client_->DidFailLoadingBody();
}

void ResponseBodyLoader::DidCancelLoadingBody() {
  if (aborted_) {
    return;
  }

  TRACE_EVENT0("blink", "ResponseBodyLoader::DidCancelLoadingBody");

  if (IsSuspended()) {
    cancel_signal_is_pending_ = true;
    return;
  }

  cancel_signal_is_pending_ = false;
  client_->DidCancelLoadingBody();
}

void ResponseBodyLoader::EvictFromBackForwardCache(
    mojom::blink::RendererEvictionReason reason) {
  if (!back_forward_cache_loader_helper_)
    return;
  DCHECK(IsSuspendedForBackForwardCache());
  back_forward_cache_loader_helper_->EvictFromBackForwardCache(reason);
}

void ResponseBodyLoader::DidBufferLoadWhileInBackForwardCache(
    size_t num_bytes) {
  if (!back_forward_cache_loader_helper_)
    return;
  back_forward_cache_loader_helper_->DidBufferLoadWhileInBackForwardCache(
      /*update_process_wide_count=*/true, num_bytes);
}

void ResponseBodyLoader::Start() {
  DCHECK(!started_);
  DCHECK(!drained_as_datapipe_);
  DCHECK(!drained_as_bytes_consumer_);

  started_ = true;
  OnStateChange();
}

void ResponseBodyLoader::Abort() {
  if (aborted_)
    return;

  aborted_ = true;

  if (bytes_consumer_ && !in_two_phase_read_) {
    bytes_consumer_->Cancel();
  }

  if (delegating_bytes_consumer_) {
    delegating_bytes_consumer_->Abort();
  }
}

void ResponseBodyLoader::Suspend(LoaderFreezeMode mode) {
  if (aborted_)
    return;

  bool was_suspended = (suspended_state_ == LoaderFreezeMode::kStrict);

  suspended_state_ = mode;
  if (IsSuspendedForBackForwardCache()) {
    DCHECK(IsInflightNetworkRequestBackForwardCacheSupportEnabled());
    // If we're already suspended (but not for back-forward cache), we might've
    // ignored some OnStateChange calls.
    if (was_suspended) {
      task_runner_->PostTask(FROM_HERE,
                             base::BindOnce(&ResponseBodyLoader::OnStateChange,
                                            WrapPersistent(this)));
    }
  }
}

void ResponseBodyLoader::EvictFromBackForwardCacheIfDrainedAsBytesConsumer() {
  if (drained_as_bytes_consumer_) {
    if (!base::FeatureList::IsEnabled(
            features::kAllowDatapipeDrainedAsBytesConsumerInBFCache)) {
      EvictFromBackForwardCache(
          mojom::blink::RendererEvictionReason::
              kNetworkRequestDatapipeDrainedAsBytesConsumer);
    }
  }
}

void ResponseBodyLoader::Resume() {
  if (aborted_)
    return;

  DCHECK(IsSuspended());
  suspended_state_ = LoaderFreezeMode::kNone;

  if (finish_signal_is_pending_) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ResponseBodyLoader::DidFinishLoadingBody,
                                  WrapPersistent(this)));
  } else if (fail_signal_is_pending_) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ResponseBodyLoader::DidFailLoadingBody,
                                  WrapPersistent(this)));
  } else if (cancel_signal_is_pending_) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ResponseBodyLoader::DidCancelLoadingBody,
                                  WrapPersistent(this)));
  } else {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&ResponseBodyLoader::OnStateChange,
                                          WrapPersistent(this)));
  }
}

void ResponseBodyLoader::OnStateChange() {
  if (!started_)
    return;

  TRACE_EVENT0("blink", "ResponseBodyLoader::OnStateChange");

  size_t num_bytes_consumed = 0;
  while (!aborted_ && (!IsSuspended() || IsSuspendedForBackForwardCache())) {
    const size_t chunk_size = network::features::GetLoaderChunkSize();
    if (chunk_size == num_bytes_consumed) {
      // We've already consumed many bytes in this task. Defer the remaining
      // to the next task.
      task_runner_->PostTask(FROM_HERE,
                             base::BindOnce(&ResponseBodyLoader::OnStateChange,
                                            WrapPersistent(this)));
      return;
    }

    if (!IsSuspended() && body_buffer_ && !body_buffer_->IsEmpty()) {
      // We need to empty |body_buffer_| first before reading more from
      // |bytes_consumer_|.
      num_bytes_consumed +=
          body_buffer_->DispatchChunk(chunk_size - num_bytes_consumed);
      continue;
    }

    base::span<const char> buffer;
    auto result = bytes_consumer_->BeginRead(buffer);
    if (result == BytesConsumer::Result::kShouldWait)
      return;
    if (result == BytesConsumer::Result::kOk) {
      TRACE_EVENT1("blink", "ResponseBodyLoader::OnStateChange", "available",
                   buffer.size());

      base::AutoReset<bool> auto_reset_for_in_two_phase_read(
          &in_two_phase_read_, true);
      buffer = buffer.first(
          std::min(buffer.size(), chunk_size - num_bytes_consumed));
      if (IsSuspendedForBackForwardCache()) {
        // Save the read data into |body_buffer_| instead.
        DidBufferLoadWhileInBackForwardCache(buffer.size());
        body_buffer_->AddChunk(buffer.data(), buffer.size());
        if (!BackForwardCacheBufferLimitTracker::Get()
                 .IsUnderPerProcessBufferLimit()) {
          // We've read too much data while suspended for back-forward cache.
          // Evict the page from the back-forward cache.
          result = bytes_consumer_->EndRead(buffer.size());
          EvictFromBackForwardCache(
              mojom::blink::RendererEvictionReason::kNetworkExceedsBufferLimit);
          return;
        }
      } else {
        DCHECK(!IsSuspended());
        DidReceiveData(buffer);
      }
      result = bytes_consumer_->EndRead(buffer.size());
      num_bytes_consumed += buffer.size();

      if (aborted_) {
        // As we cannot call Cancel in two-phase read, we need to call it here.
        bytes_consumer_->Cancel();
      }
    }
    DCHECK_NE(result, BytesConsumer::Result::kShouldWait);
    if (IsSuspendedForBackForwardCache() &&
        result != BytesConsumer::Result::kOk) {
      // Don't dispatch finish/failure messages when suspended. We'll dispatch
      // them later when we call OnStateChange again after resuming.
      return;
    }
    if (result == BytesConsumer::Result::kDone) {
      DidFinishLoadingBody();
      return;
    }
    if (result != BytesConsumer::Result::kOk) {
      DidFailLoadingBody();
      Abort();
      return;
    }
  }
}

void ResponseBodyLoader::Trace(Visitor* visitor) const {
  visitor->Trace(bytes_consumer_);
  visitor->Trace(delegating_bytes_consumer_);
  visitor->Trace(client_);
  visitor->Trace(body_buffer_);
  visitor->Trace(back_forward_cache_loader_helper_);
  ResponseBodyLoaderDrainableInterface::Trace(visitor);
  ResponseBodyLoaderClient::Trace(visitor);
  BytesConsumer::Client::Trace(visitor);
}

}  // namespace blink
