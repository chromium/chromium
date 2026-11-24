// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/h264_feature_tracker.h"

#include <algorithm>
#include <array>

#include "base/containers/span_reader.h"
#include "base/metrics/histogram_functions.h"
#include "media/formats/mp4/avc.h"
#include "media/formats/mp4/box_definitions.h"

namespace media {

namespace {

constexpr std::array<uint8_t, 4> kAnnexBStartCode = {0, 0, 0, 1};
constexpr size_t kMaxSliceHeaderBytes = 1024;
constexpr int kMaxColorDepth = 16;

bool ReadAvcNaluLength(base::SpanReader<const uint8_t>& reader,
                       size_t nal_length_size,
                       uint32_t& nalu_size) {
  if (reader.remaining() < nal_length_size) {
    return false;
  }
  nalu_size = 0;
  for (size_t i = 0; i < nal_length_size; ++i) {
    uint8_t byte = 0;
    reader.ReadU8BigEndian(byte);
    nalu_size = (nalu_size << 8) | byte;
  }
  return true;
}

bool StartsWithAnnexBStartCode(base::span<const uint8_t> data) {
  size_t zeros = 0;
  while (zeros < data.size() && data[zeros] == 0) {
    ++zeros;
  }
  return zeros >= 2 && zeros < data.size() && data[zeros] == 1;
}

bool IsValidAvcPacket(base::span<const uint8_t> data, size_t nal_length_size) {
  base::SpanReader reader(data);
  bool saw_nalu = false;
  while (reader.remaining() > 0) {
    uint32_t nalu_size = 0;
    if (!ReadAvcNaluLength(reader, nal_length_size, nalu_size)) {
      return false;
    }
    auto nalu = reader.Read(nalu_size);
    if (!nalu) {
      return false;
    }
    if (!nalu->empty() && ((*nalu)[0] & 0x80) != 0) {
      return false;
    }
    saw_nalu = true;
  }
  return saw_nalu;
}

}  // namespace

H264FeatureTracker::H264FeatureTracker() = default;

H264FeatureTracker::~H264FeatureTracker() = default;

void H264FeatureTracker::ParseExtraData(base::span<const uint8_t> extra_data) {
  if (extra_data.empty()) {
    return;
  }

  mp4::AVCDecoderConfigurationRecord avc_config;
  if (avc_config.Parse(extra_data)) {
    std::vector<uint8_t> annex_b;
    if (mp4::AVC::ConvertConfigToAnnexB(avc_config, &annex_b)) {
      ParseAnnexB(annex_b, /*parse_slices=*/false);
    } else {
      saw_parse_error_ = true;
    }
    nal_length_size_ = avc_config.length_size;
    return;
  }
  ParseAnnexB(extra_data, /*parse_slices=*/false);
}

void H264FeatureTracker::ParsePacket(base::span<const uint8_t> packet_data) {
  if (packet_data.empty()) {
    return;
  }

  // Some pipelines provide avcC extradata but pass Annex-B packets (or mix
  // both). Fall back to Annex-B per packet when the packet starts with a start
  // code and does not form valid length-prefixed AVC framing.
  bool use_avc = nal_length_size_ >= 1 && nal_length_size_ <= 4;
  if (use_avc && StartsWithAnnexBStartCode(packet_data) &&
      !IsValidAvcPacket(packet_data, nal_length_size_)) {
    use_avc = false;
  }

  if (use_avc) {
    ParseAvcNalus(packet_data);
  } else {
    ParseAnnexB(packet_data, /*parse_slices=*/true);
  }
}

void H264FeatureTracker::OnFrameDecoded() {
  has_decoded_frame_ = true;
}

void H264FeatureTracker::ReportMetrics() {
  if (!has_decoded_frame_) {
    return;
  }

  const bool is_conformant = !saw_parse_error_ && !profiles_.empty();
  base::UmaHistogramBoolean("Media.FFmpegVideoDecoder.H264.Conformant",
                            is_conformant);

  if (is_conformant) {
    for (VideoChromaSampling sampling : chroma_samplings_) {
      base::UmaHistogramEnumeration(
          "Media.FFmpegVideoDecoder.H264.ChromaSubsampling", sampling);
    }

    H264Interlacing interlacing = H264Interlacing::kProgressive;
    if (saw_paff_ && saw_mbaff_) {
      interlacing = H264Interlacing::kBoth;
    } else if (saw_paff_) {
      interlacing = H264Interlacing::kPaff;
    } else if (saw_mbaff_) {
      interlacing = H264Interlacing::kMbaff;
    }
    base::UmaHistogramEnumeration("Media.FFmpegVideoDecoder.H264.Interlacing",
                                  interlacing);

    for (VideoCodecProfile profile : profiles_) {
      base::UmaHistogramSparse("Media.FFmpegVideoDecoder.H264.Profile",
                               profile);
    }

    for (int depth : color_depths_) {
      base::UmaHistogramExactLinear("Media.FFmpegVideoDecoder.H264.ColorDepth",
                                    depth, kMaxColorDepth + 1);
    }
  }

  has_decoded_frame_ = false;
}

void H264FeatureTracker::ParseAvcNalus(base::span<const uint8_t> data) {
  base::SpanReader reader(data);
  uint32_t nalu_size = 0;
  while (ReadAvcNaluLength(reader, nal_length_size_, nalu_size)) {
    auto nalu = reader.Read(nalu_size);
    if (!nalu) {
      saw_parse_error_ = true;
      break;
    }
    if (nalu->empty()) {
      continue;
    }
    const uint8_t nal_type = (*nalu)[0] & 0x1F;
    const bool is_slice = nal_type == H264NALU::kNonIDRSlice ||
                          nal_type == H264NALU::kSliceDataA ||
                          nal_type == H264NALU::kIDRSlice;
    if (nal_type != H264NALU::kSPS && nal_type != H264NALU::kPPS && !is_slice) {
      continue;
    }
    auto nalu_slice =
        is_slice ? nalu->first(std::min(nalu->size(), kMaxSliceHeaderBytes))
                 : *nalu;
    nalu_scratch_.clear();
    nalu_scratch_.insert(nalu_scratch_.end(), kAnnexBStartCode.begin(),
                         kAnnexBStartCode.end());
    nalu_scratch_.insert(nalu_scratch_.end(), nalu_slice.begin(),
                         nalu_slice.end());
    ParseAnnexB(nalu_scratch_, /*parse_slices=*/true);
  }
  if (reader.remaining() > 0) {
    saw_parse_error_ = true;
  }
}

void H264FeatureTracker::ParseAnnexB(base::span<const uint8_t> data,
                                     bool parse_slices) {
  if (data.empty()) {
    return;
  }
  parser_.SetStream(data);
  H264NALU nalu;
  H264Parser::Result res;
  while ((res = parser_.AdvanceToNextNALU(&nalu)) == H264Parser::kOk) {
    switch (nalu.nal_unit_type) {
      case H264NALU::kSPS: {
        int sps_id = -1;
        if (parser_.ParseSPS(&sps_id) != H264Parser::kOk) {
          saw_parse_error_ = true;
        }
        break;
      }
      case H264NALU::kPPS: {
        int pps_id = -1;
        if (parser_.ParsePPS(&pps_id) != H264Parser::kOk) {
          saw_parse_error_ = true;
        }
        break;
      }
      case H264NALU::kNonIDRSlice:
      case H264NALU::kSliceDataA:
      case H264NALU::kIDRSlice: {
        if (!parse_slices) {
          break;
        }
        H264SliceHeader shdr;
        const H264Parser::Result slice_res =
            parser_.ParseSliceHeader(nalu, &shdr);
        const H264PPS* pps = parser_.GetPPS(shdr.pic_parameter_set_id);
        const H264SPS* sps =
            pps ? parser_.GetSPS(pps->seq_parameter_set_id) : nullptr;
        // H264Parser::ParseSliceHeader returns kUnsupportedStream for PAFF
        // (`field_pic_flag == true`) after populating `pic_parameter_set_id`
        // and `field_pic_flag`. Reject other unsupported features (such as
        // FMO slice groups or separate color planes) consistently with
        // ParsePPS().
        const bool is_paff_unsupported =
            slice_res == H264Parser::kUnsupportedStream && sps &&
            pps->num_slice_groups_minus1 == 0 && shdr.field_pic_flag;
        if (slice_res != H264Parser::kOk && !is_paff_unsupported) {
          saw_parse_error_ = true;
          break;
        }
        if (sps) {
          RecordSpsAndSlice(*sps, shdr);
        } else {
          saw_parse_error_ = true;
        }
        parser_.Reset();
        return;
      }
      default:
        break;
    }
  }
  if (res != H264Parser::kEOStream) {
    saw_parse_error_ = true;
  }
  parser_.Reset();
}

void H264FeatureTracker::RecordSpsAndSlice(const H264SPS& sps,
                                           const H264SliceHeader& shdr) {
  chroma_samplings_.Put(sps.GetChromaSampling());

  const int max_bit_depth_minus8 =
      std::max(sps.bit_depth_luma_minus8, sps.bit_depth_chroma_minus8);
  color_depths_.insert(max_bit_depth_minus8 + 8);

  if (!sps.frame_mbs_only_flag) {
    if (shdr.field_pic_flag) {
      saw_paff_ = true;
    } else if (sps.mb_adaptive_frame_field_flag) {
      saw_mbaff_ = true;
    }
  }

  // Map Extended profile (profile_idc == 88) locally rather than in
  // H264Parser::ProfileIDCToVideoCodecProfile(): changing the shared helper
  // would cause MSE and H264Decoder-based hardware decoders (V4L2,
  // VideoToolbox, D3D11/D3D12) to accept Extended streams, exposing a V4L2
  // CHECK failure in VideoCodecProfileToV4L2PixFmt() and breaking fallback to
  // FFmpeg for Extended-only tools (data partitioning, SI/SP slices).
  constexpr int kH264ProfileIDCExtended = 88;
  const VideoCodecProfile profile =
      sps.profile_idc == kH264ProfileIDCExtended
          ? H264PROFILE_EXTENDED
          : H264Parser::ProfileIDCToVideoCodecProfile(sps.profile_idc);
  profiles_.insert(profile);
}

}  // namespace media
