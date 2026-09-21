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
#include "media/base/video_codecs.h"
#include "media/formats/mp4/bitstream_converter.h"

namespace media {

struct SubsampleEntry;

namespace mp4 {

struct AVCDecoderConfigurationRecord;

class MEDIA_EXPORT AVC {
 public:
  // Converts a NALU stream where the header is `length_size` bytes - either 1,
  // 2, or 4 bytes. `codec` determines which NALU pattern is dropped for being a
  // dummy NALU (H264: type == 0 // HEVC: length < 2). `buffer` is the stream,
  // and `subsamples` contains a set of byte ranges for clear and cypher bytes
  // which might have to get updated if any NALUs need to be dropped.
  static bool ConvertFrameToAnnexB(size_t length_size,
                                   VideoCodec codec,
                                   std::vector<uint8_t>* buffer,
                                   std::vector<SubsampleEntry>* subsamples);

  // Inserts the SPS & PPS data from `avc_config` into `buffer`.
  // `buffer` is expected to contain AnnexB conformant data.
  // `subsamples` contains the SubsampleEntry info if `buffer` contains
  // encrypted data.
  // Returns true if the param sets were successfully inserted.
  static bool InsertParamSetsAnnexB(
      const AVCDecoderConfigurationRecord& avc_config,
      std::vector<uint8_t>* buffer,
      std::vector<SubsampleEntry>* subsamples);

  static bool ConvertConfigToAnnexB(
      const AVCDecoderConfigurationRecord& avc_config,
      std::vector<uint8_t>* buffer);

  // Analyzes the contents of `buffer` for keyframe detection. While it also
  // checks for conformance to Section 7.4.1.2.3 of ISO/IEC 14496-10, parsing is
  // intentionally lax to accommodate real-world content; out-of-order NALUs do
  // not prevent keyframe determination and are reported via `is_conformant`.
  // This method should primarily be used for keyframe probing.
  // `subsamples` contains the information about what parts of the buffer are
  // encrypted and which parts are clear.
  // `allow_bare_idr` indicates whether the analyzer should treat an IDR NAL
  // unit without accompanying SPS/PPS parameter sets as sufficient to mark a
  // frame as a keyframe. When true, the analyzer relies on a "bare" IDR NAL
  // unit alone to determine keyframe-ness.
  static BitstreamConverter::AnalysisResult AnalyzeAnnexB(
      base::span<const uint8_t> buffer,
      const std::vector<SubsampleEntry>& subsamples,
      bool allow_bare_idr = true);

  // Given a `buffer` and `subsamples` information and `pts` pointer into the
  // `buffer` finds the index of the subsample `ptr` is pointing into.
  static int FindSubsampleIndex(const std::vector<uint8_t>& buffer,
                                base::span<const SubsampleEntry> subsamples,
                                const uint8_t* ptr);

  // Convert a `buffer` from AVC bitstream to Annex-B bitstream by replacing
  // 4-byte NALU length to 4-byte NALU start code.
  static bool ConvertAVCToAnnexBInPlaceForLengthSize4(
      std::vector<uint8_t>* buffer);
};

// AVCBitstreamConverter converts AVC/H.264 bitstream from MP4 container format
// with embedded NALU lengths into AnnexB bitstream format (described in ISO/IEC
// 14496-10) with 4-byte start codes. It also knows how to handle CENC-encrypted
// streams and adjusts subsample data for those streams while converting.
class MEDIA_EXPORT AVCBitstreamConverter : public BitstreamConverter {
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
      base::span<const uint8_t> frame_buf,
      std::vector<SubsampleEntry>* subsamples) const override;
  std::unique_ptr<AVCDecoderConfigurationRecord> avc_config_;
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_FORMATS_MP4_AVC_H_
