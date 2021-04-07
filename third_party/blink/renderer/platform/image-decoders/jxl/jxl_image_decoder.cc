// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/jxl/jxl_image_decoder.h"
#include "base/logging.h"
#include "third_party/blink/renderer/platform/image-decoders/fast_shared_buffer_reader.h"

namespace blink {

JXLImageDecoder::JXLImageDecoder(AlphaOption alpha_option,
                                 const ColorBehavior& color_behavior,
                                 size_t max_decoded_bytes)
    : ImageDecoder(alpha_option,
                   ImageDecoder::kDefaultBitDepth,
                   color_behavior,
                   max_decoded_bytes) {}

JXLImageDecoder::~JXLImageDecoder() {
  JxlDecoderDestroy(dec_);
}

void JXLImageDecoder::Decode(bool only_size) {
  if (IsDecodedSizeAvailable() && only_size) {
    // Also SetEmbeddedProfile is done already if the size was set.
    return;
  }
  if (!dec_) {
    dec_ = JxlDecoderCreate(nullptr);
  } else {
    JxlDecoderReset(dec_);
  }

  // Subscribe to color encoding event even when only getting size, because
  // SetSize must be called after SetEmbeddedColorProfile
  const int events = JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING |
                     (only_size ? 0 : JXL_DEC_FULL_IMAGE);

  if (JXL_DEC_SUCCESS != JxlDecoderSubscribeEvents(dec_, events)) {
    SetFailed();
    return;
  }

  JxlBasicInfo info;
  const JxlPixelFormat format = {4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};

  FastSharedBufferReader reader(data_.get());
  size_t offset = 0;

  // SetEmbeddedColorProfile may only be used if !size_available at the
  // beginning of Decode. This means even if only_size is true, we must already
  // read the color profile as well, or we cannot set it anymore later.
  const bool size_available = IsDecodedSizeAvailable();
  // Our API guarantees that we either get JXL_DEC_ERROR, JXL_DEC_SUCCESS or
  // JXL_DEC_NEED_MORE_INPUT, and we exit the loop below in either case.
  for (;;) {
    JxlDecoderStatus status = JxlDecoderProcessInput(dec_);
    switch (status) {
      case JXL_DEC_ERROR: {
        DVLOG(1) << "Decoder error " << status;
        SetFailed();
        return;
      }
      case JXL_DEC_NEED_MORE_INPUT: {
        const size_t remaining = JxlDecoderReleaseInput(dec_);
        if (remaining != 0) {
          DVLOG(1) << "api needs more input but didn't use all " << remaining;
          SetFailed();
          return;
        }
        if (offset >= reader.size()) {
          if (IsAllDataReceived()) {
            DVLOG(1) << "need more input but all data received";
            SetFailed();
            return;
          }
          return;
        }
        const char* buffer = nullptr;
        size_t read = reader.GetSomeData(buffer, offset);
        if (JXL_DEC_SUCCESS !=
            JxlDecoderSetInput(dec_, reinterpret_cast<const uint8_t*>(buffer),
                               read)) {
          DVLOG(1) << "JxlDecoderSetInput failed";
          SetFailed();
          return;
        }
        offset += read;
        break;
      }
      case JXL_DEC_BASIC_INFO: {
        if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(dec_, &info)) {
          DVLOG(1) << "JxlDecoderGetBasicInfo failed";
          SetFailed();
          return;
        }
        if (!size_available && !SetSize(info.xsize, info.ysize))
          return;
        break;
      }
      case JXL_DEC_COLOR_ENCODING: {
        if (IgnoresColorSpace()) {
          continue;
        }

        if (!size_available) {
          size_t icc_size;
          if (JXL_DEC_SUCCESS !=
              JxlDecoderGetICCProfileSize(
                  dec_, &format, JXL_COLOR_PROFILE_TARGET_DATA, &icc_size)) {
            DVLOG(1) << "JxlDecoderGetICCProfileSize failed";
            SetFailed();
            return;
          }
          std::vector<uint8_t> icc_profile(icc_size);
          if (JXL_DEC_SUCCESS != JxlDecoderGetColorAsICCProfile(
                                     dec_, &format,
                                     JXL_COLOR_PROFILE_TARGET_DATA,
                                     icc_profile.data(), icc_profile.size())) {
            DVLOG(1) << "JxlDecoderGetColorAsICCProfile failed";
            SetFailed();
            return;
          }
          std::unique_ptr<ColorProfile> profile =
              ColorProfile::Create(icc_profile.data(), icc_profile.size());

          if (profile->GetProfile()->data_color_space == skcms_Signature_RGB) {
            SetEmbeddedColorProfile(std::move(profile));
          }
        }
        break;
      }
      case JXL_DEC_NEED_IMAGE_OUT_BUFFER: {
        // Progressive is not yet implemented, and we potentially require some
        // color transforms applied on the buffer from the frame. If we let the
        // JPEG XL decoder write to the buffer immediately, Chrome may already
        // render its intermediate stage with the wrong color format. Hence, for
        // now, only set the buffer and let JXL decode once we have all data.
        if (!IsAllDataReceived())
          return;
        // Always 0 because animation is not yet implemented.
        const size_t frame_index = 0;
        ImageFrame& frame = frame_buffer_cache_[frame_index];
        // This is guaranteed to occur after JXL_DEC_BASIC_INFO so the size
        // is correct.
        if (!InitFrameBuffer(frame_index)) {
          DVLOG(1) << "InitFrameBuffer failed";
          SetFailed();
          return;
        }
        size_t buffer_size;
        if (JXL_DEC_SUCCESS !=
            JxlDecoderImageOutBufferSize(dec_, &format, &buffer_size)) {
          DVLOG(1) << "JxlDecoderImageOutBufferSize failed";
          SetFailed();
          return;
        }
        if (buffer_size != info.xsize * info.ysize * 4) {
          DVLOG(1) << "Unexpected buffer size";
          SetFailed();
          return;
        }
        void* pixels_buffer = reinterpret_cast<void*>(frame.GetAddr(0, 0));
        if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(
                                   dec_, &format, pixels_buffer, buffer_size)) {
          DVLOG(1) << "JxlDecoderSetImageOutBuffer failed";
          SetFailed();
          return;
        }
        break;
      }
      case JXL_DEC_FULL_IMAGE: {
        ImageFrame& frame = frame_buffer_cache_[0];
        frame.SetHasAlpha(info.alpha_bits != 0);
        ColorProfileTransform* xform = ColorTransform();
        constexpr skcms_PixelFormat kSrcFormat = skcms_PixelFormat_RGBA_8888;
        const skcms_PixelFormat kDstFormat = XformColorFormat();

        if (xform || (kDstFormat != kSrcFormat) ||
            (frame.PremultiplyAlpha() && frame.HasAlpha())) {
          skcms_AlphaFormat src_alpha = skcms_AlphaFormat_Unpremul;
          skcms_AlphaFormat dst_alpha =
              (frame.PremultiplyAlpha() && info.alpha_bits)
                  ? skcms_AlphaFormat_PremulAsEncoded
                  : skcms_AlphaFormat_Unpremul;
          const auto* src_profile = xform ? xform->SrcProfile() : nullptr;
          const auto* dst_profile = xform ? xform->DstProfile() : nullptr;
          for (size_t y = 0; y < info.ysize; ++y) {
            ImageFrame::PixelData* row = frame.GetAddr(0, y);
            bool color_conversion_successful =
                skcms_Transform(row, kSrcFormat, src_alpha, src_profile, row,
                                kDstFormat, dst_alpha, dst_profile, info.xsize);
            DCHECK(color_conversion_successful);
          }
        }
        frame.SetPixelsChanged(true);
        frame.SetStatus(ImageFrame::kFrameComplete);
        // We do not support animated images yet, so return after the first
        // frame rather than wait for JXL_DEC_SUCCESS.
        JxlDecoderReset(dec_);
        return;
      }
      case JXL_DEC_SUCCESS: {
        JxlDecoderReset(dec_);
        return;
      }
      default: {
        DVLOG(1) << "Unexpected decoder status " << status;
        SetFailed();
        return;
      }
    }
  }
}

bool JXLImageDecoder::MatchesJXLSignature(
    const FastSharedBufferReader& fast_reader) {
  char buffer[12];
  if (fast_reader.size() < sizeof(buffer))
    return false;
  const char* contents = reinterpret_cast<const char*>(
      fast_reader.GetConsecutiveData(0, sizeof(buffer), buffer));
  // Direct codestream
  if (!memcmp(contents, "\xFF\x0A", 2))
    return true;
  // Box format container
  if (!memcmp(contents, "\0\0\0\x0CJXL \x0D\x0A\x87\x0A", 12))
    return true;
  return false;
}

}  // namespace blink
