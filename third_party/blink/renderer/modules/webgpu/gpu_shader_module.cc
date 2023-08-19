// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_shader_module.h"

#include <dawn/webgpu.h>

#include "base/command_line.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "gpu/config/gpu_switches.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_usvstring_uint32array.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_shader_module_descriptor.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_compilation_info.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_compilation_message.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/string_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_callback.h"

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

  const auto* wgsl_or_spirv = webgpu_desc->code();
  bool has_null_character = false;
  switch (wgsl_or_spirv->GetContentType()) {
    case V8UnionUSVStringOrUint32Array::ContentType::kUSVString: {
      WTF::String wtf_wgsl_code(wgsl_or_spirv->GetAsUSVString());
      wgsl_code = wtf_wgsl_code.Utf8();
      wgsl_desc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
      wgsl_desc.code = wgsl_code.c_str();
      dawn_desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgsl_desc);
      if (wtf_wgsl_code.find('\0') != WTF::kNotFound) {
        has_null_character = true;
      }

      break;
    }
    case V8UnionUSVStringOrUint32Array::ContentType::kUint32Array: {
      if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableUnsafeWebGPU)) {
        exception_state.ThrowTypeError(
            "SPIR-V shader module creation is disallowed. This feature "
            "requires --enable-unsafe-webgpu");
        return nullptr;
      }
      NotShared<DOMUint32Array> code = wgsl_or_spirv->GetAsUint32Array();
      uint32_t length_words = 0;
      if (!base::CheckedNumeric<uint32_t>(code->length())
               .AssignIfValid(&length_words)) {
        exception_state.ThrowRangeError(
            "The provided ArrayBuffer exceeds the maximum supported size "
            "(4294967295)");
        return nullptr;
      }
      spirv_desc.chain.sType = WGPUSType_ShaderModuleSPIRVDescriptor;
      spirv_desc.code = code->Data();
      spirv_desc.codeSize = length_words;
      dawn_desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&spirv_desc);
      break;
    }
  }

  if (webgpu_desc->hasLabel()) {
    label = webgpu_desc->label().Utf8();
    dawn_desc.label = label.c_str();
  }

  WGPUShaderModule shader_module;
  if (has_null_character) {
    shader_module = device->GetProcs().deviceCreateErrorShaderModule(
        device->GetHandle(), &dawn_desc,
        "The WGSL shader contains an illegal character '\\0'");
  } else {
    shader_module = device->GetProcs().deviceCreateShaderModule(
        device->GetHandle(), &dawn_desc);
  }
  GPUShaderModule* shader =
      MakeGarbageCollected<GPUShaderModule>(device, shader_module);
  if (webgpu_desc->hasLabel())
    shader->setLabel(webgpu_desc->label());
  return shader;
}

GPUShaderModule::GPUShaderModule(GPUDevice* device,
                                 WGPUShaderModule shader_module)
    : DawnObject<WGPUShaderModule>(device, shader_module) {}

void GPUShaderModule::OnCompilationInfoCallback(
    ScriptPromiseResolver* resolver,
    WGPUCompilationInfoRequestStatus status,
    const WGPUCompilationInfo* info) {
  if (status != WGPUCompilationInfoRequestStatus_Success || !info) {
    resolver->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kOperationError));
    return;
  }

  // Temporarily immediately create the CompilationInfo info and resolve the
  // promise.
  GPUCompilationInfo* result = MakeGarbageCollected<GPUCompilationInfo>();
  for (uint32_t i = 0; i < info->messageCount; ++i) {
    const WGPUCompilationMessage* message = &info->messages[i];
    result->AppendMessage(MakeGarbageCollected<GPUCompilationMessage>(
        StringFromASCIIAndUTF8(message->message), message->type,
        message->lineNum, message->utf16LinePos, message->utf16Offset,
        message->utf16Length));
  }

  resolver->Resolve(result);
}

ScriptPromise GPUShaderModule::getCompilationInfo(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  auto* callback =
      BindWGPUOnceCallback(&GPUShaderModule::OnCompilationInfoCallback,
                           WrapPersistent(this), WrapPersistent(resolver));

  GetProcs().shaderModuleGetCompilationInfo(
      GetHandle(), callback->UnboundCallback(), callback->AsUserdata());
  // WebGPU guarantees that promises are resolved in finite time so we
  // need to ensure commands are flushed.
  EnsureFlush(ToEventLoop(script_state));
  return promise;
}

}  // namespace blink
