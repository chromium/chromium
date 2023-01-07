// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_BUFFER_PRODUCER_H_
#define NET_SPDY_SPDY_BUFFER_PRODUCER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "net/base/net_export.h"

namespace net {

class SpdyBuffer;

// An object which provides a SpdyBuffer for writing. We pass these
// around instead of SpdyBuffers since some buffers have to be
// generated "just in time".
class NET_EXPORT_PRIVATE SpdyBufferProducer {
 public:
  SpdyBufferProducer();

  // Produces the buffer to be written. Will be called at most once.
  virtual std::unique_ptr<SpdyBuffer> ProduceBuffer() = 0;

  SpdyBufferProducer(const SpdyBufferProducer&) = delete;
  SpdyBufferProducer& operator=(const SpdyBufferProducer&) = delete;

  virtual ~SpdyBufferProducer();
};

// A simple wrapper around a single SpdyBuffer.
class NET_EXPORT_PRIVATE SimpleBufferProducer : public SpdyBufferProducer {
 public:
  explicit SimpleBufferProducer(std::unique_ptr<SpdyBuffer> buffer);

  SimpleBufferProducer(const SimpleBufferProducer&) = delete;
  SimpleBufferProducer& operator=(const SimpleBufferProducer&) = delete;

  ~SimpleBufferProducer() override;

  std::unique_ptr<SpdyBuffer> ProduceBuffer() override;

 private:
  std::unique_ptr<SpdyBuffer> buffer_;
};

}  // namespace net

#endif  // NET_SPDY_SPDY_BUFFER_PRODUCER_H_
