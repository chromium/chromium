// Copyright 2015 The Chromium Authors
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
#include "media/base/video_decoder_config.h"
#include "media/base/video_types.h"
#include "media/formats/mp4/bitstream_converter.h"
#include "media/formats/mp4/box_definitions.h"

namespace media {

struct SubsampleEntry;

namespace mp4 {

struct MEDIA_EXPORT HEVCDecoderConfigurationRecord : Box {
  DECLARE_BOX_METHODS(HEVCDecoderConfigurationRecord);

  // Parallel processing tools used by decoder.
  enum {
    kMixedParallel = 0,  // mixed mode of slice-based/tile-based/wavefront
    kSliceParallel,      // slices can be decoded independently
    kTileParallel,       // tiles can be decoded independently
    kWaveFrontParallel,  // first row of CTUs decoded normally and rest
                         // parallelized
  };
  // Parses HEVCDecoderConfigurationRecord data encoded in |data|.
  // Note: This method is intended to parse data outside the MP4StreamParser
  //       context and therefore the box header is not expected to be present
  //       in |data|.
  // Returns true if |data| was successfully parsed.
  bool Parse(base::span<const uint8_t> data);
  bool Serialize(std::vector<uint8_t>& output) const;

  uint8_t configuration_version = 0;
  uint8_t general_profile_space = 0;
  uint8_t general_tier_flag = 0;
  uint8_t general_profile_idc = 0;
  uint32_t general_profile_compatibility_flags = 0;
  uint64_t general_constraint_indicator_flags = 0;
  uint8_t general_level_idc = 0;
  uint16_t min_spatial_segmentation_idc = 0;
  uint8_t parallelism_type = 0;
  uint8_t chroma_format = 0;
  uint8_t bit_depth_luma_minus8 = 0;
  uint8_t bit_depth_chroma_minus8 = 0;
  uint16_t avg_frame_rate = 0;
  uint8_t constant_frame_rate = 0;
  uint8_t num_temporal_layers = 0;
  uint8_t temporal_id_nested = 0;
  uint8_t length_size_minus_one = 0;
  uint8_t num_of_arrays = 0;

  using HVCCNALUnit = std::vector<uint8_t>;
  struct HVCCNALArray {
    HVCCNALArray();
    HVCCNALArray(const HVCCNALArray& other);
    ~HVCCNALArray();
    uint8_t first_byte =
        0;  // array_completeness(1)/reserved0(1)/NAL_unit_type(6)
    std::vector<HVCCNALUnit> units;
  };
  std::vector<HVCCNALArray> arrays;

  VideoCodecProfile GetVideoProfile() const;
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  VideoColorSpace GetColorSpace() const;
  VideoChromaSampling GetChromaSampling() const;
  gfx::HDRMetadata GetHDRMetadata() const;
  VideoDecoderConfig::AlphaMode GetAlphaMode() const;
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

 private:
  bool ParseInternal(BufferReader* reader, MediaLog* media_log);
  VideoColorSpace color_space_;
  VideoChromaSampling chroma_sampling_ = VideoChromaSampling::kUnknown;
  gfx::HDRMetadata hdr_metadata_;
  VideoDecoderConfig::AlphaMode alpha_mode_ =
      VideoDecoderConfig::AlphaMode::kIsOpaque;
};

class MEDIA_EXPORT HEVC {
 public:
  static void ConvertConfigToAnnexB(
      const HEVCDecoderConfigurationRecord& hevc_config,
      std::vector<uint8_t>* buffer);

  static bool InsertParamSetsAnnexB(
      const HEVCDecoderConfigurationRecord& hevc_config,
      std::vector<uint8_t>* buffer,
      std::vector<SubsampleEntry>* subsamples);

  // Analyzes the contents of `buffer` for keyframe detection. While it also
  // checks for conformance to Section 7.4.2.4.4 of ISO/IEC 23008-2, parsing is
  // intentionally lax to accommodate real-world content; out-of-order NALUs do
  // not prevent keyframe determination and are reported via `is_conformant`.
  // This method should primarily be used for keyframe probing.
  // `subsamples` contains the information about what parts of the buffer are
  // encrypted and which parts are clear.
  static BitstreamConverter::AnalysisResult AnalyzeAnnexB(
      base::span<const uint8_t> buffer,
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
      base::span<const uint8_t> frame_buf,
      std::vector<SubsampleEntry>* subsamples) const override;
  std::unique_ptr<HEVCDecoderConfigurationRecord> hevc_config_;
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_FORMATS_MP4_HEVC_H_
