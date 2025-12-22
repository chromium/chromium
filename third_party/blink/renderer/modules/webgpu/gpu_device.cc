// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

#include "base/task/bind_post_task.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_compute_pipeline_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_device_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_error_filter.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_feature_name.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_query_set_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_queue_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_render_pipeline_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_uncaptured_error_event_init.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter_info.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_command_encoder.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_compute_pipeline.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device_lost_info.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_external_texture.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_internal_error.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_out_of_memory_error.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_pipeline_error.h"
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
#include "third_party/blink/renderer/modules/webgpu/string_utils.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

namespace blink {

namespace {

std::optional<V8GPUFeatureName::Enum> RequiredFeatureForTextureFormat(
    V8GPUTextureFormat::Enum format) {
  switch (format) {
    case V8GPUTextureFormat::Enum::kBc1RgbaUnorm:
    case V8GPUTextureFormat::Enum::kBc1RgbaUnormSrgb:
    case V8GPUTextureFormat::Enum::kBc2RgbaUnorm:
    case V8GPUTextureFormat::Enum::kBc2RgbaUnormSrgb:
    case V8GPUTextureFormat::Enum::kBc3RgbaUnorm:
    case V8GPUTextureFormat::Enum::kBc3RgbaUnormSrgb:
    case V8GPUTextureFormat::Enum::kBc4RUnorm:
    case V8GPUTextureFormat::Enum::kBc4RSnorm:
    case V8GPUTextureFormat::Enum::kBc5RgUnorm:
    case V8GPUTextureFormat::Enum::kBc5RgSnorm:
    case V8GPUTextureFormat::Enum::kBc6HRgbUfloat:
    case V8GPUTextureFormat::Enum::kBc6HRgbFloat:
    case V8GPUTextureFormat::Enum::kBc7RgbaUnorm:
    case V8GPUTextureFormat::Enum::kBc7RgbaUnormSrgb:
      return V8GPUFeatureName::Enum::kTextureCompressionBc;

    case V8GPUTextureFormat::Enum::kEtc2Rgb8Unorm:
    case V8GPUTextureFormat::Enum::kEtc2Rgb8UnormSrgb:
    case V8GPUTextureFormat::Enum::kEtc2Rgb8A1Unorm:
    case V8GPUTextureFormat::Enum::kEtc2Rgb8A1UnormSrgb:
    case V8GPUTextureFormat::Enum::kEtc2Rgba8Unorm:
    case V8GPUTextureFormat::Enum::kEtc2Rgba8UnormSrgb:
    case V8GPUTextureFormat::Enum::kEacR11Unorm:
    case V8GPUTextureFormat::Enum::kEacR11Snorm:
    case V8GPUTextureFormat::Enum::kEacRg11Unorm:
    case V8GPUTextureFormat::Enum::kEacRg11Snorm:
      return V8GPUFeatureName::Enum::kTextureCompressionEtc2;

    case V8GPUTextureFormat::Enum::kAstc4X4Unorm:
    case V8GPUTextureFormat::Enum::kAstc4X4UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc5X4Unorm:
    case V8GPUTextureFormat::Enum::kAstc5X4UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc5X5Unorm:
    case V8GPUTextureFormat::Enum::kAstc5X5UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc6X5Unorm:
    case V8GPUTextureFormat::Enum::kAstc6X5UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc6X6Unorm:
    case V8GPUTextureFormat::Enum::kAstc6X6UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc8X5Unorm:
    case V8GPUTextureFormat::Enum::kAstc8X5UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc8X6Unorm:
    case V8GPUTextureFormat::Enum::kAstc8X6UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc8X8Unorm:
    case V8GPUTextureFormat::Enum::kAstc8X8UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc10X5Unorm:
    case V8GPUTextureFormat::Enum::kAstc10X5UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc10X6Unorm:
    case V8GPUTextureFormat::Enum::kAstc10X6UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc10X8Unorm:
    case V8GPUTextureFormat::Enum::kAstc10X8UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc10X10Unorm:
    case V8GPUTextureFormat::Enum::kAstc10X10UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc12X10Unorm:
    case V8GPUTextureFormat::Enum::kAstc12X10UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc12X12Unorm:
    case V8GPUTextureFormat::Enum::kAstc12X12UnormSrgb:
      return V8GPUFeatureName::Enum::kTextureCompressionAstc;

    case V8GPUTextureFormat::Enum::kDepth32FloatStencil8:
      return V8GPUFeatureName::Enum::kDepth32FloatStencil8;

    case V8GPUTextureFormat::Enum::kR16Unorm:
    case V8GPUTextureFormat::Enum::kRg16Unorm:
    case V8GPUTextureFormat::Enum::kRgba16Unorm:
    case V8GPUTextureFormat::Enum::kR16Snorm:
    case V8GPUTextureFormat::Enum::kRg16Snorm:
    case V8GPUTextureFormat::Enum::kRgba16Snorm:
      return V8GPUFeatureName::Enum::kTextureFormatsTier1;

    default:
      return std::nullopt;
  }
}

std::optional<V8GPUFeatureName::Enum> RequiredFeatureForBlendFactor(
    V8GPUBlendFactor::Enum blend_factor) {
  switch (blend_factor) {
    case V8GPUBlendFactor::Enum::kSrc1:
    case V8GPUBlendFactor::Enum::kOneMinusSrc1:
    case V8GPUBlendFactor::Enum::kSrc1Alpha:
    case V8GPUBlendFactor::Enum::kOneMinusSrc1Alpha:
      return V8GPUFeatureName::Enum::kDualSourceBlending;
    default:
      return std::nullopt;
  }
}

}  // anonymous namespace

GPUDevice::GPUDevice(ExecutionContext* execution_context,
                     scoped_refptr<DawnControlClientHolder> dawn_control_client,
                     GPUAdapter* adapter,
                     const String& label)
    : ExecutionContextClient(execution_context),
      DawnObject(dawn_control_client, label),
      adapter_(adapter),
      lost_property_(MakeGarbageCollected<LostProperty>(execution_context)),
      logging_callback_(BindWGPURepeatingCallback(&GPUDevice::OnLogging,
                                                  WrapWeakPersistent(this))) {}

void GPUDevice::Initialize(wgpu::Device handle,
                           const GPUDeviceDescriptor* descriptor,
                           GPUDeviceLostInfo* lost_info) {
  SetHandle(std::move(handle));

  wgpu::SupportedFeatures features;
  GetHandle().GetFeatures(&features);
  features_ = MakeGarbageCollected<GPUSupportedFeatures>(features);

  queue_ = MakeGarbageCollected<GPUQueue>(this, GetHandle().GetQueue(),
                                          descriptor->defaultQueue()->label());

  GPUSupportedLimits::ComboLimits limits;
  GetHandle().GetLimits(limits.GetLinked());
  limits_ = MakeGarbageCollected<GPUSupportedLimits>(limits);

  adapter_info_ = adapter_->CreateAdapterInfoForAdapter();

  GetHandle().SetLoggingCallback(logging_callback_->UnboundCallback(),
                                 logging_callback_->AsUserdata());

  external_texture_cache_ = MakeGarbageCollected<ExternalTextureCache>(this);

  // If lost_info is supplied it means the device should be treated as being
  // lost at creation time.
  if (lost_info) {
    lost_property_->Resolve(lost_info);
  }
}

GPUDevice::~GPUDevice() {
  // Perform destruction that's safe to do inside a GC (as in it doesn't touch
  // other GC objects).

  // Clear the callbacks since we can't handle callbacks after finalization.
  if (GetHandle().Get() != nullptr) {
    GetHandle().SetLoggingCallback([](wgpu::LoggingType, wgpu::StringView) {});
  }
}

bool GPUDevice::IsDestroyed() const {
  return destroyed_;
}

std::string GPUDevice::GetFormattedLabel() const {
  std::string deviceLabel =
      label().empty() ? "[Device]" : "[Device \"" + label().Utf8() + "\"]";

  return deviceLabel;
}

void GPUDevice::InjectError(wgpu::ErrorType type, const char* message) {
  GetHandle().InjectError(type, message);
}

void GPUDevice::AddConsoleWarning(wgpu::StringView message) {
  AddConsoleWarning(StringFromASCIIAndUTF8(message));
}

void GPUDevice::AddConsoleWarning(const char* message) {
  AddConsoleWarning(StringFromASCIIAndUTF8(message));
}

void GPUDevice::AddConsoleWarning(const String& message) {
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

void GPUDevice::AddSingletonWarning(GPUSingletonWarning type) {
  size_t index = static_cast<size_t>(type);
  if (!singleton_warning_fired_[index]) [[unlikely]] {
    singleton_warning_fired_[index] = true;

    String message;
    switch (type) {
      case GPUSingletonWarning::kNonPreferredFormat:
        message = StrCat(
            {"WebGPU canvas configured with a different format than is "
             "preferred by this device (\"",
             FromDawnEnum(GPU::GetPreferredCanvasFormat()).AsStringView(),
             "\"). This requires an extra copy, which may impact "
             "performance."});
        break;
      case GPUSingletonWarning::kDepthKey:
        message =
            "The key \"depth\" was included in a GPUExtent3D dictionary, which "
            "has no effect. It is likely that \"depthOrArrayLayers\" was "
            "intended instead.";
        break;
      case GPUSingletonWarning::kCount:
        NOTREACHED();
    }

    ExecutionContext* execution_context = GetExecutionContext();
    if (execution_context) {
      auto* console_message = MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kRendering,
          mojom::blink::ConsoleMessageLevel::kWarning, message);
      execution_context->AddConsoleMessage(console_message);
    }
  }
}

// Validates that any features required for the given texture format are enabled
// for this device. If not, throw a TypeError to ensure consistency with
// browsers that haven't yet implemented the feature.
bool GPUDevice::ValidateTextureFormatUsage(V8GPUTextureFormat format,
                                           ExceptionState& exception_state) {
  auto requiredFeatureOptional =
      RequiredFeatureForTextureFormat(format.AsEnum());

  if (!requiredFeatureOptional) {
    return true;
  }

  V8GPUFeatureName::Enum requiredFeatureEnum = requiredFeatureOptional.value();

  if (features_->Has(requiredFeatureEnum)) {
    return true;
  }

  V8GPUFeatureName requiredFeature = V8GPUFeatureName(requiredFeatureEnum);

  exception_state.ThrowTypeError(String::Format(
      "Use of the '%s' texture format requires the '%s' feature "
      "to be enabled on %s.",
      format.AsCStr(), requiredFeature.AsCStr(), GetFormattedLabel().c_str()));
  return false;
}

// Validates that any features required for the given blend factor are enabled
// for this device. If not, throw a TypeError to ensure consistency with
// browsers that haven't yet implemented the feature.
bool GPUDevice::ValidateBlendFactor(V8GPUBlendFactor blend_factor,
                                    ExceptionState& exception_state) {
  auto requiredFeatureOptional =
      RequiredFeatureForBlendFactor(blend_factor.AsEnum());

  if (!requiredFeatureOptional) {
    return true;
  }

  V8GPUFeatureName::Enum requiredFeatureEnum = requiredFeatureOptional.value();

  if (features_->Has(requiredFeatureEnum)) {
    return true;
  }

  V8GPUFeatureName requiredFeature = V8GPUFeatureName(requiredFeatureEnum);

  exception_state.ThrowTypeError(
      String::Format("Use of the '%s' blend factor requires the '%s' feature "
                     "to be enabled on %s.",
                     blend_factor.AsCStr(), requiredFeature.AsCStr(),
                     GetFormattedLabel().c_str()));
  return false;
}

void GPUDevice::OnUncapturedError(const wgpu::Device&,
                                  wgpu::ErrorType errorType,
                                  wgpu::StringView message) {
  OnUncapturedErrorImpl(errorType, StringFromASCIIAndUTF8(message));
}

void GPUDevice::OnUncapturedErrorImpl(wgpu::ErrorType errorType,
                                      const String& message) {
  // Suppress errors once the device is lost.
  if (lost_property_->GetState() == LostProperty::kResolved) {
    return;
  }

  DCHECK_NE(errorType, wgpu::ErrorType::NoError);
  LOG(ERROR) << "GPUDevice: " << message;

  GPUUncapturedErrorEventInit* init = GPUUncapturedErrorEventInit::Create();
  if (errorType == wgpu::ErrorType::Validation) {
    init->setError(MakeGarbageCollected<GPUValidationError>(message));
  } else if (errorType == wgpu::ErrorType::OutOfMemory) {
    init->setError(MakeGarbageCollected<GPUOutOfMemoryError>(message));
  } else if (errorType == wgpu::ErrorType::Internal) {
    init->setError(MakeGarbageCollected<GPUInternalError>(message));
  } else {
    return;
  }

  GPUUncapturedErrorEvent* event =
      GPUUncapturedErrorEvent::Create(event_type_names::kUncapturederror, init);
  DispatchEvent(*event);
  if (!event->defaultPrevented()) {
    AddConsoleWarning(message);
  }
}

void GPUDevice::OnLogging(wgpu::LoggingType loggingType,
                          wgpu::StringView message) {
  std::string_view messageView = {message.data, message.length};
  // Callback function for WebGPU logging return command
  mojom::blink::ConsoleMessageLevel level;
  switch (loggingType) {
    case (wgpu::LoggingType::Verbose): {
      level = mojom::blink::ConsoleMessageLevel::kVerbose;
      break;
    }
    case (wgpu::LoggingType::Info): {
      level = mojom::blink::ConsoleMessageLevel::kInfo;
      break;
    }
    case (wgpu::LoggingType::Warning): {
      level = mojom::blink::ConsoleMessageLevel::kWarning;
      break;
    }
    case (wgpu::LoggingType::Error): {
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
        mojom::blink::ConsoleMessageSource::kRendering, level,
        StringFromASCIIAndUTF8(messageView));
    execution_context->AddConsoleMessage(console_message);
  }
}

void GPUDevice::OnDeviceLost(
    std::unique_ptr<WGPURepeatingCallback<wgpu::UncapturedErrorCallback<void>>>,
    const wgpu::Device& device,
    wgpu::DeviceLostReason reason,
    wgpu::StringView message) {
  // Early-out if the context is being destroyed (see WrapCallbackInScriptScope)
  if (!GetExecutionContext()) {
    return;
  }

  if (reason != wgpu::DeviceLostReason::Destroyed) {
    AddConsoleWarning(message);
  }

  if (lost_property_->GetState() == LostProperty::kPending) {
    auto* device_lost_info = MakeGarbageCollected<GPUDeviceLostInfo>(
        reason, StringFromASCIIAndUTF8(message));
    lost_property_->Resolve(device_lost_info);
  }
}

void GPUDevice::OnCreateRenderPipelineAsyncCallback(
    const String& label,
    ScriptPromiseResolver<GPURenderPipeline>* resolver,
    wgpu::CreatePipelineAsyncStatus status,
    wgpu::RenderPipeline render_pipeline,
    wgpu::StringView message) {
  ScriptState* script_state = resolver->GetScriptState();
  switch (status) {
    case wgpu::CreatePipelineAsyncStatus::Success: {
      GPURenderPipeline* pipeline = MakeGarbageCollected<GPURenderPipeline>(
          this, std::move(render_pipeline), label);
      resolver->Resolve(pipeline);
      break;
    }

    case wgpu::CreatePipelineAsyncStatus::ValidationError: {
      resolver->Reject(GPUPipelineError::Create(
          script_state->GetIsolate(), StringFromASCIIAndUTF8(message),
          V8GPUPipelineErrorReason::Enum::kValidation));
      break;
    }

    case wgpu::CreatePipelineAsyncStatus::InternalError:
    case wgpu::CreatePipelineAsyncStatus::CallbackCancelled: {
      resolver->Reject(GPUPipelineError::Create(
          script_state->GetIsolate(), StringFromASCIIAndUTF8(message),
          V8GPUPipelineErrorReason::Enum::kInternal));
      break;
    }
  }
}

void GPUDevice::OnCreateComputePipelineAsyncCallback(
    const String& label,
    ScriptPromiseResolver<GPUComputePipeline>* resolver,
    wgpu::CreatePipelineAsyncStatus status,
    wgpu::ComputePipeline compute_pipeline,
    wgpu::StringView message) {
  ScriptState* script_state = resolver->GetScriptState();
  switch (status) {
    case wgpu::CreatePipelineAsyncStatus::Success: {
      GPUComputePipeline* pipeline = MakeGarbageCollected<GPUComputePipeline>(
          this, std::move(compute_pipeline), label);
      resolver->Resolve(pipeline);
      break;
    }

    case wgpu::CreatePipelineAsyncStatus::ValidationError: {
      resolver->Reject(GPUPipelineError::Create(
          script_state->GetIsolate(), StringFromASCIIAndUTF8(message),
          V8GPUPipelineErrorReason::Enum::kValidation));
      break;
    }

    case wgpu::CreatePipelineAsyncStatus::InternalError:
    case wgpu::CreatePipelineAsyncStatus::CallbackCancelled: {
      resolver->Reject(GPUPipelineError::Create(
          script_state->GetIsolate(), StringFromASCIIAndUTF8(message),
          V8GPUPipelineErrorReason::Enum::kInternal));
      break;
    }
  }
}

GPUAdapter* GPUDevice::adapter() const {
  return adapter_.Get();
}

GPUSupportedFeatures* GPUDevice::features() const {
  return features_.Get();
}

GPUAdapterInfo* GPUDevice::adapterInfo() const {
  return adapter_info_.Get();
}

ScriptPromise<GPUDeviceLostInfo> GPUDevice::lost(ScriptState* script_state) {
  return lost_property_->Promise(script_state->World());
}

GPUQueue* GPUDevice::queue() {
  return queue_.Get();
}

void GPUDevice::destroy(v8::Isolate* isolate) {
  destroyed_ = true;
  external_texture_cache_->Destroy();
  // Dissociate mailboxes before destroying the device. This ensures that
  // mailbox operations which run during dissociation can succeed.
  DissociateMailboxes();
  UnmapAllMappableBuffers(isolate);
  GetHandle().Destroy();
  FlushNow();
}

GPUBuffer* GPUDevice::createBuffer(const GPUBufferDescriptor* descriptor,
                                   ExceptionState& exception_state) {
  return GPUBuffer::Create(this, descriptor, exception_state);
}

GPUTexture* GPUDevice::createTexture(const GPUTextureDescriptor* descriptor,
                                     ExceptionState& exception_state) {
  return GPUTexture::Create(this, descriptor, exception_state);
}

GPUSampler* GPUDevice::createSampler(const GPUSamplerDescriptor* descriptor) {
  return GPUSampler::Create(this, descriptor);
}

GPUExternalTexture* GPUDevice::importExternalTexture(
    const GPUExternalTextureDescriptor* descriptor,
    ExceptionState& exception_state) {
  return external_texture_cache_->Import(descriptor, exception_state);
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
    const GPUShaderModuleDescriptor* descriptor) {
  return GPUShaderModule::Create(this, descriptor);
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

ScriptPromise<GPURenderPipeline> GPUDevice::createRenderPipelineAsync(
    ScriptState* script_state,
    const GPURenderPipelineDescriptor* descriptor,
    ExceptionState& exception_state) {
  OwnedRenderPipelineDescriptor dawn_desc_info;
  ConvertToDawnType(script_state->GetIsolate(), this, descriptor,
                    &dawn_desc_info, exception_state);
  if (exception_state.HadException()) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<GPURenderPipeline>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  auto* callback = MakeWGPUOnceCallback(resolver->WrapCallbackInScriptScope(
      BindOnce(&GPUDevice::OnCreateRenderPipelineAsyncCallback,
               WrapPersistent(this), descriptor->label())));
  GetHandle().CreateRenderPipelineAsync(
      &dawn_desc_info.dawn_desc, wgpu::CallbackMode::AllowProcessEvents,
      callback->UnboundCallback(), callback->AsUserdata());

  // WebGPU guarantees that promises are resolved in finite time so we need to
  // ensure commands are flushed.
  EnsureFlush(ToEventLoop(script_state));
  return promise;
}

ScriptPromise<GPUComputePipeline> GPUDevice::createComputePipelineAsync(
    ScriptState* script_state,
    const GPUComputePipelineDescriptor* descriptor) {
  std::string desc_label;
  OwnedProgrammableStage computeStage;
  wgpu::ComputePipelineDescriptor dawn_desc =
      AsDawnType(this, descriptor, &desc_label, &computeStage);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<GPUComputePipeline>>(
          script_state);
  auto promise = resolver->Promise();
  auto* callback = MakeWGPUOnceCallback(resolver->WrapCallbackInScriptScope(
      BindOnce(&GPUDevice::OnCreateComputePipelineAsyncCallback,
               WrapPersistent(this), descriptor->label())));
  GetHandle().CreateComputePipelineAsync(
      &dawn_desc, wgpu::CallbackMode::AllowProcessEvents,
      callback->UnboundCallback(), callback->AsUserdata());

  // WebGPU guarantees that promises are resolved in finite time so we need to
  // ensure commands are flushed.
  EnsureFlush(ToEventLoop(script_state));
  return promise;
}

GPUCommandEncoder* GPUDevice::createCommandEncoder(
    const GPUCommandEncoderDescriptor* descriptor) {
  return GPUCommandEncoder::Create(this, descriptor);
}

GPURenderBundleEncoder* GPUDevice::createRenderBundleEncoder(
    const GPURenderBundleEncoderDescriptor* descriptor,
    ExceptionState& exception_state) {
  return GPURenderBundleEncoder::Create(this, descriptor, exception_state);
}

GPUQuerySet* GPUDevice::createQuerySet(const GPUQuerySetDescriptor* descriptor,
                                       ExceptionState& exception_state) {
  const V8GPUFeatureName::Enum kTimestampQuery =
      V8GPUFeatureName::Enum::kTimestampQuery;
  const V8GPUFeatureName::Enum kTimestampQueryInsidePasses =
      V8GPUFeatureName::Enum::kChromiumExperimentalTimestampQueryInsidePasses;
  if (descriptor->type() == V8GPUQueryType::Enum::kTimestamp &&
      !features_->Has(kTimestampQuery) &&
      !features_->Has(kTimestampQueryInsidePasses)) {
    exception_state.ThrowTypeError(
        String::Format("Use of timestamp queries requires the '%s' or '%s' "
                       "feature to be enabled on %s.",
                       V8GPUFeatureName(kTimestampQuery).AsCStr(),
                       V8GPUFeatureName(kTimestampQueryInsidePasses).AsCStr(),
                       GetFormattedLabel().c_str()));
    return nullptr;
  }
  return GPUQuerySet::Create(this, descriptor);
}

void GPUDevice::pushErrorScope(const V8GPUErrorFilter& filter) {
  GetHandle().PushErrorScope(AsDawnEnum(filter));
}

ScriptPromise<IDLNullable<GPUError>> GPUDevice::popErrorScope(
    ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<GPUError>>>(
          script_state);
  auto promise = resolver->Promise();

  auto* callback = MakeWGPUOnceCallback(resolver->WrapCallbackInScriptScope(
      BindOnce(&GPUDevice::OnPopErrorScopeCallback, WrapPersistent(this))));
  GetHandle().PopErrorScope(wgpu::CallbackMode::AllowProcessEvents,
                            callback->UnboundCallback(),
                            callback->AsUserdata());

  // WebGPU guarantees that promises are resolved in finite time so we
  // need to ensure commands are flushed.
  EnsureFlush(ToEventLoop(script_state));
  return promise;
}

void GPUDevice::OnPopErrorScopeCallback(
    ScriptPromiseResolver<IDLNullable<GPUError>>* resolver,
    wgpu::PopErrorScopeStatus status,
    wgpu::ErrorType type,
    wgpu::StringView message) {
  switch (status) {
    case wgpu::PopErrorScopeStatus::CallbackCancelled:
      resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                       "Instance dropped in popErrorScope");
      return;
    case wgpu::PopErrorScopeStatus::Success:
      break;
    case wgpu::PopErrorScopeStatus::Error:
      resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                       StringFromASCIIAndUTF8(message));
      return;
  }
  switch (type) {
    case wgpu::ErrorType::NoError:
      resolver->Resolve(nullptr);
      break;
    case wgpu::ErrorType::OutOfMemory:
      resolver->Resolve(MakeGarbageCollected<GPUOutOfMemoryError>(
          StringFromASCIIAndUTF8(message)));
      break;
    case wgpu::ErrorType::Validation:
      resolver->Resolve(MakeGarbageCollected<GPUValidationError>(
          StringFromASCIIAndUTF8(message)));
      break;
    case wgpu::ErrorType::Internal:
      resolver->Resolve(MakeGarbageCollected<GPUInternalError>(
          StringFromASCIIAndUTF8(message)));
      break;
    case wgpu::ErrorType::Unknown:
      resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                       "Unknown failure in popErrorScope");
      break;
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
  visitor->Trace(adapter_info_);
  visitor->Trace(queue_);
  visitor->Trace(lost_property_);
  visitor->Trace(external_texture_cache_);
  visitor->Trace(textures_with_mailbox_);
  visitor->Trace(buffers_with_mailbox_);
  visitor->Trace(mappable_buffers_);
  ExecutionContextClient::Trace(visitor);
  EventTarget::Trace(visitor);
}

