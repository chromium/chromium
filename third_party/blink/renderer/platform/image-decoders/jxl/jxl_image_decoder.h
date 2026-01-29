// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_JXL_JXL_IMAGE_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_JXL_JXL_IMAGE_DECODER_H_

#include <optional>

#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/rust/jxl/v0_3/wrapper/lib.rs.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace blink {

class FastSharedBufferReader;

class PLATFORM_EXPORT JXLImageDecoder final : public ImageDecoder {
 public:
  JXLImageDecoder(AlphaOption,
                  HighBitDepthDecodingOption,
                  ColorBehavior,
                  cc::AuxImage,
                  wtf_size_t max_decoded_bytes,
                  AnimationOption);
  JXLImageDecoder(const JXLImageDecoder&) = delete;
  JXLImageDecoder& operator=(const JXLImageDecoder&) = delete;
  ~JXLImageDecoder() override;

  // ImageDecoder:
  String FilenameExtension() const override;
  const AtomicString& MimeType() const override;
  bool ImageIsHighBitDepth() override;
  int RepetitionCount() const override;
  bool FrameIsReceivedAtIndex(wtf_size_t) const override;
  std::optional<base::TimeDelta> FrameTimestampAtIndex(
      wtf_size_t) const override;
  base::TimeDelta FrameDurationAtIndex(wtf_size_t) const override;
  wtf_size_t ClearCacheExceptFrame(wtf_size_t) override;

  // Returns true if the data in fast_reader begins with a valid JXL signature.
  static bool MatchesJXLSignature(const FastSharedBufferReader& fast_reader);

 private:
  // C++-managed Rust Box for JxlRsDecoder.
  using JxlRsDecoderPtr = rust::Box<jxl_rs::JxlRsDecoder>;

  // Decoder state machine.
  enum class DecoderState {
    kInitial,          // Waiting for basic info
    kHaveBasicInfo,    // Have basic info, waiting for frame header
    kHaveFrameHeader,  // Have frame header, ready to decode pixels
    kDone              // Decoding is done
  };

  // Frame information tracked during decoding.
  struct FrameInfo {
    base::TimeDelta duration;
    base::TimeDelta timestamp;
  };

  // ImageDecoder:
  void DecodeSize() override;
  wtf_size_t DecodeFrameCount() override;
  void InitializeNewFrame(wtf_size_t) override;
  void Decode(wtf_size_t) override;
  bool CanReusePreviousFrameBuffer(wtf_size_t) const override;

  // Internal decode function that optionally stops after metadata.
  void Decode(wtf_size_t index, bool only_size);

  // Eagerly decode all animation frames upfront.
  void DecodeAllFrames();

  // Converts JXL pixel format to Skia color type.
  SkColorType GetSkColorType() const;

  // Decoder state.
  std::optional<JxlRsDecoderPtr> decoder_;
  DecoderState decoder_state_ = DecoderState::kInitial;
  jxl_rs::JxlRsBasicInfo basic_info_{};
  bool have_basic_info_ = false;
  wtf_size_t num_decoded_frames_ = 0;     // Frames whose pixels we've decoded.
  size_t input_offset_ = 0;  // Current position in input stream.

  // Animation frame tracking.
  Vector<FrameInfo> frame_info_;

  // Color management.
  bool is_high_bit_depth_ = false;
  bool decode_to_half_float_ = false;

  // Used to call UpdateBppHistogram<"Jxl">() at most once to record the
  // bits-per-pixel value of the image when the image is successfully decoded.
  CrossThreadOnceFunction<void(gfx::Size, size_t)>
      update_bpp_histogram_callback_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_JXL_JXL_IMAGE_DECODER_H_
