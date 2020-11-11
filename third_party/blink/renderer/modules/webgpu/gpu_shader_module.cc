// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_shader_module.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_shader_module_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

// static
GPUShaderModule* GPUShaderModule::Create(
    GPUDevice* device,
    const GPUShaderModuleDescriptor* webgpu_desc,
    ExceptionState& exception_state) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  std::string wgsl_code;
  WGPUShaderModuleWGSLDescriptor wgsl_desc = {};
  WGPUShaderModuleSPIRVDescriptor spirv_desc = {};
  std::string label;
  WGPUShaderModuleDescriptor dawn_desc = {};

  auto wgsl_or_spirv = webgpu_desc->code();
  if (wgsl_or_spirv.IsUSVString()) {
    wgsl_code = wgsl_or_spirv.GetAsUSVString().Utf8();

    wgsl_desc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgsl_desc.source = wgsl_code.c_str();
    dawn_desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgsl_desc);
  } else {
    DCHECK(wgsl_or_spirv.IsUint32Array());
    NotShared<DOMUint32Array> code = wgsl_or_spirv.GetAsUint32Array();

    uint32_t length_words = 0;
    if (!base::CheckedNumeric<uint32_t>(code.View()->length())
             .AssignIfValid(&length_words)) {
      exception_state.ThrowRangeError(
          "The provided ArrayBuffer exceeds the maximum supported size "
          "(4294967295)");
      return nullptr;
    }

    spirv_desc.chain.sType = WGPUSType_ShaderModuleSPIRVDescriptor;
    spirv_desc.code = code.View()->Data();
    spirv_desc.codeSize = length_words;
    dawn_desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&spirv_desc);
  }

  if (webgpu_desc->hasLabel()) {
    label = webgpu_desc->label().Utf8();
    dawn_desc.label = label.c_str();
  }

  return MakeGarbageCollected<GPUShaderModule>(
      device, device->GetProcs().deviceCreateShaderModule(device->GetHandle(),
                                                          &dawn_desc));
}

GPUShaderModule::GPUShaderModule(GPUDevice* device,
                                 WGPUShaderModule shader_module)
    : DawnObject<WGPUShaderModule>(device, shader_module) {}

GPUShaderModule::~GPUShaderModule() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().shaderModuleRelease(GetHandle());
}

}  // namespace blink
