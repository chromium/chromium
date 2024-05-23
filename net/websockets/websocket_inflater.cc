// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/websockets/websocket_inflater.h"

#include <string.h>

#include <algorithm>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "net/base/io_buffer.h"
#include "third_party/zlib/zlib.h"

namespace net {

namespace {

class ShrinkableIOBufferWithSize : public IOBufferWithSize {
 public:
  explicit ShrinkableIOBufferWithSize(size_t size) : IOBufferWithSize(size) {}

  void Shrink(int new_size) {
    CHECK_GE(new_size, 0);
    CHECK_LE(new_size, size_);
    size_ = new_size;
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
  memset(stream_.get(), 0, sizeof(*stream_));
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

bool WebSocketInflater::AddBytes(const char* data, size_t size) {
  if (!size)
    return true;

  if (!input_queue_.IsEmpty()) {
    // choked
    input_queue_.Push(data, size);
    return true;
  }

  int result = InflateWithFlush(data, size);
  if (stream_->avail_in > 0)
    input_queue_.Push(&data[size - stream_->avail_in], stream_->avail_in);

  return result == Z_OK || result == Z_BUF_ERROR;
}

bool WebSocketInflater::Finish() {
  return AddBytes("\x00\x00\xff\xff", 4);
}

scoped_refptr<IOBufferWithSize> WebSocketInflater::GetOutput(size_t size) {
  auto buffer = base::MakeRefCounted<ShrinkableIOBufferWithSize>(size);
  size_t num_bytes_copied = 0;

  while (num_bytes_copied < size && output_buffer_.Size() > 0) {
    size_t num_bytes_to_copy =
        std::min(output_buffer_.Size(), size - num_bytes_copied);
    output_buffer_.Read(&buffer->data()[num_bytes_copied], num_bytes_to_copy);
    num_bytes_copied += num_bytes_to_copy;
    int result = InflateChokedInput();
    if (result != Z_OK && result != Z_BUF_ERROR)
      return nullptr;
  }
  buffer->Shrink(num_bytes_copied);
  return buffer;
}

int WebSocketInflater::InflateWithFlush(const char* next_in, size_t avail_in) {
  int result = Inflate(next_in, avail_in, Z_NO_FLUSH);
  if (result != Z_OK && result != Z_BUF_ERROR)
    return result;

  if (CurrentOutputSize() > 0)
    return result;
  // CurrentOutputSize() == 0 means there is no data to be output,
  // so we should make sure it by using Z_SYNC_FLUSH.
  return Inflate(reinterpret_cast<const char*>(stream_->next_in),
                 stream_->avail_in,
                 Z_SYNC_FLUSH);
}

int WebSocketInflater::Inflate(const char* next_in,
                               size_t avail_in,
                               int flush) {
  stream_->next_in = reinterpret_cast<Bytef*>(const_cast<char*>(next_in));
  stream_->avail_in = avail_in;

  int result = Z_BUF_ERROR;
  do {
    std::pair<char*, size_t> tail = output_buffer_.GetTail();
    if (!tail.second)
      break;

    stream_->next_out = reinterpret_cast<Bytef*>(tail.first);
    stream_->avail_out = tail.second;
    result = inflate(stream_.get(), flush);
    output_buffer_.AdvanceTail(tail.second - stream_->avail_out);
    if (result == Z_STREAM_END) {
      // Received a block with BFINAL set to 1. Reset the decompression state.
      result = inflateReset(stream_.get());
    } else if (tail.second == stream_->avail_out) {
      break;
    }
  } while (result == Z_OK || result == Z_BUF_ERROR);
  return result;
}

int WebSocketInflater::InflateChokedInput() {
  if (input_queue_.IsEmpty())
    return InflateWithFlush(nullptr, 0);

  int result = Z_BUF_ERROR;
  while (!input_queue_.IsEmpty()) {
    std::pair<char*, size_t> top = input_queue_.Top();

    result = InflateWithFlush(top.first, top.second);
    input_queue_.Consume(top.second - stream_->avail_in);

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

std::pair<char*, size_t> WebSocketInflater::OutputBuffer::GetTail() {
  DCHECK_LT(tail_, buffer_.size());
  return std::pair(&buffer_[tail_],
                   std::min(capacity_ - Size(), buffer_.size() - tail_));
}

void WebSocketInflater::OutputBuffer::Read(char* dest, size_t size) {
  DCHECK_LE(size, Size());

  size_t num_bytes_copied = 0;
  if (tail_ < head_) {
    size_t num_bytes_to_copy = std::min(size, buffer_.size() - head_);
    DCHECK_LT(head_, buffer_.size());
    memcpy(&dest[num_bytes_copied], &buffer_[head_], num_bytes_to_copy);
    AdvanceHead(num_bytes_to_copy);
    num_bytes_copied += num_bytes_to_copy;
  }

  if (num_bytes_copied == size)
    return;
  DCHECK_LE(head_, tail_);
  size_t num_bytes_to_copy = size - num_bytes_copied;
  DCHECK_LE(num_bytes_to_copy, tail_ - head_);
  DCHECK_LT(head_, buffer_.size());
  memcpy(&dest[num_bytes_copied], &buffer_[head_], num_bytes_to_copy);
  AdvanceHead(num_bytes_to_copy);
  num_bytes_copied += num_bytes_to_copy;
  DCHECK_EQ(size, num_bytes_copied);
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

std::pair<char*, size_t> WebSocketInflater::InputQueue::Top() {
  DCHECK(!IsEmpty());
  if (buffers_.size() == 1) {
    return std::pair(&buffers_.front()->data()[head_of_first_buffer_],
                     tail_of_last_buffer_ - head_of_first_buffer_);
  }
  return std::pair(&buffers_.front()->data()[head_of_first_buffer_],
                   capacity_ - head_of_first_buffer_);
}

void WebSocketInflater::InputQueue::Push(const char* data, size_t size) {
  if (!size)
    return;

  size_t num_copied_bytes = 0;
  if (!IsEmpty())
    num_copied_bytes += PushToLastBuffer(data, size);

  while (num_copied_bytes < size) {
    DCHECK(IsEmpty() || tail_of_last_buffer_ == capacity_);

    buffers_.push_back(base::MakeRefCounted<IOBufferWithSize>(capacity_));
    tail_of_last_buffer_ = 0;
    num_copied_bytes +=
        PushToLastBuffer(&data[num_copied_bytes], size - num_copied_bytes);
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

size_t WebSocketInflater::InputQueue::PushToLastBuffer(const char* data,
                                                       size_t size) {
  DCHECK(!IsEmpty());
  size_t num_bytes_to_copy = std::min(size, capacity_ - tail_of_last_buffer_);
  if (!num_bytes_to_copy)
    return 0;
  IOBufferWithSize* buffer = buffers_.back().get();
  memcpy(&buffer->data()[tail_of_last_buffer_], data, num_bytes_to_copy);
  tail_of_last_buffer_ += num_bytes_to_copy;
  return num_bytes_to_copy;
}

}  // namespace net
