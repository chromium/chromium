// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_programmable_stage.h"
#include "third_party/blink/renderer/modules/webgpu/string_utils.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_programmable_stage.h"

namespace blink {

void GPUProgrammableStageAsWGPUProgrammableStage(
    const GPUProgrammableStage* descriptor,
    OwnedProgrammableStage* dawn_programmable_stage) {
  DCHECK(descriptor);
  DCHECK(dawn_programmable_stage);

  if (descriptor->hasEntryPoint()) {
    dawn_programmable_stage->entry_point =
        UTF8StringFromUSVStringWithNullReplacedByReplacementCodePoint(
            descriptor->entryPoint());
  }

  if (!descriptor->hasConstants()) {
    return;
  }

  const auto& constants = descriptor->constants();
  dawn_programmable_stage->constantCount = constants.size();
  dawn_programmable_stage->constantKeys =
      std::make_unique<std::string[]>(constants.size());
  dawn_programmable_stage->constants =
      std::make_unique<wgpu::ConstantEntry[]>(constants.size());
  for (wtf_size_t i = 0; i < constants.size(); i++) {
    dawn_programmable_stage->constantKeys[i] =
        UTF8StringFromUSVStringWithNullReplacedByReplacementCodePoint(
            constants[i].first);
    dawn_programmable_stage->constants[i].key =
        dawn_programmable_stage->constantKeys[i].c_str();
    dawn_programmable_stage->constants[i].value = constants[i].second;
  }
}

}  // namespace blink
