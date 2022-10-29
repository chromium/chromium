// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"

#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_device_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_request_adapter_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_enum_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter_info.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_features.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_limits.h"
#include "third_party/blink/renderer/modules/webgpu/string_utils.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

GPUSupportedFeatures* MakeFeatureNameSet(const DawnProcTable& procs,
                                         WGPUAdapter adapter) {
  GPUSupportedFeatures* features = MakeGarbageCollected<GPUSupportedFeatures>();
  DCHECK(features->FeatureNameSet().empty());

  size_t feature_count = procs.adapterEnumerateFeatures(adapter, nullptr);
  DCHECK(feature_count <= std::numeric_limits<wtf_size_t>::max());

  Vector<WGPUFeatureName> feature_names(static_cast<wtf_size_t>(feature_count));
  procs.adapterEnumerateFeatures(adapter, feature_names.data());

  for (WGPUFeatureName f : feature_names) {
    switch (f) {
      case WGPUFeatureName_Depth32FloatStencil8:
        features->AddFeatureName("depth32float-stencil8");
        break;
      case WGPUFeatureName_TimestampQuery:
        features->AddFeatureName("timestamp-query");
        break;
      case WGPUFeatureName_TimestampQueryInsidePasses:
        features->AddFeatureName("timestamp-query-inside-passes");
        break;
      case WGPUFeatureName_PipelineStatisticsQuery:
        features->AddFeatureName("pipeline-statistics-query");
        break;
      case WGPUFeatureName_TextureCompressionBC:
        features->AddFeatureName("texture-compression-bc");
        break;
      case WGPUFeatureName_TextureCompressionETC2:
        features->AddFeatureName("texture-compression-etc2");
        break;
      case WGPUFeatureName_TextureCompressionASTC:
        features->AddFeatureName("texture-compression-astc");
        break;
      case WGPUFeatureName_IndirectFirstInstance:
        features->AddFeatureName("indirect-first-instance");
        break;
      case WGPUFeatureName_DepthClipControl:
        features->AddFeatureName("depth-clip-control");
        break;
      case WGPUFeatureName_DawnShaderFloat16:
        features->AddFeatureName("shader-float16");
        break;
      case WGPUFeatureName_DawnMultiPlanarFormats:
        features->AddFeatureName("multi-planar-formats");
        break;
      case WGPUFeatureName_RG11B10UfloatRenderable:
        features->AddFeatureName("rg11b10ufloat-renderable");
        break;
      default:
        break;
    }
  }
  return features;
}

}  // anonymous namespace

GPUAdapter::GPUAdapter(
    GPU* gpu,
    const String& name,
    WGPUAdapter handle,
    scoped_refptr<DawnControlClientHolder> dawn_control_client)
    : DawnObjectBase(dawn_control_client),
      name_(name),
      handle_(handle),
      gpu_(gpu) {
  WGPUAdapterProperties properties = {};
  GetProcs().adapterGetProperties(handle_, &properties);
  is_fallback_adapter_ = properties.adapterType == WGPUAdapterType_CPU;

  vendor_ = properties.vendorName;
  architecture_ = properties.architecture;
  if (properties.deviceID <= 0xffff) {
    device_ = String::Format("0x%04x", properties.deviceID);
  } else {
    device_ = String::Format("0x%08x", properties.deviceID);
  }
  description_ = properties.name;
  driver_ = properties.driverDescription;

  WGPUSupportedLimits limits = {};
  GetProcs().adapterGetLimits(handle_, &limits);
  limits_ = MakeGarbageCollected<GPUSupportedLimits>(limits);

  features_ = MakeFeatureNameSet(GetProcs(), handle_);
}

void GPUAdapter::AddConsoleWarning(ExecutionContext* execution_context,
                                   const char* message) {
  if (execution_context && allowed_console_warnings_remaining_ > 0) {
    auto* console_message = MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning,
        StringFromASCIIAndUTF8(message));
    execution_context->AddConsoleMessage(console_message);

    allowed_console_warnings_remaining_--;
    if (allowed_console_warnings_remaining_ == 0) {
      auto* final_message = MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kRendering,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "WebGPU: too many warnings, no more warnings will be reported to the "
          "console for this GPUAdapter.");
      execution_context->AddConsoleMessage(final_message);
    }
  }
}

const String& GPUAdapter::name() const {
  return name_;
}

GPUSupportedFeatures* GPUAdapter::features() const {
  return features_;
}

bool GPUAdapter::isFallbackAdapter() const {
  return is_fallback_adapter_;
}

