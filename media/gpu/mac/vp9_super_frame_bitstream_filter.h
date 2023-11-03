// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MAC_VP9_SUPER_FRAME_BITSTREAM_FILTER_H_
#define MEDIA_GPU_MAC_VP9_SUPER_FRAME_BITSTREAM_FILTER_H_

#include <vector>

#include <CoreMedia/CoreMedia.h>

#include "base/apple/scoped_cftyperef.h"
#include "media/base/decoder_buffer.h"
#include "media/gpu/media_gpu_export.h"

namespace media {
class Vp9RawBitsReader;

// Combines alt-ref VP9 buffers into super frames and passes through non-alt-ref
// buffers without modification.
class MEDIA_GPU_EXPORT VP9SuperFrameBitstreamFilter {
 public:
  VP9SuperFrameBitstreamFilter();
  ~VP9SuperFrameBitstreamFilter();

  // Adds a buffer for processing. Clients must call take_buffer() after this
  // to see if a buffer is ready for processing.
  bool EnqueueBuffer(scoped_refptr<DecoderBuffer> buffer);

  // Releases any pending data.
  void Flush();

  // Releases any prepared buffer. Returns null if no buffers are available.
  base::apple::ScopedCFTypeRef<CMBlockBufferRef> take_buffer() {
    return std::move(data_);
  }

  bool has_buffers_for_testing() const {
    return data_ || !partial_buffers_.empty();
  }

 private:
  bool ShouldShowFrame(Vp9RawBitsReader* reader);
  bool PreparePassthroughBuffer(scoped_refptr<DecoderBuffer> buffer);
  bool AllocateCombinedBlock(size_t total_size);
  bool MergeBuffer(const DecoderBuffer& buffer, size_t offset);
  bool BuildSuperFrame();

  // Prepared CMBlockBuffer -- either by assembling |partial_buffers_| or when
  // a super frame is unnecessary, just by passing through DecoderBuffer.
  base::apple::ScopedCFTypeRef<CMBlockBufferRef> data_;

  // Partial buffers which need to be assembled into a super frame.
  std::vector<scoped_refptr<DecoderBuffer>> partial_buffers_;
};

}  // namespace media

#endif  // MEDIA_GPU_MAC_VP9_SUPER_FRAME_BITSTREAM_FILTER_H_
