// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_COMMON_SYSMEM_BUFFER_STREAM_H_
#define MEDIA_FUCHSIA_COMMON_SYSMEM_BUFFER_STREAM_H_

#include <fuchsia/sysmem/cpp/fidl.h>

#include "base/memory/scoped_refptr.h"
#include "media/fuchsia/common/stream_processor_helper.h"

namespace media {

class DecoderBuffer;

// Abstract interface for media stream processors. SysmemBufferStream takes a
// stream of buffers in DecoderBuffer, processes them and then writes the output
// to sysmem buffers.
class MEDIA_EXPORT SysmemBufferStream {
 public:
  class Sink {
   public:
    // Called to set BufferCollectionToken for the output buffer collection.
    virtual void OnSysmemBufferStreamBufferCollectionToken(
        fuchsia::sysmem2::BufferCollectionTokenPtr token) = 0;

    // Called when a packet has been processed. The client should drop the
    // |packet| only after it's finished using it.
    virtual void OnSysmemBufferStreamOutputPacket(
        StreamProcessorHelper::IoPacket packet) = 0;

    // Called when the end of stream has been reached.
    virtual void OnSysmemBufferStreamEndOfStream() = 0;

    // Called on error.
    virtual void OnSysmemBufferStreamError() = 0;

    // Called to notify the sink that the SysmemBufferStream has stopped
    // because it doesn't have a key. It will resume automatically once a new
    // key is received.
    virtual void OnSysmemBufferStreamNoKey() = 0;

   protected:
    virtual ~Sink() = default;
  };

  SysmemBufferStream() {}
  virtual ~SysmemBufferStream() {}

  // Allocates a buffer collection for the output and starts processing the
  // stream, passing the output to the specified |sink|. |min_buffer_size| and
  // |min_buffer_count| specify the minimum number of packets in the output
  // buffer collection.
  virtual void Initialize(Sink* sink,
                          size_t min_buffer_size,
                          size_t min_buffer_count) = 0;

  // Enqueues the specified buffer to the input queue. Caller is allowed to
  // queue as many buffers as it needs without waiting for results from the
  // previous Process() calls. May be called before Initialize(). Queued buffers
  // will be processed only after Initialize().
  virtual void EnqueueBuffer(scoped_refptr<DecoderBuffer> buffer) = 0;

  // Stops processing queued buffers and drops them. Keeps the |sink| passed to
  // Start() and the output buffer collections.
  virtual void Reset() = 0;
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_COMMON_SYSMEM_BUFFER_STREAM_H_
