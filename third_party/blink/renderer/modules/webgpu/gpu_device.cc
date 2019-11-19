// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_command_encoder.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_compute_pipeline.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device_lost_info.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_pipeline_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_queue.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_bundle_encoder.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_pipeline.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_sampler.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_shader_module.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_uncaptured_error_event.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_uncaptured_error_event_init.h"

namespace blink {

// static
GPUDevice* GPUDevice::Create(
    ExecutionContext* execution_context,
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    GPUAdapter* adapter,
    const GPUDeviceDescriptor* descriptor) {
  return MakeGarbageCollected<GPUDevice>(
      execution_context, std::move(dawn_control_client), adapter, descriptor);
}

// TODO(enga): Handle adapter options and device descriptor
GPUDevice::GPUDevice(ExecutionContext* execution_context,
                     scoped_refptr<DawnControlClientHolder> dawn_control_client,
                     GPUAdapter* adapter,
                     const GPUDeviceDescriptor* descriptor)
    : ContextClient(execution_context),
      DawnObject(dawn_control_client,
                 dawn_control_client->GetInterface()->GetDefaultDevice()),
      adapter_(adapter),
      queue_(GPUQueue::Create(this, GetProcs().deviceCreateQueue(GetHandle()))),
      lost_property_(MakeGarbageCollected<LostProperty>(execution_context,
                                                        this,
                                                        LostProperty::kLost)),
      error_callback_(
          BindRepeatingDawnCallback(&GPUDevice::OnUncapturedError,
                                    WrapWeakPersistent(this),
                                    WrapWeakPersistent(execution_context))) {
  GetProcs().deviceSetUncapturedErrorCallback(
      GetHandle(), error_callback_->UnboundRepeatingCallback(),
      error_callback_->AsUserdata());
}

GPUDevice::~GPUDevice() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().deviceRelease(GetHandle());
}

void GPUDevice::OnUncapturedError(ExecutionContext* execution_context,
                                  WGPUErrorType errorType,
                                  const char* message) {
  if (execution_context) {
    DCHECK_NE(errorType, WGPUErrorType_NoError);
    LOG(ERROR) << "GPUDevice: " << message;
    ConsoleMessage* console_message =
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kRendering,
                               mojom::ConsoleMessageLevel::kWarning, message);
    execution_context->AddConsoleMessage(console_message);
  }

  // TODO: Use device lost callback instead of uncaptured error callback.
  if (errorType == WGPUErrorType_DeviceLost &&
      lost_property_->GetState() == ScriptPromisePropertyBase::kPending) {
    GPUDeviceLostInfo* device_lost_info = GPUDeviceLostInfo::Create(message);
    lost_property_->Resolve(device_lost_info);
  }

  GPUUncapturedErrorEventInit* init = GPUUncapturedErrorEventInit::Create();
  if (errorType == WGPUErrorType_Validation) {
    GPUValidationError* error = GPUValidationError::Create(message);
    init->setError(
        GPUOutOfMemoryErrorOrGPUValidationError::FromGPUValidationError(error));
  } else if (errorType == WGPUErrorType_OutOfMemory) {
    GPUOutOfMemoryError* error = GPUOutOfMemoryError::Create();
    init->setError(
        GPUOutOfMemoryErrorOrGPUValidationError::FromGPUOutOfMemoryError(
            error));
  } else {
    return;
  }
  this->DispatchEvent(*GPUUncapturedErrorEvent::Create(
      event_type_names::kUncapturederror, init));
}

GPUAdapter* GPUDevice::adapter() const {
  return adapter_;
}

ScriptPromise GPUDevice::lost(ScriptState* script_state) {
  return lost_property_->Promise(script_state->World());
}

GPUQueue* GPUDevice::defaultQueue() {
  return queue_;
}

GPUBuffer* GPUDevice::createBuffer(const GPUBufferDescriptor* descriptor) {
  return GPUBuffer::Create(this, descriptor);
}

HeapVector<ScriptValue> GPUDevice::createBufferMapped(
    ScriptState* script_state,
    const GPUBufferDescriptor* descriptor,
    ExceptionState& exception_state) {
  GPUBuffer* gpu_buffer;
  DOMArrayBuffer* array_buffer;
  std::tie(gpu_buffer, array_buffer) =
      GPUBuffer::CreateMapped(this, descriptor, exception_state);

  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Object> creation_context = script_state->GetContext()->Global();

  return HeapVector<ScriptValue>({
      ScriptValue(isolate, ToV8(gpu_buffer, creation_context, isolate)),
      ScriptValue(isolate, ToV8(array_buffer, creation_context, isolate)),
  });
}

