// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_compute_pipeline_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_device_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_external_texture_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_feature_name.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_render_pipeline_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_uncaptured_error_event_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_gpuoutofmemoryerror_gpuvalidationerror.h"
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
#include "third_party/blink/renderer/modules/webgpu/gpu_device_lost_info.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_external_texture.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_out_of_memory_error.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_pipeline_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_query_set.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_queue.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_bundle_encoder.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_pipeline.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_sampler.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_shader_module.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_features.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_limits.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_uncaptured_error_event.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_validation_error.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"

namespace blink {

namespace {

Vector<String> ToStringVector(const Vector<V8GPUFeatureName>& features) {
  Vector<String> str_features;
  for (auto&& feature : features)
    str_features.push_back(IDLEnumAsString(feature));
  return str_features;
}

}  // anonymous namespace

// TODO(enga): Handle adapter options and device descriptor
GPUDevice::GPUDevice(ExecutionContext* execution_context,
                     scoped_refptr<DawnControlClientHolder> dawn_control_client,
                     GPUAdapter* adapter,
                     WGPUDevice dawn_device,
                     const WGPUSupportedLimits* limits,
                     const GPUDeviceDescriptor* descriptor)
    : ExecutionContextClient(execution_context),
      DawnObject(dawn_control_client, dawn_device),
      adapter_(adapter),
      features_(MakeGarbageCollected<GPUSupportedFeatures>(
          ToStringVector(descriptor->requiredFeatures()))),
      queue_(MakeGarbageCollected<GPUQueue>(
          this,
          GetProcs().deviceGetQueue(GetHandle()))),
      lost_property_(MakeGarbageCollected<LostProperty>(execution_context)),
      error_callback_(BindDawnRepeatingCallback(&GPUDevice::OnUncapturedError,
                                                WrapWeakPersistent(this))),
      logging_callback_(BindDawnRepeatingCallback(&GPUDevice::OnLogging,
                                                  WrapWeakPersistent(this))),
      // Note: This is a *repeating* callback even though we expect it to only
      // be called once. This is because it may be called *zero* times.
      // Because it might never be called, the GPUDevice needs to own the
      // allocation so it can be appropriately freed on destruction. Thus, the
      // callback should not be a OnceCallback which self-deletes after it is
      // called.
      lost_callback_(BindDawnRepeatingCallback(&GPUDevice::OnDeviceLostError,
                                               WrapWeakPersistent(this))) {
  DCHECK(dawn_device);
  DCHECK(limits);
  limits_ = MakeGarbageCollected<GPUSupportedLimits>(*limits);

  GetProcs().deviceSetUncapturedErrorCallback(
      GetHandle(), error_callback_->UnboundCallback(),
      error_callback_->AsUserdata());
  GetProcs().deviceSetLoggingCallback(GetHandle(),
                                      logging_callback_->UnboundCallback(),
                                      logging_callback_->AsUserdata());
  GetProcs().deviceSetDeviceLostCallback(GetHandle(),
                                         lost_callback_->UnboundCallback(),
                                         lost_callback_->AsUserdata());

  if (descriptor->hasLabel())
    setLabel(descriptor->label());
}

GPUDevice::~GPUDevice() {
  // Clear the callbacks since we can't handle callbacks after finalization.
  // error_callback_, logging_callback_, and lost_callback_ will be deleted.
  GetProcs().deviceSetUncapturedErrorCallback(GetHandle(), nullptr, nullptr);
  GetProcs().deviceSetLoggingCallback(GetHandle(), nullptr, nullptr);
  GetProcs().deviceSetDeviceLostCallback(GetHandle(), nullptr, nullptr);
}

void GPUDevice::InjectError(WGPUErrorType type, const char* message) {
  GetProcs().deviceInjectError(GetHandle(), type, message);
}

void GPUDevice::AddConsoleWarning(const char* message) {
  ExecutionContext* execution_context = GetExecutionContext();
  if (execution_context && allowed_console_warnings_remaining_ > 0) {
    auto* console_message = MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning, message);
    execution_context->AddConsoleMessage(console_message);

    allowed_console_warnings_remaining_--;
    if (allowed_console_warnings_remaining_ == 0) {
      auto* final_message = MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kRendering,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "WebGPU: too many warnings, no more warnings will be reported to the "
          "console for this GPUDevice.");
      execution_context->AddConsoleMessage(final_message);
    }
  }
}

