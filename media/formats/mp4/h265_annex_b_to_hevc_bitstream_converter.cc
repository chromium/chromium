// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/h265_annex_b_to_hevc_bitstream_converter.h"

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/containers/span_writer.h"

namespace media {

H265AnnexBToHevcBitstreamConverter::H265AnnexBToHevcBitstreamConverter() {
  // These configuration items never change.
  config_.configurationVersion = 1;
  config_.lengthSizeMinusOne = 3;
}

H265AnnexBToHevcBitstreamConverter::~H265AnnexBToHevcBitstreamConverter() =
    default;

const mp4::HEVCDecoderConfigurationRecord&
H265AnnexBToHevcBitstreamConverter::GetCurrentConfig() {
  return config_;
}

MP4Status H265AnnexBToHevcBitstreamConverter::ConvertChunk(
    const base::span<const uint8_t> input,
    base::span<uint8_t> output,
    bool* config_changed_out,
    size_t* size_out) {
  std::vector<H265NALU> slice_units;
  size_t data_size = 0;
  bool config_changed = false;
  H265NALU nalu;
  H265Parser::Result result;
  int new_active_sps_id = -1;
  int new_active_pps_id = -1;
  int new_active_vps_id = -1;
  // Sets of SPS, PPS & VPS ids to be included into the decoder config.
  // They contain
  // - all VPS, SPS and PPS units encountered in the input chunk;
  // - any SPS referenced by PPS units encountered in the input;
  // - The active VPS/SPS/PPS pair.
  base::flat_set<int> sps_to_include;
  base::flat_set<int> pps_to_include;
  base::flat_set<int> vps_to_include;

  // Scan input buffer looking for two main types of NALUs
  //  1. VPS, SPS and PPS. They'll be added to the HEVC configuration |config_|
  //     and will *not* be copied to |output|.
  //  2. Slices. They'll being copied into the output buffer, but also affect
  //     what configuration (profile and level) is active now.
  // A configure change will only happen on IDR frame. It is expected the
  // encoder output stream repeats VPS/SPS/PPS on IDR frames.
  parser_.SetStream(input.data(), input.size());
  while ((result = parser_.AdvanceToNextNALU(&nalu)) != H265Parser::kEOStream) {
    if (result == H265Parser::kUnsupportedStream)
      return MP4Status::Codes::kUnsupportedStream;

    if (result != H265Parser::kOk)
      return MP4Status::Codes::kFailedToParse;

    switch (nalu.nal_unit_type) {
      case H265NALU::AUD_NUT: {
        break;
      }
      case H265NALU::SPS_NUT: {
        int sps_id = -1;
        result = parser_.ParseSPS(&sps_id);
        if (result == H265Parser::kUnsupportedStream)
          return MP4Status::Codes::kInvalidSPS;

        if (result != H265Parser::kOk)
          return MP4Status::Codes::kInvalidSPS;

        id2sps_.insert_or_assign(
            sps_id,
            blob(nalu.data.get(),
                 (nalu.data + base::checked_cast<size_t>(nalu.size)).get()));
        sps_to_include.insert(sps_id);
        if (auto* sps = parser_.GetSPS(sps_id)) {
          vps_to_include.insert(sps->sps_video_parameter_set_id);
        }
        config_changed = true;
        break;
      }

      case H265NALU::VPS_NUT: {
        int vps_id = -1;
        result = parser_.ParseVPS(&vps_id);
        if (result == H265Parser::kUnsupportedStream)
          return MP4Status::Codes::kInvalidVPS;

        if (result != H265Parser::kOk)
          return MP4Status::Codes::kInvalidVPS;

        id2vps_.insert_or_assign(
            vps_id,
            blob(nalu.data.get(),
                 (nalu.data + base::checked_cast<size_t>(nalu.size)).get()));
        vps_to_include.insert(vps_id);
        config_changed = true;
        break;
      }

      case H265NALU::PPS_NUT: {
        int pps_id = -1;
        result = parser_.ParsePPS(nalu, &pps_id);
        if (result == H265Parser::kUnsupportedStream)
          return MP4Status::Codes::kInvalidPPS;

        if (result != H265Parser::kOk)
          return MP4Status::Codes::kInvalidPPS;

        id2pps_.insert_or_assign(
            pps_id,
            blob(nalu.data.get(),
                 (nalu.data + base::checked_cast<size_t>(nalu.size)).get()));
        pps_to_include.insert(pps_id);
        if (auto* pps = parser_.GetPPS(pps_id))
          sps_to_include.insert(pps->pps_seq_parameter_set_id);
        config_changed = true;
        break;
      }

      // TODO: when HDR encoding is supported, we need to also move the prefix
      // SEI out of slice data and put it into the hvccBox.

      // VCL, Non-IRAP
      case H265NALU::TRAIL_N:
      case H265NALU::TRAIL_R:
      case H265NALU::TSA_N:
      case H265NALU::TSA_R:
      case H265NALU::STSA_N:
      case H265NALU::STSA_R:
      case H265NALU::RADL_N:
      case H265NALU::RADL_R:
      case H265NALU::RASL_N:
      case H265NALU::RASL_R:
      // VCL, IRAP
      case H265NALU::BLA_W_LP:
      case H265NALU::BLA_W_RADL:
      case H265NALU::BLA_N_LP:
      case H265NALU::IDR_W_RADL:
      case H265NALU::IDR_N_LP:
      case H265NALU::CRA_NUT: {
        int pps_id = -1;
        result = parser_.ParseSliceHeaderForPictureParameterSets(nalu, &pps_id);
        if (result != H265Parser::kOk) {
          return MP4Status::Codes::kInvalidSliceHeader;
        }

        const H265PPS* pps = parser_.GetPPS(pps_id);
        if (!pps) {
          return MP4Status::Codes::kFailedToLookupPPS;
        }

        const H265SPS* sps = parser_.GetSPS(pps->pps_seq_parameter_set_id);
        if (!sps) {
          return MP4Status::Codes::kFailedToLookupSPS;
        }
        const H265VPS* vps = parser_.GetVPS(sps->sps_video_parameter_set_id);
        if (!vps) {
          return MP4Status::Codes::kFailedToLookupVPS;
        }
        new_active_pps_id = pps->pps_pic_parameter_set_id;
        new_active_sps_id = sps->sps_seq_parameter_set_id;
        new_active_vps_id = vps->vps_video_parameter_set_id;
        pps_to_include.insert(new_active_pps_id);
        sps_to_include.insert(new_active_sps_id);
        vps_to_include.insert(new_active_vps_id);

        if (new_active_sps_id != active_sps_id_ ||
            new_active_vps_id != active_vps_id_) {
          if (!config_changed) {
            DCHECK(nalu.nal_unit_type == H265NALU::IDR_W_RADL ||
                   nalu.nal_unit_type == H265NALU::IDR_N_LP)
                << "SPS/VPS shouldn't change in non-IDR slice";
          }
          config_changed = true;
        }
      }
        [[fallthrough]];
      default:
        slice_units.push_back(nalu);
        data_size += config_.lengthSizeMinusOne + 1 + nalu.size;
        break;
    }
  }

  if (size_out)
    *size_out = data_size;
  if (data_size > output.size()) {
    return MP4Status::Codes::kBufferTooSmall;
  }

  // Write slice NALUs from the input buffer to the output buffer
  // prefixing them with size.
  base::SpanWriter writer(output);
  for (auto& unit : slice_units) {
    bool written_ok =
        writer.WriteU32BigEndian(unit.size) &&
        writer.Write(
            // SAFETY: `unit` is constructed with a size that is the number of
            // elements at the data pointer.
            //
            // TODO(crbug.com/40284755): The `unit` should hold a span instead
            // of a pointer.
            UNSAFE_TODO(base::span(unit.data.get(),
                                   base::checked_cast<size_t>(unit.size))));
    if (!written_ok) {
      return MP4Status::Codes::kBufferTooSmall;
    }
  }

  DCHECK_EQ(writer.num_written(), data_size);

  // Now when we are sure that everything is written and fits nicely,
  // we can update parts of the |config_| that were changed by this data chunk.
  if (config_changed) {
    if (new_active_sps_id < 0)
      new_active_sps_id = active_sps_id_;
    if (new_active_pps_id < 0)
      new_active_pps_id = active_pps_id_;
    if (new_active_vps_id < 0)
      new_active_vps_id = active_vps_id_;

    const H265SPS* active_sps = parser_.GetSPS(new_active_sps_id);
    if (!active_sps) {
      return MP4Status::Codes::kFailedToLookupSPS;
    }

    const H265PPS* active_pps = parser_.GetPPS(new_active_pps_id);
    if (!active_pps) {
      return MP4Status::Codes::kFailedToLookupPPS;
    }

    active_pps_id_ = new_active_pps_id;
    active_sps_id_ = new_active_sps_id;
    active_vps_id_ = new_active_vps_id;

    config_.arrays.clear();

    // General profile space and tier level is not provided by the parser and it
    // must always be 0.
    config_.general_profile_space = 0;
    config_.general_tier_flag = 0;

    auto ptl = active_sps->profile_tier_level;
    config_.general_profile_idc = ptl.general_profile_idc;
    config_.general_profile_compatibility_flags =
        ptl.general_profile_compatibility_flags;
    config_.general_level_idc = ptl.general_level_idc;

    if (ptl.general_progressive_source_flag) {
      config_.general_constraint_indicator_flags |= 1ull << 47;
    }
    if (ptl.general_interlaced_source_flag) {
      config_.general_constraint_indicator_flags |= 1ull << 46;
    }
    if (ptl.general_non_packed_constraint_flag) {
      config_.general_constraint_indicator_flags |= 1ull << 45;
    }
    if (ptl.general_frame_only_constraint_flag) {
      config_.general_constraint_indicator_flags |= 1ull << 44;
    }
    if (ptl.general_one_picture_only_constraint_flag) {
      config_.general_constraint_indicator_flags |= 1ull << 35;
    }

    config_.min_spatial_segmentation_idc =
        active_sps->vui_parameters.min_spatial_segmentation_idc;
    if (active_sps->vui_parameters.min_spatial_segmentation_idc == 0) {
      config_.parallelismType =
          mp4::HEVCDecoderConfigurationRecord::kMixedParallel;
    } else if (active_pps->entropy_coding_sync_enabled_flag &&
               active_pps->tiles_enabled_flag) {
      config_.parallelismType =
          mp4::HEVCDecoderConfigurationRecord::kMixedParallel;
    } else if (active_pps->entropy_coding_sync_enabled_flag) {
      config_.parallelismType =
          mp4::HEVCDecoderConfigurationRecord::kWaveFrontParallel;
    } else if (active_pps->tiles_enabled_flag) {
      config_.parallelismType =
          mp4::HEVCDecoderConfigurationRecord::kTileParallel;
    } else {
      config_.parallelismType =
          mp4::HEVCDecoderConfigurationRecord::kSliceParallel;
    }
    config_.chromaFormat = active_sps->chroma_format_idc;
    config_.bitDepthLumaMinus8 = active_sps->bit_depth_luma_minus8;
    config_.bitDepthChromaMinus8 = active_sps->bit_depth_chroma_minus8;
    // Gives the average frame rate in units of frames per 256 seconds.
    // A value of 0 indicates an unspecified average frame rate.
    config_.avgFrameRate = 0;
    // Set to 0 to indicate it may or may not be of constant frame rate.
    config_.constantFrameRate = 0;
    config_.numTemporalLayers = active_sps->sps_max_sub_layers_minus1 + 1;
    config_.temporalIdNested = active_sps->sps_temporal_id_nesting_flag;

    // We write 3 arrays, in the order of VPS array, SPS array and PPS array.
    auto hvcc_array_idx = 0;
    mp4::HEVCDecoderConfigurationRecord::HVCCNALUnit nal_unit;
    mp4::HEVCDecoderConfigurationRecord::HVCCNALArray nalu_array;
    // bit 7: array_completeness. When set to 1, corresponding type of
    // NAL unit will be in the array only and none are in the stream; otherwise
    // they may additionally be in the stream.
    uint8_t first_byte = (1 << 7) | (H265NALU::VPS_NUT & 0x3F);
    if (id2vps_.size() > 0) {
      nalu_array.first_byte = first_byte;
      for (auto& vps : id2vps_) {
        nal_unit.assign(vps.second.begin(), vps.second.end());
        nalu_array.units.push_back(nal_unit);
      }
      config_.arrays.push_back(nalu_array);
      hvcc_array_idx++;
    }

    first_byte = (1 << 7) | (H265NALU::SPS_NUT & 0x3F);
    nalu_array.units.clear();
    if (id2sps_.size() > 0) {
      nalu_array.first_byte = first_byte;
      for (auto& sps : id2sps_) {
        nal_unit.assign(sps.second.begin(), sps.second.end());
        nalu_array.units.push_back(nal_unit);
      }
      config_.arrays.push_back(nalu_array);
      hvcc_array_idx++;
    }

    first_byte = (1 << 7) | (H265NALU::PPS_NUT & 0x3F);
    nalu_array.units.clear();
    if (id2sps_.size() > 0) {
      nalu_array.first_byte = first_byte;
      for (auto& pps : id2pps_) {
        nal_unit.assign(pps.second.begin(), pps.second.end());
        nalu_array.units.push_back(nal_unit);
      }
      config_.arrays.push_back(nalu_array);
      hvcc_array_idx++;
    }

    config_.numOfArrays = hvcc_array_idx;
  }

  if (config_changed_out) {
    *config_changed_out = config_changed;
  }

  return OkStatus();
}

}  // namespace media
