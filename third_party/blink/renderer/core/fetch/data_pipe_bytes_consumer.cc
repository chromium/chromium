// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/data_pipe_bytes_consumer.h"

#include <algorithm>

#include "base/location.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

DataPipeBytesConsumer::DataPipeBytesConsumer(
    ExecutionContext* execution_context,
    mojo::ScopedDataPipeConsumerHandle data_pipe)
    : execution_context_(execution_context),
      data_pipe_(std::move(data_pipe)),
      watcher_(FROM_HERE,
               mojo::SimpleWatcher::ArmingPolicy::MANUAL,
               execution_context->GetTaskRunner(TaskType::kNetworking)) {
  if (!data_pipe_.is_valid())
    return;
  watcher_.Watch(
      data_pipe_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      WTF::BindRepeating(&DataPipeBytesConsumer::Notify, WrapPersistent(this)));
}

DataPipeBytesConsumer::~DataPipeBytesConsumer() {}

BytesConsumer::Result DataPipeBytesConsumer::BeginRead(const char** buffer,
                                                       size_t* available) {
  DCHECK(!is_in_two_phase_read_);
  *buffer = nullptr;
  *available = 0;
  if (state_ == InternalState::kClosed)
    return Result::kDone;
  if (state_ == InternalState::kErrored)
    return Result::kError;

  // If we have already reached the end of the pipe then we are simply
  // waiting for either SignalComplete() or SignalError() to be called.
  if (!data_pipe_.is_valid())
    return Result::kShouldWait;

  uint32_t pipe_available = 0;
  MojoResult rv =
      data_pipe_->BeginReadData(reinterpret_cast<const void**>(buffer),
                                &pipe_available, MOJO_READ_DATA_FLAG_NONE);

  switch (rv) {
    case MOJO_RESULT_OK:
      is_in_two_phase_read_ = true;
      *available = pipe_available;
      return Result::kOk;
    case MOJO_RESULT_SHOULD_WAIT:
      watcher_.ArmOrNotify();
      return Result::kShouldWait;
    case MOJO_RESULT_FAILED_PRECONDITION:
      ClearDataPipe();
      MaybeClose();
      // We hit the end of the pipe, but we may still need to wait for
      // SignalComplete() or SignalError() to be called.
      if (IsReadableOrWaiting())
        return Result::kShouldWait;
      return Result::kDone;
    default:
      SetError();
      return Result::kError;
  }

  NOTREACHED();
}

BytesConsumer::Result DataPipeBytesConsumer::EndRead(size_t read) {
  DCHECK(is_in_two_phase_read_);
  is_in_two_phase_read_ = false;
  DCHECK(IsReadableOrWaiting());
  MojoResult rv = data_pipe_->EndReadData(read);
  if (rv != MOJO_RESULT_OK) {
    SetError();
    return Result::kError;
  }
  if (has_pending_complete_) {
    has_pending_complete_ = false;
    SignalComplete();
    return Result::kOk;
  }
  if (has_pending_error_) {
    has_pending_error_ = false;
    SignalError();
    return Result::kError;
  }
  if (has_pending_notification_) {
    has_pending_notification_ = false;
    execution_context_->GetTaskRunner(TaskType::kNetworking)
        ->PostTask(FROM_HERE, WTF::Bind(&DataPipeBytesConsumer::Notify,
                                        WrapPersistent(this), MOJO_RESULT_OK));
  }
  return Result::kOk;
}

mojo::ScopedDataPipeConsumerHandle DataPipeBytesConsumer::DrainAsDataPipe() {
  DCHECK(!is_in_two_phase_read_);
  watcher_.Cancel();
  mojo::ScopedDataPipeConsumerHandle data_pipe = std::move(data_pipe_);
  MaybeClose();
  // The caller is responsible for calling GetPublicState to determine if
  // the consumer has closed due to draining.
  return data_pipe;
}

