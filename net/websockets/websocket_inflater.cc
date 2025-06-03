// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_inflater.h"

#include <string.h>

#include <algorithm>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/numerics/checked_math.h"
#include "net/base/io_buffer.h"
#include "third_party/zlib/zlib.h"

namespace net {

namespace {

class ShrinkableIOBufferWithSize : public IOBufferWithSize {
 public:
  explicit ShrinkableIOBufferWithSize(size_t size) : IOBufferWithSize(size) {}

  void Shrink(int new_size) {
    // The `checked_cast` addresses the < 0 case.
    CHECK_LE(new_size, size());
    SetSpan(first(base::checked_cast<size_t>(new_size)));
  }

 private:
  ~ShrinkableIOBufferWithSize() override = default;
};

}  // namespace

WebSocketInflater::WebSocketInflater()
    : input_queue_(kDefaultInputIOBufferCapacity),
      output_buffer_(kDefaultBufferCapacity) {}

WebSocketInflater::WebSocketInflater(size_t input_queue_capacity,
                                     size_t output_buffer_capacity)
    : input_queue_(input_queue_capacity),
      output_buffer_(output_buffer_capacity) {
  DCHECK_GT(input_queue_capacity, 0u);
  DCHECK_GT(output_buffer_capacity, 0u);
}

bool WebSocketInflater::Initialize(int window_bits) {
  DCHECK_LE(8, window_bits);
  DCHECK_GE(15, window_bits);
  stream_ = std::make_unique<z_stream>();
  *stream_ = z_stream{};
  int result = inflateInit2(stream_.get(), -window_bits);
  if (result != Z_OK) {
    inflateEnd(stream_.get());
    stream_.reset();
    return false;
  }
  return true;
}

WebSocketInflater::~WebSocketInflater() {
  if (stream_) {
    inflateEnd(stream_.get());
    stream_.reset();
  }
}

bool WebSocketInflater::AddBytes(base::span<const uint8_t> data) {
  if (data.empty()) {
    return true;
  }

  if (!input_queue_.IsEmpty()) {
    // choked
    input_queue_.Push(data);
    return true;
  }

  int result = InflateWithFlush(data);
  if (stream_->avail_in > 0) {
    input_queue_.Push(data.last(stream_->avail_in));
  }

  return result == Z_OK || result == Z_BUF_ERROR;
}

bool WebSocketInflater::Finish() {
  return AddBytes(base::byte_span_from_cstring("\x00\x00\xff\xff"));
}

scoped_refptr<IOBufferWithSize> WebSocketInflater::GetOutput(size_t size) {
  auto buffer = base::MakeRefCounted<ShrinkableIOBufferWithSize>(size);
  size_t num_bytes_copied = 0;

  while (num_bytes_copied < size && output_buffer_.Size() > 0) {
    size_t num_bytes_to_copy =
        std::min(output_buffer_.Size(), size - num_bytes_copied);
    output_buffer_.Read(
        buffer->span().subspan(num_bytes_copied, num_bytes_to_copy));
    num_bytes_copied += num_bytes_to_copy;
    int result = InflateChokedInput();
    if (result != Z_OK && result != Z_BUF_ERROR)
      return nullptr;
  }
  buffer->Shrink(num_bytes_copied);
  return buffer;
}

int WebSocketInflater::InflateWithFlush(base::span<const uint8_t> next_in) {
  int result = Inflate(next_in, Z_NO_FLUSH);
  if (result != Z_OK && result != Z_BUF_ERROR) {
    return result;
  }

  if (CurrentOutputSize() > 0) {
    return result;
  }
  // CurrentOutputSize() == 0 means there is no data to be output,
  // so we should make sure it by using Z_SYNC_FLUSH.
  return InflateExistingInput(Z_SYNC_FLUSH);
}

int WebSocketInflater::Inflate(base::span<const uint8_t> next_in, int flush) {
  stream_->next_in = reinterpret_cast<Bytef*>(
      const_cast<char*>(base::as_chars(next_in).data()));
  stream_->avail_in = next_in.size();
  return InflateExistingInput(flush);
}

int WebSocketInflater::InflateExistingInput(int flush) {
  int result = Z_BUF_ERROR;
  do {
    base::span<uint8_t> tail = output_buffer_.GetTail();
    if (tail.empty()) {
      break;
    }

    stream_->next_out = reinterpret_cast<Bytef*>(tail.data());
    stream_->avail_out = tail.size();
    result = inflate(stream_.get(), flush);
    output_buffer_.AdvanceTail(tail.size() - stream_->avail_out);
    if (result == Z_STREAM_END) {
      // Received a block with BFINAL set to 1. Reset the decompression state.
      result = inflateReset(stream_.get());
    } else if (tail.size() == stream_->avail_out) {
      break;
    }
  } while (result == Z_OK || result == Z_BUF_ERROR);
  return result;
}

int WebSocketInflater::InflateChokedInput() {
  if (input_queue_.IsEmpty()) {
    return InflateWithFlush({});
  }

  int result = Z_BUF_ERROR;
  while (!input_queue_.IsEmpty()) {
    base::span<const uint8_t> top = input_queue_.Top();
    result = InflateWithFlush(top);
    input_queue_.Consume(top.size() - stream_->avail_in);

    if (result != Z_OK && result != Z_BUF_ERROR)
      return result;

    if (stream_->avail_in > 0) {
      // There are some data which are not consumed.
      break;
    }
  }
  return result;
}

WebSocketInflater::OutputBuffer::OutputBuffer(size_t capacity)
    : capacity_(capacity),
      buffer_(capacity_ + 1)  // 1 for sentinel
{}

WebSocketInflater::OutputBuffer::~OutputBuffer() = default;

size_t WebSocketInflater::OutputBuffer::Size() const {
  return (tail_ + buffer_.size() - head_) % buffer_.size();
}

base::span<uint8_t> WebSocketInflater::OutputBuffer::GetTail() {
  return base::span(buffer_).subspan(
      tail_, std::min(capacity_ - Size(), buffer_.size() - tail_));
}

void WebSocketInflater::OutputBuffer::Read(base::span<uint8_t> dest) {
  DCHECK_LE(dest.size(), Size());

  if (tail_ < head_) {
    size_t num_bytes_to_copy = std::min(dest.size(), buffer_.size() - head_);
    auto first = dest.take_first(num_bytes_to_copy);
    first.copy_from(base::span(buffer_).subspan(head_, num_bytes_to_copy));
    AdvanceHead(num_bytes_to_copy);
  }

  if (dest.empty()) {
    return;
  }
  DCHECK_LE(head_, tail_);
  dest.copy_from(base::span(buffer_).subspan(head_, dest.size()));
  AdvanceHead(dest.size());
  return;
}

void WebSocketInflater::OutputBuffer::AdvanceHead(size_t advance) {
  DCHECK_LE(advance, Size());
  head_ = (head_ + advance) % buffer_.size();
}

void WebSocketInflater::OutputBuffer::AdvanceTail(size_t advance) {
  DCHECK_LE(advance + Size(), capacity_);
  tail_ = (tail_ + advance) % buffer_.size();
}

WebSocketInflater::InputQueue::InputQueue(size_t capacity)
    : capacity_(capacity) {}

WebSocketInflater::InputQueue::~InputQueue() = default;

base::span<const uint8_t> WebSocketInflater::InputQueue::Top() {
  DCHECK(!IsEmpty());
  if (buffers_.size() == 1) {
    return buffers_.front()->span().subspan(
        head_of_first_buffer_, tail_of_last_buffer_ - head_of_first_buffer_);
  }
  return buffers_.front()->span().subspan(head_of_first_buffer_,
                                          capacity_ - head_of_first_buffer_);
}

void WebSocketInflater::InputQueue::Push(base::span<const uint8_t> data) {
  while (!data.empty()) {
    if (IsEmpty() || tail_of_last_buffer_ == capacity_) {
      buffers_.push_back(base::MakeRefCounted<IOBufferWithSize>(capacity_));
      tail_of_last_buffer_ = 0;
    }
    TakeAndPushToLastBuffer(data);
  }
}

void WebSocketInflater::InputQueue::Consume(size_t size) {
  DCHECK(!IsEmpty());
  DCHECK_LE(size + head_of_first_buffer_, capacity_);

  head_of_first_buffer_ += size;
  if (head_of_first_buffer_ == capacity_) {
    buffers_.pop_front();
    head_of_first_buffer_ = 0;
  }
  if (buffers_.size() == 1 && head_of_first_buffer_ == tail_of_last_buffer_) {
    buffers_.pop_front();
    head_of_first_buffer_ = 0;
    tail_of_last_buffer_ = 0;
  }
}

void WebSocketInflater::InputQueue::TakeAndPushToLastBuffer(
    base::span<const uint8_t>& data) {
  DCHECK(!IsEmpty());
  size_t num_bytes_to_copy =
      std::min(data.size(), capacity_ - tail_of_last_buffer_);
  if (!num_bytes_to_copy) {
    return;
  }
  IOBufferWithSize* buffer = buffers_.back().get();
  buffer->span()
      .subspan(tail_of_last_buffer_, num_bytes_to_copy)
      .copy_from(data.take_first(num_bytes_to_copy));
  tail_of_last_buffer_ += num_bytes_to_copy;
}

}  // namespace net
