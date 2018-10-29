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
  MediaLog media_log;
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
  DCHECK(buffer);

  BitstreamConverter::AnalysisResult result;

  if (size == 0) {
    result.is_conformant = true;
    return result;
  }

  // TODO(servolk): Implement this, see https://crbug.com/527595. For now, we
  // report that neither conformance nor keyframe analyses were performed.
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
