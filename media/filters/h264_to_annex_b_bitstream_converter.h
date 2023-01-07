// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_H264_TO_ANNEX_B_BITSTREAM_CONVERTER_H_
#define MEDIA_FILTERS_H264_TO_ANNEX_B_BITSTREAM_CONVERTER_H_

#include <stdint.h>

#include <vector>

#include "media/base/media_export.h"

namespace media {

namespace mp4 {
struct AVCDecoderConfigurationRecord;
}

// H264ToAnnexBBitstreamConverter is a class to convert H.264 bitstream from
// MP4 format (as specified in ISO/IEC 14496-15) into H.264 bytestream
// (as specified in ISO/IEC 14496-10 Annex B).
class MEDIA_EXPORT H264ToAnnexBBitstreamConverter {
 public:
  H264ToAnnexBBitstreamConverter();

  H264ToAnnexBBitstreamConverter(const H264ToAnnexBBitstreamConverter&) =
      delete;
  H264ToAnnexBBitstreamConverter& operator=(
      const H264ToAnnexBBitstreamConverter&) = delete;

  ~H264ToAnnexBBitstreamConverter();

  // Parses the global AVCDecoderConfigurationRecord from the file format's
  // headers. Converter will remember the field length from the configuration
  // headers after this.
  //
  // Parameters
  //   configuration_record
  //     Pointer to buffer containing AVCDecoderConfigurationRecord.
  //   configuration_record_size
  //     Size of the buffer in bytes.
  //   avc_config
  //     Pointer to place the parsed AVCDecoderConfigurationRecord data into.
  //
  // Returns
  //   Returns true if |configuration_record| was successfully parsed. False
  //   is returned if a parsing error occurred.
  //   |avc_config| only contains valid data when true is returned.
  bool ParseConfiguration(const uint8_t* configuration_record,
                          int configuration_record_size,
                          mp4::AVCDecoderConfigurationRecord* avc_config);

  // Returns the buffer size needed to store the parameter sets in |avc_config|
  // in Annex B form.
  uint32_t GetConfigSize(
      const mp4::AVCDecoderConfigurationRecord& avc_config) const;

  // Calculates needed buffer size for the bitstream converted into bytestream.
  // Lightweight implementation that does not do the actual conversion.
  //
  // Parameters
  //   input
  //     Pointer to buffer containing NAL units in MP4 format.
  //   input_size
  //     Size of the buffer in bytes.
  //   avc_config
  //     The AVCDecoderConfigurationRecord that contains the parameter sets that
  //     will be inserted into the output. NULL if no parameter sets need to be
  //     inserted.
  //
  // Returns
  //   Required buffer size for the output NAL unit buffer when converted
  //   to bytestream format, or 0 if could not determine the size of
  //   the output buffer from the data in |input| and |avc_config|.
  uint32_t CalculateNeededOutputBufferSize(
      const uint8_t* input,
      uint32_t input_size,
      const mp4::AVCDecoderConfigurationRecord* avc_config) const;

  // ConvertAVCDecoderConfigToByteStream converts the
  // AVCDecoderConfigurationRecord from the MP4 headers to bytestream format.
  // Client is responsible for making sure the output buffer is large enough
  // to hold the output data. Client can precalculate the needed output buffer
  // size by using GetConfigSize().
  //
  // Parameters
  //   avc_config
  //     The AVCDecoderConfigurationRecord that contains the parameter sets that
  //     will be written to |output|.
  //   output
  //     Pointer to buffer where the output should be written to.
  //   output_size (i/o)
  //     Pointer to the size of the output buffer. Will contain the number of
  //     bytes written to output after successful call.
  //
  // Returns
  //    true  if successful conversion|
  //    false if conversion not successful (|output_size| will hold the amount
  //          of converted data)
  bool ConvertAVCDecoderConfigToByteStream(
      const mp4::AVCDecoderConfigurationRecord& avc_config,
      uint8_t* output,
      uint32_t* output_size);

  // ConvertNalUnitStreamToByteStream converts the NAL unit from MP4 format
  // to bytestream format. Client is responsible for making sure the output
  // buffer is large enough to hold the output data. Client can precalculate the
  // needed output buffer size by using CalculateNeededOutputBufferSize.
  //
  // Parameters
  //   input
  //     Pointer to buffer containing NAL units in MP4 format.
  //   input_size
  //     Size of the buffer in bytes.
  //   avc_config
  //     The AVCDecoderConfigurationRecord that contains the parameter sets to
  //     insert into the output. NULL if no parameter sets need to be inserted.
  //   output
  //     Pointer to buffer where the output should be written to.
  //   output_size (i/o)
  //     Pointer to the size of the output buffer. Will contain the number of
  //     bytes written to output after successful call.
  //
  // Returns
  //    true  if successful conversion
  //    false if conversion not successful (output_size will hold the amount
  //          of converted data)
  bool ConvertNalUnitStreamToByteStream(
      const uint8_t* input,
      uint32_t input_size,
      const mp4::AVCDecoderConfigurationRecord* avc_config,
      uint8_t* output,
      uint32_t* output_size);

 private:
  // Writes Annex B start code and |param_set| to |*out|.
  //  |*out| - Is the memory location to write the parameter set.
  //  |*out_size| - Number of bytes available for the parameter set.
  // Returns true if the start code and param set were successfully
  // written. On a successful write, |*out| is updated to point to the first
  // byte after the data that was written. |*out_size| is updated to reflect
  // the new number of bytes left in |*out|.
  bool WriteParamSet(const std::vector<uint8_t>& param_set,
                     uint8_t** out,
                     uint32_t* out_size) const;

  // Flag for indicating whether global parameter sets have been processed.
  bool configuration_processed_;
  // Flag for indicating whether next NAL unit starts new access unit.
  bool first_nal_unit_in_access_unit_;
  // Variable to hold interleaving field's length in bytes.
  uint8_t nal_unit_length_field_width_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_H264_TO_ANNEX_B_BITSTREAM_CONVERTER_H_
