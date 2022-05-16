// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"

#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_device_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_request_adapter_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webgpu/gpu.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_features.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_limits.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {
WGPUDeviceProperties AsDawnType(const GPUDeviceDescriptor* descriptor) {
  DCHECK_NE(nullptr, descriptor);

  auto&& feature_names = descriptor->requiredFeatures();

  HashSet<String> feature_set;
  for (auto& feature : feature_names)
    feature_set.insert(feature);

  WGPUDeviceProperties requested_device_properties = {};
  // TODO(crbug.com/1048603): We should validate that the feature_set is a
  // subset of the adapter's feature set.
  requested_device_properties.textureCompressionBC =
      feature_set.Contains("texture-compression-bc");
  requested_device_properties.textureCompressionETC2 =
      feature_set.Contains("texture-compression-etc2");
  requested_device_properties.textureCompressionASTC =
      feature_set.Contains("texture-compression-astc");
  requested_device_properties.shaderFloat16 =
      feature_set.Contains("shader-float16");
  requested_device_properties.pipelineStatisticsQuery =
      feature_set.Contains("pipeline-statistics-query");
  requested_device_properties.timestampQuery =
      feature_set.Contains("timestamp-query");
  requested_device_properties.depthClamping =
      feature_set.Contains("depth-clamping");
  requested_device_properties.depth24UnormStencil8 =
      feature_set.Contains("depth24unorm-stencil8");
  requested_device_properties.depth32FloatStencil8 =
      feature_set.Contains("depth32float-stencil8");

  return requested_device_properties;
}

}  // anonymous namespace

GPUAdapter::GPUAdapter(
    GPU* gpu,
    const String& name,
    uint32_t adapter_service_id,
    const WGPUDeviceProperties& properties,
    scoped_refptr<DawnControlClientHolder> dawn_control_client)
    : DawnObjectBase(dawn_control_client),
      name_(name),
      adapter_service_id_(adapter_service_id),
      adapter_properties_(properties),
      gpu_(gpu),
      limits_(MakeGarbageCollected<GPUSupportedLimits>(properties.limits)) {
  InitializeFeatureNameList();
}

void GPUAdapter::AddConsoleWarning(ExecutionContext* execution_context,
                                   const char* message) {
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
  return adapter_properties_.adapterType == WGPUAdapterType_CPU;
}

void GPUAdapter::OnRequestDeviceCallback(ScriptState* script_state,
                                         ScriptPromiseResolver* resolver,
                                         const GPUDeviceDescriptor* descriptor,
                                         WGPUDevice dawn_device,
                                         const WGPUSupportedLimits* limits,
                                         const char* error_message) {
  if (dawn_device) {
    ExecutionContext* execution_context = ExecutionContext::From(script_state);
    auto* device = MakeGarbageCollected<GPUDevice>(
        execution_context, GetDawnControlClient(), this, dawn_device, limits,
        descriptor);
    resolver->Resolve(device);
    ukm::builders::ClientRenderingAPI(execution_context->UkmSourceID())
        .SetGPUDevice(static_cast<int>(true))
        .Record(execution_context->UkmRecorder());
  } else {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kOperationError, error_message));
  }
}

void GPUAdapter::InitializeFeatureNameList() {
  features_ = MakeGarbageCollected<GPUSupportedFeatures>();
  DCHECK(features_->FeatureNameSet().IsEmpty());
  if (adapter_properties_.textureCompressionBC) {
    features_->AddFeatureName("texture-compression-bc");
  }
  if (adapter_properties_.textureCompressionETC2) {
    features_->AddFeatureName("texture-compression-etc2");
  }
  if (adapter_properties_.textureCompressionASTC) {
    features_->AddFeatureName("texture-compression-astc");
  }
  if (adapter_properties_.shaderFloat16) {
    features_->AddFeatureName("shader-float16");
  }
  if (adapter_properties_.pipelineStatisticsQuery) {
    features_->AddFeatureName("pipeline-statistics-query");
  }
  if (adapter_properties_.timestampQuery) {
    features_->AddFeatureName("timestamp-query");
  }
  if (adapter_properties_.depthClamping) {
    features_->AddFeatureName("depth-clamping");
  }
  if (adapter_properties_.depth24UnormStencil8) {
    features_->AddFeatureName("depth24unorm-stencil8");
  }
  if (adapter_properties_.depth32FloatStencil8) {
    features_->AddFeatureName("depth32float-stencil8");
  }
  if (adapter_properties_.multiPlanarFormats) {
    features_->AddFeatureName("multi-planar-formats");
  }
}

ScriptPromise GPUAdapter::requestDevice(ScriptState* script_state,
                                        GPUDeviceDescriptor* descriptor) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  WGPUDeviceProperties requested_device_properties = AsDawnType(descriptor);
  GPUSupportedLimits::MakeUndefined(&requested_device_properties.limits);
  if (descriptor->hasRequiredLimits()) {
    DOMException* exception = GPUSupportedLimits::Populate(
        &requested_device_properties.limits, descriptor->requiredLimits());
    if (exception) {
      resolver->Reject(exception);
      return promise;
    }
  }

  if (auto context_provider = GetContextProviderWeakPtr()) {
    context_provider->ContextProvider()->WebGPUInterface()->RequestDeviceAsync(
        adapter_service_id_, requested_device_properties,
        WTF::Bind(&GPUAdapter::OnRequestDeviceCallback, WrapPersistent(this),
                  WrapPersistent(script_state), WrapPersistent(resolver),
                  WrapPersistent(descriptor)));
  } else {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kOperationError, "WebGPU context lost"));
  }

  return promise;
}

void GPUAdapter::Trace(Visitor* visitor) const {
  visitor->Trace(gpu_);
  visitor->Trace(features_);
  visitor->Trace(limits_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
