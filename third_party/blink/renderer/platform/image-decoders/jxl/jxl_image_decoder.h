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
#include "third_party/libjxl/src/lib/include/jxl/decode_cxx.h"

namespace blink {

// This class decodes the JXL image format.
class PLATFORM_EXPORT JXLImageDecoder final : public ImageDecoder {
 public:
  JXLImageDecoder(AlphaOption,
                  HighBitDepthDecodingOption high_bit_depth_decoding_option,
                  const ColorBehavior&,
                  wtf_size_t max_decoded_bytes);

  // ImageDecoder:
  String FilenameExtension() const override { return "jxl"; }
  bool ImageIsHighBitDepth() override { return is_hdr_; }

  // Returns true if the data in fast_reader begins with
  static bool MatchesJXLSignature(const FastSharedBufferReader& fast_reader);

 private:
  // ImageDecoder:
  void DecodeSize() override { DecodeImpl(0, true); }
  wtf_size_t DecodeFrameCount() override;
  void Decode(wtf_size_t frame) override { DecodeImpl(frame); }
  void InitializeNewFrame(wtf_size_t) override;
  // TODO(http://crbug.com/1211339): We never clear the frame buffer for now,
  // as we'd need to restart the decoder from scratch. This can lead to
  // excessive memory use on web site such as forums, or image collection
  // sites. This should be fixed to use the existing frame cache disposal
  // methods when the JPEG XL API can handle resuming decoding from
  // intermediate frames.
  void ClearFrameBuffer(wtf_size_t frame_index) override {}

  // Decodes up to a given frame.  If |only_size| is true, stops decoding after
  // calculating the image size. If decoding fails but there is no more
  // data coming, sets the "decode failure" flag.
  void DecodeImpl(wtf_size_t frame, bool only_size = false);

  bool FrameIsReceivedAtIndex(wtf_size_t) const override;
  base::TimeDelta FrameDurationAtIndex(wtf_size_t) const override;
  int RepetitionCount() const override;
  bool CanReusePreviousFrameBuffer(wtf_size_t) const override { return false; }

  // Reads bytes from the segment reader, after releasing input from the JXL
  // decoder, which required `remaining` previous bytes to still be available.
  // Starts reading from *offset - remaining, and ensures more than remaining
  // bytes are read, if possible. Returns false if not enough bytes are
  // available or if Failed() was set.
  bool ReadBytes(size_t remaining,
                 wtf_size_t* offset,
                 WTF::Vector<uint8_t>* segment,
                 FastSharedBufferReader* reader,
                 const uint8_t** jxl_data,
                 size_t* jxl_size);

  JxlDecoderPtr dec_ = nullptr;
  wtf_size_t offset_ = 0;

  JxlDecoderPtr frame_count_dec_ = nullptr;
  wtf_size_t frame_count_offset_ = 0;

  // The image is considered to be HDR, such as using PQ or HLG transfer
  // function in the color space.
  bool is_hdr_ = false;
  bool decode_to_half_float_ = false;

  JxlBasicInfo info_;
  bool have_color_info_ = false;

  // Preserved for JXL pixel callback. Not owned.
  ColorProfileTransform* xform_;

  // For animation support.
  wtf_size_t num_decoded_frames_ = 0;
  bool finished_ = false;
  bool has_full_frame_count_ = false;
  size_t size_at_last_frame_count_ = 0;
  WTF::Vector<float> frame_durations_;
  // Multiple concatenated segments from the FastSharedBufferReader, these are
  // only used when a single segment did not contain enough data for the JXL
  // parser.
  WTF::Vector<uint8_t> segment_;
  WTF::Vector<uint8_t> frame_count_segment_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_JXL_JXL_IMAGE_DECODER_H_
