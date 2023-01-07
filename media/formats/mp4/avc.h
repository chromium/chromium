// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_AVC_H_
#define MEDIA_FORMATS_MP4_AVC_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "media/base/media_export.h"
#include "media/formats/mp4/bitstream_converter.h"

namespace media {

struct SubsampleEntry;

namespace mp4 {

struct AVCDecoderConfigurationRecord;

class MEDIA_EXPORT AVC {
 public:
  static bool ConvertFrameToAnnexB(size_t length_size,
                                   std::vector<uint8_t>* buffer,
                                   std::vector<SubsampleEntry>* subsamples);

  // Inserts the SPS & PPS data from |avc_config| into |buffer|.
  // |buffer| is expected to contain AnnexB conformant data.
  // |subsamples| contains the SubsampleEntry info if |buffer| contains
  // encrypted data.
  // Returns true if the param sets were successfully inserted.
  static bool InsertParamSetsAnnexB(
      const AVCDecoderConfigurationRecord& avc_config,
      std::vector<uint8_t>* buffer,
      std::vector<SubsampleEntry>* subsamples);

  static bool ConvertConfigToAnnexB(
      const AVCDecoderConfigurationRecord& avc_config,
      std::vector<uint8_t>* buffer);

  // Analyzes the contents of |buffer| for conformance to Section 7.4.1.2.3 of
  // ISO/IEC 14496-10. Also analyzes |buffer| and reports if it looks like a
  // keyframe, if such can be determined. Determination of keyframe-ness is done
  // only if |buffer| is conformant or if lack of conformance is detected after
  // detecting keyframe-ness.
  // |subsamples| contains the information about what parts of the buffer are
  // encrypted and which parts are clear.
  static BitstreamConverter::AnalysisResult AnalyzeAnnexB(
      const uint8_t* buffer,
      size_t size,
      const std::vector<SubsampleEntry>& subsamples);

  // Given a |buffer| and |subsamples| information and |pts| pointer into the
  // |buffer| finds the index of the subsample |ptr| is pointing into.
  static int FindSubsampleIndex(const std::vector<uint8_t>& buffer,
                                const std::vector<SubsampleEntry>* subsamples,
                                const uint8_t* ptr);
};

// AVCBitstreamConverter converts AVC/H.264 bitstream from MP4 container format
// with embedded NALU lengths into AnnexB bitstream format (described in ISO/IEC
// 14496-10) with 4-byte start codes. It also knows how to handle CENC-encrypted
// streams and adjusts subsample data for those streams while converting.
class AVCBitstreamConverter : public BitstreamConverter {
 public:
  explicit AVCBitstreamConverter(
      std::unique_ptr<AVCDecoderConfigurationRecord> avc_config);

  AVCBitstreamConverter(const AVCBitstreamConverter&) = delete;
  AVCBitstreamConverter& operator=(const AVCBitstreamConverter&) = delete;

  // BitstreamConverter interface
  bool ConvertAndAnalyzeFrame(std::vector<uint8_t>* frame_buf,
                              bool is_keyframe,
                              std::vector<SubsampleEntry>* subsamples,
                              AnalysisResult* analysis_result) const override;

 private:
  ~AVCBitstreamConverter() override;
  AnalysisResult Analyze(
      std::vector<uint8_t>* frame_buf,
      std::vector<SubsampleEntry>* subsamples) const override;
  std::unique_ptr<AVCDecoderConfigurationRecord> avc_config_;
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_FORMATS_MP4_AVC_H_