void GPUDevice::OnUncapturedError(WGPUErrorType errorType,
                                  const char* message) {
  DCHECK_NE(errorType, WGPUErrorType_NoError);
  DCHECK_NE(errorType, WGPUErrorType_DeviceLost);
  LOG(ERROR) << "GPUDevice: " << message;
  AddConsoleWarning(message);

  GPUUncapturedErrorEventInit* init = GPUUncapturedErrorEventInit::Create();
  if (errorType == WGPUErrorType_Validation) {
    init->setError(
        MakeGarbageCollected<V8UnionGPUOutOfMemoryErrorOrGPUValidationError>(
            MakeGarbageCollected<GPUValidationError>(message)));
  } else if (errorType == WGPUErrorType_OutOfMemory) {
    init->setError(
        MakeGarbageCollected<V8UnionGPUOutOfMemoryErrorOrGPUValidationError>(
            GPUOutOfMemoryError::Create()));
  } else {
    return;
  }
  DispatchEvent(*GPUUncapturedErrorEvent::Create(
      event_type_names::kUncapturederror, init));
}

void GPUDevice::OnLogging(WGPULoggingType loggingType, const char* message) {
  // Callback function for WebGPU logging return command
  mojom::blink::ConsoleMessageLevel level;
  switch (loggingType) {
    case (WGPULoggingType_Verbose): {
      level = mojom::blink::ConsoleMessageLevel::kVerbose;
      break;
    }
    case (WGPULoggingType_Info): {
      level = mojom::blink::ConsoleMessageLevel::kInfo;
      break;
    }
    case (WGPULoggingType_Warning): {
      level = mojom::blink::ConsoleMessageLevel::kWarning;
      break;
    }
    case (WGPULoggingType_Error): {
      level = mojom::blink::ConsoleMessageLevel::kError;
      break;
    }
    default: {
      level = mojom::blink::ConsoleMessageLevel::kError;
      break;
    }
  }
  ExecutionContext* execution_context = GetExecutionContext();
  if (execution_context) {
    auto* console_message = MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering, level, message);
    execution_context->AddConsoleMessage(console_message);
  }
}

void GPUDevice::OnDeviceLostError(WGPUDeviceLostReason reason,
                                  const char* message) {
  if (!GetExecutionContext())
    return;
  AddConsoleWarning(message);

  if (lost_property_->GetState() == LostProperty::kPending) {
    auto* device_lost_info =
        MakeGarbageCollected<GPUDeviceLostInfo>(reason, message);
    lost_property_->Resolve(device_lost_info);
  }
}

void GPUDevice::OnCreateRenderPipelineAsyncCallback(
    ScriptPromiseResolver* resolver,
    WGPUCreatePipelineAsyncStatus status,
    WGPURenderPipeline render_pipeline,
    const char* message) {
  switch (status) {
    case WGPUCreatePipelineAsyncStatus_Success: {
      resolver->Resolve(
          MakeGarbageCollected<GPURenderPipeline>(this, render_pipeline));
      break;
    }

    case WGPUCreatePipelineAsyncStatus_Error:
    case WGPUCreatePipelineAsyncStatus_DeviceLost:
    case WGPUCreatePipelineAsyncStatus_DeviceDestroyed:
    case WGPUCreatePipelineAsyncStatus_Unknown: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError, message));
      break;
    }

    default: {
      NOTREACHED();
    }
  }
}

