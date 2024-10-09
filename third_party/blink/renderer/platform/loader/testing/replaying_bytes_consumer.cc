// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/loader/testing/replaying_bytes_consumer.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

ReplayingBytesConsumer::ReplayingBytesConsumer(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

ReplayingBytesConsumer::~ReplayingBytesConsumer() {}

BytesConsumer::Result ReplayingBytesConsumer::BeginRead(
    base::span<const char>& buffer) {
  DCHECK(!is_in_two_phase_read_);
  ++notification_token_;
  if (commands_.empty()) {
    switch (state_) {
      case BytesConsumer::InternalState::kWaiting:
        return Result::kShouldWait;
      case BytesConsumer::InternalState::kClosed:
        return Result::kDone;
      case BytesConsumer::InternalState::kErrored:
        return Result::kError;
    }
  }
  const Command& command = commands_[0];
  switch (command.GetName()) {
    case Command::kDataAndDone:
    case Command::kData:
      DCHECK_LE(offset_, command.Body().size());
      buffer = base::span(command.Body()).subspan(offset_);
      is_in_two_phase_read_ = true;
      return Result::kOk;
    case Command::kDone:
      commands_.pop_front();
      Close();
      return Result::kDone;
    case Command::kError: {
      Error e(String::FromUTF8(command.Body().data(), command.Body().size()));
      commands_.pop_front();
      MakeErrored(std::move(e));
      return Result::kError;
    }
    case Command::kWait:
      commands_.pop_front();
      state_ = InternalState::kWaiting;
      task_runner_->PostTask(
          FROM_HERE, WTF::BindOnce(&ReplayingBytesConsumer::NotifyAsReadable,
                                   WrapPersistent(this), notification_token_));
      return Result::kShouldWait;
  }
  NOTREACHED_IN_MIGRATION();
  return Result::kError;
}

BytesConsumer::Result ReplayingBytesConsumer::EndRead(size_t read) {
  DCHECK(is_in_two_phase_read_);
  DCHECK(!commands_.empty());

  is_in_two_phase_read_ = false;
  const Command& command = commands_[0];
  const auto name = command.GetName();
  DCHECK(name == Command::kData || name == Command::kDataAndDone);
  offset_ += read;
  DCHECK_LE(offset_, command.Body().size());
  if (offset_ < command.Body().size())
    return Result::kOk;

  offset_ = 0;
  commands_.pop_front();

  if (name == Command::kData)
    return Result::kOk;

  Close();
  return Result::kDone;
}

void ReplayingBytesConsumer::SetClient(Client* client) {
  DCHECK(!client_);
  DCHECK(client);
  client_ = client;
  ++notification_token_;
}

void ReplayingBytesConsumer::ClearClient() {
  DCHECK(client_);
  client_ = nullptr;
  ++notification_token_;
}

void ReplayingBytesConsumer::Cancel() {
  Close();
  is_cancelled_ = true;
}

BytesConsumer::PublicState ReplayingBytesConsumer::GetPublicState() const {
  return GetPublicStateFromInternalState(state_);
}

BytesConsumer::Error ReplayingBytesConsumer::GetError() const {
  return error_;
}

void ReplayingBytesConsumer::NotifyAsReadable(int notification_token) {
  if (notification_token_ != notification_token) {
    // The notification is cancelled.
    return;
  }
  DCHECK(client_);
  DCHECK_NE(InternalState::kClosed, state_);
  DCHECK_NE(InternalState::kErrored, state_);
  client_->OnStateChange();
}

void ReplayingBytesConsumer::Close() {
  commands_.clear();
  offset_ = 0;
  state_ = InternalState::kClosed;
  ++notification_token_;
}

void ReplayingBytesConsumer::MakeErrored(const Error& e) {
  commands_.clear();
  offset_ = 0;
  error_ = e;
  state_ = InternalState::kErrored;
  ++notification_token_;
}

void ReplayingBytesConsumer::Trace(Visitor* visitor) const {
  visitor->Trace(client_);
  BytesConsumer::Trace(visitor);
}

}  // namespace blink
