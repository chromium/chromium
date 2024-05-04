// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"

#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_device_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_queue_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_request_adapter_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_enum_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter_info.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device_lost_info.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_memory_heap_info.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_features.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_limits.h"
#include "third_party/blink/renderer/modules/webgpu/string_utils.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

std::optional<V8GPUFeatureName::Enum> ToV8FeatureNameEnum(wgpu::FeatureName f) {
  switch (f) {
    case wgpu::FeatureName::Depth32FloatStencil8:
      return V8GPUFeatureName::Enum::kDepth32FloatStencil8;
    case wgpu::FeatureName::TimestampQuery:
      return V8GPUFeatureName::Enum::kTimestampQuery;
    case wgpu::FeatureName::ChromiumExperimentalTimestampQueryInsidePasses:
      return V8GPUFeatureName::Enum::
          kChromiumExperimentalTimestampQueryInsidePasses;
    case wgpu::FeatureName::TextureCompressionBC:
      return V8GPUFeatureName::Enum::kTextureCompressionBc;
    case wgpu::FeatureName::TextureCompressionETC2:
      return V8GPUFeatureName::Enum::kTextureCompressionEtc2;
    case wgpu::FeatureName::TextureCompressionASTC:
      return V8GPUFeatureName::Enum::kTextureCompressionAstc;
    case wgpu::FeatureName::IndirectFirstInstance:
      return V8GPUFeatureName::Enum::kIndirectFirstInstance;
    case wgpu::FeatureName::DepthClipControl:
      return V8GPUFeatureName::Enum::kDepthClipControl;
    case wgpu::FeatureName::RG11B10UfloatRenderable:
      return V8GPUFeatureName::Enum::kRg11B10UfloatRenderable;
    case wgpu::FeatureName::BGRA8UnormStorage:
      return V8GPUFeatureName::Enum::kBgra8UnormStorage;
    case wgpu::FeatureName::ChromiumExperimentalSubgroups:
      return V8GPUFeatureName::Enum::kChromiumExperimentalSubgroups;
    case wgpu::FeatureName::ChromiumExperimentalSubgroupUniformControlFlow:
      return V8GPUFeatureName::Enum::
          kChromiumExperimentalSubgroupUniformControlFlow;
    case wgpu::FeatureName::ShaderF16:
      return V8GPUFeatureName::Enum::kShaderF16;
    case wgpu::FeatureName::Float32Filterable:
      return V8GPUFeatureName::Enum::kFloat32Filterable;
    default:
      return std::nullopt;
  }
}

}  // anonymous namespace

namespace {

GPUSupportedFeatures* MakeFeatureNameSet(wgpu::Adapter adapter) {
  GPUSupportedFeatures* features = MakeGarbageCollected<GPUSupportedFeatures>();
  DCHECK(features->FeatureNameSet().empty());

  size_t feature_count = adapter.EnumerateFeatures(nullptr);
  DCHECK(feature_count <= std::numeric_limits<wtf_size_t>::max());

  Vector<wgpu::FeatureName> feature_names(
      static_cast<wtf_size_t>(feature_count));
  adapter.EnumerateFeatures(feature_names.data());

  for (wgpu::FeatureName f : feature_names) {
    auto feature_name_enum_optional = ToV8FeatureNameEnum(f);
    if (feature_name_enum_optional) {
      features->AddFeatureName(
          V8GPUFeatureName(feature_name_enum_optional.value()));
    }
  }
  return features;
}

}  // anonymous namespace

