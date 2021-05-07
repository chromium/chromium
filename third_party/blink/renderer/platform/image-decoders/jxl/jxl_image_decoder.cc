// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/jxl/jxl_image_decoder.h"
#include "base/logging.h"
#include "third_party/blink/renderer/platform/image-decoders/fast_shared_buffer_reader.h"

namespace blink {

JXLImageDecoder::JXLImageDecoder(
    AlphaOption alpha_option,
    HighBitDepthDecodingOption high_bit_depth_decoding_option,
    const ColorBehavior& color_behavior,
    size_t max_decoded_bytes)
    : ImageDecoder(alpha_option,
                   high_bit_depth_decoding_option,
                   color_behavior,
                   max_decoded_bytes) {}

JXLImageDecoder::~JXLImageDecoder() {
  JxlDecoderDestroy(dec_);
}

inline bool DecodeToHalfFloat(const JxlPixelFormat& format) {
  return format.data_type == JXL_TYPE_FLOAT16;
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

        // If the decoder was used before with only_size == true, the color
        // encoding is already decoded as well, and SetEmbeddedColorProfile
        // should not be called a second time anymore.
        if (size_available) {
          continue;
        }

        // Detect whether the JXL image is intended to be an HDR image: when it
        // uses more than 8 bits per pixel, or when it has explicitly marked
        // PQ or HLG color profile.
        if (info.bits_per_sample > 8) {
          is_hdr_ = true;
        }
        JxlColorEncoding color_encoding;
        if (JXL_DEC_SUCCESS == JxlDecoderGetColorAsEncodedProfile(
                                   dec_, &format_,
                                   JXL_COLOR_PROFILE_TARGET_ORIGINAL,
                                   &color_encoding)) {
          if (color_encoding.transfer_function == JXL_TRANSFER_FUNCTION_PQ ||
              color_encoding.transfer_function == JXL_TRANSFER_FUNCTION_HLG) {
            is_hdr_ = true;
          }
        }

        std::unique_ptr<ColorProfile> profile;

        if (is_hdr_ &&
            high_bit_depth_decoding_option_ == kHighBitDepthToHalfFloat) {
          format_.data_type = JXL_TYPE_FLOAT16;
          format_.endianness = JXL_LITTLE_ENDIAN;
        }

        bool have_data_profile = false;
        if (JXL_DEC_SUCCESS ==
            JxlDecoderGetColorAsEncodedProfile(dec_, &format_,
                                               JXL_COLOR_PROFILE_TARGET_DATA,
                                               &color_encoding)) {
          bool known_transfer_function = true;
          bool known_gamut = true;
          skcms_Matrix3x3 gamut;
          skcms_TransferFunction transfer;
          // PQ or HLG as the data may occur if the JXL image was lossless, or
          // not in XYB color space. If the JXL image was lossy with XYB, then
          // instead linear sRGB is expected here when we treat the image as HDR
          // (when format_.data_type == JXL_TYPE_FLOAT), nonlinear sRGB for SDR.
          if (color_encoding.transfer_function == JXL_TRANSFER_FUNCTION_PQ) {
            transfer = SkNamedTransferFn::kPQ;
          } else if (color_encoding.transfer_function ==
                     JXL_TRANSFER_FUNCTION_HLG) {
            transfer = SkNamedTransferFn::kHLG;
          } else if (color_encoding.transfer_function ==
                     JXL_TRANSFER_FUNCTION_LINEAR) {
            transfer = SkNamedTransferFn::kLinear;
          } else if (color_encoding.transfer_function ==
                     JXL_TRANSFER_FUNCTION_SRGB) {
            transfer = SkNamedTransferFn::kSRGB;
          } else {
            known_transfer_function = false;
          }

          if (color_encoding.white_point == JXL_WHITE_POINT_D65 &&
              color_encoding.primaries == JXL_PRIMARIES_2100) {
            gamut = SkNamedGamut::kRec2020;
          } else if (color_encoding.white_point == JXL_WHITE_POINT_D65 &&
                     color_encoding.primaries == JXL_PRIMARIES_SRGB) {
            gamut = SkNamedGamut::kSRGB;
          } else {
            known_gamut = false;
          }

          have_data_profile = known_transfer_function && known_gamut;

          if (have_data_profile) {
            skcms_ICCProfile dataProfile;
            SkColorSpace::MakeRGB(transfer, gamut)->toProfile(&dataProfile);
            profile = std::make_unique<ColorProfile>(dataProfile);
          }
        }

        // Did not handle exact enum values, get as ICC profile instead.
        if (!have_data_profile) {
          size_t icc_size;
          bool got_size =
              JXL_DEC_SUCCESS ==
              JxlDecoderGetICCProfileSize(
                  dec_, &format_, JXL_COLOR_PROFILE_TARGET_DATA, &icc_size);
          std::vector<uint8_t> icc_profile(icc_size);
          if (got_size && JXL_DEC_SUCCESS ==
                              JxlDecoderGetColorAsICCProfile(
                                  dec_, &format_, JXL_COLOR_PROFILE_TARGET_DATA,
                                  icc_profile.data(), icc_profile.size())) {
            profile =
                ColorProfile::Create(icc_profile.data(), icc_profile.size());
            have_data_profile = true;

            // Detect whether the ICC profile approximately equals PQ or HLG,
            // and set the profile to one that indicates this transfer function
            // more clearly than a raw ICC profile does, so Chrome considers
            // the profile as HDR.
            const skcms_ICCProfile* parsed = profile->GetProfile();
            skcms_ICCProfile parsed_pq = *parsed;
            parsed_pq.has_trc = true;
            for (int c = 0; c < 3; c++) {
              parsed_pq.trc[c].table_entries = 0;
              skcms_TransferFunction_makePQ(&parsed_pq.trc[c].parametric);
            }
            bool approx_pq =
                skcms_ApproximatelyEqualProfiles(parsed, &parsed_pq);

            skcms_ICCProfile parsed_hlg = *parsed;
            parsed_hlg.has_trc = true;
            for (int c = 0; c < 3; c++) {
              parsed_hlg.trc[c].table_entries = 0;
              skcms_TransferFunction_makeHLG(&parsed_hlg.trc[c].parametric);
            }
            bool approx_hlg =
                skcms_ApproximatelyEqualProfiles(parsed, &parsed_hlg);

            if (approx_pq) {
              profile = std::make_unique<ColorProfile>(parsed_pq);
              is_hdr_ = true;
            } else if (approx_hlg) {
              profile = std::make_unique<ColorProfile>(parsed_hlg);
              is_hdr_ = true;
            }
          }
        }

        if (is_hdr_ &&
            high_bit_depth_decoding_option_ == kHighBitDepthToHalfFloat) {
          format_.data_type = JXL_TYPE_FLOAT16;
          format_.endianness = JXL_LITTLE_ENDIAN;
        }

        if (have_data_profile) {
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
        // render its intermediate stage with the wrong color format_. Hence,
        // for now, only set the buffer and let JXL decode once we have all
        // data.
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
            JxlDecoderImageOutBufferSize(dec_, &format_, &buffer_size)) {
          DVLOG(1) << "JxlDecoderImageOutBufferSize failed";
          SetFailed();
          return;
        }

        void* pixels_buffer;

        if (DecodeToHalfFloat(format_)) {
          if (buffer_size != info.xsize * info.ysize * 8) {
            DVLOG(1) << "Unexpected buffer size";
            SetFailed();
            return;
          }
          pixels_buffer = reinterpret_cast<void*>(frame.GetAddrF16(0, 0));
        } else {
          if (buffer_size != info.xsize * info.ysize * 4) {
            DVLOG(1) << "Unexpected buffer size";
            SetFailed();
            return;
          }
          pixels_buffer = reinterpret_cast<void*>(frame.GetAddr(0, 0));
        }

        if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(dec_, &format_,
                                                           pixels_buffer,
                                                           buffer_size)) {
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
        const skcms_PixelFormat kSrcFormat = DecodeToHalfFloat(format_)
                                                 ? skcms_PixelFormat_RGBA_hhhh
                                                 : skcms_PixelFormat_RGBA_8888;
        const skcms_PixelFormat kDstFormat = DecodeToHalfFloat(format_)
                                                 ? skcms_PixelFormat_RGBA_hhhh
                                                 : XformColorFormat();

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
            void* row = DecodeToHalfFloat(format_)
                            ? reinterpret_cast<void*>(frame.GetAddrF16(0, y))
                            : reinterpret_cast<void*>(frame.GetAddr(0, y));
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

void JXLImageDecoder::InitializeNewFrame(size_t index) {
  auto& buffer = frame_buffer_cache_[index];
  if (DecodeToHalfFloat(format_))
    buffer.SetPixelFormat(ImageFrame::PixelFormat::kRGBA_F16);
}

}  // namespace blink