void GPUDevice::OnCreateComputePipelineAsyncCallback(
    ScriptPromiseResolver* resolver,
    WGPUCreatePipelineAsyncStatus status,
    WGPUComputePipeline compute_pipeline,
    const char* message) {
  switch (status) {
    case WGPUCreatePipelineAsyncStatus_Success: {
      resolver->Resolve(
          MakeGarbageCollected<GPUComputePipeline>(this, compute_pipeline));
      break;
    }

    case WGPUCreatePipelineAsyncStatus_Error:
    case WGPUCreatePipelineAsyncStatus_DeviceLost:
    case WGPUCreatePipelineAsyncStatus_DeviceDestroyed:
    case WGPUCreatePipelineAsyncStatus_Unknown: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError, message));
      break;
    }

    default: {
      NOTREACHED();
    }
  }
}

GPUAdapter* GPUDevice::adapter() const {
  return adapter_;
}

GPUSupportedFeatures* GPUDevice::features() const {
  return features_;
}

ScriptPromise GPUDevice::lost(ScriptState* script_state) {
  return lost_property_->Promise(script_state->World());
}

GPUQueue* GPUDevice::queue() {
  return queue_;
}

void GPUDevice::destroy() {
  GetProcs().deviceDestroy(GetHandle());
  FlushNow();
}

GPUBuffer* GPUDevice::createBuffer(const GPUBufferDescriptor* descriptor) {
  return GPUBuffer::Create(this, descriptor);
}

GPUTexture* GPUDevice::createTexture(const GPUTextureDescriptor* descriptor,
                                     ExceptionState& exception_state) {
  return GPUTexture::Create(this, descriptor, exception_state);
}

GPUTexture* GPUDevice::experimentalImportTexture(
    HTMLCanvasElement* canvas,
    unsigned int usage_flags,
    ExceptionState& exception_state) {
  return GPUTexture::FromCanvas(this, canvas,
                                static_cast<WGPUTextureUsage>(usage_flags),
                                exception_state);
}

GPUSampler* GPUDevice::createSampler(const GPUSamplerDescriptor* descriptor) {
  return GPUSampler::Create(this, descriptor);
}

GPUExternalTexture* GPUDevice::importExternalTexture(
    const GPUExternalTextureDescriptor* descriptor,
    ExceptionState& exception_state) {
  GPUExternalTexture* externalTexture =
      GPUExternalTexture::Create(this, descriptor, exception_state);
  if (externalTexture)
    EnsureExternalTextureDestroyed(externalTexture);
  return externalTexture;
}

GPUBindGroup* GPUDevice::createBindGroup(
    const GPUBindGroupDescriptor* descriptor,
    ExceptionState& exception_state) {
  return GPUBindGroup::Create(this, descriptor, exception_state);
}

GPUBindGroupLayout* GPUDevice::createBindGroupLayout(
    const GPUBindGroupLayoutDescriptor* descriptor,
    ExceptionState& exception_state) {
  return GPUBindGroupLayout::Create(this, descriptor, exception_state);
}

GPUPipelineLayout* GPUDevice::createPipelineLayout(
    const GPUPipelineLayoutDescriptor* descriptor) {
  return GPUPipelineLayout::Create(this, descriptor);
}

GPUShaderModule* GPUDevice::createShaderModule(
    const GPUShaderModuleDescriptor* descriptor,
    ExceptionState& exception_state) {
  return GPUShaderModule::Create(this, descriptor, exception_state);
}

GPURenderPipeline* GPUDevice::createRenderPipeline(
    ScriptState* script_state,
    const GPURenderPipelineDescriptor* descriptor) {
  return GPURenderPipeline::Create(script_state, this, descriptor);
}

GPUComputePipeline* GPUDevice::createComputePipeline(
    const GPUComputePipelineDescriptor* descriptor,
    ExceptionState& exception_state) {
  return GPUComputePipeline::Create(this, descriptor);
}

