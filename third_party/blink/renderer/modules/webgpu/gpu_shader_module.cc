// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webgpu/gpu_shader_module.h"

#include "base/numerics/clamped_math.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_shader_module_descriptor.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_compilation_info.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_compilation_message.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/string_utils.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_callback.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_cpp.h"

namespace blink {

// static
GPUShaderModule* GPUShaderModule::Create(
    GPUDevice* device,
    const GPUShaderModuleDescriptor* webgpu_desc) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  wgpu::ShaderSourceWGSL wgsl_desc = {};
  const WTF::String& wtf_wgsl_code = webgpu_desc->code();
  std::string wgsl_code = wtf_wgsl_code.Utf8();
  wgsl_desc.code = wgsl_code.c_str();

  wgpu::ShaderModuleDescriptor dawn_desc = {};
  dawn_desc.nextInChain = &wgsl_desc;

  std::string label = webgpu_desc->label().Utf8();
  if (!label.empty()) {
    dawn_desc.label = label.c_str();
  }

  wgpu::ShaderModule shader_module;
  bool has_null_character = (wtf_wgsl_code.find('\0') != WTF::kNotFound);
  if (has_null_character) {
    shader_module = device->GetHandle().CreateErrorShaderModule(
        &dawn_desc, "The WGSL shader contains an illegal character '\\0'");
  } else {
    shader_module = device->GetHandle().CreateShaderModule(&dawn_desc);
  }

  GPUShaderModule* shader = MakeGarbageCollected<GPUShaderModule>(
      device, std::move(shader_module), webgpu_desc->label());

  // Very roughly approximate how much memory Tint might need for this shader.
  // Pessimizes if Tint actually holds less memory than this (including if the
  // shader module ends up being invalid).
  //
  // The actual estimate (100x code size) is chosen by profiling: large enough
  // to show some improvement in peak GPU process memory usage, small enough to
  // not slow down shader conformance tests (which are much, much heavier on
  // shader creation than normal workloads) more than a few percent.
  //
  // TODO(crbug.com/dawn/2367): Get a real memory estimate from Tint.
  base::ClampedNumeric<int32_t> input_code_size = wgsl_code.size();
  shader->tint_memory_estimate_.Set(v8::Isolate::GetCurrent(),
                                    input_code_size * 100);

  return shader;
}

GPUShaderModule::GPUShaderModule(GPUDevice* device,
                                 wgpu::ShaderModule shader_module,
                                 const String& label)
    : DawnObject<wgpu::ShaderModule>(device, std::move(shader_module), label) {}

void GPUShaderModule::OnCompilationInfoCallback(
    ScriptPromiseResolver<GPUCompilationInfo>* resolver,
    wgpu::CompilationInfoRequestStatus status,
    const wgpu::CompilationInfo* info) {
  if (status != wgpu::CompilationInfoRequestStatus::Success || !info) {
    const char* message = nullptr;
    switch (status) {
      case wgpu::CompilationInfoRequestStatus::Success:
        NOTREACHED_IN_MIGRATION();
        break;
      case wgpu::CompilationInfoRequestStatus::Error:
        message = "Unexpected error in getCompilationInfo";
        break;
      case wgpu::CompilationInfoRequestStatus::DeviceLost:
        message =
            "Device lost during getCompilationInfo (do not use this error for "
            "recovery - it is NOT guaranteed to happen on device loss)";
        break;
      case wgpu::CompilationInfoRequestStatus::InstanceDropped:
        message = "Instance dropped error in getCompilationInfo";
        break;
      case wgpu::CompilationInfoRequestStatus::Unknown:
        message = "Unknown failure in getCompilationInfo";
        break;
    }
    resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                     message);
    return;
  }

  // Temporarily immediately create the CompilationInfo info and resolve the
  // promise.
  GPUCompilationInfo* result = MakeGarbageCollected<GPUCompilationInfo>();
  for (uint32_t i = 0; i < info->messageCount; ++i) {
    const wgpu::CompilationMessage* message = &info->messages[i];
    result->AppendMessage(MakeGarbageCollected<GPUCompilationMessage>(
        StringFromASCIIAndUTF8(message->message), message->type,
        message->lineNum, message->utf16LinePos, message->utf16Offset,
        message->utf16Length));
  }

  resolver->Resolve(result);
}

GPUShaderModule::~GPUShaderModule() {
  tint_memory_estimate_.Clear(v8::Isolate::GetCurrent());
}

ScriptPromise<GPUCompilationInfo> GPUShaderModule::getCompilationInfo(
    ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<GPUCompilationInfo>>(
          script_state);
  auto promise = resolver->Promise();

  auto* callback =
      MakeWGPUOnceCallback(resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          &GPUShaderModule::OnCompilationInfoCallback, WrapPersistent(this))));

  GetHandle().GetCompilationInfo(wgpu::CallbackMode::AllowSpontaneous,
                                 callback->UnboundCallback(),
                                 callback->AsUserdata());
  // WebGPU guarantees that promises are resolved in finite time so we
  // need to ensure commands are flushed.
  EnsureFlush(ToEventLoop(script_state));
  return promise;
}

}  // namespace blink
