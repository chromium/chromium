// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/jxl/jxl_image_decoder.h"

#include <cstdint>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/image-decoders/fast_shared_buffer_reader.h"
#include "third_party/blink/renderer/platform/image-decoders/image_frame.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "third_party/rust/jxl/v0_4/wrapper/lib.rs.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace blink {

using jxl_rs::jxl_rs_decoder_create;
using jxl_rs::jxl_rs_scan_decoder_create;
using jxl_rs::jxl_rs_seek_decoder_to_frame;
using jxl_rs::jxl_rs_signature_check;
using jxl_rs::JxlRsBasicInfo;
using jxl_rs::JxlRsDecoder;
using jxl_rs::JxlRsPixelFormat;
using jxl_rs::JxlRsProcessResult;
using jxl_rs::JxlRsStatus;
using jxl_rs::JxlRsVisibleFrameInfo;

namespace {
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
// 16 million pixels in a RGBA image.
constexpr uint64_t kMaxDecodedSamples = 64ULL * 1024 * 1024;
#else
// The maximum number of decoded samples we allow. This helps prevent resource
// exhaustion from malicious files. The jxl-rs API counts pixels * channels,
// so an RGBA image counts 4 samples per pixel. JPEG XL codestream level 5
// limits specify ~268M pixels, so we allow ~1B samples to support that.
constexpr uint64_t kMaxDecodedSamples = 1024ULL * 1024 * 1024;
#endif

}  // namespace

JXLImageDecoder::JXLImageDecoder(AlphaOption alpha_option,
                                 HighBitDepthDecodingOption hbd_option,
                                 ColorBehavior color_behavior,
                                 cc::AuxImage aux_image,
                                 wtf_size_t max_decoded_bytes,
                                 AnimationOption animation_option)
    : ImageDecoder(alpha_option,
                   hbd_option,
                   color_behavior,
                   aux_image,
                   max_decoded_bytes) {}

JXLImageDecoder::~JXLImageDecoder() = default;

String JXLImageDecoder::FilenameExtension() const {
  return "jxl";
}

const AtomicString& JXLImageDecoder::MimeType() const {
  DEFINE_STATIC_LOCAL(const AtomicString, jxl_mime_type, ("image/jxl"));
  return jxl_mime_type;
}

bool JXLImageDecoder::ImageIsHighBitDepth() {
  return basic_info_.has_value() && basic_info_->bits_per_sample > 8;
}

bool JXLImageDecoder::MatchesJXLSignature(
    const FastSharedBufferReader& fast_reader) {
  uint8_t buffer[12];
  if (fast_reader.size() < sizeof(buffer)) {
    return false;
  }
  auto data = fast_reader.GetConsecutiveData(0, sizeof(buffer), buffer);
  return jxl_rs_signature_check(
      rust::Slice<const uint8_t>(data.data(), data.size()));
}

// ---------------------------------------------------------------------------
// Shared basic-info processing
// ---------------------------------------------------------------------------

void JXLImageDecoder::SetPixelFormat(JxlRsDecoder* decoder) {
  CHECK(basic_info_.has_value());
  bool decode_to_half_float =
      ImageIsHighBitDepth() &&
      high_bit_depth_decoding_option_ == kHighBitDepthToHalfFloat;
  // Set pixel format on decoder.
  // Use native 8-bit ordering for kN32, and RGBA F16 for half float.
#if SK_PMCOLOR_BYTE_ORDER(B, G, R, A)
  constexpr JxlRsPixelFormat kNativePixelFormat = JxlRsPixelFormat::Bgra8;
#elif SK_PMCOLOR_BYTE_ORDER(R, G, B, A)
  constexpr JxlRsPixelFormat kNativePixelFormat = JxlRsPixelFormat::Rgba8;
#else
#error "Unsupported Skia pixel order"
#endif
  JxlRsPixelFormat pixel_format =
      decode_to_half_float ? JxlRsPixelFormat::RgbaF16 : kNativePixelFormat;
  decoder->set_pixel_format(pixel_format, basic_info_->num_extra_channels);
}

