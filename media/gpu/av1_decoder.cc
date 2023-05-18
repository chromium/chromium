// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/av1_decoder.h"

#include <bitset>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "media/base/limits.h"
#include "media/gpu/av1_picture.h"
#include "third_party/libgav1/src/src/decoder_state.h"
#include "third_party/libgav1/src/src/gav1/status_code.h"
#include "third_party/libgav1/src/src/utils/constants.h"
#include "ui/gfx/hdr_metadata.h"

namespace media {
namespace {
// (Section 6.4.1):
//
// - "An operating point specifies which spatial and temporal layers should be
//   decoded."
//
// - "The order of operating points indicates the preferred order for producing
//   an output: a decoder should select the earliest operating point in the list
//   that meets its decoding capabilities as expressed by the level associated
//   with each operating point."
//
// For simplicity, we always select operating point 0 and will validate that it
// doesn't have scalability information.
constexpr unsigned int kDefaultOperatingPoint = 0;

// Conversion function from libgav1 profiles to media::VideoCodecProfile.
VideoCodecProfile AV1ProfileToVideoCodecProfile(
    libgav1::BitstreamProfile profile) {
  switch (profile) {
    case libgav1::kProfile0:
      return AV1PROFILE_PROFILE_MAIN;
    case libgav1::kProfile1:
      return AV1PROFILE_PROFILE_HIGH;
    case libgav1::kProfile2:
      return AV1PROFILE_PROFILE_PRO;
    default:
      // ObuParser::ParseSequenceHeader() validates the profile.
      NOTREACHED() << "Invalid profile: " << base::strict_cast<int>(profile);
      return AV1PROFILE_PROFILE_MAIN;
  }
}

// Returns true iff the sequence has spatial or temporal scalability information
// for the selected operating point.
bool SequenceUsesScalability(int operating_point_idc) {
  return operating_point_idc != 0;
}

bool IsValidBitDepth(uint8_t bit_depth, VideoCodecProfile profile) {
  // Spec 6.4.1.
  switch (profile) {
    case AV1PROFILE_PROFILE_MAIN:
    case AV1PROFILE_PROFILE_HIGH:
      return bit_depth == 8u || bit_depth == 10u;
    case AV1PROFILE_PROFILE_PRO:
      return bit_depth == 8u || bit_depth == 10u || bit_depth == 12u;
    default:
      NOTREACHED_NORETURN();
  }
}

VideoChromaSampling GetAV1ChromaSampling(
    const libgav1::ColorConfig& color_config) {
  // Spec section 6.4.2
  int8_t subsampling_x = color_config.subsampling_x;
  int8_t subsampling_y = color_config.subsampling_y;
  bool monochrome = color_config.is_monochrome;
  if (monochrome) {
    return VideoChromaSampling::k400;
  } else {
    if (subsampling_x == 0 && subsampling_y == 0) {
      return VideoChromaSampling::k444;
    } else if (subsampling_x == 1u && subsampling_y == 0) {
      return VideoChromaSampling::k422;
    } else if (subsampling_x == 1u && subsampling_y == 1u) {
      return VideoChromaSampling::k420;
    } else {
      DLOG(WARNING) << "Unknown chroma sampling format.";
      return VideoChromaSampling::kUnknown;
    }
  }
}

void PopulateColorVolumeMetadata(const libgav1::ObuMetadataHdrMdcv& mdcv,
                                 gfx::HdrMetadataSmpteSt2086& smpte_st_2086) {
  constexpr auto kChromaDenominator = 65536.0f;
  constexpr auto kLumaMaxDenoninator = 256.0f;
  constexpr auto kLumaMinDenoninator = 16384.0f;
  // display primaries are in R/G/B order in metadata_hdr_mdcv OBU Metadata.
  smpte_st_2086.primaries = {
      mdcv.primary_chromaticity_x[0] / kChromaDenominator,
      mdcv.primary_chromaticity_y[0] / kChromaDenominator,
      mdcv.primary_chromaticity_x[1] / kChromaDenominator,
      mdcv.primary_chromaticity_y[1] / kChromaDenominator,
      mdcv.primary_chromaticity_x[2] / kChromaDenominator,
      mdcv.primary_chromaticity_y[2] / kChromaDenominator,
      mdcv.white_point_chromaticity_x / kChromaDenominator,
      mdcv.white_point_chromaticity_y / kChromaDenominator};
  smpte_st_2086.luminance_max = mdcv.luminance_max / kLumaMaxDenoninator;
  smpte_st_2086.luminance_min = mdcv.luminance_min / kLumaMinDenoninator;
}

void PopulateHDRMetadata(const libgav1::ObuMetadataHdrCll& cll,
                         gfx::HDRMetadata& hdr_metadata) {
  hdr_metadata.cta_861_3 = gfx::HdrMetadataCta861_3(cll.max_cll, cll.max_fall);
}
}  // namespace

AV1Decoder::AV1Decoder(std::unique_ptr<AV1Accelerator> accelerator,
                       VideoCodecProfile profile,
                       const VideoColorSpace& container_color_space)
    : buffer_pool_(std::make_unique<libgav1::BufferPool>(
          /*on_frame_buffer_size_changed=*/nullptr,
          /*get_frame_buffer=*/nullptr,
          /*release_frame_buffer=*/nullptr,
          /*callback_private_data=*/nullptr)),
      state_(std::make_unique<libgav1::DecoderState>()),
      accelerator_(std::move(accelerator)),
      profile_(profile),
      container_color_space_(container_color_space) {
  ref_frames_.fill(nullptr);
}

AV1Decoder::~AV1Decoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // |buffer_pool_| checks that all the allocated frames are released in its
  // dtor. Explicitly destruct |state_| before |buffer_pool_| to release frames
  // in |reference_frame| in |state_|.
  state_.reset();
}