void GPUDevice::Dispose() {
  // This call accesses other GC objects, so it cannot be called inside GC
  // objects destructors. Instead call it in the pre-finalizer.
  if (external_texture_cache_ != nullptr) {
    external_texture_cache_->Destroy();
  }
}

void GPUDevice::DissociateMailboxes() {
  for (auto& texture : textures_with_mailbox_) {
    texture->DissociateMailbox();
  }
  textures_with_mailbox_.clear();

  for (auto& buffer : buffers_with_mailbox_) {
    buffer->DissociateMailbox();
  }
  buffers_with_mailbox_.clear();
}

void GPUDevice::UnmapAllMappableBuffers(v8::Isolate* isolate) {
  for (GPUBuffer* buffer : mappable_buffers_) {
    buffer->unmap(isolate);
  }
}

void GPUDevice::TrackMappableBuffer(GPUBuffer* buffer) {
  mappable_buffers_.insert(buffer);
}

void GPUDevice::UntrackMappableBuffer(GPUBuffer* buffer) {
  mappable_buffers_.erase(buffer);
}

void GPUDevice::TrackTextureWithMailbox(GPUTexture* texture) {
  DCHECK(texture);
  textures_with_mailbox_.insert(texture);
}

void GPUDevice::UntrackTextureWithMailbox(GPUTexture* texture) {
  DCHECK(texture);
  textures_with_mailbox_.erase(texture);
}

