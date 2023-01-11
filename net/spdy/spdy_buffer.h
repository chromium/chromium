// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_BUFFER_H_
#define NET_SPDY_SPDY_BUFFER_H_

#include <cstddef>
#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"

namespace spdy {
class SpdySerializedFrame;
}  // namespace spdy

namespace net {

class IOBuffer;

// SpdyBuffer is a class to hold data read from or to be written to a
// SPDY connection. It is similar to a DrainableIOBuffer but is not
// ref-counted and will include a way to get notified when Consume()
// is called.
//
// NOTE(akalin): This explicitly does not inherit from IOBuffer to
// avoid the needless ref-counting and to avoid working around the
// fact that IOBuffer member functions are not virtual.
class NET_EXPORT_PRIVATE SpdyBuffer {
 public:
  // The source of a call to a ConsumeCallback.
  enum ConsumeSource {
    // Called via a call to Consume().
    CONSUME,
    // Called via the SpdyBuffer being destroyed.
    DISCARD
  };

  // A Callback that gets called when bytes are consumed with the
  // (non-zero) number of bytes consumed and the source of the
  // consume. May be called any number of times with CONSUME as the
  // source followed by at most one call with DISCARD as the
  // source. The sum of the number of bytes consumed equals the total
  // size of the buffer.
  typedef base::RepeatingCallback<void(size_t, ConsumeSource)> ConsumeCallback;

  // Construct with the data in the given frame. Assumes that data is
  // owned by |frame| or outlives it.
  explicit SpdyBuffer(std::unique_ptr<spdy::SpdySerializedFrame> frame);

  // Construct with a copy of the given raw data. |data| must be
  // non-NULL and |size| must be non-zero.
  SpdyBuffer(const char* data, size_t size);

  SpdyBuffer(const SpdyBuffer&) = delete;
  SpdyBuffer& operator=(const SpdyBuffer&) = delete;

  // If there are bytes remaining in the buffer, triggers a call to
  // any consume callbacks with a DISCARD source.
  ~SpdyBuffer();

  // Returns the remaining (unconsumed) data.
  const char* GetRemainingData() const;

  // Returns the number of remaining (unconsumed) bytes.
  size_t GetRemainingSize() const;

  // Add a callback to be called when bytes are consumed. The
  // ConsumeCallback should not do anything complicated; ideally it
  // should only update a counter. In particular, it must *not* cause
  // the SpdyBuffer itself to be destroyed.
  void AddConsumeCallback(const ConsumeCallback& consume_callback);

  // Consume the given number of bytes, which must be positive but not
  // greater than GetRemainingSize().
  void Consume(size_t consume_size);

  // Returns an IOBuffer pointing to the data starting at
  // GetRemainingData(). Use with care; the returned IOBuffer is not
  // updated when Consume() is called. However, it may still be used
  // past the lifetime of this object.
  //
  // This is used with Socket::Write(), which takes an IOBuffer* that
  // may be written to even after the socket itself is destroyed. (See
  // http://crbug.com/249725 .)
  scoped_refptr<IOBuffer> GetIOBufferForRemainingData();

 private:
  void ConsumeHelper(size_t consume_size, ConsumeSource consume_source);

  // Ref-count the passed-in spdy::SpdySerializedFrame to support the semantics
  // of |GetIOBufferForRemainingData()|.
  typedef base::RefCountedData<std::unique_ptr<spdy::SpdySerializedFrame>>
      SharedFrame;

  class SharedFrameIOBuffer;

  const scoped_refptr<SharedFrame> shared_frame_;
  std::vector<ConsumeCallback> consume_callbacks_;
  size_t offset_ = 0;
};

}  // namespace net

#endif  // NET_SPDY_SPDY_BUFFER_H_
