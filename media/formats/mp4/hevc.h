// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_HEVC_H_
#define MEDIA_FORMATS_MP4_HEVC_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "media/base/media_export.h"
#include "media/base/video_codecs.h"
#include "media/formats/mp4/bitstream_converter.h"
#include "media/formats/mp4/box_definitions.h"

namespace media {

struct SubsampleEntry;

namespace mp4 {

struct MEDIA_EXPORT HEVCDecoderConfigurationRecord : Box {
  DECLARE_BOX_METHODS(HEVCDecoderConfigurationRecord);

  // Parses HEVCDecoderConfigurationRecord data encoded in |data|.
  // Note: This method is intended to parse data outside the MP4StreamParser
  //       context and therefore the box header is not expected to be present
  //       in |data|.
  // Returns true if |data| was successfully parsed.
  bool Parse(const uint8_t* data, int data_size);

  uint8_t configurationVersion;
  uint8_t general_profile_space;
  uint8_t general_tier_flag;
  uint8_t general_profile_idc;
  uint32_t general_profile_compatibility_flags;
  uint64_t general_constraint_indicator_flags;
  uint8_t general_level_idc;
  uint16_t min_spatial_segmentation_idc;
  uint8_t parallelismType;
  uint8_t chromaFormat;
  uint8_t bitDepthLumaMinus8;
  uint8_t bitDepthChromaMinus8;
  uint16_t avgFrameRate;
  uint8_t constantFrameRate;
  uint8_t numTemporalLayers;
  uint8_t temporalIdNested;
  uint8_t lengthSizeMinusOne;
  uint8_t numOfArrays;

  typedef std::vector<uint8_t> HVCCNALUnit;
  struct HVCCNALArray {
    HVCCNALArray();
    HVCCNALArray(const HVCCNALArray& other);
    ~HVCCNALArray();
    uint8_t first_byte;
    std::vector<HVCCNALUnit> units;
  };
  std::vector<HVCCNALArray> arrays;

  VideoCodecProfile GetVideoProfile() const;

 private:
  bool ParseInternal(BufferReader* reader, MediaLog* media_log);
};

class MEDIA_EXPORT HEVC {
 public:
  static bool ConvertConfigToAnnexB(
      const HEVCDecoderConfigurationRecord& hevc_config,
      std::vector<uint8_t>* buffer);

  static bool InsertParamSetsAnnexB(
      const HEVCDecoderConfigurationRecord& hevc_config,
      std::vector<uint8_t>* buffer,
      std::vector<SubsampleEntry>* subsamples);

  // Analyzes the contents of |buffer| for conformance to
  // Section 7.4.2.4.4 of ISO/IEC 23008-2, and if conformant, further inspects
  // |buffer| to report whether or not it looks like a keyframe.
  // |subsamples| contains the information about what parts of the buffer are
  // encrypted and which parts are clear.
  static BitstreamConverter::AnalysisResult AnalyzeAnnexB(
      const uint8_t* buffer,
      size_t size,
      const std::vector<SubsampleEntry>& subsamples);
};

class HEVCBitstreamConverter : public BitstreamConverter {
 public:
  explicit HEVCBitstreamConverter(
      std::unique_ptr<HEVCDecoderConfigurationRecord> hevc_config);

  // BitstreamConverter interface
  bool ConvertAndAnalyzeFrame(std::vector<uint8_t>* frame_buf,
                              bool is_keyframe,
                              std::vector<SubsampleEntry>* subsamples,
                              AnalysisResult* analysis_result) const override;

 private:
  ~HEVCBitstreamConverter() override;
  AnalysisResult Analyze(
      std::vector<uint8_t>* frame_buf,
      std::vector<SubsampleEntry>* subsamples) const override;
  std::unique_ptr<HEVCDecoderConfigurationRecord> hevc_config_;
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_FORMATS_MP4_HEVC_H_
