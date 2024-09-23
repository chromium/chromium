// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_H265_ANNEX_B_TO_HEVC_BITSTREAM_CONVERTER_H_
#define MEDIA_FORMATS_MP4_H265_ANNEX_B_TO_HEVC_BITSTREAM_CONVERTER_H_

#include <stdint.h>

#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "media/base/media_export.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/hevc.h"
#include "media/formats/mp4/mp4_status.h"
#include "media/parsers/h265_nalu_parser.h"
#include "media/parsers/h265_parser.h"

namespace media {

// H265AnnexBToHevcBitstreamConverter is a class to convert H.265 bitstream from
// Annex B (ISO/IEC 14496-10) to HEVC (as specified in ISO/IEC 14496-15).
class MEDIA_EXPORT H265AnnexBToHevcBitstreamConverter {
 public:
  H265AnnexBToHevcBitstreamConverter();

  H265AnnexBToHevcBitstreamConverter(
      const H265AnnexBToHevcBitstreamConverter&) = delete;
  H265AnnexBToHevcBitstreamConverter& operator=(
      const H265AnnexBToHevcBitstreamConverter&) = delete;

  ~H265AnnexBToHevcBitstreamConverter();

  // Converts a video chunk from a format with in-place decoder configuration
  // into a format where configuration needs to be sent separately.
  //
  // |input| - where to read the data from
  // |output| - where to put the converted video data
  // If error kBufferTooSmall is returned, it means that |output| was not
  // big enough to contain a converted video chunk. In this case |size_out|
  // is populated.
  // |config_changed_out| is set to True if the video chunk
  // processed by this call contained decoder configuration information.
  // In this case latest configuration information can be obtained
  // from GetCurrentConfig().
  // |size_out| - number of bytes written to |output|, or desired size of
  // |output| if it's too small.
  MP4Status ConvertChunk(base::span<const uint8_t> input,
                         base::span<uint8_t> output,
                         bool* config_changed_out,
                         size_t* size_out);

  // Returns the latest version of decoder configuration, found in converted
  // video chunks.
  const mp4::HEVCDecoderConfigurationRecord& GetCurrentConfig();

 private:
  H265Parser parser_;
  mp4::HEVCDecoderConfigurationRecord config_;

  using blob = std::vector<uint8_t>;
  base::flat_map<int, blob> id2sps_;
  base::flat_map<int, blob> id2pps_;
  base::flat_map<int, blob> id2vps_;

  int active_sps_id_ = -1;
  int active_pps_id_ = -1;
  int active_vps_id_ = -1;
};

}  // namespace media

#endif  // MEDIA_FORMATS_MP4_H265_ANNEX_B_TO_HEVC_BITSTREAM_CONVERTER_H_
