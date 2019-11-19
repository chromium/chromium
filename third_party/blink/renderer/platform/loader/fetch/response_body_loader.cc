// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/response_body_loader.h"

#include <algorithm>
#include <utility>
#include "base/auto_reset.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

constexpr size_t ResponseBodyLoader::kMaxNumConsumedBytesInTask;

class ResponseBodyLoader::DelegatingBytesConsumer final
    : public BytesConsumer,
      public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(DelegatingBytesConsumer);

 public:
  DelegatingBytesConsumer(
      BytesConsumer& bytes_consumer,
      ResponseBodyLoader& loader,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : bytes_consumer_(bytes_consumer),
        loader_(loader),
        task_runner_(std::move(task_runner)) {}

  Result BeginRead(const char** buffer, size_t* available) override {
    *buffer = nullptr;
    *available = 0;
    if (loader_->IsAborted()) {
      return Result::kError;
    }
    if (loader_->IsSuspended()) {
      return Result::kShouldWait;
    }
    auto result = bytes_consumer_->BeginRead(buffer, available);
    if (result == Result::kOk) {
      *available = std::min(*available, lookahead_bytes_);
      if (*available == 0) {
        result = bytes_consumer_->EndRead(0);
        *buffer = nullptr;
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
    bytes_consumer_->Cancel();

    if (in_on_state_change_) {
      has_pending_state_change_signal_ = true;
      return;
    }
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ResponseBodyLoader::DidCancelLoadingBody,
                                  WrapWeakPersistent(loader_.Get())));
  }
  PublicState GetPublicState() const override {
    if (loader_->IsAborted())
      return PublicState::kErrored;
    return bytes_consumer_->GetPublicState();
  }
  Error GetError() const override { return bytes_consumer_->GetError(); }
  String DebugName() const override { return "DelegatingBytesConsumer"; }

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

    if (loader_->IsAborted() || loader_->IsSuspended() ||
        state_ == State::kCancelled) {
      return;
    }
    while (state_ == State::kLoading) {
      // Peek available bytes from |bytes_consumer_| and report them to
      // |loader_|.
      const char* buffer = nullptr;
      size_t available = 0;
      // Possible state change caused by BeginRead will be realized by the
      // following logic, so we don't need to worry about it here.
      auto result = bytes_consumer_->BeginRead(&buffer, &available);
      if (result == Result::kOk) {
        if (lookahead_bytes_ < available) {
          loader_->DidReceiveData(base::make_span(
              buffer + lookahead_bytes_, available - lookahead_bytes_));
          lookahead_bytes_ = available;
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
          NOTREACHED();
          break;
        case State::kDone:
          loader_->DidFinishLoadingBody();
          break;
        case State::kErrored:
          loader_->DidFailLoadingBody();
          break;
        case State::kCancelled:
          loader_->DidCancelLoadingBody();
          break;
      }
    }
  }

  void Trace(Visitor* visitor) override {
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

ResponseBodyLoader::ResponseBodyLoader(
    BytesConsumer& bytes_consumer,
    ResponseBodyLoaderClient& client,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : bytes_consumer_(bytes_consumer),
      client_(client),
      task_runner_(std::move(task_runner)) {
  bytes_consumer_->SetClient(this);
}

mojo::ScopedDataPipeConsumerHandle ResponseBodyLoader::DrainAsDataPipe(
    ResponseBodyLoaderClient** client) {
  DCHECK(!started_);
  DCHECK(!drained_);
  DCHECK(!aborted_);

  *client = nullptr;
  DCHECK(bytes_consumer_);
  auto data_pipe = bytes_consumer_->DrainAsDataPipe();
  if (!data_pipe) {
    return data_pipe;
  }

  drained_ = true;
  bytes_consumer_ = nullptr;
  *client = this;
  return data_pipe;
}

BytesConsumer& ResponseBodyLoader::DrainAsBytesConsumer() {
  DCHECK(!started_);
  DCHECK(!drained_);
  DCHECK(!aborted_);
  DCHECK(bytes_consumer_);
  DCHECK(!delegating_bytes_consumer_);

  delegating_bytes_consumer_ = MakeGarbageCollected<DelegatingBytesConsumer>(
      *bytes_consumer_, *this, task_runner_);
  bytes_consumer_->ClearClient();
  bytes_consumer_->SetClient(delegating_bytes_consumer_);
  bytes_consumer_ = nullptr;
  drained_ = true;
  return *delegating_bytes_consumer_;
}

void ResponseBodyLoader::DidReceiveData(base::span<const char> data) {
  if (aborted_)
    return;

  client_->DidReceiveData(data);
}

void ResponseBodyLoader::DidFinishLoadingBody() {
  if (aborted_)
    return;

  if (suspended_) {
    finish_signal_is_pending_ = true;
    return;
  }

  finish_signal_is_pending_ = false;
  client_->DidFinishLoadingBody();
}

void ResponseBodyLoader::DidFailLoadingBody() {
  if (aborted_)
    return;

  if (suspended_) {
    fail_signal_is_pending_ = true;
    return;
  }

  fail_signal_is_pending_ = false;
  client_->DidFailLoadingBody();
}

void ResponseBodyLoader::DidCancelLoadingBody() {
  if (aborted_)
    return;

  if (suspended_) {
    cancel_signal_is_pending_ = true;
    return;
  }

  cancel_signal_is_pending_ = false;
  client_->DidCancelLoadingBody();
}

void ResponseBodyLoader::Start() {
  DCHECK(!started_);
  DCHECK(!drained_);

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

void ResponseBodyLoader::Suspend() {
  if (aborted_)
    return;

  DCHECK(!suspended_);
  suspended_ = true;
}

void ResponseBodyLoader::Resume() {
  if (aborted_)
    return;

  DCHECK(suspended_);
  suspended_ = false;

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

  while (!aborted_ && !suspended_) {
    if (kMaxNumConsumedBytesInTask == num_bytes_consumed) {
      // We've already consumed many bytes in this task. Defer the remaining
      // to the next task.
      task_runner_->PostTask(FROM_HERE,
                             base::BindOnce(&ResponseBodyLoader::OnStateChange,
                                            WrapPersistent(this)));
      return;
    }

    const char* buffer = nullptr;
    size_t available = 0;
    auto result = bytes_consumer_->BeginRead(&buffer, &available);
    if (result == BytesConsumer::Result::kShouldWait)
      return;
    if (result == BytesConsumer::Result::kOk) {
      TRACE_EVENT1("blink", "ResponseBodyLoader::OnStateChange", "available",
                   available);
      in_two_phase_read_ = true;

      available =
          std::min(available, kMaxNumConsumedBytesInTask - num_bytes_consumed);
      DidReceiveData(base::make_span(buffer, available));
      result = bytes_consumer_->EndRead(available);
      in_two_phase_read_ = false;
      num_bytes_consumed += available;

      if (aborted_) {
        // As we cannot call Cancel in two-phase read, we need to call it here.
        bytes_consumer_->Cancel();
      }
    }
    DCHECK_NE(result, BytesConsumer::Result::kShouldWait);
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

void ResponseBodyLoader::Trace(Visitor* visitor) {
  visitor->Trace(bytes_consumer_);
  visitor->Trace(delegating_bytes_consumer_);
  visitor->Trace(client_);
  ResponseBodyLoaderDrainableInterface::Trace(visitor);
  ResponseBodyLoaderClient::Trace(visitor);
  BytesConsumer::Client::Trace(visitor);
}

}  // namespace blink
