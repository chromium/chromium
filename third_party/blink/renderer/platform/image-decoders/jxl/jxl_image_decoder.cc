// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/jxl/jxl_image_decoder.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/image-decoders/fast_shared_buffer_reader.h"
#include "third_party/skia/include/core/SkColorSpace.h"

namespace blink {

namespace {
// Returns transfer function which approximates HLG with linear range 0..1,
// while skcms_TransferFunction_makeHLGish uses linear range 0..12.
void MakeTransferFunctionHLG01(skcms_TransferFunction* tf) {
  skcms_TransferFunction_makeScaledHLGish(
      tf, 1 / 12.0f, 2.0f, 2.0f, 1 / 0.17883277f, 0.28466892f, 0.55991073f);
}

// The input profile must outlive the output one as they will share their
// buffers.
skcms_ICCProfile ReplaceTransferFunction(skcms_ICCProfile profile,
                                         const skcms_TransferFunction& tf) {
  // Override the transfer function with a known parametric curve.
  profile.has_trc = true;
  for (int c = 0; c < 3; c++) {
    profile.trc[c].table_entries = 0;
    profile.trc[c].parametric = tf;
  }
  return profile;
}

// Computes whether the transfer function from the ColorProfile, that was
// created from a parsed ICC profile, approximately matches the given parametric
// transfer function.
bool ApproximatelyMatchesTF(const ColorProfile& profile,
                            const skcms_TransferFunction& tf) {
  skcms_ICCProfile parsed_copy =
      ReplaceTransferFunction(*profile.GetProfile(), tf);
  return skcms_ApproximatelyEqualProfiles(profile.GetProfile(), &parsed_copy);
}

std::unique_ptr<ColorProfile> NewColorProfileWithSameBuffer(
    const ColorProfile& buffer_donor,
    skcms_ICCProfile new_profile) {
  // The input ColorProfile owns the buffer memory, make a new copy for
  // the newly created one and pass the ownership of the new copy to the new
  // color profile.
  std::unique_ptr<uint8_t[]> owned_buffer(
      new uint8_t[buffer_donor.GetProfile()->size]);
  memcpy(owned_buffer.get(), buffer_donor.GetProfile()->buffer,
         buffer_donor.GetProfile()->size);
  new_profile.buffer = owned_buffer.get();
  return std::make_unique<ColorProfile>(new_profile, std::move(owned_buffer));
}
}  // namespace

JXLImageDecoder::JXLImageDecoder(
    AlphaOption alpha_option,
    HighBitDepthDecodingOption high_bit_depth_decoding_option,
    const ColorBehavior& color_behavior,
    wtf_size_t max_decoded_bytes)
    : ImageDecoder(alpha_option,
                   high_bit_depth_decoding_option,
                   color_behavior,
                   max_decoded_bytes) {
  info_.have_animation = false;
}

bool JXLImageDecoder::ReadBytes(size_t remaining,
                                wtf_size_t* offset,
                                WTF::Vector<uint8_t>* segment,
                                FastSharedBufferReader* reader,
                                const uint8_t** jxl_data,
                                size_t* jxl_size) {
  *offset -= remaining;
  if (*offset + remaining >= reader->size()) {
    segment->clear();
    if (IsAllDataReceived()) {
      DVLOG(1) << "need more input but all data received";
      SetFailed();
      return false;
    }
    // Return because we need more input from the reader, to continue
    // decoding in the next call.
    return false;
  }
  const char* buffer = nullptr;
  size_t read = reader->GetSomeData(buffer, *offset);

  if (read > remaining) {
    // Sufficient data present in the segment from the
    // FastSharedBufferReader, no need to copy to segment_.
    *jxl_data = reinterpret_cast<const uint8_t*>(buffer);
    *jxl_size = read;
    *offset += read;
    segment->clear();
  } else {
    if (segment->size() == remaining) {
      // Keep reading from the end of the segment_ we already are
      // appending to. The above read is ignored, and start reading after the
      // end of the data we already have.
      *offset += remaining;
      read = 0;
    } else {
      // segment_->size() could be greater than or smaller than remaining.
      // Typically, it'll be smaller than. If it is greater than, then we could
      // do something similar as in the segment->size() == remaining case but
      // remove the non-remaining bytes from the beginning of the segment_
      // vector. This would avoid re-reading, however the case where
      // segment->size() > remaining is rare since normally if the JXL decoder
      // returns a positive value for remaining, it will be consistent, making
      // the sizes match exactly, so this more complex case is not implemented.
      // Clear the segment, the bytes from the GetSomeData above will be
      // appended and then we continue reading from the position after the
      // above GetSomeData read.
      segment->clear();
    }

    for (;;) {
      if (read) {
        *offset += read;
        segment->Append(buffer, base::checked_cast<wtf_size_t>(read));
      }
      if (segment->size() > remaining) {
        *jxl_data = segment->data();
        *jxl_size = segment->size();
        // Have enough data, break and continue JXL decoding, rather than
        // copy more input than needed into segment_.
        break;
      }
      read = reader->GetSomeData(buffer, *offset);
      if (read == 0) {
        // We tested above that *offset + remaining >= reader.size() so
        // should be able to read all data.
        DVLOG(1) << "couldn't read all available data";
        SetFailed();
        return false;
      }
    }
  }
  return true;
}

void JXLImageDecoder::DecodeImpl(wtf_size_t index, bool only_size) {
  if (Failed())
    return;

  if (IsDecodedSizeAvailable() && only_size) {
    // Also SetEmbeddedProfile is done already if the size was set.
    return;
  }

  DCHECK_LE(num_decoded_frames_, frame_buffer_cache_.size());
  if (num_decoded_frames_ > index &&
      frame_buffer_cache_[index].GetStatus() == ImageFrame::kFrameComplete) {
    // Frame already complete
    return;
  }
  if ((index < num_decoded_frames_) && dec_) {
    // An animation frame that already has been decoded, but does not have
    // status ImageFrame::kFrameComplete, was requested.
    // This can mean two things:
    // (1) an earlier animation frame was purged but is to be re-decoded now.
    // Rewind the decoder and skip to the requested frame.
    // (2) During progressive decoding the frame has the status
    // ImageFrame::kFramePartial.
    JxlDecoderRewind(dec_.get());
    offset_ = 0;
    // No longer subscribe to JXL_DEC_BASIC_INFO or JXL_DEC_COLOR_ENCODING.
    if (JXL_DEC_SUCCESS !=
        JxlDecoderSubscribeEvents(
            dec_.get(), JXL_DEC_FULL_IMAGE | JXL_DEC_FRAME_PROGRESSION)) {
      SetFailed();
      return;
    }
    JxlDecoderSkipFrames(dec_.get(), index);
    num_decoded_frames_ = index;
  }

  if (!dec_) {
    dec_ = JxlDecoderMake(nullptr);
    // Subscribe to color encoding event even when only getting size, because
    // SetSize must be called after SetEmbeddedColorProfile
    const int events = JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING |
                       JXL_DEC_FULL_IMAGE | JXL_DEC_FRAME_PROGRESSION;

    if (JXL_DEC_SUCCESS != JxlDecoderSubscribeEvents(dec_.get(), events)) {
      SetFailed();
      return;
    }
    if (JXL_DEC_SUCCESS !=
        JxlDecoderSetProgressiveDetail(dec_.get(), JxlProgressiveDetail::kDC)) {
      SetFailed();
      return;
    }
  } else {
    offset_ -= JxlDecoderReleaseInput(dec_.get());
  }

  FastSharedBufferReader reader(data_.get());

  const JxlPixelFormat format = {4, JXL_TYPE_FLOAT, JXL_NATIVE_ENDIAN, 0};

  const bool size_available = IsDecodedSizeAvailable();

  if (have_color_info_) {
    xform_ = ColorTransform();
  }

  // The JXL API guarantees that we eventually get JXL_DEC_ERROR,
  // JXL_DEC_SUCCESS or JXL_DEC_NEED_MORE_INPUT, and we exit the loop below in
  // each case.
  for (;;) {
    if (only_size && have_color_info_)
      return;
    JxlDecoderStatus status = JxlDecoderProcessInput(dec_.get());
    switch (status) {
      case JXL_DEC_ERROR: {
        DVLOG(1) << "Decoder error " << status;
        SetFailed();
        return;
      }
      case JXL_DEC_NEED_MORE_INPUT: {
        // The decoder returns how many bytes it has not yet processed, and
        // must be included in the next JxlDecoderSetInput call.
        const size_t remaining = JxlDecoderReleaseInput(dec_.get());
        const uint8_t* jxl_data = nullptr;
        size_t jxl_size = 0;
        if (!ReadBytes(remaining, &offset_, &segment_, &reader, &jxl_data,
                       &jxl_size)) {
          if (IsAllDataReceived()) {
            // Happens only if a partial image file was transferred, otherwise
            // status will be JXL_DEC_FULL_IMAGE or JXL_DEC_SUCCESS. In
            // this case we flush one more time in order to get the progressive
            // image plus everything known so far. The progressive image was not
            // flushed when status was JXL_DEC_FRAME_PROGRESSION because all
            // data seemed to have been received (not knowing then that it was
            // only a partial file).
            if (JXL_DEC_SUCCESS != JxlDecoderFlushImage(dec_.get())) {
              DVLOG(1) << "JxlDecoderSetImageOutCallback failed";
              SetFailed();
              return;
            }
            ImageFrame& frame = frame_buffer_cache_[num_decoded_frames_ - 1];
            frame.SetPixelsChanged(true);
            frame.SetStatus(ImageFrame::kFramePartial);
          }
          return;
        }

        if (JXL_DEC_SUCCESS !=
            JxlDecoderSetInput(dec_.get(), jxl_data, jxl_size)) {
          DVLOG(1) << "JxlDecoderSetInput failed";
          SetFailed();
          return;
        }
        break;
      }
      case JXL_DEC_BASIC_INFO: {
        if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(dec_.get(), &info_)) {
          DVLOG(1) << "JxlDecoderGetBasicInfo failed";
          SetFailed();
          return;
        }
        if (!size_available && !SetSize(info_.xsize, info_.ysize)) {
          return;
        }
        break;
      }
      case JXL_DEC_COLOR_ENCODING: {
        if (IgnoresColorSpace()) {
          have_color_info_ = true;
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
        if (info_.bits_per_sample > 8) {
          is_hdr_ = true;
        }
        JxlColorEncoding color_encoding;
        if (JXL_DEC_SUCCESS == JxlDecoderGetColorAsEncodedProfile(
                                   dec_.get(), &format,
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
          decode_to_half_float_ = true;
        }

        bool have_data_profile = false;
        if (JXL_DEC_SUCCESS ==
            JxlDecoderGetColorAsEncodedProfile(dec_.get(), &format,
                                               JXL_COLOR_PROFILE_TARGET_DATA,
                                               &color_encoding)) {
          bool known_transfer_function = true;
          bool known_gamut = true;
          gfx::ColorSpace::PrimaryID gamut;
          gfx::ColorSpace::TransferID transfer;
          if (color_encoding.transfer_function == JXL_TRANSFER_FUNCTION_PQ) {
            transfer = gfx::ColorSpace::TransferID::PQ;
          } else if (color_encoding.transfer_function ==
                     JXL_TRANSFER_FUNCTION_HLG) {
            transfer = gfx::ColorSpace::TransferID::HLG;
          } else if (color_encoding.transfer_function ==
                     JXL_TRANSFER_FUNCTION_LINEAR) {
            transfer = gfx::ColorSpace::TransferID::LINEAR;
          } else if (color_encoding.transfer_function ==
                     JXL_TRANSFER_FUNCTION_SRGB) {
            transfer = gfx::ColorSpace::TransferID::SRGB;
          } else {
            known_transfer_function = false;
          }

          if (color_encoding.white_point == JXL_WHITE_POINT_D65 &&
              color_encoding.primaries == JXL_PRIMARIES_2100) {
            gamut = gfx::ColorSpace::PrimaryID::BT2020;
          } else if (color_encoding.white_point == JXL_WHITE_POINT_D65 &&
                     color_encoding.primaries == JXL_PRIMARIES_SRGB) {
            gamut = gfx::ColorSpace::PrimaryID::BT709;
          } else if (color_encoding.white_point == JXL_WHITE_POINT_D65 &&
                     color_encoding.primaries == JXL_PRIMARIES_P3) {
            gamut = gfx::ColorSpace::PrimaryID::P3;
          } else {
            known_gamut = false;
          }

          have_data_profile = known_transfer_function && known_gamut;

          if (have_data_profile) {
            skcms_ICCProfile dataProfile;
            gfx::ColorSpace(gamut, transfer)
                .ToSkColorSpace()
                ->toProfile(&dataProfile);
            profile = std::make_unique<ColorProfile>(dataProfile);
          }
        }

        // Did not handle exact enum values, get as ICC profile instead.
        if (!have_data_profile) {
          size_t icc_size;
          bool got_size =
              JXL_DEC_SUCCESS == JxlDecoderGetICCProfileSize(
                                     dec_.get(), &format,
                                     JXL_COLOR_PROFILE_TARGET_DATA, &icc_size);
          std::vector<uint8_t> icc_profile(icc_size);
          if (got_size &&
              JXL_DEC_SUCCESS == JxlDecoderGetColorAsICCProfile(
                                     dec_.get(), &format,
                                     JXL_COLOR_PROFILE_TARGET_DATA,
                                     icc_profile.data(), icc_profile.size())) {
            profile =
                ColorProfile::Create(icc_profile.data(), icc_profile.size());
            have_data_profile = true;

            // Detect whether the ICC profile approximately equals PQ or HLG,
            // and set the profile to one that indicates this transfer function
            // more clearly than a raw ICC profile does, so Chrome considers
            // the profile as HDR.
            skcms_TransferFunction tf_pq;
            skcms_TransferFunction tf_hlg01;
            skcms_TransferFunction tf_hlg12;
            skcms_TransferFunction_makePQ(&tf_pq);
            MakeTransferFunctionHLG01(&tf_hlg01);
            skcms_TransferFunction_makeHLG(&tf_hlg12);

            if (ApproximatelyMatchesTF(*profile, tf_pq)) {
              is_hdr_ = true;
              auto hdr10 = gfx::ColorSpace::CreateHDR10().ToSkColorSpace();
              skcms_TransferFunction pq;
              hdr10->transferFn(&pq);
              profile = NewColorProfileWithSameBuffer(
                  *profile,
                  ReplaceTransferFunction(*profile->GetProfile(), pq));
            } else {
              for (skcms_TransferFunction tf : {tf_hlg01, tf_hlg12}) {
                if (ApproximatelyMatchesTF(*profile, tf)) {
                  is_hdr_ = true;
                  auto hlg_colorspace =
                      gfx::ColorSpace::CreateHLG().ToSkColorSpace();
                  skcms_TransferFunction hlg;
                  hlg_colorspace->transferFn(&hlg);
                  profile = NewColorProfileWithSameBuffer(
                      *profile,
                      ReplaceTransferFunction(*profile->GetProfile(), hlg));
                  break;
                }
              }
            }
          }
        }

        if (is_hdr_ &&
            high_bit_depth_decoding_option_ == kHighBitDepthToHalfFloat) {
          decode_to_half_float_ = true;
        }

        if (have_data_profile) {
          if (profile->GetProfile()->data_color_space == skcms_Signature_RGB) {
            SetEmbeddedColorProfile(std::move(profile));
          }
        }
        have_color_info_ = true;
        break;
      }
      case JXL_DEC_NEED_IMAGE_OUT_BUFFER: {
        const wtf_size_t frame_index = num_decoded_frames_++;
        ImageFrame& frame = frame_buffer_cache_[frame_index];
        // This is guaranteed to occur after JXL_DEC_BASIC_INFO so the size
        // is correct.
        if (!InitFrameBuffer(frame_index)) {
          DVLOG(1) << "InitFrameBuffer failed";
          SetFailed();
          return;
        }
        frame.SetHasAlpha(info_.alpha_bits != 0);

        size_t buffer_size;
        if (JXL_DEC_SUCCESS !=
            JxlDecoderImageOutBufferSize(dec_.get(), &format, &buffer_size)) {
          DVLOG(1) << "JxlDecoderImageOutBufferSize failed";
          SetFailed();
          return;
        }
        if (buffer_size != info_.xsize * info_.ysize * 16) {
          DVLOG(1) << "Unexpected buffer size";
          SetFailed();
          return;
        }

        // TODO(http://crbug.com/1210465): Add Munsell chart color accuracy
        // tests for JXL
        xform_ = ColorTransform();
        auto callback = [](void* opaque, size_t x, size_t y, size_t num_pixels,
                           const void* pixels) {
          JXLImageDecoder* self = reinterpret_cast<JXLImageDecoder*>(opaque);
          ImageFrame& frame =
              self->frame_buffer_cache_[self->num_decoded_frames_ - 1];
          void* row_dst = self->decode_to_half_float_
                              ? reinterpret_cast<void*>(frame.GetAddrF16(
                                    static_cast<int>(x), static_cast<int>(y)))
                              : reinterpret_cast<void*>(frame.GetAddr(
                                    static_cast<int>(x), static_cast<int>(y)));

          bool dst_premultiply = frame.PremultiplyAlpha();

          const skcms_PixelFormat kSrcFormat = skcms_PixelFormat_RGBA_ffff;
          const skcms_PixelFormat kDstFormat = self->decode_to_half_float_
                                                   ? skcms_PixelFormat_RGBA_hhhh
                                                   : XformColorFormat();

          if (self->xform_ || (kDstFormat != kSrcFormat) ||
              (dst_premultiply && frame.HasAlpha())) {
            skcms_AlphaFormat src_alpha = skcms_AlphaFormat_Unpremul;
            skcms_AlphaFormat dst_alpha =
                (dst_premultiply && self->info_.alpha_bits)
                    ? skcms_AlphaFormat_PremulAsEncoded
                    : skcms_AlphaFormat_Unpremul;
            const auto* src_profile =
                self->xform_ ? self->xform_->SrcProfile() : nullptr;
            const auto* dst_profile =
                self->xform_ ? self->xform_->DstProfile() : nullptr;
            bool color_conversion_successful = skcms_Transform(
                pixels, kSrcFormat, src_alpha, src_profile, row_dst, kDstFormat,
                dst_alpha, dst_profile, num_pixels);
            DCHECK(color_conversion_successful);
          }
        };
        if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutCallback(
                                   dec_.get(), &format, callback, this)) {
          DVLOG(1) << "JxlDecoderSetImageOutCallback failed";
          SetFailed();
          return;
        }
        break;
      }
      case JXL_DEC_FRAME_PROGRESSION: {
        if (IsAllDataReceived()) {
          break;
        } else {
          if (JXL_DEC_SUCCESS != JxlDecoderFlushImage(dec_.get())) {
            DVLOG(1) << "JxlDecoderSetImageOutCallback failed";
            SetFailed();
            return;
          }
          ImageFrame& frame = frame_buffer_cache_[num_decoded_frames_ - 1];
          frame.SetPixelsChanged(true);
          frame.SetStatus(ImageFrame::kFramePartial);
          break;
        }
      }
      case JXL_DEC_FULL_IMAGE: {
        ImageFrame& frame = frame_buffer_cache_[num_decoded_frames_ - 1];
        frame.SetPixelsChanged(true);
        frame.SetStatus(ImageFrame::kFrameComplete);
        // All required frames were decoded.
        if (num_decoded_frames_ > index) {
          return;
        }
        break;
      }
      case JXL_DEC_SUCCESS: {
        // Finished decoding entire image, with all frames in case of animation.
        // Don't reset dec_, since we may want to rewind it if an earlier
        // animation frame has to be decoded again.
        segment_.clear();
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

void JXLImageDecoder::InitializeNewFrame(wtf_size_t index) {
  auto& buffer = frame_buffer_cache_[index];
  if (decode_to_half_float_)
    buffer.SetPixelFormat(ImageFrame::PixelFormat::kRGBA_F16);
  buffer.SetHasAlpha(info_.alpha_bits != 0);
  buffer.SetPremultiplyAlpha(premultiply_alpha_);
}

bool JXLImageDecoder::FrameIsReceivedAtIndex(wtf_size_t index) const {
  return IsAllDataReceived() ||
         (index < num_decoded_frames_ &&
          frame_buffer_cache_[index].GetStatus() == ImageFrame::kFrameComplete);
}

int JXLImageDecoder::RepetitionCount() const {
  if (!info_.have_animation)
    return kAnimationNone;

  if (info_.animation.num_loops == 0)
    return kAnimationLoopInfinite;

  if (info_.animation.num_loops == 1)
    return kAnimationLoopOnce;

  return info_.animation.num_loops;
}

base::TimeDelta JXLImageDecoder::FrameDurationAtIndex(wtf_size_t index) const {
  if (index < frame_durations_.size())
    return base::Seconds(frame_durations_[index]);

  return base::TimeDelta();
}

wtf_size_t JXLImageDecoder::DecodeFrameCount() {
  DecodeSize();
  if (!info_.have_animation) {
    frame_durations_.resize(1);
    frame_durations_[0] = 0;
    return 1;
  }

  FastSharedBufferReader reader(data_.get());
  if (has_full_frame_count_ || size_at_last_frame_count_ == reader.size()) {
    return frame_buffer_cache_.size();
  }
  size_at_last_frame_count_ = reader.size();

  // Decode the metadata of every frame that is available.
  if (frame_count_dec_ == nullptr) {
    frame_durations_.clear();
    frame_count_dec_ = JxlDecoderMake(nullptr);
    frame_count_offset_ = 0;
    if (JXL_DEC_SUCCESS !=
        JxlDecoderSubscribeEvents(frame_count_dec_.get(), JXL_DEC_FRAME)) {
      SetFailed();
      return frame_buffer_cache_.size();
    }
  }

  for (;;) {
    JxlDecoderStatus status = JxlDecoderProcessInput(frame_count_dec_.get());
    switch (status) {
      case JXL_DEC_ERROR: {
        DVLOG(1) << "Decoder error " << status;
        SetFailed();
        return frame_buffer_cache_.size();
      }
      case JXL_DEC_NEED_MORE_INPUT: {
        // The decoder returns how many bytes it has not yet processed, and
        // must be included in the next JxlDecoderSetInput call.
        const size_t remaining = JxlDecoderReleaseInput(frame_count_dec_.get());
        const uint8_t* jxl_data = nullptr;
        size_t jxl_size = 0;
        if (!ReadBytes(remaining, &frame_count_offset_, &frame_count_segment_,
                       &reader, &jxl_data, &jxl_size)) {
          if (Failed()) {
            return frame_buffer_cache_.size();
          }
          return frame_durations_.size();
        }

        if (JXL_DEC_SUCCESS !=
            JxlDecoderSetInput(frame_count_dec_.get(), jxl_data, jxl_size)) {
          DVLOG(1) << "JxlDecoderSetInput failed";
          SetFailed();
          return frame_buffer_cache_.size();
        }
        break;
      }
      case JXL_DEC_FRAME: {
        JxlFrameHeader frame_header;
        if (JxlDecoderGetFrameHeader(frame_count_dec_.get(), &frame_header) !=
            JXL_DEC_SUCCESS) {
          DVLOG(1) << "GetFrameHeader failed";
          SetFailed();
          return frame_buffer_cache_.size();
        }
        if (frame_header.is_last) {
          has_full_frame_count_ = true;
        }
        frame_durations_.push_back(1.0f * frame_header.duration *
                                   info_.animation.tps_denominator /
                                   info_.animation.tps_numerator);
        break;
      }
      case JXL_DEC_SUCCESS: {
        // If the file is fully processed, we won't need to run the decoder
        // anymore: we can free the memory.
        frame_count_dec_ = nullptr;
        DCHECK(has_full_frame_count_);
        frame_count_segment_.clear();
        return frame_durations_.size();
      }
      default: {
        DVLOG(1) << "Unexpected decoder status " << status;
        SetFailed();
        return frame_buffer_cache_.size();
      }
    }
  }
}

}  // namespace blink
