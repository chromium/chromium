// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/h265_to_annex_b_bitstream_converter.h"

#include <stddef.h>

#include "base/check_op.h"
#include "base/containers/span_reader.h"
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
    base::span<const uint8_t> configuration_record,
    mp4::HEVCDecoderConfigurationRecord* hevc_config) {
  DCHECK(!configuration_record.empty());
  DCHECK(hevc_config);

  if (!hevc_config->Parse(configuration_record)) {
    return false;
  }

  nal_unit_length_field_width_ = hevc_config->lengthSizeMinusOne + 1;
  CHECK_LE(nal_unit_length_field_width_, 4u);

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
    base::span<const uint8_t> input,
    const mp4::HEVCDecoderConfigurationRecord* hevc_config) const {
  uint32_t output_size = 0;
  bool first_nal_in_this_access_unit = first_nal_unit_in_access_unit_;

  if (input.empty()) {
    return 0;  // Error: invalid input data
  }

  if (!configuration_processed_) {
    return 0;  // Error: configuration not handled, we don't know nal unit width
  }

  if (hevc_config)
    output_size += GetConfigSize(*hevc_config);

  // Then add the needed size for the actual packet
  base::SpanReader input_reader(input);
  while (input_reader.remaining() > 0) {
    if (input_reader.remaining() < nal_unit_length_field_width_) {
      return 0;  // Error: not enough data for correct conversion.
    }

    // Read the next NAL unit length from the input buffer
    uint32_t nal_unit_length = 0;
    for (uint8_t i = 0; i < nal_unit_length_field_width_; ++i) {
      uint8_t inscan;
      input_reader.ReadU8NativeEndian(inscan);

      nal_unit_length <<= 8;
      nal_unit_length |= inscan;
    }

    if (nal_unit_length == 0) {
      break;  // Signifies that no more data left in the buffer
    } else if (nal_unit_length > input_reader.remaining()) {
      return 0;  // Error: Not enough data for correct conversion
    }

    // Six bits after forbidden_zero_bit of first NAL unit byte signify
    // nal_unit_type.
    const int nal_unit_type = (input_reader.remaining_span()[0] >> 1) & 0x3F;
    if (first_nal_in_this_access_unit ||
        IsAccessUnitBoundaryNal(nal_unit_type)) {
      output_size += 1;  // Extra zero_byte for these nal units
      first_nal_in_this_access_unit = false;
    }
    // Start code prefix
    output_size += sizeof(kStartCodePrefix);
    // Actual NAL unit size
    output_size += nal_unit_length;
    input_reader.Skip(nal_unit_length);
    // No need for trailing zero bits
  }
  return output_size;
}

bool H265ToAnnexBBitstreamConverter::ConvertHEVCDecoderConfigToByteStream(
    const mp4::HEVCDecoderConfigurationRecord& hevc_config,
    base::SpanWriter<uint8_t>& writer) {
  for (auto& nalu_array : hevc_config.arrays) {
    for (auto& nalu : nalu_array.units) {
      if (!WriteParamSet(nalu, writer)) {
        return false;
      }
    }
  }

  configuration_processed_ = true;
  return true;
}

bool H265ToAnnexBBitstreamConverter::ConvertNalUnitStreamToByteStream(
    base::span<const uint8_t> input,
    const mp4::HEVCDecoderConfigurationRecord* hevc_config,
    base::SpanWriter<uint8_t>& writer) {
  if (input.empty() || writer.remaining() == 0) {
    return false;  // Error: invalid input
  }

  const size_t original_num_written = writer.num_written();
  // Do the actual conversion for the actual input packet
  int nal_unit_count = 0;
  base::SpanReader input_reader(input);
  while (input_reader.remaining() > 0) {
    uint32_t nal_unit_length = 0;

    // Read the next NAL unit length from the input buffer by scanning
    // the input stream with the specific length field width
    if (nal_unit_length_field_width_ > input_reader.remaining()) {
      return false;
    }
    for (uint8_t i = 0; i < nal_unit_length_field_width_; ++i) {
      uint8_t inscan;
      input_reader.ReadU8NativeEndian(inscan);

      nal_unit_length <<= 8;
      nal_unit_length |= inscan;
    }

    if (nal_unit_length == 0) {
      break;  // Successful conversion, end of buffer
    } else if (nal_unit_length > input_reader.remaining()) {
      return false;  // Error: not enough data for correct conversion
    }

    // Six bits after forbidden_zero_bit of first NAL unit byte signify
    // nal_unit_type.
    int nal_unit_type = (input_reader.remaining_span()[0] >> 1) & 0x3F;
    nal_unit_count++;

    // Insert the config after the AUD if an AUD is the first NAL unit or
    // before all NAL units if the first one isn't an AUD.
    if (hevc_config &&
        (nal_unit_type != H265NALU::AUD_NUT || nal_unit_count > 1)) {
      if (!ConvertHEVCDecoderConfigToByteStream(*hevc_config, writer)) {
        DVLOG(1) << "Failed to insert parameter sets.";
        return false;  // Failed to convert the buffer.
      }
      hevc_config = nullptr;
    }

    // Check if this packet marks access unit boundary by checking the
    // packet type.
    if (IsAccessUnitBoundaryNal(nal_unit_type)) {
      first_nal_unit_in_access_unit_ = true;
    }

    const uint32_t start_code_len =
        sizeof(kStartCodePrefix) + (first_nal_unit_in_access_unit_ ? 1 : 0);
    if (writer.remaining() < start_code_len + nal_unit_length) {
      return false;  // Error: too small output buffer
    }

    // Write extra zero-byte before start code prefix if this packet
    // signals next access unit.
    if (first_nal_unit_in_access_unit_) {
      writer.WriteU8NativeEndian(0);
      first_nal_unit_in_access_unit_ = false;
    }

    // No need to write leading zero bits.
    // Write start-code prefix.
    writer.Write(kStartCodePrefix);
    // Then write the actual NAL unit from the input buffer.
    writer.Write(input_reader.Read(nal_unit_length).value());
    // No need for trailing zero bits.
  }
  // Successful conversion, output the freshly allocated bitstream buffer.
  return writer.num_written() > original_num_written;
}

bool H265ToAnnexBBitstreamConverter::WriteParamSet(
    const std::vector<uint8_t>& param_set,
    base::SpanWriter<uint8_t>& writer) const {
  // Strip trailing null bytes.
  size_t size = param_set.size();
  while (size && param_set[size - 1] == 0)
    size--;
  if (!size)
    return false;

  // Verify space.
  if (writer.remaining() < kParamSetStartCodeSize + size) {
    return false;
  }

  // Write the 4 byte Annex B start code.
  writer.WriteU8NativeEndian(0);
  writer.Write(kStartCodePrefix);

  // Copy the data.
  writer.Write(base::span(param_set).first(size));
  return true;
}

}  // namespace media
