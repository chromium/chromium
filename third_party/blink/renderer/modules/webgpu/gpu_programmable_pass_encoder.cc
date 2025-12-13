// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_programmable_pass_encoder.h"

namespace blink {

// static
bool GPUProgrammablePassEncoder::ValidateSetBindGroupDynamicOffsets(
    const base::span<const uint32_t> dynamic_offsets_data,
    uint64_t dynamic_offsets_data_start,
    uint32_t dynamic_offsets_data_length,
    ExceptionState& exception_state) {
  const uint64_t src_length =
      static_cast<uint64_t>(dynamic_offsets_data.size());

  if (dynamic_offsets_data_start > src_length) {
    exception_state.ThrowRangeError("dynamicOffsetsDataStart too large");
    return false;
  }

  if (static_cast<uint64_t>(dynamic_offsets_data_length) >
      src_length - dynamic_offsets_data_start) {
    exception_state.ThrowRangeError("dynamicOffsetsDataLength too large");
    return false;
  }

  return true;
}

// static
bool GPUProgrammablePassEncoder::ValidateSetImmediatesAndSubSpan(
    ExceptionState& exception_state,
    base::span<const uint8_t>* out,
    uint32_t range_byte_offset,
    base::span<const uint8_t> data,
    uint32_t data_bytes_per_element,
    uint64_t data_element_offset,
    std::optional<uint64_t> data_element_size) {
  CHECK_LE(data_bytes_per_element, 8u);

  // Convert element offset and element size to bytes.
  if (data_element_offset > data.size() / data_bytes_per_element) {
    exception_state.ThrowRangeError("dataOffset is larger than data's size.");
    return false;
  }

  // Size defaults to dataSize - dataOffset. Span out at the last step so
  // convert bytes here.
  uint64_t data_byte_offset = data_element_offset * data_bytes_per_element;
  uint64_t data_byte_size = data.size() - data_byte_offset;

  if (data_element_size.has_value()) {
    if (data_element_size >
        std::numeric_limits<uint64_t>::max() / data_bytes_per_element) {
      exception_state.ThrowRangeError("size overflows.");
      return false;
    }

    data_byte_size = data_element_size.value() * data_bytes_per_element;
  }

  // Data byte size must be multiples of 4.
  if (data_byte_size % 4 != 0) {
    exception_state.ThrowRangeError(
        "size, converted to bytes, must be a multiple of 4.");
    return false;
  }

  // Check OOB.
  uint64_t max_content_byte_size = data.size() - data_byte_offset;

  if (data_byte_size > max_content_byte_size) {
    exception_state.ThrowRangeError(
        "size is larger than the remaining size of data after dataOffset.");
    return false;
  }

  *out = data.subspan(static_cast<size_t>(data_byte_offset),
                      static_cast<size_t>(data_byte_size));
  return true;
}

}  // namespace blink
