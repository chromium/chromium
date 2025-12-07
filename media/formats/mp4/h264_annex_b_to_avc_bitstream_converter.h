// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_H264_ANNEX_B_TO_AVC_BITSTREAM_CONVERTER_H_
#define MEDIA_FORMATS_MP4_H264_ANNEX_B_TO_AVC_BITSTREAM_CONVERTER_H_

#include <stdint.h>

#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "media/base/media_export.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/mp4_status.h"
#include "media/parsers/h264_parser.h"

namespace media {

// H264AnnexBToAvcBitstreamConverter is a class to convert H.264 bitstream from
// Annex B (ISO/IEC 14496-10) to AVC (as specified in ISO/IEC 14496-15).
class MEDIA_EXPORT H264AnnexBToAvcBitstreamConverter {
 public:
  // Construct the bitstream converter.
  //
  // `add_parameter_sets_in_bitstream` - indicates whether the parameter sets
  // can be copied to the output bitstream or not. When set to false, parameter
  // sets are only stored in the `AVCDecoderConfigurationRecord`, which complies
  // with the requirements of `avc1` as defined in ISO/IEC 14496-15:2019
  // - 5.3.2. When set to true, parameter sets are stored both in the output
  // bitstream and in the `AVCDecoderConfigurationRecord`, which complies with
  // the requirements of `avc3` as defined in ISO/IEC 14496-15:2019 - 5.3.2.
  //
  // NOTE: for `avc3`, the spec doesn't require the muxer to insert parameter
  // sets into the bitstream, and only states that it is optional, nevertheless,
  // this converter always assumes that they need to have the parameter sets
  // inserted.
  explicit H264AnnexBToAvcBitstreamConverter(
      bool add_parameter_sets_in_bitstream);

  H264AnnexBToAvcBitstreamConverter(const H264AnnexBToAvcBitstreamConverter&) =
      delete;
  H264AnnexBToAvcBitstreamConverter& operator=(
      const H264AnnexBToAvcBitstreamConverter&) = delete;

  ~H264AnnexBToAvcBitstreamConverter();

  // Converts a video chunk from a format with in-place decoder configuration
  // into a format where configuration needs to be sent separately.
  //
  // `input` - where to read the data from
  // `output` - where to put the converted video data
  // If error kBufferTooSmall is returned, it means that `output` was not
  // big enough to contain a converted video chunk. In this case `size_out`
  // is populated.
  // `config_changed_out` is set to True if the video chunk
  // processed by this call contained decoder configuration information.
  // In this case latest configuration information can be obtained
  // from GetCurrentConfig().
  // `size_out` - number of bytes written to `output`, or desired size of
  // `output` if it's too small.
  MP4Status ConvertChunk(base::span<const uint8_t> input,
                         base::span<uint8_t> output,
                         bool* config_changed_out,
                         size_t* size_out);

  // Returns the latest version of decoder configuration, found in converted
  // video chunks.
  const mp4::AVCDecoderConfigurationRecord& GetCurrentConfig();

 private:
  H264Parser parser_;
  mp4::AVCDecoderConfigurationRecord config_;

  using blob = std::vector<uint8_t>;
  base::flat_map<int, blob> id2sps_;
  base::flat_map<int, blob> id2sps_ext_;
  base::flat_map<int, blob> id2pps_;

  int active_sps_id_ = -1;
  int active_pps_id_ = -1;

  const bool add_parameter_sets_in_bitstream_ = false;
};

}  // namespace media

#endif  // MEDIA_FORMATS_MP4_H264_ANNEX_B_TO_AVC_BITSTREAM_CONVERTER_H_