GPUAdapter::GPUAdapter(
    GPU* gpu,
    wgpu::Adapter handle,
    scoped_refptr<DawnControlClientHolder> dawn_control_client)
    : DawnObject(dawn_control_client, std::move(handle), String()), gpu_(gpu) {
  wgpu::AdapterProperties properties = {};
  wgpu::ChainedStructOut** propertiesChain = &properties.nextInChain;
  wgpu::AdapterPropertiesMemoryHeaps memoryHeapProperties = {};
  if (GetHandle().HasFeature(wgpu::FeatureName::AdapterPropertiesMemoryHeaps)) {
    *propertiesChain = &memoryHeapProperties;
    propertiesChain = &(*propertiesChain)->nextInChain;
  }
  wgpu::AdapterPropertiesD3D d3dProperties = {};
  bool supportsPropertiesD3D =
      GetHandle().HasFeature(wgpu::FeatureName::AdapterPropertiesD3D);
  if (supportsPropertiesD3D) {
    *propertiesChain = &d3dProperties;
    propertiesChain = &(*propertiesChain)->nextInChain;
  }
  wgpu::AdapterPropertiesVk vkProperties = {};
  bool supportsPropertiesVk =
      GetHandle().HasFeature(wgpu::FeatureName::AdapterPropertiesVk);
  if (supportsPropertiesVk) {
    *propertiesChain = &vkProperties;
    propertiesChain = &(*propertiesChain)->nextInChain;
  }
  GetHandle().GetProperties(&properties);
  is_fallback_adapter_ = properties.adapterType == wgpu::AdapterType::CPU;
  adapter_type_ = properties.adapterType;
  backend_type_ = properties.backendType;
  is_compatibility_mode_ = properties.compatibilityMode;

  vendor_ = properties.vendorName;
  architecture_ = properties.architecture;
  if (properties.deviceID <= 0xffff) {
    device_ = String::Format("0x%04x", properties.deviceID);
  } else {
    device_ = String::Format("0x%08x", properties.deviceID);
  }
  description_ = properties.name;
  driver_ = properties.driverDescription;
  for (size_t i = 0; i < memoryHeapProperties.heapCount; ++i) {
    memory_heaps_.push_back(MakeGarbageCollected<GPUMemoryHeapInfo>(
        memoryHeapProperties.heapInfo[i]));
  }
  if (supportsPropertiesD3D) {
    d3d_shader_model_ = d3dProperties.shaderModel;
  }
  if (supportsPropertiesVk) {
    vk_driver_version_ = vkProperties.driverVersion;
  }

  features_ = MakeFeatureNameSet(GetHandle());

  wgpu::SupportedLimits limits = {};
  // Chain to get experimental subgroup limits, if support experimental
  // subgroups feature.
  wgpu::DawnExperimentalSubgroupLimits subgroupLimits = {};
  if (features_->has(V8GPUFeatureName::Enum::kChromiumExperimentalSubgroups)) {
    limits.nextInChain = &subgroupLimits;
  }

  GetHandle().GetLimits(&limits);
  limits_ = MakeGarbageCollected<GPUSupportedLimits>(limits);
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

GPUSupportedFeatures* GPUAdapter::features() const {
  return features_.Get();
}

bool GPUAdapter::isFallbackAdapter() const {
  return is_fallback_adapter_;
}

wgpu::BackendType GPUAdapter::backendType() const {
  return backend_type_;
}

bool GPUAdapter::SupportsMultiPlanarFormats() const {
  return GetHandle().HasFeature(wgpu::FeatureName::DawnMultiPlanarFormats);
}

bool GPUAdapter::isCompatibilityMode() const {
  return is_compatibility_mode_;
}

void GPUAdapter::OnRequestDeviceCallback(
    ScriptState* script_state,
    const GPUDeviceDescriptor* descriptor,
    ScriptPromiseResolver<GPUDevice>* resolver,
    WGPURequestDeviceStatus cStatus,
    WGPUDevice cDevice,
    const char* error_message) {
  wgpu::RequestDeviceStatus status =
      static_cast<wgpu::RequestDeviceStatus>(cStatus);
  wgpu::Device dawn_device = wgpu::Device::Acquire(cDevice);
  switch (status) {
    case wgpu::RequestDeviceStatus::Success: {
      DCHECK(dawn_device);

      GPUDeviceLostInfo* device_lost_info = nullptr;
      if (is_consumed_) {
        // Immediately force the device to be lost.
        // TODO: Ideally this should be handled in Dawn, which can return an
        // error device.
        device_lost_info = MakeGarbageCollected<GPUDeviceLostInfo>(
            wgpu::DeviceLostReason::Unknown,
            StringFromASCIIAndUTF8(
                "The adapter is invalid because it has already been used to "
                "create a device. A lost device has been returned."));
      }
      is_consumed_ = true;

      ExecutionContext* execution_context =
          ExecutionContext::From(script_state);
      auto* device = MakeGarbageCollected<GPUDevice>(
          execution_context, GetDawnControlClient(), this,
          std::move(dawn_device), descriptor, device_lost_info);

      if (device_lost_info) {
        // Ensure the Dawn device is marked as lost as well.
        device->InjectError(
            wgpu::ErrorType::DeviceLost,
            "Device was marked as lost due to a stale adapter.");
      }

      resolver->Resolve(device);

      ukm::builders::ClientRenderingAPI(execution_context->UkmSourceID())
          .SetGPUDevice(static_cast<int>(true))
          .Record(execution_context->UkmRecorder());
      break;
    }

    case wgpu::RequestDeviceStatus::Error:
    case wgpu::RequestDeviceStatus::Unknown:
    case wgpu::RequestDeviceStatus::InstanceDropped:
      if (dawn_device) {
        // Immediately force the device to be lost.
        auto* device_lost_info = MakeGarbageCollected<GPUDeviceLostInfo>(
            wgpu::DeviceLostReason::Unknown,
            StringFromASCIIAndUTF8(error_message));
        ExecutionContext* execution_context =
            ExecutionContext::From(script_state);
        auto* device = MakeGarbageCollected<GPUDevice>(
            execution_context, GetDawnControlClient(), this,
            std::move(dawn_device), descriptor, device_lost_info);
        // Resolve with the lost device.
        resolver->Resolve(device);
      } else {
        // If a device is not returned, that means that an error occurred while
        // validating features or limits, and as a result the promise should be
        // rejected with an OperationError.
        resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                         StringFromASCIIAndUTF8(error_message));
      }
      break;
  }
}