void GPUDevice::TrackBufferWithMailbox(GPUBuffer* buffer) {
  DCHECK(buffer);
  buffers_with_mailbox_.insert(buffer);
}

void GPUDevice::UntrackBufferWithMailbox(GPUBuffer* buffer) {
  DCHECK(buffer);
  buffers_with_mailbox_.erase(buffer);
}

void GPUDevice::SetDescriptorCallbacks(wgpu::DeviceDescriptor& dawn_desc) {
  ExecutionContext* execution_context = GetExecutionContext();

  // Set the uncaptured error callback first because its ownership will be
  // passed to the device lost callback immediately after. The uncaptured error
  // callback happens spontaneously which means it may happen on the IO thread
  // in some cases (i.e. potentially on workers). To handle this, we check
  // whether we are being handled off the main thread, if so, we proxy to the
  // main thread.
  std::unique_ptr<WGPURepeatingCallback<wgpu::UncapturedErrorCallback<void>>>
      error_callback;
  if (IsWebGPUMultithreadedWorker(execution_context)) {
    error_callback.reset(MakeWGPURepeatingCallback(
        base::BindPostTask(execution_context->GetTaskRunner(TaskType::kWebGPU),
                           BindRepeating(&GPUDevice::OnUncapturedError,
                                         WrapWeakPersistent(this)))));
  } else {
    error_callback.reset(MakeWGPURepeatingCallback(BindRepeating(
        &GPUDevice::OnUncapturedError, WrapWeakPersistent(this))));
  }
  dawn_desc.SetUncapturedErrorCallback(error_callback->UnboundCallback(),
                                       error_callback->AsUserdata());

  auto* lost_callback = MakeWGPUOnceCallback(
      BindOnce(&GPUDevice::OnDeviceLost, WrapWeakPersistent(this),
               std::move(error_callback)));
  dawn_desc.SetDeviceLostCallback(wgpu::CallbackMode::AllowProcessEvents,
                                  lost_callback->UnboundCallback(),
                                  lost_callback->AsUserdata());
}
}  // namespace blink