bool JXLImageDecoder::SetBasicInfo() {
  if (basic_info_.has_value()) {
    return true;
  }

  CHECK(scanner_.has_value());

  basic_info_ = (*scanner_)->get_basic_info();
  if (!SetSize(basic_info_->width, basic_info_->height)) {
    return false;
  }

  // The output ICC profile may depend on the pixel format. Thus, let's ensure
  // that we set the pixel format here.
  SetPixelFormat(&**scanner_);

  // Extract ICC color profile.
  rust::Slice<const uint8_t> icc_data = (*scanner_)->get_icc_profile();
  if (!IgnoresColorSpace() && !icc_data.empty()) {
    auto profile = ColorProfile::Create(icc_data);
    if (profile) {
      SetEmbeddedColorProfile(std::move(profile));
    }
  }

  // Record bpp information only for 8-bit, color, still images without
  // alpha.
  if (basic_info_->bits_per_sample == 8 && !basic_info_->is_grayscale &&
      !basic_info_->have_animation && !basic_info_->has_alpha) {
    static constexpr char kType[] = "Jxl";
    update_bpp_histogram_callback_ =
        CrossThreadBindOnce(&UpdateBppHistogram<kType>);
  }

  return true;
}

// ---------------------------------------------------------------------------
// Frame scanning (no pixel decoding)
// ---------------------------------------------------------------------------

void JXLImageDecoder::ScanFrames() {
  if (scanner_done_ || Failed()) {
    return;
  }

  if (!scanner_.has_value()) {
    scanner_ = jxl_rs_scan_decoder_create(kMaxDecodedSamples);
  }

  FastSharedBufferReader reader(data_.get());
  const size_t data_size = reader.size();
  if (data_size < scanner_input_offset_) {
    // In some cases, the input buffer may shrink. This in particular seems to
    // happen when changing tabs during a partial decode of the image. If that
    // happens, we cannot possibly make progress, so wait to be called again
    // with a bigger buffer.
    return;
  }

  while (!scanner_done_ &&
         (scanner_input_offset_ < data_size ||
          (IsAllDataReceived() && scanner_input_offset_ == data_size))) {
    base::span<const uint8_t> data_span =
        scanner_input_offset_ < data_size
            ? reader.GetSomeData(scanner_input_offset_)
            : base::span<const uint8_t>();
    CHECK_LE(scanner_input_offset_ + data_span.size(), data_size);
    bool all_input = IsAllDataReceived() &&
                     (scanner_input_offset_ + data_span.size() == data_size);
    rust::Slice<const uint8_t> input_slice(data_span.data(), data_span.size());

    JxlRsProcessResult result =
        (*scanner_)->process(input_slice, rust::Slice<uint8_t>(), 0, 0, 0);

    if (result.status == JxlRsStatus::Error) {
      SetFailed();
      return;
    }

    scanner_input_offset_ += result.bytes_consumed;
    CHECK_LE(scanner_input_offset_, data_size);

    if (result.status == JxlRsStatus::NeedMoreInput) {
      // If more data is available in the buffer, continue feeding.
      if (result.bytes_consumed > 0 && scanner_input_offset_ < data_size) {
        continue;
      }
      if (all_input) {
        SetFailed();
      }
      return;
    }

    if (!(*scanner_)->has_more_frames()) {
      scanner_done_ = true;
    }
  }

  // Extract basic info from scanner if not yet available.
  if ((*scanner_)->has_basic_info()) {
    if (!SetBasicInfo()) {
      return;
    }
  }

  // Update frame_infos_ / frame_timings_ from the scanner's discovered frames.
  size_t scanned_count = (*scanner_)->frame_count();
  base::TimeDelta cumulative_time;

  if (!frame_timings_.empty()) {
    const auto& last = frame_timings_.back();
    cumulative_time = last.timestamp + last.duration;
  }

  for (size_t i = frame_infos_.size(); i < scanned_count; i++) {
    frame_infos_.push_back((*scanner_)->get_frame_info(i));

    FrameTiming timing;
    timing.duration = base::Milliseconds(frame_infos_.back().duration_ms);
    timing.timestamp = cumulative_time;
    base::TimeDelta next_cumulative_time = cumulative_time + timing.duration;
    // TimeDelta overflow clamps to Min/Max, and TimeDelta::is_inf() reports
    // those sentinel values as infinities.
    if (next_cumulative_time.is_inf() && !cumulative_time.is_inf() &&
        !timing.duration.is_inf()) {
      SetFailed();
      return;
    }
    cumulative_time = next_cumulative_time;
    frame_timings_.push_back(timing);
  }
}