ScriptPromise<GPUDevice> GPUAdapter::requestDevice(
    ScriptState* script_state,
    GPUDeviceDescriptor* descriptor) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<GPUDevice>>(
      script_state, ExceptionContext(ExceptionContextType::kOperationInvoke,
                                     "GPUAdapter", "requestDevice"));
  auto promise = resolver->Promise();

  wgpu::DeviceDescriptor dawn_desc = {};

  wgpu::RequiredLimits required_limits = {};
  if (descriptor->hasRequiredLimits()) {
    dawn_desc.requiredLimits = &required_limits;
    GPUSupportedLimits::MakeUndefined(&required_limits);
    if (!GPUSupportedLimits::Populate(&required_limits,
                                      descriptor->requiredLimits(), resolver)) {
      return promise;
    }
  }

  Vector<wgpu::FeatureName> required_features;
  if (descriptor->hasRequiredFeatures()) {
    // Insert features into a set to dedup them.
    HashSet<wgpu::FeatureName> required_features_set;
    for (const V8GPUFeatureName& f : descriptor->requiredFeatures()) {
      // If the feature is not a valid feature reject with a type error.
      if (!features_->has(f.AsEnum())) {
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
    dawn_desc.requiredFeatureCount = required_features.size();
  }

  std::string label = descriptor->label().Utf8();
  if (!label.empty()) {
    dawn_desc.label = label.c_str();
  }

  std::string queueLabel = descriptor->defaultQueue()->label().Utf8();
  if (!queueLabel.empty()) {
    dawn_desc.defaultQueue.label = queueLabel.c_str();
  }

  auto* callback = MakeWGPUOnceCallback(resolver->WrapCallbackInScriptScope(
      WTF::BindOnce(&GPUAdapter::OnRequestDeviceCallback, WrapPersistent(this),
                    WrapPersistent(script_state), WrapPersistent(descriptor))));

  GetHandle().RequestDevice(&dawn_desc, callback->UnboundCallback(),
                            callback->AsUserdata());
  EnsureFlush(ToEventLoop(script_state));

  return promise;
}

ScriptPromise<GPUAdapterInfo> GPUAdapter::requestAdapterInfo(
    ScriptState* script_state) {
  GPUAdapterInfo* adapter_info;
  if (RuntimeEnabledFeatures::WebGPUDeveloperFeaturesEnabled()) {
    // If WebGPU developer features have been enabled then provide all available
    // adapter info values.
    adapter_info = MakeGarbageCollected<GPUAdapterInfo>(
        vendor_, architecture_, device_, description_, driver_,
        FromDawnEnum(backend_type_), FromDawnEnum(adapter_type_),
        d3d_shader_model_, vk_driver_version_);
    for (GPUMemoryHeapInfo* memory_heap : memory_heaps_) {
      adapter_info->AppendMemoryHeapInfo(memory_heap);
    }
  } else {
    adapter_info = MakeGarbageCollected<GPUAdapterInfo>(vendor_, architecture_);
  }

  return ToResolvedPromise<GPUAdapterInfo>(script_state, adapter_info);
}

void GPUAdapter::Trace(Visitor* visitor) const {
  visitor->Trace(gpu_);
  visitor->Trace(features_);
  visitor->Trace(limits_);
  visitor->Trace(memory_heaps_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
