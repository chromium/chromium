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
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

namespace {
WGPUDeviceProperties AsDawnType(const GPUDeviceDescriptor* descriptor) {
  DCHECK_NE(nullptr, descriptor);

  const Vector<String>& feature_names = descriptor->hasExtensions()
                                            ? descriptor->extensions()
                                            :  // Deprecated path
                                            descriptor->nonGuaranteedFeatures();

  HashSet<String> feature_set;
  for (auto& feature : feature_names)
    feature_set.insert(feature);

  WGPUDeviceProperties requested_device_properties = {};
  // TODO(crbug.com/1048603): We should validate that the feature_set is a
  // subset of the adapter's feature set.
  requested_device_properties.textureCompressionBC =
      feature_set.Contains("texture-compression-bc");
  requested_device_properties.shaderFloat16 =
      feature_set.Contains("shader-float16");
  requested_device_properties.pipelineStatisticsQuery =
      feature_set.Contains("pipeline-statistics-query");
  requested_device_properties.timestampQuery =
      feature_set.Contains("timestamp-query");

  return requested_device_properties;
}
}  // anonymous namespace

GPUAdapter::GPUAdapter(
    const String& name,
    uint32_t adapter_service_id,
    const WGPUDeviceProperties& properties,
    scoped_refptr<DawnControlClientHolder> dawn_control_client)
    : DawnObjectBase(dawn_control_client),
      name_(name),
      adapter_service_id_(adapter_service_id),
      adapter_properties_(properties) {
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

Vector<String> GPUAdapter::features() const {
  return feature_name_list_;
}

Vector<String> GPUAdapter::extensions(ExecutionContext* execution_context) {
  AddConsoleWarning(
      execution_context,
      "The extensions attribute has been deprecated in favor of the features "
      "attribute, and will soon be removed.");
  return feature_name_list_;
}

void GPUAdapter::OnRequestDeviceCallback(ScriptPromiseResolver* resolver,
                                         const GPUDeviceDescriptor* descriptor,
                                         WGPUDevice dawn_device) {
  if (dawn_device) {
    ExecutionContext* execution_context = resolver->GetExecutionContext();
    auto* device = MakeGarbageCollected<GPUDevice>(execution_context,
                                                   GetDawnControlClient(), this,
                                                   dawn_device, descriptor);
    resolver->Resolve(device);
    ukm::builders::ClientRenderingAPI(execution_context->UkmSourceID())
        .SetGPUDevice(static_cast<int>(true))
        .Record(execution_context->UkmRecorder());
  } else {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kOperationError,
        "Fail to request GPUDevice with the given GPUDeviceDescriptor"));
  }
}

void GPUAdapter::InitializeFeatureNameList() {
  DCHECK(feature_name_list_.IsEmpty());
  if (adapter_properties_.textureCompressionBC) {
    feature_name_list_.emplace_back("texture-compression-bc");
  }
  if (adapter_properties_.shaderFloat16) {
    feature_name_list_.emplace_back("shader-float16");
  }
  if (adapter_properties_.pipelineStatisticsQuery) {
    feature_name_list_.emplace_back("pipeline-statistics-query");
  }
  if (adapter_properties_.timestampQuery) {
    feature_name_list_.emplace_back("timestamp-query");
  }
}

ScriptPromise GPUAdapter::requestDevice(ScriptState* script_state,
                                        GPUDeviceDescriptor* descriptor) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (descriptor->hasExtensions()) {
    AddConsoleWarning(
        ExecutionContext::From(script_state),
        "Specifying extensions when requesting a GPUDevice is deprecated in "
        "favor of specifying nonGuaranteedFeatures, and will soon be removed.");
    descriptor->setNonGuaranteedFeatures(descriptor->extensions());
  }

  WGPUDeviceProperties requested_device_properties = AsDawnType(descriptor);

  GetInterface()->RequestDeviceAsync(
      adapter_service_id_, requested_device_properties,
      WTF::Bind(&GPUAdapter::OnRequestDeviceCallback, WrapPersistent(this),
                WrapPersistent(resolver), WrapPersistent(descriptor)));

  return promise;
}

}  // namespace blink
