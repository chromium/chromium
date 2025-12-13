// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/h264_to_annex_b_bitstream_converter.h"

#include <stddef.h>

#include "base/containers/span_reader.h"
#include "base/containers/span_writer.h"
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
    base::span<const uint8_t> configuration_record,
    mp4::AVCDecoderConfigurationRecord* avc_config) {
  DCHECK(!configuration_record.empty());
  DCHECK(avc_config);

  if (!avc_config->Parse(configuration_record)) {
    return false;  // Error: invalid input
  }

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
    base::span<const uint8_t> input,
    const mp4::AVCDecoderConfigurationRecord* avc_config) const {
  uint32_t output_size = 0;
  bool first_nal_in_this_access_unit = first_nal_unit_in_access_unit_;

  if (input.empty()) {
    return 0;  // Error: invalid input data
  }

  if (!configuration_processed_) {
    return 0;  // Error: configuration not handled, we don't know nal unit width
  }

  if (avc_config)
    output_size += GetConfigSize(*avc_config);

  CHECK(nal_unit_length_field_width_ == 1 ||
        nal_unit_length_field_width_ == 2 ||
        nal_unit_length_field_width_ == 4);

  // Then add the needed size for the actual packet
  base::SpanReader input_reader(input);
  while (input_reader.remaining() > 0) {
    if (input_reader.remaining() < nal_unit_length_field_width_) {
      return 0;  // Error: not enough data for correct conversion.
    }

    // Read the next NAL unit length from the input buffer
    uint8_t size_of_len_field = nal_unit_length_field_width_;
    uint32_t nal_unit_length = 0;
    for (; size_of_len_field > 0; size_of_len_field--) {
      uint8_t i;
      input_reader.ReadU8NativeEndian(i);

      nal_unit_length <<= 8;
      nal_unit_length |= i;
    }

    if (nal_unit_length == 0 || input_reader.remaining() == 0) {
      break;  // Signifies that no more data left in the buffer
    } else if (nal_unit_length > input_reader.remaining()) {
      return 0;  // Error: Not enough data for correct conversion
    }

    // five least significant bits of first NAL unit byte signify nal_unit_type
    int nal_unit_type = input_reader.remaining_span()[0] & 0x1F;
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

bool H264ToAnnexBBitstreamConverter::ConvertAVCDecoderConfigToByteStream(
    const mp4::AVCDecoderConfigurationRecord& avc_config,
    base::span<uint8_t> output,
    uint32_t* output_size) {
  base::SpanWriter writer(output);
  for (const auto& i : avc_config.sps_list) {
    if (!WriteParamSet(i, writer)) {
      *output_size = 0;
      return false;
    }
  }

  for (const auto& i : avc_config.pps_list) {
    if (!WriteParamSet(i, writer)) {
      *output_size = 0;
      return false;
    }
  }

  nal_unit_length_field_width_ = avc_config.length_size;
  configuration_processed_ = true;
  *output_size = writer.num_written();
  return true;
}

bool H264ToAnnexBBitstreamConverter::ConvertNalUnitStreamToByteStream(
    base::span<const uint8_t> input,
    const mp4::AVCDecoderConfigurationRecord* avc_config,
    base::span<uint8_t> output,
    uint32_t* output_size) {
  if (input.empty() || *output_size == 0) {
    *output_size = 0;
    return false;  // Error: invalid input
  }

  // NAL unit width should be known at this point
  CHECK(nal_unit_length_field_width_ == 1 ||
        nal_unit_length_field_width_ == 2 ||
        nal_unit_length_field_width_ == 4);

  // Do the actual conversion for the actual input packet
  int nal_unit_count = 0;
  base::SpanWriter writer(output);
  base::SpanReader input_reader(input);
  while (input_reader.remaining() > 0) {
    uint8_t i;
    uint32_t nal_unit_length;

    // Read the next NAL unit length from the input buffer by scanning
    // the input stream with the specific length field width
    for (nal_unit_length = 0, i = nal_unit_length_field_width_;
         i > 0 && input_reader.remaining() > 0; i--) {
      uint8_t inscan;
      input_reader.ReadU8NativeEndian(inscan);

      nal_unit_length <<= 8;
      nal_unit_length |= inscan;
    }

    if (nal_unit_length == 0 || input_reader.remaining() == 0) {
      break;  // Successful conversion, end of buffer
    } else if (nal_unit_length > input_reader.remaining()) {
      *output_size = 0;
      return false;  // Error: not enough data for correct conversion
    }

    // Five least significant bits of first NAL unit byte signify
    // nal_unit_type.
    int nal_unit_type = input_reader.remaining_span()[0] & 0x1F;
    nal_unit_count++;

    // Insert the config after the AUD if an AUD is the first NAL unit or
    // before all NAL units if the first one isn't an AUD.
    if (avc_config &&
        (nal_unit_type != H264NALU::kAUD ||  nal_unit_count > 1)) {
      uint32_t output_bytes_used = writer.num_written();

      DCHECK_GE(*output_size, output_bytes_used);

      uint32_t config_size = *output_size - output_bytes_used;
      if (!ConvertAVCDecoderConfigToByteStream(
              *avc_config, writer.remaining_span(), &config_size)) {
        DVLOG(1) << "Failed to insert parameter sets.";
        *output_size = 0;
        return false;  // Failed to convert the buffer.
      }
      writer.Skip(config_size);
      avc_config = nullptr;
    }
    uint32_t start_code_len;
    first_nal_unit_in_access_unit_ ?
        start_code_len = sizeof(kStartCodePrefix) + 1 :
        start_code_len = sizeof(kStartCodePrefix);
    if (writer.num_written() + start_code_len + nal_unit_length >
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
  *output_size = writer.num_written();
  return *output_size != 0;
}

bool H264ToAnnexBBitstreamConverter::WriteParamSet(
    const std::vector<uint8_t>& param_set,
    base::SpanWriter<uint8_t>& writer) const {
  // Strip trailing null bytes.
  size_t size = param_set.size();
  while (size && param_set[size - 1] == 0) {
    size--;
  }
  if (!size) {
    return false;
  }

  // Verify space.
  if (writer.remaining() < kParamSetStartCodeSize ||
      writer.remaining() - kParamSetStartCodeSize < size) {
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