bool AV1Decoder::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "Decoder flush";
  Reset();
  return true;
}

void AV1Decoder::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ClearCurrentFrame();

  // We must reset the |current_sequence_header_| to ensure we don't try to
  // decode frames using an incorrect sequence header. If the first
  // DecoderBuffer after the reset doesn't contain a sequence header, we'll just
  // skip it and will keep skipping until we get a sequence header.
  current_sequence_header_.reset();
  stream_id_ = 0;
  stream_ = nullptr;
  stream_size_ = 0;
  on_error_ = false;

  state_ = std::make_unique<libgav1::DecoderState>();
  ClearReferenceFrames();
  parser_.reset();
  decrypt_config_.reset();

  buffer_pool_ = std::make_unique<libgav1::BufferPool>(
      /*on_frame_buffer_size_changed=*/nullptr,
      /*get_frame_buffer=*/nullptr,
      /*release_frame_buffer=*/nullptr,
      /*callback_private_data=*/nullptr);
}

void AV1Decoder::SetStream(int32_t id, const DecoderBuffer& decoder_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_id_ = id;
  stream_ = decoder_buffer.data();
  stream_size_ = decoder_buffer.data_size();
  ClearCurrentFrame();

  parser_ = base::WrapUnique(new (std::nothrow) libgav1::ObuParser(
      decoder_buffer.data(), decoder_buffer.data_size(), kDefaultOperatingPoint,
      buffer_pool_.get(), state_.get()));
  if (!parser_) {
    on_error_ = true;
    return;
  }

  if (current_sequence_header_)
    parser_->set_sequence_header(*current_sequence_header_);
  if (decoder_buffer.decrypt_config())
    decrypt_config_ = decoder_buffer.decrypt_config()->Clone();
  else
    decrypt_config_.reset();
}

void AV1Decoder::ClearCurrentFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_frame_.reset();
  current_frame_header_.reset();
  pending_pic_.reset();
}

AcceleratedVideoDecoder::DecodeResult AV1Decoder::Decode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (on_error_)
    return kDecodeError;
  auto result = DecodeInternal();
  on_error_ = result == kDecodeError;
  return result;
}