ScriptPromise GPUDevice::createBufferMappedAsync(
    ScriptState* script_state,
    const GPUBufferDescriptor* descriptor,
    ExceptionState& exception_state) {
  GPUBuffer* gpu_buffer;
  DOMArrayBuffer* array_buffer;
  std::tie(gpu_buffer, array_buffer) =
      GPUBuffer::CreateMapped(this, descriptor, exception_state);

  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Object> creation_context = script_state->GetContext()->Global();

  v8::Local<v8::Value> elements[] = {
      ToV8(gpu_buffer, creation_context, isolate),
      ToV8(array_buffer, creation_context, isolate),
  };

  ScriptValue result(isolate, v8::Array::New(isolate, elements, 2));

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  // TODO(enga): CreateBufferMappedAsync is intended to spend more time to
  // create an optimal mapping for the buffer. It resolves the promise when the
  // mapping is complete. Currently, there is always a staging buffer in the
  // wire so this is already the optimal path and the promise is immediately
  // resolved. When we can create a buffer such that the memory is mapped
  // directly in the renderer process, this promise should be resolved
  // asynchronously.

  if (exception_state.HadException()) {
    resolver->Reject(exception_state);
  } else {
    resolver->Resolve(result);
  }
  return promise;
}

GPUTexture* GPUDevice::createTexture(const GPUTextureDescriptor* descriptor,
                                     ExceptionState& exception_state) {
  return GPUTexture::Create(this, descriptor, exception_state);
}

GPUSampler* GPUDevice::createSampler(const GPUSamplerDescriptor* descriptor) {
  return GPUSampler::Create(this, descriptor);
}

GPUBindGroup* GPUDevice::createBindGroup(
    const GPUBindGroupDescriptor* descriptor) {
  return GPUBindGroup::Create(this, descriptor);
}

GPUBindGroupLayout* GPUDevice::createBindGroupLayout(
    const GPUBindGroupLayoutDescriptor* descriptor) {
  return GPUBindGroupLayout::Create(this, descriptor);
}

GPUPipelineLayout* GPUDevice::createPipelineLayout(
    const GPUPipelineLayoutDescriptor* descriptor) {
  return GPUPipelineLayout::Create(this, descriptor);
}

GPUShaderModule* GPUDevice::createShaderModule(
    const GPUShaderModuleDescriptor* descriptor) {
  return GPUShaderModule::Create(this, descriptor);
}

GPURenderPipeline* GPUDevice::createRenderPipeline(
    ScriptState* script_state,
    const GPURenderPipelineDescriptor* descriptor) {
  return GPURenderPipeline::Create(script_state, this, descriptor);
}

GPUComputePipeline* GPUDevice::createComputePipeline(
    const GPUComputePipelineDescriptor* descriptor) {
  return GPUComputePipeline::Create(this, descriptor);
}

GPUCommandEncoder* GPUDevice::createCommandEncoder(
    const GPUCommandEncoderDescriptor* descriptor) {
  return GPUCommandEncoder::Create(this, descriptor);
}

GPURenderBundleEncoder* GPUDevice::createRenderBundleEncoder(
    const GPURenderBundleEncoderDescriptor* descriptor) {
  return GPURenderBundleEncoder::Create(this, descriptor);
}

void GPUDevice::pushErrorScope(const WTF::String& filter) {
  GetProcs().devicePushErrorScope(GetHandle(),
                                  AsDawnEnum<WGPUErrorFilter>(filter));
}

ScriptPromise GPUDevice::popErrorScope(ScriptState* script_state) {
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  auto* callback =
      BindDawnCallback(&GPUDevice::OnPopErrorScopeCallback,
                       WrapPersistent(this), WrapPersistent(resolver));

  if (!GetProcs().devicePopErrorScope(GetHandle(), callback->UnboundCallback(),
                                      callback->AsUserdata())) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kOperationError, "No error scopes to pop."));
    delete callback;
    return promise;
  }

  // WebGPU guarantees callbacks complete in finite time. Flush now so that
  // commands reach the GPU process. TODO(enga): This should happen at the end
  // of the task.
  GetInterface()->FlushCommands();

  return promise;
}

void GPUDevice::OnPopErrorScopeCallback(ScriptPromiseResolver* resolver,
                                        WGPUErrorType type,
                                        const char* message) {
  v8::Isolate* isolate = resolver->GetScriptState()->GetIsolate();
  switch (type) {
    case WGPUErrorType_NoError:
      resolver->Resolve(v8::Null(isolate));
      break;
    case WGPUErrorType_OutOfMemory:
      resolver->Resolve(GPUOutOfMemoryError::Create());
      break;
    case WGPUErrorType_Validation:
      resolver->Resolve(GPUValidationError::Create(message));
      break;
    case WGPUErrorType_Unknown:
    case WGPUErrorType_DeviceLost:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError));
      break;
    default:
      NOTREACHED();
  }
}

ExecutionContext* GPUDevice::GetExecutionContext() const {
  return ContextClient::GetExecutionContext();
}

const AtomicString& GPUDevice::InterfaceName() const {
  return event_target_names::kGPUDevice;
}

void GPUDevice::Trace(blink::Visitor* visitor) {
  visitor->Trace(adapter_);
  visitor->Trace(queue_);
  visitor->Trace(lost_property_);
  ContextClient::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
