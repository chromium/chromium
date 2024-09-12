// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/parsers/temporal_scalability_id_extractor.h"

#include <bitset>

#include "base/memory/ptr_util.h"
#include "base/notimplemented.h"

namespace media {
namespace {

// Returns true iff the current stream has multiple spatial layers.
bool IsAV1SpatialLayerStream(int operating_point_idc) {
  // Spec 6.4.1.
  constexpr int kTemporalLayerBitMaskBits = 8;
  const int kUsedSpatialLayerBitMask =
      (operating_point_idc >> kTemporalLayerBitMaskBits) & 0b1111;
  // In case of an only temporal layer encoding e.g. L1T3, spatial layer#0 bit
  // is 1. We allow this case.
  return kUsedSpatialLayerBitMask > 1;
}

}  // namespace

TemporalScalabilityIdExtractor::TemporalScalabilityIdExtractor(VideoCodec codec,
                                                               int layer_count)
    : codec_(codec), num_temporal_layers_(layer_count) {
  switch (codec_) {
    case VideoCodec::kH264:
      h264_ = std::make_unique<H264Parser>();
      break;
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    case VideoCodec::kHEVC:
      h265_ = std::make_unique<H265NaluParser>();
      break;
#endif
    case VideoCodec::kVP9:
      vp9_ = std::make_unique<Vp9Parser>(false);
      break;
    case VideoCodec::kAV1:
      buffer_pool_ = std::make_unique<libgav1::BufferPool>(
          /*on_frame_buffer_size_changed=*/nullptr,
          /*get_frame_buffer=*/nullptr,
          /*release_frame_buffer=*/nullptr,
          /*callback_private_data=*/nullptr);
      av1_decoder_state_ = std::make_unique<libgav1::DecoderState>();
      break;
    default:
      break;
  }
}

bool TemporalScalabilityIdExtractor::ParseChunk(base::span<const uint8_t> chunk,
                                                uint32_t frame_id,
                                                BitstreamMetadata& md) {
  int tid_by_svc_spec = AssignTemporalIdBySvcSpec(frame_id);
  md.temporal_id = tid_by_svc_spec;
  switch (codec_) {
    case VideoCodec::kH264:
      return ParseH264(chunk, md);
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    case VideoCodec::kHEVC:
      return ParseHEVC(chunk, md);
#endif
    case VideoCodec::kVP9:
      return ParseVP9(chunk, frame_id, tid_by_svc_spec, md);
    case VideoCodec::kAV1:
      return ParseAV1(chunk, frame_id, tid_by_svc_spec, md);
    default:
      return false;
  }
}

bool TemporalScalabilityIdExtractor::ParseH264(base::span<const uint8_t> chunk,
                                               BitstreamMetadata& md) {
  h264_->SetStream(chunk.data(), chunk.size());
  H264NALU nalu;
  H264Parser::Result result;
  while ((result = h264_->AdvanceToNextNALU(&nalu)) != H264Parser::kEOStream) {
    if (result == H264Parser::Result::kInvalidStream) {
      return false;
    }
    // See the 7.3.1 NAL unit syntax in H264 spec.
    // https://www.itu.int/rec/T-REC-H.264
    // H264 can parse the temporal id from nal_unit_header_svc_extension
    // located in Nalu(7.3.1 NAL unit syntax).
    constexpr size_t kPrefixNALLocatedBytePos = 3;
    constexpr size_t kH264SVCExtensionFlagLocatedBytePos = 1;
    if (nalu.nal_unit_type == H264NALU::kPrefix &&
        static_cast<size_t>(nalu.size) > kPrefixNALLocatedBytePos) {
      bool svc_extension_flag =
          (nalu.data[kH264SVCExtensionFlagLocatedBytePos] & 0b1000'0000) >> 7;
      // nal_unit_header_svc_extension exists iff svc_extension_flag is true.
      if (svc_extension_flag) {
        md.temporal_id =
            (nalu.data[kPrefixNALLocatedBytePos] & 0b1110'0000) >> 5;
        return true;
      }
    }
  }
  return true;
}

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
bool TemporalScalabilityIdExtractor::ParseHEVC(base::span<const uint8_t> chunk,
                                               BitstreamMetadata& md) {
  h265_->SetStream(chunk.data(), chunk.size());
  H265NALU nalu;
  H265NaluParser::Result result;
  while ((result = h265_->AdvanceToNextNALU(&nalu)) !=
         H265NaluParser::kEOStream) {
    if (result == H265NaluParser::Result::kInvalidStream) {
      return false;
    }
    // See section 7.3.1.1, NAL unit syntax in H265 spec.
    // https://www.itu.int/rec/T-REC-H.265
    // Unlike AVC, HEVC stores the temporal ID information in VCL NAL unit
    // header instead of using prefix NAL unit. According to HEVC spec,
    // TemporalId = nuh_temporal_id_plus1 − 1.
    if (nalu.nal_unit_type <= H265NALU::RSV_VCL31) {
      md.temporal_id = nalu.nuh_temporal_id_plus1 - 1;
      return true;
    }
  }
  return true;
}
#endif

bool TemporalScalabilityIdExtractor::ParseVP9(base::span<const uint8_t> chunk,
                                              uint32_t frame_id,
                                              int tid_by_svc_spec,
                                              BitstreamMetadata& md) {
  Vp9FrameHeader header;
  gfx::Size coded_size;
  vp9_->SetStream(chunk.data(), chunk.size(), nullptr);

  if (vp9_->ParseNextFrame(&header, &coded_size, nullptr) != Vp9Parser::kOk) {
    return false;
  }
  // VP9 bitstream spec doesn't provide the temporal information, we can
  // only assign it based on spec.
  md.temporal_id = tid_by_svc_spec;
  // Calculate the diffs of frame id between current frame and the
  // referenced frames.
  if (!header.IsKeyframe()) {
    std::bitset<kVp9NumRefFrames> reference_frame_flags;
    for (size_t i = 0; i < kVp9NumRefsPerFrame; i++) {
      uint8_t idx = header.ref_frame_idx[i];
      if (idx >= reference_frame_flags.size()) {
        return false;
      }
      if (!reference_frame_flags[idx]) {
        // References upper temporal layer is not allowed.
        if (vp9_ref_buffer_[idx].temporal_id > md.temporal_id) {
          return false;
        }
        md.ref_frame_list.push_back(vp9_ref_buffer_[idx]);
      }
      reference_frame_flags.set(idx, true);
    }
  }
  for (size_t idx = 0; idx < vp9_ref_buffer_.size(); idx++) {
    if (header.RefreshFlag(idx)) {
      ReferenceBufferSlot& slot = vp9_ref_buffer_[idx];
      slot.frame_id = frame_id;
      slot.temporal_id = md.temporal_id;
    }
  }
  return true;
}

bool TemporalScalabilityIdExtractor::ParseAV1(base::span<const uint8_t> chunk,
                                              uint32_t frame_id,
                                              int tid_by_svc_spec,
                                              BitstreamMetadata& md) {
  auto parser = base::WrapUnique(new (std::nothrow) libgav1::ObuParser(
      chunk.data(), chunk.size(), 0, buffer_pool_.get(),
      av1_decoder_state_.get()));
  if (av1_sequence_header_) {
    parser->set_sequence_header(*av1_sequence_header_);
  }
  while (parser->HasData()) {
    libgav1::RefCountedBufferPtr current_frame;
    libgav1::StatusCode status = parser->ParseOneFrame(&current_frame);
    if (status != libgav1::kStatusOk) {
      return false;
    }
    if (!current_frame) {
      // No frame found.
      break;
    }

    if (parser->sequence_header_changed()) {
      auto sequence_header = parser->sequence_header();
      if (IsAV1SpatialLayerStream(sequence_header.operating_point_idc[0])) {
        // AV1 spatial layer stream is not supported.
        return false;
      }
      av1_sequence_header_ = sequence_header;
    }

    auto frame_header = parser->frame_header();
    md.refresh_frame_flags = frame_header.refresh_frame_flags;
    md.reference_idx_flags = 0;
    if (!libgav1::IsIntraFrame(frame_header.frame_type)) {
      for (size_t i = 0; i < libgav1::kNumInterReferenceFrameTypes; i++) {
        md.reference_idx_flags |= 1 << frame_header.reference_frame_index[i];
      }
    }

    av1_decoder_state_->UpdateReferenceFrames(
        current_frame,
        base::strict_cast<int>(frame_header.refresh_frame_flags));
  }

  if (!parser->obu_headers().empty()) {
    md.temporal_id = parser->obu_headers().back().temporal_id;
  } else {
    md.temporal_id = tid_by_svc_spec;
  }

  return true;
}

int TemporalScalabilityIdExtractor::AssignTemporalIdBySvcSpec(
    uint32_t frame_id) {
  switch (num_temporal_layers_) {
    case 1:
      return 0;
    case 2: {
      constexpr static std::array<int, 2> kTwoTemporalLayers = {0, 1};
      return kTwoTemporalLayers[frame_id % kTwoTemporalLayers.size()];
    }
    case 3: {
      constexpr static std::array<int, 4> kThreeTemporalLayers = {0, 2, 1, 2};
      return kThreeTemporalLayers[frame_id % kThreeTemporalLayers.size()];
    }
    default:
      NOTIMPLEMENTED() << "Unsupported number of layers: "
                       << num_temporal_layers_;
      return 0;
  }
}

TemporalScalabilityIdExtractor::~TemporalScalabilityIdExtractor() = default;
TemporalScalabilityIdExtractor::BitstreamMetadata::BitstreamMetadata() =
    default;
TemporalScalabilityIdExtractor::BitstreamMetadata::~BitstreamMetadata() =
    default;

}  // namespace media
