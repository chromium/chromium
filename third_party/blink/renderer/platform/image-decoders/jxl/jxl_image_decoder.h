/*
 * Copyright (c) 2021, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_JXL_JXL_IMAGE_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_JXL_JXL_IMAGE_DECODER_H_

#include "third_party/blink/renderer/platform/image-decoders/fast_shared_buffer_reader.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"

#include "third_party/libjxl/src/lib/include/jxl/decode.h"

namespace blink {

// This class decodes the JXL image format.
class PLATFORM_EXPORT JXLImageDecoder final : public ImageDecoder {
 public:
  JXLImageDecoder(AlphaOption,
                  HighBitDepthDecodingOption high_bit_depth_decoding_option,
                  const ColorBehavior&,
                  size_t max_decoded_bytes);

  ~JXLImageDecoder() override;

  // ImageDecoder:
  String FilenameExtension() const override { return "jxl"; }
  bool ImageIsHighBitDepth() override { return is_hdr_; }

  // Returns true if the data in fast_reader begins with
  static bool MatchesJXLSignature(const FastSharedBufferReader& fast_reader);

 private:
  // ImageDecoder:
  void DecodeSize() override { Decode(true); }
  size_t DecodeFrameCount() override {
    Decode(true);
    return 1;
  }
  void Decode(size_t) override { Decode(false); }
  void InitializeNewFrame(size_t) override;

  // Decodes the image.  If |only_size| is true, stops decoding after
  // calculating the image size. If decoding fails but there is no more
  // data coming, sets the "decode failure" flag.
  void Decode(bool only_size);

  JxlDecoder* dec_ = nullptr;

  // The image is considered to be HDR, such as using PQ or HLG transfer
  // function in the color space.
  bool is_hdr_ = false;

  JxlPixelFormat format_ = {4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_JXL_JXL_IMAGE_DECODER_H_
