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

}  // namespace blink
