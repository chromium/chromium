// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_buffer.h"

#include <cstring>
#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "net/base/io_buffer.h"
#include "net/third_party/quiche/src/spdy/core/spdy_protocol.h"

namespace net {

namespace {

// Bound on largest frame any SPDY version has allowed.
const size_t kMaxSpdyFrameSize = 0x00ffffff;

// Makes a spdy::SpdySerializedFrame with |size| bytes of data copied from
// |data|. |data| must be non-NULL and |size| must be positive.
std::unique_ptr<spdy::SpdySerializedFrame> MakeSpdySerializedFrame(
    const char* data,
    size_t size) {
  DCHECK(data);
  CHECK_GT(size, 0u);
  CHECK_LE(size, kMaxSpdyFrameSize);

  auto frame_data = std::make_unique<char[]>(size);
  std::memcpy(frame_data.get(), data, size);
  return std::make_unique<spdy::SpdySerializedFrame>(frame_data.release(), size,
                                                     true /* owns_buffer */);
}

}  // namespace

// This class is an IOBuffer implementation that simply holds a
// reference to a SharedFrame object and a fixed offset. Used by
// SpdyBuffer::GetIOBufferForRemainingData().
class SpdyBuffer::SharedFrameIOBuffer : public IOBuffer {
 public:
  SharedFrameIOBuffer(const scoped_refptr<SharedFrame>& shared_frame,
                      size_t offset)
      : IOBuffer(shared_frame->data->data() + offset),
        shared_frame_(shared_frame) {}

 private:
  ~SharedFrameIOBuffer() override {
    // Prevent ~IOBuffer() from trying to delete |data_|.
    data_ = nullptr;
  }

  const scoped_refptr<SharedFrame> shared_frame_;

  DISALLOW_COPY_AND_ASSIGN(SharedFrameIOBuffer);
};

SpdyBuffer::SpdyBuffer(std::unique_ptr<spdy::SpdySerializedFrame> frame)
    : shared_frame_(new SharedFrame(std::move(frame))), offset_(0) {}

// The given data may not be strictly a SPDY frame; we (ab)use
// |frame_| just as a container.
SpdyBuffer::SpdyBuffer(const char* data, size_t size) :
    shared_frame_(new SharedFrame()),
    offset_(0) {
  CHECK_GT(size, 0u);
  CHECK_LE(size, kMaxSpdyFrameSize);
  shared_frame_->data = MakeSpdySerializedFrame(data, size);
}

SpdyBuffer::~SpdyBuffer() {
  if (GetRemainingSize() > 0)
    ConsumeHelper(GetRemainingSize(), DISCARD);
}

const char* SpdyBuffer::GetRemainingData() const {
  return shared_frame_->data->data() + offset_;
}

size_t SpdyBuffer::GetRemainingSize() const {
  return shared_frame_->data->size() - offset_;
}

void SpdyBuffer::AddConsumeCallback(const ConsumeCallback& consume_callback) {
  consume_callbacks_.push_back(consume_callback);
}

void SpdyBuffer::Consume(size_t consume_size) {
  ConsumeHelper(consume_size, CONSUME);
}

scoped_refptr<IOBuffer> SpdyBuffer::GetIOBufferForRemainingData() {
  return base::MakeRefCounted<SharedFrameIOBuffer>(shared_frame_, offset_);
}

size_t SpdyBuffer::EstimateMemoryUsage() const {
  // TODO(xunjieli): Estimate |consume_callbacks_|. https://crbug.com/669108.
  return base::trace_event::EstimateMemoryUsage(shared_frame_->data);
}

void SpdyBuffer::ConsumeHelper(size_t consume_size,
                               ConsumeSource consume_source) {
  DCHECK_GE(consume_size, 1u);
  DCHECK_LE(consume_size, GetRemainingSize());
  offset_ += consume_size;
  for (std::vector<ConsumeCallback>::const_iterator it =
           consume_callbacks_.begin(); it != consume_callbacks_.end(); ++it) {
    it->Run(consume_size, consume_source);
  }
}

}  // namespace net
