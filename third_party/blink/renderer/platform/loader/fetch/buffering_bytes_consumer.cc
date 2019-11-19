// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/buffering_bytes_consumer.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

// static
BufferingBytesConsumer* BufferingBytesConsumer::CreateWithDelay(
    BytesConsumer* bytes_consumer,
    scoped_refptr<base::SingleThreadTaskRunner> timer_task_runner) {
  if (!base::FeatureList::IsEnabled(features::kBufferingBytesConsumerDelay))
    return Create(bytes_consumer);

  return MakeGarbageCollected<BufferingBytesConsumer>(
      util::PassKey<BufferingBytesConsumer>(), bytes_consumer,
      std::move(timer_task_runner),
      base::TimeDelta::FromMilliseconds(
          features::kBufferingBytesConsumerDelayMilliseconds.Get()));
}

// static
BufferingBytesConsumer* BufferingBytesConsumer::Create(
    BytesConsumer* bytes_consumer) {
  return MakeGarbageCollected<BufferingBytesConsumer>(
      util::PassKey<BufferingBytesConsumer>(), bytes_consumer, nullptr,
      base::TimeDelta());
}

BufferingBytesConsumer::BufferingBytesConsumer(
    util::PassKey<BufferingBytesConsumer> key,
    BytesConsumer* bytes_consumer,
    scoped_refptr<base::SingleThreadTaskRunner> timer_task_runner,
    base::TimeDelta buffering_start_delay)
    : bytes_consumer_(bytes_consumer),
      timer_(std::move(timer_task_runner),
             this,
             &BufferingBytesConsumer::OnTimerFired) {
  bytes_consumer_->SetClient(this);
  if (buffering_start_delay.is_zero()) {
    MaybeStartBuffering();
    return;
  }
  timer_.StartOneShot(buffering_start_delay, FROM_HERE);
}

BufferingBytesConsumer::~BufferingBytesConsumer() = default;

void BufferingBytesConsumer::MaybeStartBuffering() {
  if (buffering_state_ != BufferingState::kDelayed)
    return;
  timer_.Stop();
  buffering_state_ = BufferingState::kStarted;
  BufferData();
}

void BufferingBytesConsumer::StopBuffering() {
  timer_.Stop();
  buffering_state_ = BufferingState::kStopped;
}

BytesConsumer::Result BufferingBytesConsumer::BeginRead(const char** buffer,
                                                        size_t* available) {
  // Stop delaying buffering on the first read as it will no longer be safe to
  // drain the underlying |bytes_consumer_| anyway.
  MaybeStartBuffering();

  if (buffer_.IsEmpty()) {
    if (buffering_state_ != BufferingState::kStarted)
      return bytes_consumer_->BeginRead(buffer, available);

    if (has_seen_error_)
      return Result::kError;

    if (has_seen_end_of_data_) {
      ClearClient();
      return Result::kDone;
    }

    BufferData();

    if (has_seen_error_)
      return Result::kError;

    if (buffer_.IsEmpty())
      return has_seen_end_of_data_ ? Result::kDone : Result::kShouldWait;
  }

  DCHECK_LT(offset_for_first_chunk_, buffer_[0].size());
  *buffer = buffer_[0].data() + offset_for_first_chunk_;
  *available = buffer_[0].size() - offset_for_first_chunk_;
  return Result::kOk;
}

BytesConsumer::Result BufferingBytesConsumer::EndRead(size_t read_size) {
  if (buffer_.IsEmpty()) {
    if (buffering_state_ != BufferingState::kStarted)
      return bytes_consumer_->EndRead(read_size);

    DCHECK(has_seen_error_);
    return Result::kError;
  }

  DCHECK_LE(offset_for_first_chunk_ + read_size, buffer_[0].size());
  offset_for_first_chunk_ += read_size;

  if (offset_for_first_chunk_ == buffer_[0].size()) {
    offset_for_first_chunk_ = 0;
    buffer_.pop_front();
  }

  if (buffer_.IsEmpty() && has_seen_end_of_data_) {
    ClearClient();
    return Result::kDone;
  }
  return Result::kOk;
}

scoped_refptr<BlobDataHandle> BufferingBytesConsumer::DrainAsBlobDataHandle(
    BlobSizePolicy policy) {
  return bytes_consumer_->DrainAsBlobDataHandle(policy);
}

scoped_refptr<EncodedFormData> BufferingBytesConsumer::DrainAsFormData() {
  return bytes_consumer_->DrainAsFormData();
}

mojo::ScopedDataPipeConsumerHandle BufferingBytesConsumer::DrainAsDataPipe() {
  if (buffering_state_ != BufferingState::kStarted)
    return bytes_consumer_->DrainAsDataPipe();

  // We intentionally return an empty handle here, because returning a DataPipe
  // may activate back pressure.
  return {};
}

void BufferingBytesConsumer::SetClient(BytesConsumer::Client* client) {
  client_ = client;
}

void BufferingBytesConsumer::ClearClient() {
  client_ = nullptr;
}

void BufferingBytesConsumer::Cancel() {
  ClearClient();
  bytes_consumer_->Cancel();
}

BytesConsumer::PublicState BufferingBytesConsumer::GetPublicState() const {
  if (buffer_.IsEmpty())
    return bytes_consumer_->GetPublicState();
  return PublicState::kReadableOrWaiting;
}

BytesConsumer::Error BufferingBytesConsumer::GetError() const {
  return bytes_consumer_->GetError();
}

void BufferingBytesConsumer::Trace(Visitor* visitor) {
  visitor->Trace(bytes_consumer_);
  visitor->Trace(client_);
  BytesConsumer::Trace(visitor);
  BytesConsumer::Client::Trace(visitor);
}

void BufferingBytesConsumer::OnTimerFired(TimerBase*) {
  MaybeStartBuffering();
}

void BufferingBytesConsumer::OnStateChange() {
  BytesConsumer::Client* client = client_;
  BufferData();
  if (client)
    client->OnStateChange();
}

void BufferingBytesConsumer::BufferData() {
  if (buffering_state_ != BufferingState::kStarted)
    return;

  while (true) {
    const char* p = nullptr;
    size_t available = 0;
    auto result = bytes_consumer_->BeginRead(&p, &available);
    if (result == Result::kShouldWait)
      return;
    if (result == Result::kOk) {
      Vector<char> chunk;
      chunk.Append(p, SafeCast<wtf_size_t>(available));
      buffer_.push_back(std::move(chunk));
      result = bytes_consumer_->EndRead(available);
    }
    if (result == Result::kDone) {
      has_seen_end_of_data_ = true;
      ClearClient();
      return;
    }
    if (result != Result::kOk) {
      buffer_.clear();
      has_seen_error_ = true;
      ClearClient();
      return;
    }
  }
}

}  // namespace blink
