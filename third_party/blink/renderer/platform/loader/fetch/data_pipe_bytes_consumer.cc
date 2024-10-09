// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/data_pipe_bytes_consumer.h"

#include <algorithm>

#include "base/containers/span.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

void DataPipeBytesConsumer::CompletionNotifier::SignalComplete() {
  if (bytes_consumer_)
    bytes_consumer_->SignalComplete();
}

void DataPipeBytesConsumer::CompletionNotifier::SignalSize(uint64_t size) {
  if (bytes_consumer_)
    bytes_consumer_->SignalSize(size);
}

void DataPipeBytesConsumer::CompletionNotifier::SignalError(
    const BytesConsumer::Error& error) {
  if (bytes_consumer_)
    bytes_consumer_->SignalError(error);
}

void DataPipeBytesConsumer::CompletionNotifier::Trace(Visitor* visitor) const {
  visitor->Trace(bytes_consumer_);
}

DataPipeBytesConsumer::DataPipeBytesConsumer(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojo::ScopedDataPipeConsumerHandle data_pipe,
    CompletionNotifier** notifier)
    : task_runner_(std::move(task_runner)),
      data_pipe_(std::move(data_pipe)),
      watcher_(FROM_HERE,
               mojo::SimpleWatcher::ArmingPolicy::MANUAL,
               task_runner_) {
  DCHECK(data_pipe_.is_valid());
  *notifier = MakeGarbageCollected<CompletionNotifier>(this);
  watcher_.Watch(
      data_pipe_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      WTF::BindRepeating(&DataPipeBytesConsumer::Notify, WrapPersistent(this)));
}

DataPipeBytesConsumer::~DataPipeBytesConsumer() {}

BytesConsumer::Result DataPipeBytesConsumer::BeginRead(
    base::span<const char>& buffer) {
  DCHECK(!is_in_two_phase_read_);
  buffer = {};
  if (state_ == InternalState::kClosed)
    return Result::kDone;
  if (state_ == InternalState::kErrored)
    return Result::kError;

  // If we have already reached the end of the pipe then we are simply
  // waiting for either SignalComplete() or SignalError() to be called.
  if (!data_pipe_.is_valid())
    return Result::kShouldWait;

  base::span<const uint8_t> bytes;
  MojoResult rv = data_pipe_->BeginReadData(MOJO_READ_DATA_FLAG_NONE, bytes);
  switch (rv) {
    case MOJO_RESULT_OK:
      is_in_two_phase_read_ = true;
      buffer = base::as_chars(bytes);
      return Result::kOk;
    case MOJO_RESULT_SHOULD_WAIT:
      watcher_.ArmOrNotify();
      return Result::kShouldWait;
    case MOJO_RESULT_FAILED_PRECONDITION:
      ClearDataPipe();
      if (total_size_ && num_read_bytes_ < *total_size_) {
        SetError(Error("error"));
        return Result::kError;
      }
      MaybeClose();
      // We hit the end of the pipe, but we may still need to wait for
      // SignalComplete() or SignalError() to be called.
      if (IsWaiting()) {
        return Result::kShouldWait;
      }
      return Result::kDone;
    default:
      SetError(Error("error"));
      return Result::kError;
  }

  NOTREACHED_IN_MIGRATION();
}

BytesConsumer::Result DataPipeBytesConsumer::EndRead(size_t read) {
  DCHECK(is_in_two_phase_read_);
  is_in_two_phase_read_ = false;
  DCHECK(IsWaiting());
  MojoResult rv = data_pipe_->EndReadData(base::checked_cast<uint32_t>(read));
  if (rv != MOJO_RESULT_OK) {
    SetError(Error("error"));
    return Result::kError;
  }
  num_read_bytes_ += read;
  if (has_pending_complete_) {
    has_pending_complete_ = false;
    SignalComplete();
    return Result::kOk;
  }
  if (has_pending_error_) {
    has_pending_error_ = false;
    SignalError(Error("error"));
    return Result::kError;
  }
  if (total_size_ == num_read_bytes_) {
    ClearDataPipe();
    ClearClient();
    SignalComplete();
    return Result::kDone;
  }

  if (has_pending_notification_) {
    has_pending_notification_ = false;
    task_runner_->PostTask(FROM_HERE,
                           WTF::BindOnce(&DataPipeBytesConsumer::Notify,
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
  if (IsWaiting()) {
    client_ = client;
  }
}

void DataPipeBytesConsumer::ClearClient() {
  client_ = nullptr;
}

void DataPipeBytesConsumer::Cancel() {
  DCHECK(!is_in_two_phase_read_);
  ClearClient();
  ClearDataPipe();
  SignalComplete();
}

BytesConsumer::PublicState DataPipeBytesConsumer::GetPublicState() const {
  return GetPublicStateFromInternalState(state_);
}

void DataPipeBytesConsumer::Trace(Visitor* visitor) const {
  visitor->Trace(client_);
  BytesConsumer::Trace(visitor);
}

bool DataPipeBytesConsumer::IsWaiting() const {
  return state_ == InternalState::kWaiting;
}

void DataPipeBytesConsumer::MaybeClose() {
  DCHECK(!is_in_two_phase_read_);
  if (!completion_signaled_ || data_pipe_.is_valid() || !IsWaiting()) {
    return;
  }
  DCHECK(!watcher_.IsWatching());
  state_ = InternalState::kClosed;
  ClearClient();
}

void DataPipeBytesConsumer::SignalComplete() {
  if (!IsWaiting() || has_pending_complete_ || has_pending_error_) {
    return;
  }
  if (is_in_two_phase_read_) {
    has_pending_complete_ = true;
    return;
  }
  completion_signaled_ = true;
  Client* client = client_;
  MaybeClose();
  if (!IsWaiting()) {
    if (client)
      client->OnStateChange();
    return;
  }
  // We have the explicit completion signal, but we may still need to wait
  // to hit the end of the pipe.  Arm the watcher to make sure we see the
  // pipe close even if the stream is not being actively read.
  watcher_.ArmOrNotify();
}

void DataPipeBytesConsumer::SignalSize(uint64_t size) {
  if (!IsWaiting() || has_pending_complete_ || has_pending_error_) {
    return;
  }
  total_size_ = std::make_optional(size);
  DCHECK_LE(num_read_bytes_, *total_size_);
  if (!data_pipe_.is_valid() && num_read_bytes_ < *total_size_) {
    SignalError(Error());
    return;
  }

  if (!is_in_two_phase_read_ && *total_size_ == num_read_bytes_) {
    ClearDataPipe();
    SignalComplete();
  }
}

void DataPipeBytesConsumer::SignalError(const Error& error) {
  if (!IsWaiting() || has_pending_complete_ || has_pending_error_) {
    return;
  }
  if (is_in_two_phase_read_) {
    has_pending_error_ = true;
    return;
  }
  Client* client = client_;
  // When we hit an error we switch states immediately.  We don't wait for the
  // end of the pipe to be read.
  SetError(error);
  if (client)
    client->OnStateChange();
}

void DataPipeBytesConsumer::SetError(const Error& error) {
  DCHECK(!is_in_two_phase_read_);
  if (!IsWaiting()) {
    return;
  }
  ClearDataPipe();
  state_ = InternalState::kErrored;
  error_ = error;
  ClearClient();
}

void DataPipeBytesConsumer::Notify(MojoResult) {
  if (!IsWaiting()) {
    return;
  }

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
    if (IsWaiting()) {
      return;
    }
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

void DataPipeBytesConsumer::Dispose() {
  watcher_.Cancel();
}

}  // namespace blink
