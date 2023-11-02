// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SHADER_STAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SHADER_STAGE_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_shader_stage.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class GPUShaderStage : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // gpu_shader_stage.idl
  static constexpr uint32_t kVertex = V8GPUShaderStage::Constant::kVertex;
  static constexpr uint32_t kFragment = V8GPUShaderStage::Constant::kFragment;
  static constexpr uint32_t kCompute = V8GPUShaderStage::Constant::kCompute;

  GPUShaderStage(const GPUShaderStage&) = delete;
  GPUShaderStage& operator=(const GPUShaderStage&) = delete;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SHADER_STAGE_H_
