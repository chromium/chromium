// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/filters/h264_to_annex_b_bitstream_converter.h"

#include <stddef.h>

#include "base/logging.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/parsers/h264_parser.h"

namespace media {

static const uint8_t kStartCodePrefix[3] = {0, 0, 1};
static const uint32_t kParamSetStartCodeSize = 1 + sizeof(kStartCodePrefix);

// Helper function which determines whether NAL unit of given type marks
// access unit boundary.
static bool IsAccessUnitBoundaryNal(int nal_unit_type) {
  // Check if this packet marks access unit boundary by checking the
  // packet type.
  if (nal_unit_type == 6 ||  // Supplemental enhancement information
      nal_unit_type == 7 ||  // Picture parameter set
      nal_unit_type == 8 ||  // Sequence parameter set
      nal_unit_type == 9 ||  // Access unit delimiter
      (nal_unit_type >= 14 && nal_unit_type <= 18)) {  // Reserved types
    return true;
  }
  return false;
}

H264ToAnnexBBitstreamConverter::H264ToAnnexBBitstreamConverter()
    : configuration_processed_(false),
      first_nal_unit_in_access_unit_(true),
      nal_unit_length_field_width_(0) {
}

H264ToAnnexBBitstreamConverter::~H264ToAnnexBBitstreamConverter() = default;

bool H264ToAnnexBBitstreamConverter::ParseConfiguration(
    const uint8_t* configuration_record,
    int configuration_record_size,
    mp4::AVCDecoderConfigurationRecord* avc_config) {
  DCHECK(configuration_record);
  DCHECK_GT(configuration_record_size, 0);
  DCHECK(avc_config);

  if (!avc_config->Parse(configuration_record, configuration_record_size))
    return false;  // Error: invalid input

  // We're done processing the AVCDecoderConfigurationRecord,
  // store the needed information for parsing actual payload
  nal_unit_length_field_width_ = avc_config->length_size;
  configuration_processed_ = true;
  return true;
}

uint32_t H264ToAnnexBBitstreamConverter::GetConfigSize(
    const mp4::AVCDecoderConfigurationRecord& avc_config) const {
  uint32_t config_size = 0;

  for (size_t i = 0; i < avc_config.sps_list.size(); ++i)
    config_size += kParamSetStartCodeSize + avc_config.sps_list[i].size();

  for (size_t i = 0; i < avc_config.pps_list.size(); ++i)
    config_size += kParamSetStartCodeSize + avc_config.pps_list[i].size();

  return config_size;
}

uint32_t H264ToAnnexBBitstreamConverter::CalculateNeededOutputBufferSize(
    const uint8_t* input,
    uint32_t input_size,
    const mp4::AVCDecoderConfigurationRecord* avc_config) const {
  uint32_t output_size = 0;
  uint32_t data_left = input_size;
  bool first_nal_in_this_access_unit = first_nal_unit_in_access_unit_;

  if (input_size == 0)
    return 0;  // Error: invalid input data

  if (!configuration_processed_) {
    return 0;  // Error: configuration not handled, we don't know nal unit width
  }

  if (avc_config)
    output_size += GetConfigSize(*avc_config);

  CHECK(nal_unit_length_field_width_ == 1 ||
        nal_unit_length_field_width_ == 2 ||
        nal_unit_length_field_width_ == 4);

  // Then add the needed size for the actual packet
  while (data_left > 0) {
    if (data_left < nal_unit_length_field_width_) {
      return 0;  // Error: not enough data for correct conversion.
    }

    // Read the next NAL unit length from the input buffer
    uint8_t size_of_len_field;
    uint32_t nal_unit_length;
    for (nal_unit_length = 0, size_of_len_field = nal_unit_length_field_width_;
         size_of_len_field > 0;
         input++, size_of_len_field--, data_left--) {
      nal_unit_length <<= 8;
      nal_unit_length |= *input;
    }

    if (nal_unit_length == 0) {
      break;  // Signifies that no more data left in the buffer
    } else if (nal_unit_length > data_left) {
      return 0;  // Error: Not enough data for correct conversion
    }
    data_left -= nal_unit_length;

    // five least significant bits of first NAL unit byte signify nal_unit_type
    int nal_unit_type = *input & 0x1F;
    if (first_nal_in_this_access_unit ||
        IsAccessUnitBoundaryNal(nal_unit_type)) {
      output_size += 1;  // Extra zero_byte for these nal units
      first_nal_in_this_access_unit = false;
    }
    // Start code prefix
    output_size += sizeof(kStartCodePrefix);
    // Actual NAL unit size
    output_size += nal_unit_length;
    input += nal_unit_length;
    // No need for trailing zero bits
  }
  return output_size;
}

bool H264ToAnnexBBitstreamConverter::ConvertAVCDecoderConfigToByteStream(
    const mp4::AVCDecoderConfigurationRecord& avc_config,
    uint8_t* output,
    uint32_t* output_size) {
  uint8_t* out = output;
  uint32_t out_size = *output_size;
  *output_size = 0;
  for (size_t i = 0; i < avc_config.sps_list.size(); ++i) {
    if (!WriteParamSet(avc_config.sps_list[i], &out, &out_size))
      return false;
  }

  for (size_t i = 0; i < avc_config.pps_list.size(); ++i) {
    if (!WriteParamSet(avc_config.pps_list[i], &out, &out_size))
      return false;
  }

  nal_unit_length_field_width_ = avc_config.length_size;
  configuration_processed_ = true;
  *output_size = out - output;
  return true;
}

bool H264ToAnnexBBitstreamConverter::ConvertNalUnitStreamToByteStream(
    const uint8_t* input,
    uint32_t input_size,
    const mp4::AVCDecoderConfigurationRecord* avc_config,
    uint8_t* output,
    uint32_t* output_size) {
  const uint8_t* inscan = input;  // We read the input from here progressively
  uint8_t* outscan = output;      // We write the output to here progressively
  uint32_t data_left = input_size;

  if (input_size == 0 || *output_size == 0) {
    *output_size = 0;
    return false;  // Error: invalid input
  }

  // NAL unit width should be known at this point
  CHECK(nal_unit_length_field_width_ == 1 ||
        nal_unit_length_field_width_ == 2 ||
        nal_unit_length_field_width_ == 4);

  // Do the actual conversion for the actual input packet
  int nal_unit_count = 0;
  while (data_left > 0) {
    uint8_t i;
    uint32_t nal_unit_length;

    // Read the next NAL unit length from the input buffer by scanning
    // the input stream with the specific length field width
    for (nal_unit_length = 0, i = nal_unit_length_field_width_;
         i > 0 && data_left > 0;
         inscan++, i--, data_left--) {
      nal_unit_length <<= 8;
      nal_unit_length |= *inscan;
    }

    if (nal_unit_length == 0) {
      break;  // Successful conversion, end of buffer
    } else if (nal_unit_length > data_left) {
      *output_size = 0;
      return false;  // Error: not enough data for correct conversion
    }

    // Five least significant bits of first NAL unit byte signify
    // nal_unit_type.
    int nal_unit_type = *inscan & 0x1F;
    nal_unit_count++;

    // Insert the config after the AUD if an AUD is the first NAL unit or
    // before all NAL units if the first one isn't an AUD.
    if (avc_config &&
        (nal_unit_type != H264NALU::kAUD ||  nal_unit_count > 1)) {
      uint32_t output_bytes_used = outscan - output;

      DCHECK_GE(*output_size, output_bytes_used);

      uint32_t config_size = *output_size - output_bytes_used;
      if (!ConvertAVCDecoderConfigToByteStream(*avc_config,
                                               outscan,
                                               &config_size)) {
        DVLOG(1) << "Failed to insert parameter sets.";
        *output_size = 0;
        return false;  // Failed to convert the buffer.
      }
      outscan += config_size;
      avc_config = NULL;
    }
    uint32_t start_code_len;
    first_nal_unit_in_access_unit_ ?
        start_code_len = sizeof(kStartCodePrefix) + 1 :
        start_code_len = sizeof(kStartCodePrefix);
    if (static_cast<uint32_t>(outscan - output) + start_code_len +
            nal_unit_length >
        *output_size) {
      *output_size = 0;
      return false;  // Error: too small output buffer
    }

    // Check if this packet marks access unit boundary by checking the
    // packet type.
    if (IsAccessUnitBoundaryNal(nal_unit_type)) {
      first_nal_unit_in_access_unit_ = true;
    }

    // Write extra zero-byte before start code prefix if this packet
    // signals next access unit.
    if (first_nal_unit_in_access_unit_) {
      *outscan = 0;
      outscan++;
      first_nal_unit_in_access_unit_ = false;
    }

    // No need to write leading zero bits.
    // Write start-code prefix.
    memcpy(outscan, kStartCodePrefix, sizeof(kStartCodePrefix));
    outscan += sizeof(kStartCodePrefix);
    // Then write the actual NAL unit from the input buffer.
    memcpy(outscan, inscan, nal_unit_length);
    inscan += nal_unit_length;
    data_left -= nal_unit_length;
    outscan += nal_unit_length;
    // No need for trailing zero bits.
  }
  // Successful conversion, output the freshly allocated bitstream buffer.
  *output_size = static_cast<uint32_t>(outscan - output);
  return true;
}

bool H264ToAnnexBBitstreamConverter::WriteParamSet(
    const std::vector<uint8_t>& param_set,
    uint8_t** out,
    uint32_t* out_size) const {
  // Strip trailing null bytes.
  size_t size = param_set.size();
  while (size && param_set[size - 1] == 0)
    size--;
  if (!size)
    return false;

  // Verify space.
  uint32_t bytes_left = *out_size;
  if (bytes_left < kParamSetStartCodeSize ||
      bytes_left - kParamSetStartCodeSize < size) {
    return false;
  }

  uint8_t* start = *out;
  uint8_t* buf = start;

  // Write the 4 byte Annex B start code.
  *buf++ = 0;  // zero byte
  memcpy(buf, kStartCodePrefix, sizeof(kStartCodePrefix));
  buf += sizeof(kStartCodePrefix);

  // Copy the data.
  memcpy(buf, &param_set[0], size);
  buf += size;

  *out = buf;
  *out_size -= buf - start;
  return true;
}

}  // namespace media
