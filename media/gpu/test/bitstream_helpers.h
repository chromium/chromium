// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_BITSTREAM_HELPERS_H_
#define MEDIA_GPU_TEST_BITSTREAM_HELPERS_H_

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "media/video/video_encode_accelerator.h"

namespace media {

class DecoderBuffer;

namespace test {

// This class defines an abstract interface for classes that are interested in
// processing bitstreams (e.g. BitstreamValidator, DecoderBufferValidator, ...).
class BitstreamProcessor {
 public:
  struct BitstreamRef : public base::RefCountedThreadSafe<BitstreamRef> {
    static scoped_refptr<BitstreamRef> Create(
        scoped_refptr<DecoderBuffer> buffer,
        const BitstreamBufferMetadata& metadata,
        int32_t id,
        base::TimeTicks source_timestamp,
        base::OnceClosure release_cb);
    BitstreamRef() = delete;
    BitstreamRef(const BitstreamRef&) = delete;
    BitstreamRef& operator=(const BitstreamRef&) = delete;

    const scoped_refptr<DecoderBuffer> buffer;
    const BitstreamBufferMetadata metadata;
    const int32_t id;
    // |source_timestamp| is the timestamp when the VideoFrame for this
    // bitstream is received to an encoder.
    const base::TimeTicks source_timestamp;

   private:
    friend class base::RefCountedThreadSafe<BitstreamRef>;
    BitstreamRef(scoped_refptr<DecoderBuffer> buffer,
                 const BitstreamBufferMetadata& metadata,
                 int32_t id,
                 base::TimeTicks source_timestamp,
                 base::OnceClosure release_cb);
    ~BitstreamRef();

    base::OnceClosure release_cb;
  };

  virtual ~BitstreamProcessor() = default;

  // Process the specified bitstream buffer. This can e.g. validate the
  // buffer's contents or calculate the buffer's checksum.
  virtual void ProcessBitstream(scoped_refptr<BitstreamRef> bitstream,
                                size_t frame_index) = 0;

  // Wait until all currently scheduled bitstream buffers have been processed.
  // Returns whether processing was successful.
  virtual bool WaitUntilDone() = 0;
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_BITSTREAM_HELPERS_H_
