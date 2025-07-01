// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_buffer.h"

#include <cstring>
#include <utility>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "net/base/io_buffer.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"

namespace net {

namespace {

// Bound on largest frame any SPDY version has allowed.
const size_t kMaxSpdyFrameSize = 0x00ffffff;

// Makes a spdy::SpdySerializedFrame with |size| bytes of data copied from
// |data|. |data| must be non-NULL and |size| must be positive.
std::unique_ptr<spdy::SpdySerializedFrame> MakeSpdySerializedFrame(
    base::span<const uint8_t> data) {
  CHECK(!data.empty());
  CHECK_LE(data.size(), kMaxSpdyFrameSize);

  auto frame_data = std::make_unique<char[]>(data.size());
  // SAFETY: `frame_data` has size `data.size()`, and so does `data`. The type
  // is needed to transfer ownership over to Quiche.
  UNSAFE_BUFFERS(std::memcpy(frame_data.get(), data.data(), data.size()));
  return std::make_unique<spdy::SpdySerializedFrame>(std::move(frame_data),
                                                     data.size());
}

}  // namespace

// This class is an IOBuffer implementation that simply holds a
// reference to a SharedFrame object and a fixed offset. Used by
// SpdyBuffer::GetIOBufferForRemainingData().
class SpdyBuffer::SharedFrameIOBuffer : public IOBuffer {
 public:
  SharedFrameIOBuffer(const scoped_refptr<SharedFrame>& shared_frame,
                      size_t offset)
      : IOBuffer(base::span(*shared_frame->data).subspan(offset)),
        shared_frame_(shared_frame) {}

  SharedFrameIOBuffer(const SharedFrameIOBuffer&) = delete;
  SharedFrameIOBuffer& operator=(const SharedFrameIOBuffer&) = delete;

 private:
  ~SharedFrameIOBuffer() override {
    // Prevent `data_` from dangling should this destructor remove the
    // last reference to `shared_frame`.
    ClearSpan();
  }

  const scoped_refptr<SharedFrame> shared_frame_;
};

SpdyBuffer::SpdyBuffer(std::unique_ptr<spdy::SpdySerializedFrame> frame)
    : shared_frame_(base::MakeRefCounted<SharedFrame>(std::move(frame))) {}

// The given data may not be strictly a SPDY frame; we (ab)use
// |frame_| just as a container.
SpdyBuffer::SpdyBuffer(base::span<const uint8_t> data)
    : shared_frame_(base::MakeRefCounted<SharedFrame>()) {
  CHECK_GT(data.size(), 0u);
  CHECK_LE(data.size(), kMaxSpdyFrameSize);
  shared_frame_->data = MakeSpdySerializedFrame(data);
}

SpdyBuffer::~SpdyBuffer() {
  if (GetRemainingSize() > 0)
    ConsumeHelper(GetRemainingSize(), DISCARD);
}

base::span<const uint8_t> SpdyBuffer::GetRemaining() const {
  std::string_view frame_view(*shared_frame_->data);
  return base::as_byte_span(frame_view).subspan(offset_);
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
