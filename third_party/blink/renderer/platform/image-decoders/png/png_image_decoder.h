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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_PNG_PNG_IMAGE_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_PNG_PNG_IMAGE_DECODER_H_

#include <memory>

#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/png/png_image_reader.h"

namespace blink {

class PLATFORM_EXPORT PNGImageDecoder final : public ImageDecoder {
 public:
  PNGImageDecoder(AlphaOption,
                  HighBitDepthDecodingOption,
                  const ColorBehavior&,
                  size_t max_decoded_bytes,
                  size_t offset = 0);
  ~PNGImageDecoder() override;

  // ImageDecoder:
  String FilenameExtension() const override { return "png"; }
  bool SetSize(unsigned, unsigned) override;
  int RepetitionCount() const override;
  bool ImageIsHighBitDepth() override;
  bool FrameIsReceivedAtIndex(size_t) const override;
  base::TimeDelta FrameDurationAtIndex(size_t) const override;
  bool SetFailed() override;

  // Callbacks from libpng
  void HeaderAvailable();
  void RowAvailable(unsigned char* row, unsigned row_index, int);
  void FrameComplete();

  void SetColorSpace();
  void SetRepetitionCount(int);
  void SetBitDepth();

 private:
  using ParseQuery = PNGImageReader::ParseQuery;

  // ImageDecoder:
  void DecodeSize() override { Parse(ParseQuery::kSize); }
  void Decode(size_t) override;
  void Parse(ParseQuery);
  size_t DecodeFrameCount() override;
  void InitializeNewFrame(size_t) override;
  void ClearFrameBuffer(size_t) override;
  bool CanReusePreviousFrameBuffer(size_t) const override;

  std::unique_ptr<PNGImageReader> reader_;
  const unsigned offset_;
  size_t current_frame_;
  int repetition_count_;
  bool has_alpha_channel_;
  bool current_buffer_saw_alpha_;
  bool decode_to_half_float_;
  size_t bit_depth_;
  std::unique_ptr<ImageFrame::PixelData[]> color_transform_scanline_;

  DISALLOW_COPY_AND_ASSIGN(PNGImageDecoder);
};

}  // namespace blink

#endif
