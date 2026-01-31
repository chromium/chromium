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
using jxl_rs::jxl_rs_signature_check;
using jxl_rs::JxlRsBasicInfo;
using jxl_rs::JxlRsDecoder;
using jxl_rs::JxlRsFrameHeader;
using jxl_rs::JxlRsPixelFormat;
using jxl_rs::JxlRsProcessResult;
using jxl_rs::JxlRsStatus;

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

void JXLImageDecoder::DecodeSize() {
  Decode(0, /*only_size=*/true);
}

wtf_size_t JXLImageDecoder::DecodeFrameCount() {
  // Parse metadata (BasicInfo) to know if this is an animation.
  if (!have_basic_info_) {
    Decode(0, /*only_size=*/true);
  }

  if (!basic_info_.have_animation) {
    return 1;
  }

  // If we have received all the data, we must produce the correct
  // frame count. Thus, we always decode all the data we have.
  // TODO(veluca): for long animations, this will currently decode
  // the entire file, using a large amount of memory and CPU time.
  // Avoid doing that once jxl-rs supports seeking and/or frame
  // skipping.
  while (decoder_state_ != DecoderState::kDone) {
    size_t offset_pre = input_offset_;
    size_t decoded_frames_pre = num_decoded_frames_;
    Decode(num_decoded_frames_, /*only_size=*/false);
    // Exit the loop if the image is corrupted or we didn't make any progress.
    if (Failed() || (offset_pre == input_offset_ &&
                     num_decoded_frames_ == decoded_frames_pre)) {
      break;
    }
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
  if (index < frame_info_.size()) {
    const FrameInfo& info = frame_info_[index];
    buffer.SetDuration(info.duration);
    buffer.SetTimestamp(info.timestamp);
  }
}

void JXLImageDecoder::Decode(wtf_size_t index) {
  Decode(index, false);
}

void JXLImageDecoder::Decode(wtf_size_t index, bool only_size) {
  if (Failed()) {
    return;
  }

  if (only_size && IsDecodedSizeAvailable() && have_basic_info_) {
    return;
  }

  // Early return if the requested frame is already fully decoded and cached.
  if (!only_size && index < frame_buffer_cache_.size()) {
    auto status = frame_buffer_cache_[index].GetStatus();
    if (status == ImageFrame::kFrameComplete) {
      return;
    }
  }

  FastSharedBufferReader reader(data_.get());
  size_t data_size = reader.size();

  // Handle animation loop rewind.
  if (decoder_.has_value() && !only_size && basic_info_.have_animation) {
    bool frame_already_cached =
        index < frame_buffer_cache_.size() &&
        frame_buffer_cache_[index].GetStatus() == ImageFrame::kFrameComplete;

    if (!frame_already_cached && index < num_decoded_frames_) {
      (*decoder_)->rewind();
      decoder_state_ = DecoderState::kInitial;
      num_decoded_frames_ = 0;
      input_offset_ = 0;
      // Keep basic_info_ and have_basic_info_ since the stream hasn't changed.
    }
  }

  // Create decoder if needed. Pass premultiply_alpha_ so jxl-rs handles
  // premultiplication natively (faster and handles alpha_associated correctly).
  if (!decoder_.has_value()) {
    decoder_ = jxl_rs_decoder_create(kMaxDecodedPixels, premultiply_alpha_);
  }

  // Process until we get what we need.
  for (;;) {
    size_t remaining_size = data_size - input_offset_;
    // When all data is received, process it all at once for efficiency.
    // Only use smaller chunks for true progressive loading (streaming data).
    size_t chunk_size;
    if (IsAllDataReceived()) {
      chunk_size = remaining_size;  // Process all available data
    } else {
      // Progressive streaming: use smaller chunks to allow partial rendering
      constexpr size_t kMaxChunkSize = 64 * 1024;
      chunk_size = std::min(remaining_size, kMaxChunkSize);
    }

    base::span<const uint8_t> data_span;
    Vector<uint8_t> chunk_buffer;
    if (chunk_size > 0) {
      chunk_buffer.resize(chunk_size);
      data_span = reader.GetConsecutiveData(input_offset_, chunk_size,
                                            base::span(chunk_buffer));
    }

    bool all_input =
        IsAllDataReceived() && (input_offset_ + chunk_size >= data_size);
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
          input_offset_ += result.bytes_consumed;
          if (all_input) {
            SetFailed();
          }
          return;
        }

        // Success - got basic info
        basic_info_ = (*decoder_)->get_basic_info();
        input_offset_ += result.bytes_consumed;

        if (!SetSize(basic_info_.width, basic_info_.height)) {
          return;
        }

        if (basic_info_.bits_per_sample > 8) {
          is_high_bit_depth_ = true;
        }

        decode_to_half_float_ =
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
        JxlRsPixelFormat pixel_format = decode_to_half_float_
                                            ? JxlRsPixelFormat::RgbaF16
                                            : kNativePixelFormat;
        (*decoder_)->set_pixel_format(pixel_format,
                                      basic_info_.num_extra_channels);

        // Extract ICC color profile.
        if (!IgnoresColorSpace()) {
          auto icc_data = (*decoder_)->get_icc_profile();
          if (!icc_data.empty()) {
            Vector<uint8_t> icc_copy;
            icc_copy.AppendRange(icc_data.begin(), icc_data.end());
            auto profile = ColorProfile::Create(base::span(icc_copy));
            if (profile) {
              SetEmbeddedColorProfile(std::move(profile));
            }
          }
        }

        // Record bpp information only for 8-bit, color, still images without
        // alpha.
        if (!have_basic_info_ && basic_info_.bits_per_sample == 8 &&
            !basic_info_.is_grayscale && !basic_info_.have_animation &&
            !basic_info_.has_alpha) {
          static constexpr char kType[] = "Jxl";
          update_bpp_histogram_callback_ =
              CrossThreadBindOnce(&UpdateBppHistogram<kType>);
        }

        have_basic_info_ = true;
        decoder_state_ = DecoderState::kHaveBasicInfo;

        if (only_size) {
          return;
        }
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
          input_offset_ += result.bytes_consumed;
          return;
        }

        input_offset_ += result.bytes_consumed;

        // Successfully parsed a frame header - increment discovered count.
        JxlRsFrameHeader header = (*decoder_)->get_frame_header();

        if (basic_info_.have_animation) {
          wtf_size_t frame_idx = num_decoded_frames_;
          FrameInfo info;
          info.duration = base::Milliseconds(header.duration_ms);
          info.timestamp = base::TimeDelta();

          if (frame_idx > 0 && frame_idx - 1 < frame_info_.size()) {
            const FrameInfo& prev = frame_info_[frame_idx - 1];
            info.timestamp = prev.timestamp + prev.duration;
          }

          if (frame_idx < frame_info_.size()) {
            frame_info_[frame_idx] = info;
          } else {
            CHECK_EQ(frame_idx, frame_info_.size());
            frame_info_.push_back(info);
          }
        }

        decoder_state_ = DecoderState::kHaveFrameHeader;
        break;
      }

      case DecoderState::kHaveFrameHeader: {
        wtf_size_t frame_index = num_decoded_frames_;

        // Ensure frame buffer cache is large enough.
        if (frame_buffer_cache_.size() <= frame_index) {
          frame_buffer_cache_.resize(frame_index + 1);
        }

        ImageFrame& frame = frame_buffer_cache_[frame_index];
        if (frame.GetStatus() == ImageFrame::kFrameEmpty) {
          // We call InitializeNewFrame manually here because JXLImageDecoder,
          // unlike other image decoder classes, handles the frame buffer cache
          // in the decode loop. This happens because decoding the frame count
          // also fully renders the frames - when we switch to lightweight
          // decoding for frame count + decoding individual frames via seeking,
          // we will likely be able to remove this call.
          //
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
          input_offset_ += result.bytes_consumed;

          // Signal that pixels may have changed for progressive rendering.
          // TODO(veluca): set the frame status to kFramePartial if and only
          // if jxl-rs signals that some data has been painted (jxl-rs
          // does not yet expose this functionality, nor does it do
          // progressive rendering properly).
          frame.SetStatus(ImageFrame::kFramePartial);
          frame.SetPixelsChanged(true);
          if (all_input) {
            SetFailed();
          }
          return;
        }

        input_offset_ += result.bytes_consumed;
        frame.SetPixelsChanged(true);
        frame.SetStatus(ImageFrame::kFrameComplete);

        if (frame_index < frame_info_.size()) {
          const FrameInfo& info = frame_info_[frame_index];
          frame.SetDuration(info.duration);
          frame.SetTimestamp(info.timestamp);
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
        break;
    }
  }
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
  // Use frame_info_ which is populated at header parsing time,
  // not frame_buffer_cache_ which is only set after decoding.
  if (index < frame_info_.size()) {
    return frame_info_[index].timestamp;
  }
  return std::nullopt;
}

base::TimeDelta JXLImageDecoder::FrameDurationAtIndex(wtf_size_t index) const {
  // Durations are available in frame_info_ for all discovered frames.
  // Frame discovery happens in DecodeFrameCount() which is called by
  // FrameCount() whenever new data arrives.
  if (index < frame_info_.size()) {
    return frame_info_[index].duration;
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
  if (basic_info_.have_animation) {
    // TODO(veluca): jxl-rs does not (yet) support seeking to specific frames.
    // For now, deal with this by disallowing clearing the cache.

    return 0;
  }

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
