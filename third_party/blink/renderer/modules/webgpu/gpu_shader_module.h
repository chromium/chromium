// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SHADER_MODULE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SHADER_MODULE_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

#include <dawn/webgpu.h>

namespace blink {

class GPUShaderModuleDescriptor;
class ExceptionState;
class ScriptPromise;
class ScriptPromiseResolver;

class GPUShaderModule : public DawnObject<WGPUShaderModule> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUShaderModule* Create(GPUDevice* device,
                                 const GPUShaderModuleDescriptor* webgpu_desc,
                                 ExceptionState& exception_state);
  explicit GPUShaderModule(GPUDevice* device, WGPUShaderModule shader_module);

  ScriptPromise compilationInfo(ScriptState* script_state);

 private:
  void OnCompilationInfoCallback(ScriptPromiseResolver* resolver,
                                 WGPUCompilationInfoRequestStatus status,
                                 const WGPUCompilationInfo* info);

  DISALLOW_COPY_AND_ASSIGN(GPUShaderModule);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SHADER_MODULE_H_