AcceleratedVideoDecoder::DecodeResult AV1Decoder::DecodeInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!parser_) {
    DLOG(WARNING) << "Decode() is called before SetStream()";
    return kRanOutOfStreamData;
  }
  while (parser_->HasData() || current_frame_header_) {
    base::ScopedClosureRunner clear_current_frame(
        base::BindOnce(&AV1Decoder::ClearCurrentFrame, base::Unretained(this)));
    if (pending_pic_) {
      const AV1Accelerator::Status status = DecodeAndOutputPicture(
          std::move(pending_pic_), parser_->tile_buffers());
      if (status == AV1Accelerator::Status::kFail)
        return kDecodeError;
      if (status == AV1Accelerator::Status::kTryAgain) {
        clear_current_frame.ReplaceClosure(base::DoNothing());
        return kTryAgain;
      }
      // Continue so that we force |clear_current_frame| to run before moving
      // on.
      continue;
    }
    if (!current_frame_header_) {
      libgav1::StatusCode status_code = parser_->ParseOneFrame(&current_frame_);
      if (status_code != libgav1::kStatusOk) {
        DLOG(WARNING) << "Failed to parse OBU: "
                      << libgav1::GetErrorString(status_code);
        return kDecodeError;
      }
      if (!current_frame_) {
        DLOG(WARNING) << "No frame found. Skipping the current stream";
        continue;
      }

      current_frame_header_ = parser_->frame_header();
      // Detects if a new coded video sequence is starting.
      if (parser_->sequence_header_changed()) {
        // TODO(b/171853869): Remove this check once libgav1::ObuParser does
        // this check.
        if (current_frame_header_->frame_type != libgav1::kFrameKey ||
            !current_frame_header_->show_frame ||
            current_frame_header_->show_existing_frame ||
            current_frame_->temporal_id() != 0) {
          // Section 7.5.
          DVLOG(1)
              << "The first frame successive to sequence header OBU must be a "
              << "keyframe with show_frame=1, show_existing_frame=0 and "
              << "temporal_id=0";
          return kDecodeError;
        }
        if (SequenceUsesScalability(
                parser_->sequence_header()
                    .operating_point_idc[kDefaultOperatingPoint])) {
          DVLOG(3) << "Either temporal or spatial layer decoding is not "
                   << "supported";
          return kDecodeError;
        }

        current_sequence_header_ = parser_->sequence_header();
        VideoChromaSampling new_chroma_sampling =
            GetAV1ChromaSampling(current_sequence_header_->color_config);
        if (new_chroma_sampling != chroma_sampling_) {
          chroma_sampling_ = new_chroma_sampling;
          base::UmaHistogramEnumeration(
              "Media.PlatformVideoDecoding.ChromaSampling", chroma_sampling_);
        }

        if (chroma_sampling_ != VideoChromaSampling::k420) {
          DVLOG(1) << "Only YUV 4:2:0 is supported";
          return kDecodeError;
        }

        const VideoCodecProfile new_profile =
            AV1ProfileToVideoCodecProfile(current_sequence_header_->profile);
        const uint8_t new_bit_depth = base::checked_cast<uint8_t>(
            current_sequence_header_->color_config.bitdepth);
        if (!IsValidBitDepth(new_bit_depth, new_profile)) {
          DVLOG(1) << "Invalid bit depth="
                   << base::strict_cast<int>(new_bit_depth)
                   << ", profile=" << GetProfileName(new_profile);
          return kDecodeError;
        }

        const gfx::Size new_frame_size(
            base::strict_cast<int>(current_sequence_header_->max_frame_width),
            base::strict_cast<int>(current_sequence_header_->max_frame_height));
        gfx::Rect new_visible_rect(
            base::strict_cast<int>(current_frame_header_->render_width),
            base::strict_cast<int>(current_frame_header_->render_height));
        DCHECK(!new_frame_size.IsEmpty());
        if (!gfx::Rect(new_frame_size).Contains(new_visible_rect)) {
          DVLOG(1) << "Render size exceeds picture size. render size: "
                   << new_visible_rect.ToString()
                   << ", picture size: " << new_frame_size.ToString();
          new_visible_rect = gfx::Rect(new_frame_size);
        }

        ClearReferenceFrames();
        // Issues kConfigChange only if either the dimensions, profile or bit
        // depth is changed.
        if (frame_size_ != new_frame_size ||
            visible_rect_ != new_visible_rect || profile_ != new_profile ||
            bit_depth_ != new_bit_depth) {
          frame_size_ = new_frame_size;
          visible_rect_ = new_visible_rect;
          profile_ = new_profile;
          bit_depth_ = new_bit_depth;
          clear_current_frame.ReplaceClosure(base::DoNothing());
          return kConfigChange;
        }
      }
    }

    if (!current_sequence_header_) {
      // Decoding is not doable because we haven't received a sequence header.
      // This occurs when seeking a video.
      DVLOG(3) << "Discarded the current frame because no sequence header has "
               << "been found yet";
      continue;
    }

    DCHECK(current_frame_header_);
    const auto& frame_header = *current_frame_header_;
    if (frame_header.show_existing_frame) {
      const size_t frame_to_show =
          base::checked_cast<size_t>(frame_header.frame_to_show);
      DCHECK_LE(0u, frame_to_show);
      DCHECK_LT(frame_to_show, ref_frames_.size());
      if (!CheckAndCleanUpReferenceFrames()) {
        DLOG(ERROR) << "The states of reference frames are different between "
                    << "|ref_frames_| and |state_|";
        return kDecodeError;
      }

      auto pic = ref_frames_[frame_to_show];
      CHECK(pic);
      pic = pic->Duplicate();
      if (!pic) {
        DVLOG(1) << "Failed duplication";
        return kDecodeError;
      }

      pic->set_bitstream_id(stream_id_);
      if (!accelerator_->OutputPicture(*pic)) {
        return kDecodeError;
      }

      // libgav1::ObuParser sets |current_frame_| to the frame to show while
      // |current_frame_header_| is the frame header of the currently parsed
      // frame. If |current_frame_| is a keyframe, then refresh_frame_flags must
      // be 0xff. Otherwise, refresh_frame_flags must be 0x00 (Section 5.9.2).
      DCHECK(current_frame_->frame_type() == libgav1::kFrameKey ||
             current_frame_header_->refresh_frame_flags == 0x00);
      DCHECK(current_frame_->frame_type() != libgav1::kFrameKey ||
             current_frame_header_->refresh_frame_flags == 0xff);
      UpdateReferenceFrames(std::move(pic));
      continue;
    }

    if (parser_->tile_buffers().empty()) {
      // The last call to ParseOneFrame() didn't actually have any tile groups.
      // This could happen in rare cases (for example, if there is a Metadata
      // OBU after the TileGroup OBU). Ignore this case.
      continue;
    }

    const gfx::Size current_frame_size(
        base::strict_cast<int>(frame_header.width),
        base::strict_cast<int>(frame_header.height));
    if (current_frame_size != frame_size_) {
      // TODO(hiroh): This must be handled in decoding spatial layer.
      DVLOG(1) << "Resolution change in the middle of video sequence (i.e."
               << " between sequence headers) is not supported";
      return kDecodeError;
    }
    if (current_frame_size.width() !=
        base::strict_cast<int>(frame_header.upscaled_width)) {
      DVLOG(1) << "Super resolution is not supported";
      return kDecodeError;
    }
    const gfx::Rect current_visible_rect(
        base::strict_cast<int>(frame_header.render_width),
        base::strict_cast<int>(frame_header.render_height));
    if (current_visible_rect != visible_rect_) {
      // TODO(andrescj): Handle the visible rectangle change in the middle of
      // video sequence.
      DVLOG(1) << "Visible rectangle change in the middle of video sequence"
               << "(i.e. between sequence headers) is not supported";
      return kDecodeError;
    }

    // AV1 HDR metadata may appears in the below places:
    // 1. Container.
    // 2. Bitstream.
    // 3. Both container and bitstream.
    // Thus we should also extract HDR metadata here in case we
    // miss the information.
    if (current_frame_->hdr_mdcv_set() && current_frame_->hdr_cll_set()) {
      if (!hdr_metadata_)
        hdr_metadata_ = gfx::HDRMetadata();
      PopulateColorVolumeMetadata(current_frame_->hdr_mdcv(),
                                  hdr_metadata_->smpte_st_2086);
      PopulateHDRMetadata(current_frame_->hdr_cll(), hdr_metadata_.value());
    }

    DCHECK(current_sequence_header_->film_grain_params_present ||
           !frame_header.film_grain_params.apply_grain);
    auto pic = accelerator_->CreateAV1Picture(
        frame_header.film_grain_params.apply_grain);
    if (!pic) {
      clear_current_frame.ReplaceClosure(base::DoNothing());
      return kRanOutOfSurfaces;
    }

    pic->set_visible_rect(current_visible_rect);
    pic->set_bitstream_id(stream_id_);

    // For AV1, prefer the frame color space over the config.
    const auto& cc = current_sequence_header_->color_config;
    const auto cs = VideoColorSpace(
        cc.color_primary, cc.transfer_characteristics, cc.matrix_coefficients,
        cc.color_range == libgav1::kColorRangeStudio
            ? gfx::ColorSpace::RangeID::LIMITED
            : gfx::ColorSpace::RangeID::FULL);
    if (cs.IsSpecified())
      pic->set_colorspace(cs);
    else if (container_color_space_.IsSpecified())
      pic->set_colorspace(container_color_space_);

    if (hdr_metadata_)
      pic->set_hdr_metadata(hdr_metadata_);

    pic->frame_header = frame_header;
    if (decrypt_config_)
      pic->set_decrypt_config(decrypt_config_->Clone());
    const AV1Accelerator::Status status =
        DecodeAndOutputPicture(std::move(pic), parser_->tile_buffers());
    if (status == AV1Accelerator::Status::kFail)
      return kDecodeError;
    if (status == AV1Accelerator::Status::kTryAgain) {
      clear_current_frame.ReplaceClosure(base::DoNothing());
      return kTryAgain;
    }
  }
  return kRanOutOfStreamData;
}

