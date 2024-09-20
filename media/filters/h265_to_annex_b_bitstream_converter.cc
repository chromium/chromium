// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/filters/h265_to_annex_b_bitstream_converter.h"

#include <stddef.h>

#include "base/logging.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/hevc.h"
#include "media/parsers/h265_nalu_parser.h"

namespace media {
namespace {

static const uint8_t kStartCodePrefix[3] = {0, 0, 1};
static const uint32_t kParamSetStartCodeSize = 1 + sizeof(kStartCodePrefix);

// Helper function which determines whether NAL unit of given type marks
// access unit boundary.
static bool IsAccessUnitBoundaryNal(int nal_unit_type) {
  // Spec 7.4.2.4.4
  // Check if this packet marks access unit boundary by checking the
  // packet type.
  if (nal_unit_type == media::H265NALU::VPS_NUT ||
      nal_unit_type == media::H265NALU::SPS_NUT ||
      nal_unit_type == media::H265NALU::PPS_NUT ||
      nal_unit_type == media::H265NALU::AUD_NUT ||
      nal_unit_type == media::H265NALU::PREFIX_SEI_NUT ||
      (nal_unit_type >= media::H265NALU::RSV_NVCL41 &&
       nal_unit_type <= media::H265NALU::RSV_NVCL44) ||
      (nal_unit_type >= media::H265NALU::UNSPEC48 &&
       nal_unit_type <= media::H265NALU::UNSPEC55)) {
    return true;
  }
  return false;
}

}  // namespace

H265ToAnnexBBitstreamConverter::H265ToAnnexBBitstreamConverter() = default;

H265ToAnnexBBitstreamConverter::~H265ToAnnexBBitstreamConverter() = default;

bool H265ToAnnexBBitstreamConverter::ParseConfiguration(
    const uint8_t* configuration_record,
    int configuration_record_size,
    mp4::HEVCDecoderConfigurationRecord* hevc_config) {
  DCHECK(configuration_record);
  DCHECK_GT(configuration_record_size, 0);
  DCHECK(hevc_config);

  if (!hevc_config->Parse(configuration_record, configuration_record_size))
    return false;

  nal_unit_length_field_width_ = hevc_config->lengthSizeMinusOne + 1;
  configuration_processed_ = true;
  return true;
}

uint32_t H265ToAnnexBBitstreamConverter::GetConfigSize(
    const mp4::HEVCDecoderConfigurationRecord& hevc_config) const {
  uint32_t config_size = 0;

  for (auto& nalu_array : hevc_config.arrays) {
    for (auto& nalu : nalu_array.units) {
      config_size += kParamSetStartCodeSize + nalu.size();
    }
  }
  return config_size;
}

uint32_t H265ToAnnexBBitstreamConverter::CalculateNeededOutputBufferSize(
    const uint8_t* input,
    uint32_t input_size,
    const mp4::HEVCDecoderConfigurationRecord* hevc_config) const {
  uint32_t output_size = 0;
  uint32_t data_left = input_size;
  bool first_nal_in_this_access_unit = first_nal_unit_in_access_unit_;

  if (input_size == 0)
    return 0;  // Error: invalid input data

  if (!configuration_processed_) {
    return 0;  // Error: configuration not handled, we don't know nal unit width
  }

  if (hevc_config)
    output_size += GetConfigSize(*hevc_config);

  // Then add the needed size for the actual packet
  while (data_left > 0) {
    if (data_left < nal_unit_length_field_width_) {
      return 0;  // Error: not enough data for correct conversion.
    }

    // Read the next NAL unit length from the input buffer
    uint8_t size_of_len_field;
    uint32_t nal_unit_length;
    for (nal_unit_length = 0, size_of_len_field = nal_unit_length_field_width_;
         size_of_len_field > 0; input++, size_of_len_field--, data_left--) {
      nal_unit_length <<= 8;
      nal_unit_length |= *input;
    }

    if (nal_unit_length == 0) {
      break;  // Signifies that no more data left in the buffer
    } else if (nal_unit_length > data_left) {
      return 0;  // Error: Not enough data for correct conversion
    }
    data_left -= nal_unit_length;

    // Six bits after forbidden_zero_bit of first NAL unit byte signify
    // nal_unit_type.
    int nal_unit_type = (*input >> 1) & 0x3F;
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

bool H265ToAnnexBBitstreamConverter::ConvertHEVCDecoderConfigToByteStream(
    const mp4::HEVCDecoderConfigurationRecord& hevc_config,
    uint8_t* output,
    uint32_t* output_size) {
  uint8_t* out = output;
  uint32_t out_size = *output_size;
  *output_size = 0;

  for (auto& nalu_array : hevc_config.arrays) {
    for (auto& nalu : nalu_array.units) {
      if (!WriteParamSet(nalu, &out, &out_size)) {
        return false;
      }
    }
  }

  configuration_processed_ = true;
  *output_size = out - output;
  return true;
}

bool H265ToAnnexBBitstreamConverter::ConvertNalUnitStreamToByteStream(
    const uint8_t* input,
    uint32_t input_size,
    const mp4::HEVCDecoderConfigurationRecord* hevc_config,
    uint8_t* output,
    uint32_t* output_size) {
  const uint8_t* inscan = input;  // We read the input from here progressively
  uint8_t* outscan = output;      // We write the output to here progressively
  uint32_t data_left = input_size;

  if (input_size == 0 || *output_size == 0) {
    *output_size = 0;
    return false;  // Error: invalid input
  }

  // Do the actual conversion for the actual input packet
  int nal_unit_count = 0;
  while (data_left > 0) {
    uint8_t i;
    uint32_t nal_unit_length;

    // Read the next NAL unit length from the input buffer by scanning
    // the input stream with the specific length field width
    for (nal_unit_length = 0, i = nal_unit_length_field_width_;
         i > 0 && data_left > 0; inscan++, i--, data_left--) {
      nal_unit_length <<= 8;
      nal_unit_length |= *inscan;
    }

    if (nal_unit_length == 0) {
      break;  // Successful conversion, end of buffer
    } else if (nal_unit_length > data_left) {
      *output_size = 0;
      return false;  // Error: not enough data for correct conversion
    }

    // Six bits after forbidden_zero_bit of first NAL unit byte signify
    // nal_unit_type.
    int nal_unit_type = (*inscan >> 1) & 0x3F;
    nal_unit_count++;

    // Insert the config after the AUD if an AUD is the first NAL unit or
    // before all NAL units if the first one isn't an AUD.
    if (hevc_config &&
        (nal_unit_type != H265NALU::AUD_NUT || nal_unit_count > 1)) {
      uint32_t output_bytes_used = outscan - output;

      DCHECK_GE(*output_size, output_bytes_used);

      uint32_t config_size = *output_size - output_bytes_used;
      if (!ConvertHEVCDecoderConfigToByteStream(*hevc_config, outscan,
                                                &config_size)) {
        DVLOG(1) << "Failed to insert parameter sets.";
        *output_size = 0;
        return false;  // Failed to convert the buffer.
      }
      outscan += config_size;
      hevc_config = nullptr;
    }
    uint32_t start_code_len;
    first_nal_unit_in_access_unit_
        ? start_code_len = sizeof(kStartCodePrefix) + 1
        : start_code_len = sizeof(kStartCodePrefix);
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

bool H265ToAnnexBBitstreamConverter::WriteParamSet(
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
