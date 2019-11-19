// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/hevc.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "media/base/decrypt_config.h"
#include "media/base/media_util.h"
#include "media/formats/mp4/avc.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/box_reader.h"
#include "media/video/h265_parser.h"

namespace media {
namespace mp4 {

HEVCDecoderConfigurationRecord::HEVCDecoderConfigurationRecord()
    : configurationVersion(0),
      general_profile_space(0),
      general_tier_flag(0),
      general_profile_idc(0),
      general_profile_compatibility_flags(0),
      general_constraint_indicator_flags(0),
      general_level_idc(0),
      min_spatial_segmentation_idc(0),
      parallelismType(0),
      chromaFormat(0),
      bitDepthLumaMinus8(0),
      bitDepthChromaMinus8(0),
      avgFrameRate(0),
      constantFrameRate(0),
      numTemporalLayers(0),
      temporalIdNested(0),
      lengthSizeMinusOne(0),
      numOfArrays(0) {}

HEVCDecoderConfigurationRecord::~HEVCDecoderConfigurationRecord() {}
FourCC HEVCDecoderConfigurationRecord::BoxType() const { return FOURCC_HVCC; }

bool HEVCDecoderConfigurationRecord::Parse(BoxReader* reader) {
  return ParseInternal(reader, reader->media_log());
}

bool HEVCDecoderConfigurationRecord::Parse(const uint8_t* data, int data_size) {
  BufferReader reader(data, data_size);
  // TODO(wolenetz): Questionable MediaLog usage, http://crbug.com/712310
  NullMediaLog media_log;
  return ParseInternal(&reader, &media_log);
}

HEVCDecoderConfigurationRecord::HVCCNALArray::HVCCNALArray()
    : first_byte(0) {}

HEVCDecoderConfigurationRecord::HVCCNALArray::HVCCNALArray(
    const HVCCNALArray& other) = default;

HEVCDecoderConfigurationRecord::HVCCNALArray::~HVCCNALArray() {}

bool HEVCDecoderConfigurationRecord::ParseInternal(BufferReader* reader,
                                                   MediaLog* media_log) {
  uint8_t profile_indication = 0;
  uint32_t general_constraint_indicator_flags_hi = 0;
  uint16_t general_constraint_indicator_flags_lo = 0;
  uint8_t misc = 0;
  RCHECK(reader->Read1(&configurationVersion) && configurationVersion == 1 &&
         reader->Read1(&profile_indication) &&
         reader->Read4(&general_profile_compatibility_flags) &&
         reader->Read4(&general_constraint_indicator_flags_hi) &&
         reader->Read2(&general_constraint_indicator_flags_lo) &&
         reader->Read1(&general_level_idc) &&
         reader->Read2(&min_spatial_segmentation_idc) &&
         reader->Read1(&parallelismType) &&
         reader->Read1(&chromaFormat) &&
         reader->Read1(&bitDepthLumaMinus8) &&
         reader->Read1(&bitDepthChromaMinus8) &&
         reader->Read2(&avgFrameRate) &&
         reader->Read1(&misc) &&
         reader->Read1(&numOfArrays));

  general_profile_space = profile_indication >> 6;
  general_tier_flag = (profile_indication >> 5) & 1;
  general_profile_idc = profile_indication & 0x1f;

  general_constraint_indicator_flags = general_constraint_indicator_flags_hi;
  general_constraint_indicator_flags <<= 16;
  general_constraint_indicator_flags |= general_constraint_indicator_flags_lo;

  min_spatial_segmentation_idc &= 0xfff;
  parallelismType &= 3;
  chromaFormat &= 3;
  bitDepthLumaMinus8 &= 7;
  bitDepthChromaMinus8 &= 7;

  constantFrameRate = misc >> 6;
  numTemporalLayers = (misc >> 3) & 7;
  temporalIdNested = (misc >> 2) & 1;
  lengthSizeMinusOne = misc & 3;

  DVLOG(2) << __func__ << " numOfArrays=" << (int)numOfArrays;
  arrays.resize(numOfArrays);
  for (uint32_t j = 0; j < numOfArrays; j++) {
    RCHECK(reader->Read1(&arrays[j].first_byte));
    uint16_t numNalus = 0;
    RCHECK(reader->Read2(&numNalus));
    arrays[j].units.resize(numNalus);
    for (uint32_t i = 0; i < numNalus; ++i) {
      uint16_t naluLength = 0;
      RCHECK(reader->Read2(&naluLength) &&
             reader->ReadVec(&arrays[j].units[i], naluLength));
      DVLOG(4) << __func__ << " naluType=" << (int)(arrays[j].first_byte & 0x3f)
               << " size=" << arrays[j].units[i].size();
    }
  }

  return true;
}

VideoCodecProfile HEVCDecoderConfigurationRecord::GetVideoProfile() const {
  // The values of general_profile_idc are taken from the HEVC standard, see
  // the latest https://www.itu.int/rec/T-REC-H.265/en section A.3
  switch (general_profile_idc) {
    case 1:
      return HEVCPROFILE_MAIN;
    case 2:
      return HEVCPROFILE_MAIN10;
    case 3:
      return HEVCPROFILE_MAIN_STILL_PICTURE;
  }
  return VIDEO_CODEC_PROFILE_UNKNOWN;
}

static const uint8_t kAnnexBStartCode[] = {0, 0, 0, 1};
static const int kAnnexBStartCodeSize = 4;

// static
bool HEVC::InsertParamSetsAnnexB(
    const HEVCDecoderConfigurationRecord& hevc_config,
    std::vector<uint8_t>* buffer,
    std::vector<SubsampleEntry>* subsamples) {
  DCHECK(HEVC::AnalyzeAnnexB(buffer->data(), buffer->size(), *subsamples)
             .is_conformant.value_or(true));

  std::unique_ptr<H265Parser> parser(new H265Parser());
  const uint8_t* start = buffer->data();
  parser->SetEncryptedStream(start, buffer->size(), *subsamples);

  H265NALU nalu;
  if (parser->AdvanceToNextNALU(&nalu) != H265Parser::kOk)
    return false;

  std::vector<uint8_t>::iterator config_insert_point = buffer->begin();

  if (nalu.nal_unit_type == H265NALU::AUD_NUT) {
    // Move insert point to just after the AUD.
    config_insert_point += (nalu.data + nalu.size) - start;
  }

  // Clear |parser| and |start| since they aren't needed anymore and
  // will hold stale pointers once the insert happens.
  parser.reset();
  start = NULL;

  std::vector<uint8_t> param_sets;
  RCHECK(HEVC::ConvertConfigToAnnexB(hevc_config, &param_sets));
  DVLOG(4) << __func__ << " converted hvcC to AnnexB "
           << " size=" << param_sets.size() << " inserted at "
           << (int)(config_insert_point - buffer->begin());

  if (subsamples && !subsamples->empty()) {
    int subsample_index = AVC::FindSubsampleIndex(*buffer, subsamples,
                                                  &(*config_insert_point));
    // Update the size of the subsample where SPS/PPS is to be inserted.
    (*subsamples)[subsample_index].clear_bytes += param_sets.size();
  }

  buffer->insert(config_insert_point,
                 param_sets.begin(), param_sets.end());

  DCHECK(HEVC::AnalyzeAnnexB(buffer->data(), buffer->size(), *subsamples)
             .is_conformant.value_or(true));
  return true;
}

// static
bool HEVC::ConvertConfigToAnnexB(
    const HEVCDecoderConfigurationRecord& hevc_config,
    std::vector<uint8_t>* buffer) {
  DCHECK(buffer->empty());
  buffer->clear();

  for (size_t j = 0; j < hevc_config.arrays.size(); j++) {
    uint8_t naluType = hevc_config.arrays[j].first_byte & 0x3f;
    for (size_t i = 0; i < hevc_config.arrays[j].units.size(); ++i) {
      DVLOG(3) << __func__ << " naluType=" << (int)naluType
               << " size=" << hevc_config.arrays[j].units[i].size();
      buffer->insert(buffer->end(), kAnnexBStartCode,
                     kAnnexBStartCode + kAnnexBStartCodeSize);
      buffer->insert(buffer->end(), hevc_config.arrays[j].units[i].begin(),
                     hevc_config.arrays[j].units[i].end());
    }
  }

  return true;
}

// static
BitstreamConverter::AnalysisResult HEVC::AnalyzeAnnexB(
    const uint8_t* buffer,
    size_t size,
    const std::vector<SubsampleEntry>& subsamples) {
  DVLOG(3) << __func__;
  DCHECK(buffer);

  BitstreamConverter::AnalysisResult result;
  result.is_conformant = false;  // Will change if needed before return.

  if (size == 0) {
    result.is_conformant = true;
    return result;
  }

  H265Parser parser;
  parser.SetEncryptedStream(buffer, size, subsamples);

  enum NALUOrderState {
    kAUDAllowed,
    kBeforeFirstVCL,
    kAfterFirstVCL,
    kEOBitstreamAllowed,
    kNoMoreDataAllowed,
  };

  H265NALU nalu;
  NALUOrderState order_state = kAUDAllowed;

  // Rec. ITU-T H.265 v5 (02/2018)
  // 7.4.2.4.4 Order of NAL units and coded pictures and their association to
  // access units
  while (true) {
    H265Parser::Result h265_result = parser.AdvanceToNextNALU(&nalu);
    if (h265_result == H265Parser::kEOStream) {
      break;
    }

    if (h265_result != H265Parser::kOk) {
      DCHECK_NE(h265_result, H265Parser::kUnsupportedStream)
          << "AdvanceToNextNALU() returned kUnsupportedStream!";
      return result;
    }

    DVLOG(3) << "nal_unit_type " << nalu.nal_unit_type;

    // Definition of "access unit" and "base layer" is only applied to NALs with
    // nuh_layer_id equals 0.
    if (nalu.nuh_layer_id != 0) {
      LOG(WARNING) << "Unrecognized layer ID " << nalu.nuh_layer_id
                   << ", skip.";
      continue;
    }

    if (order_state == kNoMoreDataAllowed) {
      DVLOG(1) << "No more data is allowed after EOB_NUT.";
      return result;
    }

    if (order_state == kEOBitstreamAllowed &&
        nalu.nal_unit_type != H265NALU::EOB_NUT) {
      DVLOG(1) << "Only EOB_NUT is allowed after EOS_NUT.";
      return result;
    }

    switch (nalu.nal_unit_type) {
      // When an access unit delimiter NAL unit with nuh_layer_id equal to 0 is
      // present, it shall be the first NAL unit. There shall be at most one
      // access unit delimiter NAL unit with nuh_layer_id equal to 0 in any
      // access unit.
      case H265NALU::AUD_NUT:
        if (order_state > kAUDAllowed) {
          DVLOG(1) << "Unexpected AUD in order_state " << order_state;
          return result;
        }
        order_state = kBeforeFirstVCL;
        break;

      // When any VPS NAL units, SPS NAL units, PPS NAL units, prefix SEI NAL
      // units, NAL units with nal_unit_type in the range of
      // RSV_NVCL41..RSV_NVCL44, or NAL units with nal_unit_type in the range of
      // UNSPEC48..UNSPEC55 are present, they shall not follow the last VCL NAL
      // unit of the access unit.
      case H265NALU::VPS_NUT:
      case H265NALU::SPS_NUT:
      case H265NALU::PPS_NUT:
      case H265NALU::PREFIX_SEI_NUT:
      case H265NALU::RSV_NVCL41:
      case H265NALU::RSV_NVCL42:
      case H265NALU::RSV_NVCL43:
      case H265NALU::RSV_NVCL44:
      case H265NALU::UNSPEC48:
      case H265NALU::UNSPEC49:
      case H265NALU::UNSPEC50:
      case H265NALU::UNSPEC51:
      case H265NALU::UNSPEC52:
      case H265NALU::UNSPEC53:
      case H265NALU::UNSPEC54:
      case H265NALU::UNSPEC55:
        if (order_state > kBeforeFirstVCL) {
          DVLOG(1) << "Unexpected NALU type " << nalu.nal_unit_type
                   << " in order_state " << order_state;
          return result;
        }
        order_state = kBeforeFirstVCL;
        break;

      // NAL units having nal_unit_type equal to FD_NUT or SUFFIX_SEI_NUT or in
      // the range of RSV_NVCL45..RSV_NVCL47 or UNSPEC56..UNSPEC63 shall not
      // precede the first VCL NAL unit of the access unit.
      case H265NALU::FD_NUT:
      case H265NALU::SUFFIX_SEI_NUT:
      case H265NALU::RSV_NVCL45:
      case H265NALU::RSV_NVCL46:
      case H265NALU::RSV_NVCL47:
      case H265NALU::UNSPEC56:
      case H265NALU::UNSPEC57:
      case H265NALU::UNSPEC58:
      case H265NALU::UNSPEC59:
      case H265NALU::UNSPEC60:
      case H265NALU::UNSPEC61:
      case H265NALU::UNSPEC62:
      case H265NALU::UNSPEC63:
        if (order_state < kAfterFirstVCL) {
          DVLOG(1) << "Unexpected NALU type " << nalu.nal_unit_type
                   << " in order_state " << order_state;
          return result;
        }
        break;

      // When an end of sequence NAL unit with nuh_layer_id equal to 0 is
      // present, it shall be the last NAL unit among all NAL units with
      // nuh_layer_id equal to 0 in the access unit other than an end of
      // bitstream NAL unit (when present).
      case H265NALU::EOS_NUT:
        if (order_state != kAfterFirstVCL) {
          DVLOG(1) << "Unexpected EOS in order_state " << order_state;
          return result;
        }
        order_state = kEOBitstreamAllowed;
        break;

      // When an end of bitstream NAL unit is present, it shall be the last NAL
      // unit in the access unit.
      case H265NALU::EOB_NUT:
        if (order_state < kAfterFirstVCL) {
          DVLOG(1) << "Unexpected EOB in order_state " << order_state;
          return result;
        }
        order_state = kNoMoreDataAllowed;
        break;

      // VCL, non-IRAP
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
      case H265NALU::RSV_VCL_N10:
      case H265NALU::RSV_VCL_R11:
      case H265NALU::RSV_VCL_N12:
      case H265NALU::RSV_VCL_R13:
      case H265NALU::RSV_VCL_N14:
      case H265NALU::RSV_VCL_R15:
      case H265NALU::RSV_VCL24:
      case H265NALU::RSV_VCL25:
      case H265NALU::RSV_VCL26:
      case H265NALU::RSV_VCL27:
      case H265NALU::RSV_VCL28:
      case H265NALU::RSV_VCL29:
      case H265NALU::RSV_VCL30:
      case H265NALU::RSV_VCL31:
        if (order_state > kAfterFirstVCL) {
          DVLOG(1) << "Unexpected VCL in order_state " << order_state;
          return result;
        }

        if (!result.is_keyframe.has_value())
          result.is_keyframe = false;

        order_state = kAfterFirstVCL;
        break;

      // VCL, IRAP
      case H265NALU::BLA_W_LP:
      case H265NALU::BLA_W_RADL:
      case H265NALU::BLA_N_LP:
      case H265NALU::IDR_W_RADL:
      case H265NALU::IDR_N_LP:
      case H265NALU::CRA_NUT:
      case H265NALU::RSV_IRAP_VCL22:
      case H265NALU::RSV_IRAP_VCL23:
        if (order_state > kAfterFirstVCL) {
          DVLOG(1) << "Unexpected VCL in order_state " << order_state;
          return result;
        }

        if (!result.is_keyframe.has_value())
          result.is_keyframe = true;

        order_state = kAfterFirstVCL;
        break;

      default:
        NOTREACHED() << "Unsupported NALU type " << nalu.nal_unit_type;
    }
  }

  if (order_state < kAfterFirstVCL)
    return result;

  result.is_conformant = true;
  DCHECK(result.is_keyframe.has_value());
  return result;
}

HEVCBitstreamConverter::HEVCBitstreamConverter(
    std::unique_ptr<HEVCDecoderConfigurationRecord> hevc_config)
    : hevc_config_(std::move(hevc_config)) {
  DCHECK(hevc_config_);
}

HEVCBitstreamConverter::~HEVCBitstreamConverter() {
}

bool HEVCBitstreamConverter::ConvertAndAnalyzeFrame(
    std::vector<uint8_t>* frame_buf,
    bool is_keyframe,
    std::vector<SubsampleEntry>* subsamples,
    AnalysisResult* analysis_result) const {
  RCHECK(AVC::ConvertFrameToAnnexB(hevc_config_->lengthSizeMinusOne + 1,
                                   frame_buf, subsamples));
  // |is_keyframe| may be incorrect. Analyze the frame to see if it is a
  // keyframe. |is_keyframe| will be used if the analysis is inconclusive.
  // Also, provide the analysis result to the caller via out parameter
  // |analysis_result|.
  *analysis_result = Analyze(frame_buf, subsamples);

  if (analysis_result->is_keyframe.value_or(is_keyframe)) {
    // If this is a keyframe, we (re-)inject HEVC params headers at the start of
    // a frame. If subsample info is present, we also update the clear byte
    // count for that first subsample.
    RCHECK(HEVC::InsertParamSetsAnnexB(*hevc_config_, frame_buf, subsamples));
  }

  return true;
}

BitstreamConverter::AnalysisResult HEVCBitstreamConverter::Analyze(
    std::vector<uint8_t>* frame_buf,
    std::vector<SubsampleEntry>* subsamples) const {
  return HEVC::AnalyzeAnnexB(frame_buf->data(), frame_buf->size(), *subsamples);
}

}  // namespace mp4
}  // namespace media