void JXLImageDecoder::DecodeSize() {
  if (!basic_info_.has_value()) {
    ScanFrames();
  }
}

wtf_size_t JXLImageDecoder::DecodeFrameCount() {
  ScanFrames();

  if (!basic_info_.has_value() || !basic_info_->have_animation) {
    return 1;
  }

  // Resize the frame buffer cache to match discovered frames.
  if (frame_infos_.size() > frame_buffer_cache_.size()) {
    frame_buffer_cache_.resize(frame_infos_.size());
  }

  return frame_buffer_cache_.size();
}

void JXLImageDecoder::InitializeNewFrame(wtf_size_t index) {
  CHECK_LT(index, frame_buffer_cache_.size());
  auto& frame = frame_buffer_cache_[index];

  if (ImageIsHighBitDepth() &&
      high_bit_depth_decoding_option_ == kHighBitDepthToHalfFloat) {
    frame.SetPixelFormat(ImageFrame::PixelFormat::kRGBA_F16);
  }

  frame.SetPremultiplyAlpha(premultiply_alpha_);
  frame.SetHasAlpha(basic_info_.has_value() && basic_info_->has_alpha);
  frame.SetOriginalFrameRect(gfx::Rect(Size()));
  frame.SetRequiredPreviousFrameIndex(kNotFound);

  // Set duration/timestamp if the frame header has been parsed.
  // This is available before the frame is fully decoded.
  if (index < frame_timings_.size()) {
    const FrameTiming& timing = frame_timings_[index];
    frame.SetDuration(timing.duration);
    frame.SetTimestamp(timing.timestamp);
  }
}

void JXLImageDecoder::Decode(wtf_size_t index) {
  Decode(index, false);
}

