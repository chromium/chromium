// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_PROGRAMMABLE_STAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_PROGRAMMABLE_STAGE_H_

#include <dawn/webgpu.h>
#include <memory>
#include <string>

namespace blink {

class GPUProgrammableStage;

struct OwnedProgrammableStage {
  OwnedProgrammableStage() = default;

  //  This struct should be non-copyable non-movable because it contains
  //  self-referencing pointers that would be invalidated when moved / copied.
  OwnedProgrammableStage(const OwnedProgrammableStage& desc) = delete;
  OwnedProgrammableStage(OwnedProgrammableStage&& desc) = delete;
  OwnedProgrammableStage& operator=(const OwnedProgrammableStage& desc) =
      delete;
  OwnedProgrammableStage& operator=(OwnedProgrammableStage&& desc) = delete;

  std::string entry_point;
  std::unique_ptr<std::string[]> constantKeys;
  std::unique_ptr<WGPUConstantEntry[]> constants;
  uint32_t constantCount = 0;
};

void GPUProgrammableStageAsWGPUProgrammableStage(
    const GPUProgrammableStage* descriptor,
    OwnedProgrammableStage* dawn_programmable_stage);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_PROGRAMMABLE_STAGE_H_
