// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_PROGRAMMABLE_PASS_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_PROGRAMMABLE_PASS_ENCODER_H_

#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

class GPUProgrammablePassEncoder {
 protected:
  static bool ValidateSetBindGroupDynamicOffsets(
      base::span<const uint32_t> dynamic_offsets_data,
      uint64_t dynamic_offsets_data_start,
      uint32_t dynamic_offsets_data_length,
      ExceptionState& exception_state);

  static bool ValidateSetImmediatesAndSubSpan(
      ExceptionState& exception_state,
      base::span<const uint8_t>* out,
      uint32_t range_byte_offset,
      base::span<const uint8_t> data,
      uint32_t data_bytes_per_element,
      uint64_t data_element_offset,
      std::optional<uint64_t> data_element_size = std::nullopt);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_PROGRAMMABLE_PASS_ENCODER_H_
