// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_buffer_producer.h"

#include <utility>

#include "base/check.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "net/spdy/spdy_buffer.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"

namespace net {

SpdyBufferProducer::SpdyBufferProducer() = default;

SpdyBufferProducer::~SpdyBufferProducer() = default;

SimpleBufferProducer::SimpleBufferProducer(std::unique_ptr<SpdyBuffer> buffer)
    : buffer_(std::move(buffer)) {}

SimpleBufferProducer::~SimpleBufferProducer() = default;

std::unique_ptr<SpdyBuffer> SimpleBufferProducer::ProduceBuffer() {
  DCHECK(buffer_);
  return std::move(buffer_);
}

}  // namespace net