void DataPipeBytesConsumer::SetClient(BytesConsumer::Client* client) {
  DCHECK(!client_);
  DCHECK(client);
  if (IsReadableOrWaiting())
    client_ = client;
}

void DataPipeBytesConsumer::ClearClient() {
  client_ = nullptr;
}

void DataPipeBytesConsumer::Cancel() {
  DCHECK(!is_in_two_phase_read_);
  ClearDataPipe();
  SignalComplete();
}

BytesConsumer::PublicState DataPipeBytesConsumer::GetPublicState() const {
  return GetPublicStateFromInternalState(state_);
}

void DataPipeBytesConsumer::Trace(blink::Visitor* visitor) {
  visitor->Trace(execution_context_);
  visitor->Trace(client_);
  BytesConsumer::Trace(visitor);
}

bool DataPipeBytesConsumer::IsReadableOrWaiting() const {
  return state_ == InternalState::kReadable ||
         state_ == InternalState::kWaiting;
}

void DataPipeBytesConsumer::MaybeClose() {
  DCHECK(!is_in_two_phase_read_);
  if (!completion_signaled_ || data_pipe_.is_valid() || !IsReadableOrWaiting())
    return;
  DCHECK(!watcher_.IsWatching());
  state_ = InternalState::kClosed;
  ClearClient();
}

void DataPipeBytesConsumer::SignalComplete() {
  if (!IsReadableOrWaiting() || has_pending_complete_ || has_pending_error_)
    return;
  if (is_in_two_phase_read_) {
    has_pending_complete_ = true;
    return;
  }
  completion_signaled_ = true;
  Client* client = client_;
  MaybeClose();
  if (!IsReadableOrWaiting()) {
    if (client)
      client->OnStateChange();
    return;
  }
  // We have the explicit completion signal, but we may still need to wait
  // to hit the end of the pipe.  Arm the watcher to make sure we see the
  // pipe close even if the stream is not being actively read.
  watcher_.ArmOrNotify();
}

void DataPipeBytesConsumer::SignalError() {
  if (!IsReadableOrWaiting() || has_pending_complete_ || has_pending_error_)
    return;
  if (is_in_two_phase_read_) {
    has_pending_error_ = true;
    return;
  }
  Client* client = client_;
  // When we hit an error we switch states immediately.  We don't wait for the
  // end of the pipe to be read.
  SetError();
  if (client)
    client->OnStateChange();
}

void DataPipeBytesConsumer::SetError() {
  DCHECK(!is_in_two_phase_read_);
  if (!IsReadableOrWaiting())
    return;
  ClearDataPipe();
  state_ = InternalState::kErrored;
  error_ = Error("error");
  ClearClient();
}

void DataPipeBytesConsumer::Notify(MojoResult) {
  if (!IsReadableOrWaiting())
    return;

  // If the pipe signals us in the middle of our client reading, then delay
  // processing the signal until the read is complete.
  if (is_in_two_phase_read_) {
    has_pending_notification_ = true;
    return;
  }

  // Use QuerySignalsState() instead of a zero-length read so that we can
  // detect a closed pipe with data left to read.  A zero-length read cannot
  // distinguish that case from the end of the pipe.
  mojo::HandleSignalsState state = data_pipe_->QuerySignalsState();

  BytesConsumer::Client* client = client_;

  if (state.never_readable()) {
    // We've reached the end of the pipe.
    ClearDataPipe();
    MaybeClose();
    // If we're still waiting for the explicit completion signal then
    // return immediately.  The client needs to keep waiting.
    if (IsReadableOrWaiting())
      return;
  } else if (!state.readable()) {
    // We were signaled, but the pipe is still not readable.  Continue to wait.
    // We don't need to notify the client.
    watcher_.ArmOrNotify();
    return;
  }

  if (client)
    client->OnStateChange();
}

void DataPipeBytesConsumer::ClearDataPipe() {
  DCHECK(!is_in_two_phase_read_);
  watcher_.Cancel();
  data_pipe_.reset();
}

}  // namespace blink
