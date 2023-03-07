// Copyright 2019 The Chromium Authors
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

  GPUShaderModule(const GPUShaderModule&) = delete;
  GPUShaderModule& operator=(const GPUShaderModule&) = delete;

  ScriptPromise getCompilationInfo(ScriptState* script_state);

 private:
  void OnCompilationInfoCallback(ScriptPromiseResolver* resolver,
                                 WGPUCompilationInfoRequestStatus status,
                                 const WGPUCompilationInfo* info);

  void setLabelImpl(const String& value) override {
    std::string utf8_label = value.Utf8();
    GetProcs().shaderModuleSetLabel(GetHandle(), utf8_label.c_str());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SHADER_MODULE_H_