void JXLImageDecoder::Decode(wtf_size_t index, bool only_size) {
  // Ensure that the frame scanner has fully caught up with the file received so
  // far.
  ScanFrames();
  if (Failed()) {
    return;
  }

  // If we have basic image information, it has already been processed by the
  // frame scanner. Thus, we don't need to do anything else here.
  // Moreover, if we do *not* have basic info, there is nothing we can do.
  if (only_size || !basic_info_.has_value()) {
    return;
  }

  // Early return if the requested frame is already fully decoded and cached.
  if (index < frame_buffer_cache_.size()) {
    auto status = frame_buffer_cache_[index].GetStatus();
    if (status == ImageFrame::kFrameComplete) {
      return;
    }
  }

  // If we want to decode a frame that is *not* the next frame, seek to that
  // frame.
  if (basic_info_->have_animation && index != next_frame_to_decode_) {
    CHECK_GE(decoder_state_, DecoderState::kHaveBasicInfo);
    SeekToFrame(index);
  }

  FastSharedBufferReader reader(data_.get());
  const size_t data_size = reader.size();
  if (data_size < decoder_input_offset_) {
    // In some cases, the input buffer may shrink. This in particular seems to
    // happen when changing tabs during a partial decode of the image. If that
    // happens, we cannot possibly make progress, so wait to be called again
    // with a bigger buffer.
    return;
  }

  // Create decoder if needed. Pass premultiply_alpha_ so jxl-rs handles
  // premultiplication natively (faster and handles alpha_associated correctly).
  if (!decoder_.has_value()) {
    decoder_ = jxl_rs_decoder_create(kMaxDecodedSamples, premultiply_alpha_);
  }

  auto flush_partial_frame = [this](wtf_size_t frame_index) -> bool {
    if (basic_info_->have_animation) {
      return true;
    }

    // Since we never flush frames in animations, the frame index must be 0.
    CHECK_EQ(frame_index, 0U);

    CHECK_LT(frame_index, frame_buffer_cache_.size());
    ImageFrame& frame = frame_buffer_cache_[frame_index];

    const SkBitmap& bitmap = frame.Bitmap();
    uint8_t* frame_pixels = static_cast<uint8_t*>(bitmap.getPixels());
    const size_t row_stride = bitmap.rowBytes();
    if (!frame_pixels) {
      return false;
    }

    const size_t buffer_size = row_stride * basic_info_->height;
    rust::Slice<uint8_t> output_slice(frame_pixels, buffer_size);
    JxlRsProcessResult flush_result = (*decoder_)->flush_pixels(
        output_slice, basic_info_->width, basic_info_->height, row_stride);
    if (flush_result.status == JxlRsStatus::Error) {
      return false;
    }
    if (flush_result.status == JxlRsStatus::Success &&
        frame.GetStatus() != ImageFrame::kFrameComplete) {
      frame.SetPixelsChanged(true);
      frame.SetStatus(ImageFrame::kFramePartial);
    }

    return true;
  };

  // Process until we get what we need. Uses GetSomeData() to read one
  // buffer segment at a time, avoiding copies across segment boundaries.
  while (decoder_state_ != DecoderState::kDone) {
    CHECK_LE(decoder_input_offset_, data_size);
    if (decoder_input_offset_ == data_size && !IsAllDataReceived()) {
      return;
    }

    base::span<const uint8_t> data_span =
        decoder_input_offset_ < data_size
            ? reader.GetSomeData(decoder_input_offset_)
            : base::span<const uint8_t>();
    bool all_input = IsAllDataReceived() &&
                     (decoder_input_offset_ + data_span.size() == data_size);
    rust::Slice<const uint8_t> input_slice(data_span.data(), data_span.size());

    rust::Slice<uint8_t> output_slice;

    const uint32_t width = basic_info_->width;
    const uint32_t height = basic_info_->height;
    size_t row_stride = 0;

    if (decoder_state_ >= DecoderState::kHaveBasicInfo) {
      if (frame_buffer_cache_.size() <= next_frame_to_decode_) {
        frame_buffer_cache_.resize(next_frame_to_decode_ + 1);
      }

      ImageFrame& frame = frame_buffer_cache_[next_frame_to_decode_];
      if (frame.GetStatus() == ImageFrame::kFrameEmpty) {
        // IMPORTANT: InitializeNewFrame() must run before InitFrameBuffer(),
        // so the base class allocates the correct backing store (e.g.
        // RGBA_F16 for high bit depth + half float).
        InitializeNewFrame(next_frame_to_decode_);
        if (!InitFrameBuffer(next_frame_to_decode_)) {
          SetFailed();
          return;
        }
      }

      frame.SetHasAlpha(basic_info_->has_alpha);

      // Get direct access to the frame buffer's backing store.
      const SkBitmap& bitmap = frame.Bitmap();
      uint8_t* frame_pixels = static_cast<uint8_t*>(bitmap.getPixels());
      row_stride = bitmap.rowBytes();

      if (!frame_pixels) {
        SetFailed();
        return;
      }

      // Calculate buffer size for the decoder.
      size_t buffer_size = row_stride * height;
      output_slice = rust::Slice<uint8_t>(frame_pixels, buffer_size);
    }

    // Decode directly into the frame buffer.
    // Premultiplication is handled by jxl-rs based on premultiply_alpha_.
    JxlRsProcessResult result = (*decoder_)->process(input_slice, output_slice,
                                                     width, height, row_stride);

    if (result.status == JxlRsStatus::Error) {
      SetFailed();
      return;
    }

    decoder_input_offset_ += result.bytes_consumed;
    CHECK_LE(decoder_input_offset_, data_size);

    if (result.status == JxlRsStatus::NeedMoreInput) {
      // If more data is available in the buffer, continue feeding.
      if (result.bytes_consumed > 0 && decoder_input_offset_ < data_size) {
        continue;
      }
      // We've exhausted all available data or made no progress.
      if (all_input) {
        // All network data received and fed, but decoder still wants more.
        // This is a truncated or corrupt file.
        SetFailed();
        return;
      }
      // If we got here, the frame scanner has found basic info.
      // Hence, we should have enough data to reach the end of basic info
      // ourselves.
      CHECK_GE(decoder_state_, DecoderState::kHaveBasicInfo);

      // We may already be able to output some pixel data before we
      // reach the first visible frame.
      if (!flush_partial_frame(next_frame_to_decode_)) {
        SetFailed();
        return;
      }
      return;
    }

    switch (decoder_state_) {
      case DecoderState::kInitial: {
        SetPixelFormat(&**decoder_);
        decoder_state_ = DecoderState::kHaveBasicInfo;
        break;
      }
      case DecoderState::kHaveBasicInfo: {
        decoder_state_ = DecoderState::kHaveFrameHeader;
        break;
      }
      case DecoderState::kHaveFrameHeader: {
        ImageFrame& frame = frame_buffer_cache_[next_frame_to_decode_];
        frame.SetPixelsChanged(true);
        frame.SetStatus(ImageFrame::kFrameComplete);

        CHECK_LT(next_frame_to_decode_, frame_timings_.size());
        const FrameTiming& timing = frame_timings_[next_frame_to_decode_];
        frame.SetDuration(timing.duration);
        frame.SetTimestamp(timing.timestamp);

        next_frame_to_decode_++;

        // Record bpp histogram for still images when fully decoded.
        if (IsAllDataReceived() && update_bpp_histogram_callback_) {
          std::move(update_bpp_histogram_callback_).Run(Size(), data_->size());
        }

        if ((*decoder_)->has_more_frames()) {
          // Go back to waiting for next frame header.
          decoder_state_ = DecoderState::kHaveBasicInfo;
        } else {
          decoder_state_ = DecoderState::kDone;
        }

        // Check if we've decoded the requested frame.
        if (next_frame_to_decode_ > index) {
          return;
        }
        break;
      }
      case DecoderState::kDone: {
        LOG(FATAL) << "DecoderState::kDone is unreachable";
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Seek setup (integrates with the main decode loop)
// ---------------------------------------------------------------------------

void JXLImageDecoder::SeekToFrame(wtf_size_t index) {
  CHECK_LT(index, frame_infos_.size());
  CHECK(scanner_.has_value());

  // Position the decoder at the start of the frame group containing
  // the target frame. The full seek target (including internal jxl-rs
  // state) is looked up from the scanner. Visible frame skipping (for
  // non-keyframes) is handled automatically by jxl-rs.
  jxl_rs_seek_decoder_to_frame(**scanner_, **decoder_, index);
  decoder_input_offset_ = frame_infos_[index].decode_start_file_offset;

  decoder_state_ = DecoderState::kHaveBasicInfo;
  next_frame_to_decode_ = index;
}

bool JXLImageDecoder::CanReusePreviousFrameBuffer(
    wtf_size_t frame_index) const {
  CHECK(frame_index < frame_buffer_cache_.size());
  return true;
}

bool JXLImageDecoder::FrameIsReceivedAtIndex(wtf_size_t index) const {
  return IsAllDataReceived() ||
         (index < frame_buffer_cache_.size() &&
          frame_buffer_cache_[index].GetStatus() == ImageFrame::kFrameComplete);
}

std::optional<base::TimeDelta> JXLImageDecoder::FrameTimestampAtIndex(
    wtf_size_t index) const {
  // Use frame_timings_ which is populated at frame scanning time,
  // not frame_buffer_cache_ which is only set after decoding.
  if (index < frame_timings_.size()) {
    return frame_timings_[index].timestamp;
  }
  return std::nullopt;
}

base::TimeDelta JXLImageDecoder::FrameDurationAtIndex(wtf_size_t index) const {
  // Durations are available in frame_timings_ for all discovered frames.
  // Frame discovery happens in ScanFrames() which is called by
  // DecodeFrameCount() whenever new data arrives.
  if (index < frame_timings_.size()) {
    return frame_timings_[index].duration;
  }
  return base::TimeDelta();
}

int JXLImageDecoder::RepetitionCount() const {
  CHECK(basic_info_.has_value());
  if (!basic_info_->have_animation) {
    return kAnimationNone;
  }

  if (basic_info_->animation_loop_count == 0) {
    return kAnimationLoopInfinite;
  }
  return basic_info_->animation_loop_count;
}

wtf_size_t JXLImageDecoder::ClearCacheExceptFrame(
    wtf_size_t clear_except_frame) {
  // With frame seeking support, we can clear cached frames and re-decode
  // them on demand by seeking to the appropriate offset.
  return ImageDecoder::ClearCacheExceptFrame(clear_except_frame);
}

void JXLImageDecoder::OnSetData(scoped_refptr<SegmentReader> data) {
  if (data) {
    // Ensure frame metadata is fully up to date.
    ScanFrames();
  }
}

}  // namespace blink