void GPUAdapter::OnRequestDeviceCallback(ScriptState* script_state,
                                         ScriptPromiseResolver* resolver,
                                         const GPUDeviceDescriptor* descriptor,
                                         WGPURequestDeviceStatus status,
                                         WGPUDevice dawn_device,
                                         const char* error_message) {
  switch (status) {
    case WGPURequestDeviceStatus_Success: {
      DCHECK(dawn_device);
      ExecutionContext* execution_context =
          ExecutionContext::From(script_state);

      auto* device = MakeGarbageCollected<GPUDevice>(
          execution_context, GetDawnControlClient(), this, dawn_device,
          descriptor);
      if (is_invalid_) {
        GetProcs().deviceForceLoss(
            device->GetHandle(), WGPUDeviceLostReason_Undefined,
            "Cannot request device on invalidated adapter.");
        FlushNow();
      }
      resolver->Resolve(device);

      ukm::builders::ClientRenderingAPI(execution_context->UkmSourceID())
          .SetGPUDevice(static_cast<int>(true))
          .Record(execution_context->UkmRecorder());
      break;
    }

    case WGPURequestDeviceStatus_Error:
    case WGPURequestDeviceStatus_Unknown:
      DCHECK_EQ(dawn_device, nullptr);
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError,
          StringFromASCIIAndUTF8(error_message)));
      break;
    default:
      NOTREACHED();
  }
}

ScriptPromise GPUAdapter::requestDevice(ScriptState* script_state,
                                        GPUDeviceDescriptor* descriptor) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state,
      ExceptionContext(ExceptionContext::Context::kOperationInvoke,
                       "GPUAdapter", "requestDevice"));
  ScriptPromise promise = resolver->Promise();

  WGPUDeviceDescriptor dawn_desc = {};

  WGPURequiredLimits required_limits = {};
  if (descriptor->hasRequiredLimits()) {
    dawn_desc.requiredLimits = &required_limits;
    GPUSupportedLimits::MakeUndefined(&required_limits);
    DOMException* exception = GPUSupportedLimits::Populate(
        &required_limits, descriptor->requiredLimits());
    if (exception) {
      resolver->Reject(exception);
      return promise;
    }
  }

  Vector<WGPUFeatureName> required_features;
  if (descriptor->hasRequiredFeatures()) {
    // Insert features into a set to dedup them.
    HashSet<WGPUFeatureName> required_features_set;
    for (const V8GPUFeatureName& f : descriptor->requiredFeatures()) {
      // If the feature is not a valid feature reject with a type error.
      if (!features_->has(f.AsString())) {
        resolver->RejectWithTypeError(
            String::Format("Unsupported feature: %s", f.AsCStr()));
        return promise;
      }
      required_features_set.insert(AsDawnEnum(f));
    }

    // Then, push the deduped features into a vector.
    required_features.AppendRange(required_features_set.begin(),
                                  required_features_set.end());
    dawn_desc.requiredFeatures = required_features.data();
    dawn_desc.requiredFeaturesCount = required_features.size();
  }

  auto* callback = BindWGPUOnceCallback(
      &GPUAdapter::OnRequestDeviceCallback, WrapPersistent(this),
      WrapPersistent(script_state), WrapPersistent(resolver),
      WrapPersistent(descriptor));

  GetProcs().adapterRequestDevice(
      handle_, &dawn_desc, callback->UnboundCallback(), callback->AsUserdata());
  EnsureFlush(ToEventLoop(script_state));

  return promise;
}

ScriptPromise GPUAdapter::requestAdapterInfo(
    ScriptState* script_state,
    const Vector<String>& unmask_hints) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // If any unmask hints have been given, the method must also have been called
  // during user activation. If not, reject the promise.
  if (unmask_hints.size()) {
    LocalDOMWindow* domWindow = gpu_->DomWindow();
    if (!domWindow ||
        !LocalFrame::HasTransientUserActivation(domWindow->GetFrame())) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "requestAdapterInfo requires user activation if any unmaskHints are "
          "given."));
      return promise;
    }
  }

  GPUAdapterInfo* adapter_info;
  if (RuntimeEnabledFeatures::WebGPUDeveloperFeaturesEnabled()) {
    // If WebGPU developer features have been enabled then provide unmasked
    // versions of all available adapter info values, including some that are
    // only available when the flag is enabled.
    adapter_info = MakeGarbageCollected<GPUAdapterInfo>(
        vendor_, architecture_, device_, description_, driver_);
  } else {
    // TODO(dawn:1427): If unmask_hints are given ask the user for consent to
    // expose more information and, if given, include device_ and description_
    // in the returned GPUAdapterInfo.
    adapter_info = MakeGarbageCollected<GPUAdapterInfo>(vendor_, architecture_);
  }

  resolver->Resolve(adapter_info);

  return promise;
}

void GPUAdapter::Trace(Visitor* visitor) const {
  visitor->Trace(gpu_);
  visitor->Trace(features_);
  visitor->Trace(limits_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
