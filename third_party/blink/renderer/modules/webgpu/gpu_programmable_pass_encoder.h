// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_PROGRAMMABLE_PASS_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_PROGRAMMABLE_PASS_ENCODER_H_

#include "third_party/blink/renderer/core/typed_arrays/typed_flexible_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

class GPUProgrammablePassEncoder {
 protected:
  bool ValidateSetBindGroupDynamicOffsets(
      const FlexibleUint32Array& dynamic_offsets_data,
      uint64_t dynamic_offsets_data_start,
      uint32_t dynamic_offsets_data_length,
      ExceptionState& exception_state);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_PROGRAMMABLE_PASS_ENCODER_H_
