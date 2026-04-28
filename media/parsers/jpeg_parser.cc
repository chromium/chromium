// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/parsers/jpeg_parser.h"

#include <cstring>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "media/parsers/parse_jpeg.rs.h"

namespace media {

const std::array<JpegHuffmanTable, kJpegMaxHuffmanTableNumBaseline>
    kDefaultDcTable = {{
        // luminance DC coefficients
        {
            true,
            {0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0},
            {{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
              0x0b}},
        },
        // chrominance DC coefficients
        {
            true,
            {0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
            {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb}},
        },
    }};
const std::array<JpegHuffmanTable, kJpegMaxHuffmanTableNumBaseline>
    kDefaultAcTable = {{
        // luminance AC coefficients
        {
            true,
            {0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d},
            {{0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41,
              0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91,
              0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0, 0x24,
              0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a,
              0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38,
              0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53,
              0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66,
              0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
              0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93,
              0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
              0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
              0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9,
              0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1,
              0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2,
              0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa}},
        },
        // chrominance AC coefficients
        {
            true,
            {0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77},
            {{0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12,
              0x41, 0x51, 0x07, 0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08, 0x14,
              0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0, 0x15,
              0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17,
              0x18, 0x19, 0x1a, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37,
              0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a,
              0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65,
              0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
              0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a,
              0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3,
              0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5,
              0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
              0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9,
              0xda, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2,
              0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa}},
        },
    }};

constexpr uint8_t kZigZag8x8[64] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

constexpr JpegQuantizationTable kDefaultQuantTable[2] = {
    // Table K.1 Luminance quantization table values.
    {
        true,
        {16, 11, 10, 16, 24,  40,  51,  61,  12, 12, 14, 19, 26,  58,  60,  55,
         14, 13, 16, 24, 40,  57,  69,  56,  14, 17, 22, 29, 51,  87,  80,  62,
         18, 22, 37, 56, 68,  109, 103, 77,  24, 35, 55, 64, 81,  104, 113, 92,
         49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99},
    },
    // Table K.2 Chrominance quantization table values.
    {
        true,
        {17, 18, 24, 47, 99, 99, 99, 99, 18, 21, 26, 66, 99, 99, 99, 99,
         24, 26, 56, 99, 99, 99, 99, 99, 47, 66, 99, 99, 99, 99, 99, 99,
         99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
         99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99},
    },
};

bool ParseJpegPicture(base::span<const uint8_t> buffer,
                      JpegParseResult* result) {
  CHECK(result);
  if (buffer.empty()) {
    return false;
  }
  auto ffi_res = parsers::parse_jpeg_picture_ffi(
      rust::Slice<const uint8_t>(buffer.data(), buffer.size()));
  if (ffi_res.error_code != parsers::JpegParserError::Ok) {
    DLOG(ERROR) << "JPEG parser error: "
                << static_cast<int>(ffi_res.error_code);
    return false;
  }

  *result = {};

  result->frame_header.visible_width = ffi_res.frame_header.visible_width;
  result->frame_header.visible_height = ffi_res.frame_header.visible_height;
  result->frame_header.coded_width = ffi_res.frame_header.coded_width;
  result->frame_header.coded_height = ffi_res.frame_header.coded_height;
  result->frame_header.num_components = ffi_res.frame_header.num_components;

  const size_t num_components = std::min<size_t>(
      ffi_res.frame_header.components.size(), kJpegMaxComponents);
  for (size_t i = 0; i < num_components; ++i) {
    const auto& src = ffi_res.frame_header.components[i];
    auto& dst = result->frame_header.components[i];
    dst.id = src.id;
    dst.horizontal_sampling_factor = src.horizontal_sampling_factor;
    dst.vertical_sampling_factor = src.vertical_sampling_factor;
    dst.quantization_table_selector = src.quantization_table_selector;
  }

  auto dc_tables = base::span(result->dc_table);
  const size_t num_dc_tables =
      std::min<size_t>(std::size(ffi_res.dc_table), dc_tables.size());
  for (size_t i = 0; i < num_dc_tables; ++i) {
    const auto& src = ffi_res.dc_table[i];
    auto& dst = dc_tables[i];
    dst.valid = src.valid;
    if (dst.valid) {
      dst.code_length = src.code_length;
      dst.code_value = src.code_value;
    }
  }

  auto ac_tables = base::span(result->ac_table);
  const size_t num_ac_tables =
      std::min<size_t>(std::size(ffi_res.ac_table), ac_tables.size());
  for (size_t i = 0; i < num_ac_tables; ++i) {
    const auto& src = ffi_res.ac_table[i];
    auto& dst = ac_tables[i];
    dst.valid = src.valid;
    if (dst.valid) {
      dst.code_length = src.code_length;
      dst.code_value = src.code_value;
    }
  }

  auto q_tables = base::span(result->q_table);
  const size_t num_q_tables =
      std::min<size_t>(std::size(ffi_res.q_table), q_tables.size());
  for (size_t i = 0; i < num_q_tables; ++i) {
    const auto& src = ffi_res.q_table[i];
    auto& dst = q_tables[i];
    dst.valid = src.valid;
    if (dst.valid) {
      base::span(dst.value).copy_from(base::span(src.value));
    }
  }

  result->restart_interval = ffi_res.restart_interval;

  result->scan.num_components = ffi_res.scan.num_components;
  const size_t num_scan_components =
      std::min<size_t>(ffi_res.scan.components.size(), kJpegMaxComponents);
  for (size_t i = 0; i < num_scan_components; ++i) {
    const auto& src = ffi_res.scan.components[i];
    auto& dst = result->scan.components[i];
    dst.component_selector = src.component_selector;
    dst.dc_selector = src.dc_selector;
    dst.ac_selector = src.ac_selector;
  }

  if (ffi_res.data_offset + ffi_res.data_length > buffer.size()) {
    return false;
  }
  result->data = buffer.subspan(ffi_res.data_offset, ffi_res.data_length);
  result->image_size = ffi_res.image_size;

  return true;
}

std::ostream& operator<<(std::ostream& os, const JpegParseResult& result) {
  os << "{ visible_width: " << result.frame_header.visible_width
     << ", visible_height: " << result.frame_header.visible_height
     << ", coded_width: " << result.frame_header.coded_width
     << ", coded_height: " << result.frame_header.coded_height
     << ", num_components: "
     << static_cast<int>(result.frame_header.num_components)
     << ", restart_interval: " << result.restart_interval
     << ", scan_num_components: "
     << static_cast<int>(result.scan.num_components)
     << ", data_size: " << result.data.size()
     << ", image_size: " << result.image_size << " }";
  return os;
}

}  // namespace media
