/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_GIF_GIF_IMAGE_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_GIF_GIF_IMAGE_DECODER_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"

#include "third_party/skia/include/codec/SkCodec.h"

namespace blink {

class SegmentStream;

// This class decodes the GIF image format.
class PLATFORM_EXPORT GIFImageDecoder final : public ImageDecoder {
 public:
  GIFImageDecoder(AlphaOption, const ColorBehavior&, size_t max_decoded_bytes);
  ~GIFImageDecoder() override;

  // ImageDecoder:
  String FilenameExtension() const override { return "gif"; }
  void OnSetData(SegmentReader* data) override;
  int RepetitionCount() const override;
  bool FrameIsReceivedAtIndex(size_t) const override;
  base::TimeDelta FrameDurationAtIndex(size_t) const override;
  // CAUTION: SetFailed() deletes |codec_|.  Be careful to avoid
  // accessing deleted memory.
  bool SetFailed() override;

  size_t ClearCacheExceptFrame(size_t) override;

 private:
  // ImageDecoder:
  void DecodeSize() override {}
  size_t DecodeFrameCount() override;
  void InitializeNewFrame(size_t) override;
  void Decode(size_t) override;
  // When the disposal method of the frame is DisposeOverWritePrevious, the
  // next frame will use a previous frame's buffer as its starting state, so
  // we can't take over the data in that case. Before calling this method, the
  // caller must verify that the frame exists.
  bool CanReusePreviousFrameBuffer(size_t) const override;

  // When a frame depends on a previous frame's content, there is a list of
  // candidate reference frames. This function will find a previous frame from
  // that list which satisfies the requirements of being a reference frame
  // (kFrameComplete, not kDisposeOverwritePrevious).
  // If no frame is found, it returns kNotFound.
  size_t GetViableReferenceFrameIndex(size_t) const;

  std::unique_ptr<SkCodec> codec_;
  // |codec_| owns the SegmentStream, but we need access to it to append more
  // data as it arrives.
  SegmentStream* segment_stream_ = nullptr;
  mutable int repetition_count_ = kAnimationLoopOnce;
  int prior_frame_ = SkCodec::kNoFrame;

  DISALLOW_COPY_AND_ASSIGN(GIFImageDecoder);
};

}  // namespace blink

#endif