void AV1Decoder::UpdateReferenceFrames(scoped_refptr<AV1Picture> pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_);
  DCHECK(current_frame_header_);
  const uint8_t refresh_frame_flags =
      current_frame_header_->refresh_frame_flags;
  const std::bitset<libgav1::kNumReferenceFrameTypes> update_reference_frame(
      refresh_frame_flags);
  for (size_t i = 0; i < libgav1::kNumReferenceFrameTypes; ++i) {
    if (update_reference_frame[i])
      ref_frames_[i] = pic;
  }
  state_->UpdateReferenceFrames(current_frame_,
                                base::strict_cast<int>(refresh_frame_flags));
}

void AV1Decoder::ClearReferenceFrames() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_);
  ref_frames_.fill(nullptr);
  // If AV1Decoder has decided to clear the reference frames, then ObuParser
  // must have also decided to do so.
  DCHECK_EQ(base::ranges::count(state_->reference_frame, nullptr),
            static_cast<int>(state_->reference_frame.size()));
}

bool AV1Decoder::CheckAndCleanUpReferenceFrames() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_);
  DCHECK(current_frame_header_);
  for (size_t i = 0; i < libgav1::kNumReferenceFrameTypes; ++i) {
    if (state_->reference_frame[i] && !ref_frames_[i])
      return false;
    if (!state_->reference_frame[i] && ref_frames_[i])
      ref_frames_[i].reset();
  }

  // If we get here, we know |ref_frames_| includes all and only those frames
  // that can be currently used as reference frames. Now we'll assert that for
  // non-intra frames, all the necessary reference frames are in |ref_frames_|.
  // For intra frames, we don't need this assertion because they shouldn't
  // depend on reference frames.
  if (!libgav1::IsIntraFrame(current_frame_header_->frame_type)) {
    for (size_t i = 0; i < libgav1::kNumInterReferenceFrameTypes; ++i) {
      const auto ref_frame_index =
          current_frame_header_->reference_frame_index[i];

      // Unless an error occurred in libgav1, |ref_frame_index| should be valid,
      // and since CheckAndCleanUpReferenceFrames() only gets called if parsing
      // succeeded, we can assert that validity.
      CHECK_GE(ref_frame_index, 0);
      CHECK_LT(ref_frame_index, libgav1::kNumReferenceFrameTypes);
      CHECK(ref_frames_[ref_frame_index]);
    }
  }

  // If we get here, we know that all the reference frames needed by the current
  // frame are in |ref_frames_|.
  return true;
}

