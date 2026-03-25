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
#include "third_party/rust/jxl/v0_4/wrapper/lib.rs.h"
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
  // C++-managed Rust Box types.
  using JxlRsDecoderPtr = rust::Box<jxl_rs::JxlRsDecoder>;
  using JxlRsScannerPtr = rust::Box<jxl_rs::JxlRsFrameScanner>;

  // Decoder state machine for the pixel decoder.
  enum class DecoderState {
    kInitial,          // Waiting for basic info
    kHaveBasicInfo,    // Have basic info, waiting for frame header
    kHaveFrameHeader,  // Have frame header, ready to decode pixels
    kDone              // Decoding is done
  };

  // Blink-specific timing for a visible frame (cumulative timestamp).
  struct FrameTiming {
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

  // Run the frame scanner to discover frame metadata without decoding pixels.
  void ScanFrames();

  // Process basic info after it has been parsed by either the scanner or
  // decoder. Sets size, bit depth, color profile, etc. Returns false on
  // failure (SetSize failed).
  bool ProcessBasicInfo(rust::Slice<const uint8_t> icc_data);

  // Set up the decoder to seek to a specific frame. Uses the scanner
  // to look up the full seek target, configures seek_target_index_,
  // and transitions the state machine to kHaveBasicInfo.
  // Must be called when decoder_state_ >= kHaveBasicInfo and scanner_ is valid.
  void SetupSeek(wtf_size_t index);

  // Converts JXL pixel format to Skia color type.
  SkColorType GetSkColorType() const;

  // Lightweight frame scanner -- discovers frame count, durations, and seek
  // offsets without decoding any pixels.
  std::optional<JxlRsScannerPtr> scanner_;
  size_t scanner_input_offset_ = 0;
  bool scanner_done_ = false;

  // Full pixel decoder with state machine.
  std::optional<JxlRsDecoderPtr> decoder_;
  DecoderState decoder_state_ = DecoderState::kInitial;
  size_t decoder_input_offset_ = 0;
  wtf_size_t num_decoded_frames_ = 0;

  // Seek state: set when decoding a non-sequential animation frame.
  std::optional<wtf_size_t> seek_target_index_;

  // Cached metadata.
  jxl_rs::JxlRsBasicInfo basic_info_{};
  bool have_basic_info_ = false;

  // Per-frame info populated by the scanner. frame_infos_ stores the Rust
  // struct directly (seek offsets, keyframe flag, etc.); frame_timings_ stores
  // Blink-specific cumulative timestamps.
  Vector<jxl_rs::JxlRsVisibleFrameInfo> frame_infos_;
  Vector<FrameTiming> frame_timings_;

  // Color management.
  bool is_high_bit_depth_ = false;
  bool decode_to_half_float_ = false;

  // Used to call UpdateBppHistogram<"Jxl">() at most once.
  CrossThreadOnceFunction<void(gfx::Size, size_t)>
      update_bpp_histogram_callback_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_JXL_JXL_IMAGE_DECODER_H_
