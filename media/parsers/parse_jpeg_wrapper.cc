// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/parsers/parse_jpeg_wrapper.h"

#include <algorithm>
#include <cstring>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "media/parsers/parse_jpeg.rs.h"

namespace media {

bool ParseJpegPictureRust(base::span<const uint8_t> buffer,
                          JpegParseResult* result) {
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

}  // namespace media