AV1Decoder::AV1Accelerator::Status AV1Decoder::DecodeAndOutputPicture(
    scoped_refptr<AV1Picture> pic,
    const libgav1::Vector<libgav1::TileBuffer>& tile_buffers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pic);
  DCHECK(current_sequence_header_);
  DCHECK(stream_);
  DCHECK_GT(stream_size_, 0u);
  if (!CheckAndCleanUpReferenceFrames()) {
    DLOG(ERROR) << "The states of reference frames are different between "
                << "|ref_frames_| and |state_|";
    return AV1Accelerator::Status::kFail;
  }
  const AV1Accelerator::Status status = accelerator_->SubmitDecode(
      *pic, *current_sequence_header_, ref_frames_, tile_buffers,
      base::make_span(stream_, stream_size_));
  if (status != AV1Accelerator::Status::kOk) {
    if (status == AV1Accelerator::Status::kTryAgain)
      pending_pic_ = std::move(pic);
    return status;
  }

  if (pic->frame_header.show_frame && !accelerator_->OutputPicture(*pic))
    return AV1Accelerator::Status::kFail;

  // |current_frame_header_->refresh_frame_flags| should be 0xff if the frame is
  // either a SWITCH_FRAME or a visible KEY_FRAME (Spec 5.9.2).
  DCHECK(!(current_frame_header_->frame_type == libgav1::kFrameSwitch ||
           (current_frame_header_->frame_type == libgav1::kFrameKey &&
            current_frame_header_->show_frame)) ||
         current_frame_header_->refresh_frame_flags == 0xff);
  UpdateReferenceFrames(std::move(pic));
  return AV1Accelerator::Status::kOk;
}

absl::optional<gfx::HDRMetadata> AV1Decoder::GetHDRMetadata() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return hdr_metadata_;
}

gfx::Size AV1Decoder::GetPicSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(hiroh): It should be safer to align this by 64 or 128 (depending on
  // use_128x128_superblock) so that a driver doesn't touch out of the buffer.
  return frame_size_;
}

gfx::Rect AV1Decoder::GetVisibleRect() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return visible_rect_;
}

VideoCodecProfile AV1Decoder::GetProfile() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return profile_;
}

uint8_t AV1Decoder::GetBitDepth() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return bit_depth_;
}

VideoChromaSampling AV1Decoder::GetChromaSampling() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return chroma_sampling_;
}

size_t AV1Decoder::GetRequiredNumOfPictures() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  constexpr size_t kPicsInPipeline = limits::kMaxVideoFrames + 1;
  DCHECK(current_sequence_header_);
  return (kPicsInPipeline + GetNumReferenceFrames()) *
         (1 + current_sequence_header_->film_grain_params_present);
}

size_t AV1Decoder::GetNumReferenceFrames() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return libgav1::kNumReferenceFrameTypes;
}
}  // namespace media
