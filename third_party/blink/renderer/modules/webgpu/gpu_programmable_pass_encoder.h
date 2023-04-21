// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_PROGRAMMABLE_PASS_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_PROGRAMMABLE_PASS_ENCODER_H_

#include "third_party/blink/renderer/core/typed_arrays/typed_flexible_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/no_alloc_direct_call_host.h"

namespace blink {

class GPUProgrammablePassEncoder : public NoAllocDirectCallHost {
 protected:
  bool ValidateSetBindGroupDynamicOffsets(
      const FlexibleUint32Array& dynamic_offsets_data,
      uint64_t dynamic_offsets_data_start,
      uint32_t dynamic_offsets_data_length,
      ExceptionState& exception_state);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_PROGRAMMABLE_PASS_ENCODER_H_