ScriptPromise GPUDevice::createRenderPipelineAsync(
    ScriptState* script_state,
    const GPURenderPipelineDescriptor* descriptor) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  v8::Isolate* isolate = script_state->GetIsolate();
  ExceptionState exception_state(isolate, ExceptionState::kExecutionContext,
                                 "GPUDevice", "createRenderPipelineAsync");
  OwnedRenderPipelineDescriptor dawn_desc_info;
  ConvertToDawnType(isolate, this, descriptor, &dawn_desc_info,
                    exception_state);
  if (exception_state.HadException()) {
    resolver->Reject(exception_state);
  } else {
    auto* callback =
        BindDawnOnceCallback(&GPUDevice::OnCreateRenderPipelineAsyncCallback,
                             WrapPersistent(this), WrapPersistent(resolver));
    GetProcs().deviceCreateRenderPipelineAsync(
        GetHandle(), &dawn_desc_info.dawn_desc, callback->UnboundCallback(),
        callback->AsUserdata());
  }

  // WebGPU guarantees that promises are resolved in finite time so we need to
  // ensure commands are flushed.
  EnsureFlush();
  return promise;
}

ScriptPromise GPUDevice::createComputePipelineAsync(
    ScriptState* script_state,
    const GPUComputePipelineDescriptor* descriptor) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  std::string label;
  OwnedProgrammableStageDescriptor computeStageDescriptor;
  WGPUComputePipelineDescriptor dawn_desc =
      AsDawnType(descriptor, &label, &computeStageDescriptor, this);

  auto* callback =
      BindDawnOnceCallback(&GPUDevice::OnCreateComputePipelineAsyncCallback,
                           WrapPersistent(this), WrapPersistent(resolver));
  GetProcs().deviceCreateComputePipelineAsync(GetHandle(), &dawn_desc,
                                              callback->UnboundCallback(),
                                              callback->AsUserdata());
  // WebGPU guarantees that promises are resolved in finite time so we need to
  // ensure commands are flushed.
  EnsureFlush();
  return promise;
}

GPUCommandEncoder* GPUDevice::createCommandEncoder(
    const GPUCommandEncoderDescriptor* descriptor) {
  return GPUCommandEncoder::Create(this, descriptor);
}

GPURenderBundleEncoder* GPUDevice::createRenderBundleEncoder(
    const GPURenderBundleEncoderDescriptor* descriptor) {
  return GPURenderBundleEncoder::Create(this, descriptor);
}

GPUQuerySet* GPUDevice::createQuerySet(
    const GPUQuerySetDescriptor* descriptor) {
  return GPUQuerySet::Create(this, descriptor);
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
      BindDawnOnceCallback(&GPUDevice::OnPopErrorScopeCallback,
                           WrapPersistent(this), WrapPersistent(resolver));

  if (!GetProcs().devicePopErrorScope(GetHandle(), callback->UnboundCallback(),
                                      callback->AsUserdata())) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kOperationError, "No error scopes to pop."));
    delete callback;
    return promise;
  }

  // WebGPU guarantees that promises are resolved in finite time so we
  // need to ensure commands are flushed.
  EnsureFlush();
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
      resolver->Resolve(MakeGarbageCollected<GPUValidationError>(message));
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
  return ExecutionContextClient::GetExecutionContext();
}

const AtomicString& GPUDevice::InterfaceName() const {
  return event_target_names::kGPUDevice;
}

void GPUDevice::Trace(Visitor* visitor) const {
  visitor->Trace(adapter_);
  visitor->Trace(features_);
  visitor->Trace(limits_);
  visitor->Trace(queue_);
  visitor->Trace(lost_property_);
  visitor->Trace(external_textures_pending_destroy_);
  ExecutionContextClient::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

void GPUDevice::EnsureExternalTextureDestroyed(
    GPUExternalTexture* externalTexture) {
  DCHECK(externalTexture);
  external_textures_pending_destroy_.push_back(externalTexture);

  if (has_pending_microtask_)
    return;

  Microtask::EnqueueMicrotask(WTF::Bind(
      &GPUDevice::DestroyExternalTexturesMicrotask, WrapWeakPersistent(this)));
  has_pending_microtask_ = true;
}

void GPUDevice::DestroyExternalTexturesMicrotask() {
  has_pending_microtask_ = false;

  auto externalTextures = std::move(external_textures_pending_destroy_);
  for (Member<GPUExternalTexture> externalTexture : externalTextures) {
    externalTexture->Destroy();
  }
}

}  // namespace blink
