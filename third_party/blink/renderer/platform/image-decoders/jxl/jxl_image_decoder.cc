// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/jxl/jxl_image_decoder.h"

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/image-decoders/fast_shared_buffer_reader.h"
#include "third_party/blink/renderer/platform/image-decoders/image_frame.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace blink {

using jxl_rs::jxl_rs_decoder_create;
using jxl_rs::jxl_rs_frame_scanner_create;
using jxl_rs::jxl_rs_seek_decoder_to_frame;
using jxl_rs::jxl_rs_signature_check;
using jxl_rs::JxlRsBasicInfo;
using jxl_rs::JxlRsDecoder;
using jxl_rs::JxlRsFrameHeader;
using jxl_rs::JxlRsFrameScanner;
using jxl_rs::JxlRsPixelFormat;
using jxl_rs::JxlRsProcessResult;
using jxl_rs::JxlRsStatus;
using jxl_rs::JxlRsVisibleFrameInfo;

namespace {

// The maximum number of decoded samples we allow. This helps prevent resource
// exhaustion from malicious files. The jxl-rs API counts pixels * channels,
// so an RGBA image counts 4 samples per pixel. JPEG XL codestream level 5
// limits specify ~268M pixels, so we allow ~1B samples to support that.
constexpr uint64_t kMaxDecodedPixels = 1024ULL * 1024 * 1024;

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
  return is_high_bit_depth_;
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

bool JXLImageDecoder::ProcessBasicInfo(rust::Slice<const uint8_t> icc_data) {
  if (!SetSize(basic_info_.width, basic_info_.height)) {
    return false;
  }

  if (basic_info_.bits_per_sample > 8) {
    is_high_bit_depth_ = true;
  }

  decode_to_half_float_ =
      ImageIsHighBitDepth() &&
      high_bit_depth_decoding_option_ == kHighBitDepthToHalfFloat;

  // Extract ICC color profile.
  if (!IgnoresColorSpace() && !icc_data.empty()) {
    auto profile = ColorProfile::Create(icc_data);
    if (profile) {
      SetEmbeddedColorProfile(std::move(profile));
    }
  }

  // Record bpp information only for 8-bit, color, still images without
  // alpha.
  if (basic_info_.bits_per_sample == 8 && !basic_info_.is_grayscale &&
      !basic_info_.have_animation && !basic_info_.has_alpha) {
    static constexpr char kType[] = "Jxl";
    update_bpp_histogram_callback_ =
        CrossThreadBindOnce(&UpdateBppHistogram<kType>);
  }

  have_basic_info_ = true;
  return true;
}

// ---------------------------------------------------------------------------
// Frame scanning (no pixel decoding)
// ---------------------------------------------------------------------------

void JXLImageDecoder::ScanFrames() {
  if (scanner_done_) {
    return;
  }

  if (!scanner_.has_value()) {
    scanner_ = jxl_rs_frame_scanner_create(kMaxDecodedPixels);
  }

  FastSharedBufferReader reader(data_.get());
  size_t data_size = reader.size();

  // Feed available data segments to the scanner incrementally using
  // GetSomeData() to avoid copying all remaining data into a contiguous
  // buffer.
  while (scanner_input_offset_ < data_size ||
         (IsAllDataReceived() && scanner_input_offset_ == data_size)) {
    CHECK_LE(scanner_input_offset_, data_size);
    base::span<const uint8_t> data_span =
        scanner_input_offset_ < data_size
            ? reader.GetSomeData(scanner_input_offset_)
            : base::span<const uint8_t>();
    bool all_input = IsAllDataReceived() &&
                     (scanner_input_offset_ + data_span.size() >= data_size);
    rust::Slice<const uint8_t> input_slice(data_span.data(), data_span.size());

    JxlRsProcessResult result = (*scanner_)->feed(input_slice, all_input);

    if (result.status == JxlRsStatus::Error) {
      SetFailed();
      return;
    }

    if (result.bytes_consumed > data_size - scanner_input_offset_) {
      SetFailed();
      return;
    }
    scanner_input_offset_ += result.bytes_consumed;

    if (result.status == JxlRsStatus::Success) {
      scanner_done_ = true;
      break;
    }

    // NeedMoreInput: if no bytes were consumed, the scanner needs more
    // contiguous data than this segment provides; wait for more network data.
    // If all data has already been received, the stream is truncated.
    if (result.bytes_consumed == 0) {
      if (all_input) {
        SetFailed();
        return;
      }
      break;
    }
  }

  // If all data received but scanner hasn't finished, the stream is truncated.
  if (IsAllDataReceived() && !scanner_done_ &&
      scanner_input_offset_ >= data_size) {
    SetFailed();
    return;
  }

  // Extract basic info from scanner if not yet available.
  if (!have_basic_info_ && (*scanner_)->has_basic_info()) {
    basic_info_ = (*scanner_)->get_basic_info();
    if (!ProcessBasicInfo((*scanner_)->get_icc_profile())) {
      return;
    }
  }

  // Update frame_infos_ / frame_timings_ from the scanner's discovered frames.
  size_t scanned_count = (*scanner_)->frame_count();
  base::TimeDelta cumulative_time;

  if (!frame_timings_.empty()) {
    const auto& last = frame_timings_.back();
    base::TimeDelta last_end_timestamp = last.timestamp + last.duration;
    if (last_end_timestamp.is_inf() && !last.timestamp.is_inf() &&
        !last.duration.is_inf()) {
      SetFailed();
      return;
    }
    cumulative_time = last_end_timestamp;
  }

  for (size_t i = frame_infos_.size(); i < scanned_count; i++) {
    frame_infos_.push_back((*scanner_)->get_frame_info(i));

    FrameTiming timing;
    timing.duration = base::Milliseconds(frame_infos_.back().duration_ms);
    timing.timestamp = cumulative_time;
    base::TimeDelta next_cumulative_time = cumulative_time + timing.duration;
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
  if (!have_basic_info_) {
    ScanFrames();
  }
}

wtf_size_t JXLImageDecoder::DecodeFrameCount() {
  // Use the lightweight scanner to discover frames (and basic info if needed)
  // without decoding any pixels.
  ScanFrames();

  if (!have_basic_info_ || !basic_info_.have_animation) {
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
  auto& buffer = frame_buffer_cache_[index];

  if (is_high_bit_depth_ &&
      high_bit_depth_decoding_option_ == kHighBitDepthToHalfFloat) {
    buffer.SetPixelFormat(ImageFrame::PixelFormat::kRGBA_F16);
  }

  buffer.SetPremultiplyAlpha(premultiply_alpha_);
  buffer.SetHasAlpha(basic_info_.has_alpha);
  buffer.SetOriginalFrameRect(gfx::Rect(Size()));
  buffer.SetRequiredPreviousFrameIndex(kNotFound);

  // Set duration/timestamp if the frame header has been parsed.
  // This is available before the frame is fully decoded.
  if (index < frame_timings_.size()) {
    const FrameTiming& timing = frame_timings_[index];
    buffer.SetDuration(timing.duration);
    buffer.SetTimestamp(timing.timestamp);
  }
}

void JXLImageDecoder::Decode(wtf_size_t index) {
  Decode(index, false);
}

void JXLImageDecoder::Decode(wtf_size_t index, bool only_size) {
  if (Failed()) {
    return;
  }

  // For size-only queries, use the lightweight scanner instead of the full
  // decoder. ScanFrames() calls ProcessBasicInfo() which sets size, bit
  // depth, color profile, etc.
  if (only_size) {
    if (!have_basic_info_) {
      ScanFrames();
    }
    return;
  }

  // Early return if the requested frame is already fully decoded and cached.
  if (index < frame_buffer_cache_.size()) {
    auto status = frame_buffer_cache_[index].GetStatus();
    if (status == ImageFrame::kFrameComplete) {
      return;
    }
  }

  // For animation frames that need seeking (not the next sequential frame),
  // set up the seek so the main loop handles it.
  if (basic_info_.have_animation && index != num_decoded_frames_) {
    CHECK(have_basic_info_);
    CHECK_GE(decoder_state_, DecoderState::kHaveBasicInfo);
    // DecodeFrameBufferAtIndex() calls FrameCount() -> ScanFrames() before
    // calling Decode(), so seek info is always available at this point.
    CHECK_LT(index, frame_infos_.size());
    SetupSeek(index);
  }

  FastSharedBufferReader reader(data_.get());
  size_t data_size = reader.size();

  // Create decoder if needed. Pass premultiply_alpha_ so jxl-rs handles
  // premultiplication natively (faster and handles alpha_associated correctly).
  if (!decoder_.has_value()) {
    decoder_ = jxl_rs_decoder_create(kMaxDecodedPixels, premultiply_alpha_);
  }

  auto flush_partial_frame = [&](wtf_size_t frame_index) -> bool {
    if (basic_info_.have_animation) {
      return true;
    }

    if (frame_buffer_cache_.size() <= frame_index) {
      frame_buffer_cache_.resize(frame_index + 1);
    }

    ImageFrame& frame = frame_buffer_cache_[frame_index];
    if (frame.GetStatus() == ImageFrame::kFrameEmpty) {
      // IMPORTANT: InitializeNewFrame() must run before InitFrameBuffer(),
      // so the base class allocates the correct backing store (e.g.
      // RGBA_F16 for high bit depth + half float).
      InitializeNewFrame(frame_index);
      if (!InitFrameBuffer(frame_index)) {
        return false;
      }
    }

    frame.SetHasAlpha(basic_info_.has_alpha);

    const SkBitmap& bitmap = frame.Bitmap();
    uint8_t* frame_pixels = static_cast<uint8_t*>(bitmap.getPixels());
    const size_t row_stride = bitmap.rowBytes();
    if (!frame_pixels) {
      return false;
    }

    const size_t buffer_size = row_stride * basic_info_.height;
    rust::Slice<uint8_t> output_slice(frame_pixels, buffer_size);
    JxlRsProcessResult flush_result = (*decoder_)->flush_pixels(
        output_slice, basic_info_.width, basic_info_.height, row_stride);
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

  auto advance_decoder_input_offset = [&](size_t bytes_consumed) -> bool {
    if (bytes_consumed > data_size - decoder_input_offset_) {
      SetFailed();
      return false;
    }
    decoder_input_offset_ += bytes_consumed;
    return true;
  };

  // Process until we get what we need. Uses GetSomeData() to read one
  // buffer segment at a time, avoiding copies across segment boundaries.
  for (;;) {
    CHECK_LE(decoder_input_offset_, data_size);
    if (decoder_input_offset_ == data_size && !IsAllDataReceived()) {
      return;
    }

    base::span<const uint8_t> data_span =
        decoder_input_offset_ < data_size
            ? reader.GetSomeData(decoder_input_offset_)
            : base::span<const uint8_t>();
    bool all_input = IsAllDataReceived() &&
                     (decoder_input_offset_ + data_span.size() >= data_size);
    rust::Slice<const uint8_t> input_slice(data_span.data(), data_span.size());

    switch (decoder_state_) {
      case DecoderState::kInitial: {
        JxlRsProcessResult result =
            (*decoder_)->parse_basic_info(input_slice, all_input);

        if (result.status == JxlRsStatus::Error) {
          SetFailed();
          return;
        }
        if (result.status == JxlRsStatus::NeedMoreInput) {
          if (!advance_decoder_input_offset(result.bytes_consumed)) {
            return;
          }
          if (all_input) {
            SetFailed();
            return;
          }
          // If more data is available in the buffer, continue feeding.
          if (result.bytes_consumed > 0 && decoder_input_offset_ < data_size) {
            break;
          }
          return;
        }

        // Success - got basic info
        basic_info_ = (*decoder_)->get_basic_info();
        if (!advance_decoder_input_offset(result.bytes_consumed)) {
          return;
        }

        if (!ProcessBasicInfo((*decoder_)->get_icc_profile())) {
          return;
        }

        // Set pixel format on decoder.
        // Use native 8-bit ordering for kN32, and RGBA F16 for half float.
#if SK_PMCOLOR_BYTE_ORDER(B, G, R, A)
        constexpr JxlRsPixelFormat kNativePixelFormat = JxlRsPixelFormat::Bgra8;
#elif SK_PMCOLOR_BYTE_ORDER(R, G, B, A)
        constexpr JxlRsPixelFormat kNativePixelFormat = JxlRsPixelFormat::Rgba8;
#else
#error "Unsupported Skia pixel order"
#endif
        JxlRsPixelFormat pixel_format = decode_to_half_float_
                                            ? JxlRsPixelFormat::RgbaF16
                                            : kNativePixelFormat;
        (*decoder_)->set_pixel_format(pixel_format,
                                      basic_info_.num_extra_channels);

        decoder_state_ = DecoderState::kHaveBasicInfo;

        break;
      }

      case DecoderState::kHaveBasicInfo: {
        JxlRsProcessResult result =
            (*decoder_)->parse_frame_header(input_slice, all_input);

        if (result.status == JxlRsStatus::Error) {
          SetFailed();
          return;
        }
        if (result.status == JxlRsStatus::NeedMoreInput) {
          if (!advance_decoder_input_offset(result.bytes_consumed)) {
            return;
          }
          if (all_input) {
            SetFailed();
            return;
          }
          // If more data is available in the buffer, continue feeding.
          if (result.bytes_consumed > 0 && decoder_input_offset_ < data_size) {
            break;
          }

          // Try progressive flush while still parsing the frame header.
          // This can expose LF preview data for still images earlier.
          wtf_size_t frame_index = seek_target_index_.has_value()
                                       ? seek_target_index_.value()
                                       : num_decoded_frames_;
          if (!flush_partial_frame(frame_index)) {
            SetFailed();
            return;
          }
          return;
        }

        if (!advance_decoder_input_offset(result.bytes_consumed)) {
          return;
        }

        // Successfully parsed a frame header.
        JxlRsFrameHeader header = (*decoder_)->get_frame_header();

        if (basic_info_.have_animation) {
          wtf_size_t frame_idx = num_decoded_frames_;

          // Update frame timing if we don't have it yet from the scanner.
          if (frame_idx >= frame_timings_.size()) {
            FrameTiming timing;
            timing.duration = base::Milliseconds(header.duration_ms);
            timing.timestamp = base::TimeDelta();
            if (frame_idx > 0 && frame_idx - 1 < frame_timings_.size()) {
              const FrameTiming& prev = frame_timings_[frame_idx - 1];
              base::TimeDelta cumulative_timestamp =
                  prev.timestamp + prev.duration;
              if (cumulative_timestamp.is_inf() && !prev.timestamp.is_inf() &&
                  !prev.duration.is_inf()) {
                SetFailed();
                return;
              }
              timing.timestamp = cumulative_timestamp;
            }
            frame_timings_.push_back(timing);
          }
        }

        decoder_state_ = DecoderState::kHaveFrameHeader;
        break;
      }

      case DecoderState::kHaveFrameHeader: {
        bool is_seeking = seek_target_index_.has_value();
        wtf_size_t frame_index =
            is_seeking ? seek_target_index_.value() : num_decoded_frames_;

        // Ensure frame buffer cache is large enough.
        if (frame_buffer_cache_.size() <= frame_index) {
          frame_buffer_cache_.resize(frame_index + 1);
        }

        ImageFrame& frame = frame_buffer_cache_[frame_index];
        if (frame.GetStatus() == ImageFrame::kFrameEmpty) {
          // IMPORTANT: InitializeNewFrame() must run before InitFrameBuffer(),
          // so the base class allocates the correct backing store (e.g.
          // RGBA_F16 for high bit depth + half float).
          InitializeNewFrame(frame_index);
          if (!InitFrameBuffer(frame_index)) {
            SetFailed();
            return;
          }
        }

        frame.SetHasAlpha(basic_info_.has_alpha);

        const uint32_t width = basic_info_.width;
        const uint32_t height = basic_info_.height;

        // Get direct access to the frame buffer's backing store.
        const SkBitmap& bitmap = frame.Bitmap();
        uint8_t* frame_pixels = static_cast<uint8_t*>(bitmap.getPixels());
        size_t row_stride = bitmap.rowBytes();

        if (!frame_pixels) {
          SetFailed();
          return;
        }

        // Calculate buffer size for the decoder.
        size_t buffer_size = row_stride * height;
        rust::Slice<uint8_t> output_slice(frame_pixels, buffer_size);

        // Decode directly into the frame buffer.
        // Premultiplication is handled by jxl-rs based on premultiply_alpha_.
        JxlRsProcessResult result = (*decoder_)->decode_frame_with_stride(
            input_slice, all_input, output_slice, width, height, row_stride);

        if (result.status == JxlRsStatus::Error) {
          SetFailed();
          return;
        }
        if (result.status == JxlRsStatus::NeedMoreInput) {
          // Update offset with consumed bytes for progressive decoding.
          if (!advance_decoder_input_offset(result.bytes_consumed)) {
            return;
          }

          // If more data is available in the buffer, continue feeding.
          if (result.bytes_consumed > 0 && decoder_input_offset_ < data_size) {
            break;
          }

          // All buffered data consumed. Progressively flush decoded pixels
          // for still images only. For animations, each frame must be fully
          // decoded before display.
          if (!basic_info_.have_animation) {
            JxlRsProcessResult flush_result = (*decoder_)->flush_pixels(
                output_slice, width, height, row_stride);
            if (flush_result.status == JxlRsStatus::Error) {
              SetFailed();
              return;
            }
            if (flush_result.status == JxlRsStatus::Success) {
              frame.SetPixelsChanged(true);
              frame.SetStatus(ImageFrame::kFramePartial);
            }
          }

          if (all_input) {
            SetFailed();
          }
          return;
        }

        if (!advance_decoder_input_offset(result.bytes_consumed)) {
          return;
        }
        frame.SetPixelsChanged(true);
        frame.SetStatus(ImageFrame::kFrameComplete);

        if (frame_index < frame_timings_.size()) {
          const FrameTiming& timing = frame_timings_[frame_index];
          frame.SetDuration(timing.duration);
          frame.SetTimestamp(timing.timestamp);
        }

        if (is_seeking) {
          // Target-frame decode after seeking is complete. Reset seek state;
          // the decoder is ready for the next seek or sequential decode from
          // this point.
          seek_target_index_.reset();
          decoder_state_ = DecoderState::kHaveBasicInfo;
          return;
        }

        num_decoded_frames_++;

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
        if (frame_index >= index) {
          return;
        }
        break;
      }
      case DecoderState::kDone:
        return;
    }
  }
}

// ---------------------------------------------------------------------------
// Seek setup (integrates with the main decode loop)
// ---------------------------------------------------------------------------

void JXLImageDecoder::SetupSeek(wtf_size_t index) {
  CHECK_LT(index, frame_infos_.size());
  CHECK(scanner_.has_value());

  // Position the decoder at the start of the frame group containing
  // the target frame. The full seek target (including internal jxl-rs
  // state) is looked up from the scanner. Visible frame skipping (for
  // non-keyframes) is handled automatically by jxl-rs.
  jxl_rs_seek_decoder_to_frame(**scanner_, **decoder_, index);
  decoder_input_offset_ = frame_infos_[index].decode_start_file_offset;

  seek_target_index_ = index;
  decoder_state_ = DecoderState::kHaveBasicInfo;
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
  if (!basic_info_.have_animation) {
    return kAnimationNone;
  }

  if (basic_info_.animation_loop_count == 0) {
    return kAnimationLoopInfinite;
  }
  return basic_info_.animation_loop_count;
}

wtf_size_t JXLImageDecoder::ClearCacheExceptFrame(
    wtf_size_t clear_except_frame) {
  // With frame seeking support, we can clear cached frames and re-decode
  // them on demand by seeking to the appropriate offset.
  return ImageDecoder::ClearCacheExceptFrame(clear_except_frame);
}

SkColorType JXLImageDecoder::GetSkColorType() const {
  if (is_high_bit_depth_ &&
      high_bit_depth_decoding_option_ == kHighBitDepthToHalfFloat) {
    return kRGBA_F16_SkColorType;
  }
  return kN32_SkColorType;
}

}  // namespace blink
