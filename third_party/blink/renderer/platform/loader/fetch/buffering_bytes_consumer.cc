// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/buffering_bytes_consumer.h"

#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {
constexpr int32_t kDelayMilliseconds = 50;
}  // namespace

// static
BufferingBytesConsumer* BufferingBytesConsumer::CreateWithDelay(
    BytesConsumer* bytes_consumer,
    scoped_refptr<base::SingleThreadTaskRunner> timer_task_runner) {
  return MakeGarbageCollected<BufferingBytesConsumer>(
      base::PassKey<BufferingBytesConsumer>(), bytes_consumer,
      std::move(timer_task_runner), base::Milliseconds(kDelayMilliseconds));
}

// static
BufferingBytesConsumer* BufferingBytesConsumer::Create(
    BytesConsumer* bytes_consumer) {
  return MakeGarbageCollected<BufferingBytesConsumer>(
      base::PassKey<BufferingBytesConsumer>(), bytes_consumer, nullptr,
      base::TimeDelta());
}

BufferingBytesConsumer::BufferingBytesConsumer(
    base::PassKey<BufferingBytesConsumer> key,
    BytesConsumer* bytes_consumer,
    scoped_refptr<base::SingleThreadTaskRunner> timer_task_runner,
    base::TimeDelta buffering_start_delay)
    : bytes_consumer_(bytes_consumer),
      timer_(std::move(timer_task_runner),
             this,
             &BufferingBytesConsumer::OnTimerFired),
      is_limiting_total_buffer_size_(
          RuntimeEnabledFeatures::BufferedBytesConsumerLimitSizeEnabled()) {
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

BytesConsumer::Result BufferingBytesConsumer::BeginRead(
    base::span<const char>& buffer) {
  // Stop delaying buffering on the first read as it will no longer be safe to
  // drain the underlying |bytes_consumer_| anyway.
  MaybeStartBuffering();

  if (buffer_.empty()) {
    if (buffering_state_ != BufferingState::kStarted)
      return bytes_consumer_->BeginRead(buffer);

    if (has_seen_error_)
      return Result::kError;

    if (has_seen_end_of_data_) {
      ClearClient();
      return Result::kDone;
    }

    BufferData();

    if (has_seen_error_)
      return Result::kError;

    if (buffer_.empty())
      return has_seen_end_of_data_ ? Result::kDone : Result::kShouldWait;
  }

  HeapVector<char>* first_chunk = buffer_[0];
  DCHECK_LT(offset_for_first_chunk_, first_chunk->size());
  buffer = base::span(*first_chunk).subspan(offset_for_first_chunk_);
  return Result::kOk;
}

BytesConsumer::Result BufferingBytesConsumer::EndRead(size_t read_size) {
  if (buffer_.empty()) {
    if (buffering_state_ != BufferingState::kStarted)
      return bytes_consumer_->EndRead(read_size);

    DCHECK(has_seen_error_);
    return Result::kError;
  }

  HeapVector<char>* first_chunk = buffer_[0];

  DCHECK_LE(offset_for_first_chunk_ + read_size, first_chunk->size());
  offset_for_first_chunk_ += read_size;

  if (offset_for_first_chunk_ == first_chunk->size()) {
    const bool was_waiting_for_capacity = is_limiting_total_buffer_size_ &&
                                          !has_seen_end_of_data_ &&
                                          total_buffer_size_ >= kMaxBufferSize;
    total_buffer_size_ -= first_chunk->size();
    offset_for_first_chunk_ = 0;
    // Actively clear the unused HeapVector at this point. This allows the GC to
    // immediately reclaim it before any garbage collection is otherwise
    // triggered. This is useful in this high-performance case.
    first_chunk->clear();
    first_chunk = nullptr;
    buffer_.pop_front();
    if (was_waiting_for_capacity && total_buffer_size_ < kMaxBufferSize) {
      // We might have stopped buffering due to not having enough space, so try
      // reading more.
      BufferData();
      if (has_seen_error_) {
        DCHECK(buffer_.empty());
        return Result::kError;
      }
    }
  }

  if (buffer_.empty() && has_seen_end_of_data_) {
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_ = client;
}

void BufferingBytesConsumer::ClearClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_ = nullptr;
}

void BufferingBytesConsumer::Cancel() {
  ClearClient();
  bytes_consumer_->Cancel();
}

BytesConsumer::PublicState BufferingBytesConsumer::GetPublicState() const {
  if (buffer_.empty())
    return bytes_consumer_->GetPublicState();
  return PublicState::kReadableOrWaiting;
}

BytesConsumer::Error BufferingBytesConsumer::GetError() const {
  return bytes_consumer_->GetError();
}

String BufferingBytesConsumer::DebugName() const {
  StringBuilder builder;
  builder.Append("BufferingBytesConsumer(");
  builder.Append(bytes_consumer_->DebugName());
  builder.Append(")");
  return builder.ToString();
}

void BufferingBytesConsumer::Trace(Visitor* visitor) const {
  visitor->Trace(bytes_consumer_);
  visitor->Trace(client_);
  visitor->Trace(timer_);
  visitor->Trace(buffer_);
  BytesConsumer::Trace(visitor);
  BytesConsumer::Client::Trace(visitor);
}

void BufferingBytesConsumer::OnTimerFired(TimerBase*) {
  MaybeStartBuffering();
}

void BufferingBytesConsumer::OnStateChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BytesConsumer::Client* client = client_;
  BufferData();
  if (client)
    client->OnStateChange();
}

void BufferingBytesConsumer::BufferData() {
  if (buffering_state_ != BufferingState::kStarted)
    return;

  DCHECK(bytes_consumer_);
  while (!is_limiting_total_buffer_size_ ||
         total_buffer_size_ < kMaxBufferSize) {
    base::span<const char> p;
    auto result = bytes_consumer_->BeginRead(p);
    if (result == Result::kShouldWait)
      return;
    if (result == Result::kOk) {
      auto* chunk = MakeGarbageCollected<HeapVector<char>>();
      chunk->AppendSpan(p);
      buffer_.push_back(chunk);
      total_buffer_size_ += chunk->size();
      result = bytes_consumer_->EndRead(p.size());
    }
    if (result == Result::kDone) {
      has_seen_end_of_data_ = true;
      ClearClient();
      return;
    }
    if (result != Result::kOk) {
      buffer_.clear();
      total_buffer_size_ = 0;
      has_seen_error_ = true;
      ClearClient();
      return;
    }
  }
}

}  // namespace blink
