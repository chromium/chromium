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
#include "third_party/blink/renderer/modules/webgpu/gpu_subgroup_matrix_config.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_features.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_limits.h"
#include "third_party/blink/renderer/modules/webgpu/string_utils.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_callback.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

GPUSupportedFeatures* MakeFeatureNameSet(wgpu::Adapter adapter) {
  GPUSupportedFeatures* features = MakeGarbageCollected<GPUSupportedFeatures>();
  DCHECK(features->FeatureNameSet().empty());

  wgpu::SupportedFeatures supported_features;
  adapter.GetFeatures(&supported_features);
  // SAFETY: Required from caller
  const auto features_span = UNSAFE_BUFFERS(base::span<const wgpu::FeatureName>(
      supported_features.features, supported_features.featureCount));
  for (const auto& f : features_span) {
    auto feature_name_enum_optional =
        GPUSupportedFeatures::ToV8FeatureNameEnum(f);
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
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    const GPURequestAdapterOptions* options)
    : DawnObject(dawn_control_client, std::move(handle), String()), gpu_(gpu) {
  wgpu::AdapterInfo info = {};
  wgpu::ChainedStructOut** propertiesChain = &info.nextInChain;
  if (GetHandle().HasFeature(wgpu::FeatureName::AdapterPropertiesMemoryHeaps)) {
    *propertiesChain = &memory_heaps_;
    propertiesChain = &(*propertiesChain)->nextInChain;
  }
  if (GetHandle().HasFeature(
          wgpu::FeatureName::ChromiumExperimentalSubgroupMatrix)) {
    *propertiesChain = &subgroup_matrix_configs_;
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
  wgpu::DawnAdapterPropertiesPowerPreference powerProperties{};
  *propertiesChain = &powerProperties;
  propertiesChain = &(*propertiesChain)->nextInChain;

  GetHandle().GetInfo(&info);
  adapter_type_ = info.adapterType;
  backend_type_ = info.backendType;

  // TODO(crbug.com/359418629): Report xr compatibility in GetInfo()
  is_xr_compatible_ = options->xrCompatible();

  vendor_ = String::FromUTF8(info.vendor);
  architecture_ = String::FromUTF8(info.architecture);
  if (info.deviceID <= 0xffff) {
    device_ = String::Format("0x%04x", info.deviceID);
  } else {
    device_ = String::Format("0x%08x", info.deviceID);
  }
  description_ = String::FromUTF8(info.device);
  driver_ = String::FromUTF8(info.description);
  if (supportsPropertiesD3D) {
    d3d_shader_model_ = d3dProperties.shaderModel;
  }
  if (supportsPropertiesVk) {
    vk_driver_version_ = vkProperties.driverVersion;
  }
  subgroup_min_size_ = info.subgroupMinSize;
  subgroup_max_size_ = info.subgroupMaxSize;
  power_preference_ = powerProperties.powerPreference;

  features_ = MakeFeatureNameSet(GetHandle());

  GPUSupportedLimits::ComboLimits limits;
  GetHandle().GetLimits(limits.GetLinked());
  limits_ = MakeGarbageCollected<GPUSupportedLimits>(limits);

  info_ = CreateAdapterInfoForAdapter();
}

GPUAdapterInfo* GPUAdapter::CreateAdapterInfoForAdapter() {
  bool is_fallback_adapter = adapter_type_ == wgpu::AdapterType::CPU;

  GPUAdapterInfo* info;
  if (RuntimeEnabledFeatures::WebGPUDeveloperFeaturesEnabled()) {
    // If WebGPU developer features have been enabled then provide all available
    // adapter info values.
    info = MakeGarbageCollected<GPUAdapterInfo>(
        vendor_, architecture_, subgroup_min_size_, subgroup_max_size_,
        is_fallback_adapter, device_, description_, driver_,
        FromDawnEnum(backend_type_), FromDawnEnum(adapter_type_),
        d3d_shader_model_, vk_driver_version_, FromDawnEnum(power_preference_));

    // SAFETY: Required from caller
    const auto memory_heaps_span =
        UNSAFE_BUFFERS(base::span<const wgpu::MemoryHeapInfo>(
            memory_heaps_.heapInfo, memory_heaps_.heapCount));
    for (const auto& m : memory_heaps_span) {
      info->AppendMemoryHeapInfo(MakeGarbageCollected<GPUMemoryHeapInfo>(m));
    }
  } else {
    info = MakeGarbageCollected<GPUAdapterInfo>(
        vendor_, architecture_, subgroup_min_size_, subgroup_max_size_,
        is_fallback_adapter);
  }

  // SAFETY: Required from caller
  const auto subgroup_matrix_configs_span =
      UNSAFE_BUFFERS(base::span<const wgpu::SubgroupMatrixConfig>(
          subgroup_matrix_configs_.configs,
          subgroup_matrix_configs_.configCount));
  for (const auto& c : subgroup_matrix_configs_span) {
    info->AppendSubgroupMatrixConfig(
        MakeGarbageCollected<GPUSubgroupMatrixConfig>(c));
  }

  return info;
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

GPUAdapterInfo* GPUAdapter::info() const {
  return info_.Get();
}

wgpu::BackendType GPUAdapter::backendType() const {
  return backend_type_;
}

bool GPUAdapter::SupportsMultiPlanarFormats() const {
  return GetHandle().HasFeature(wgpu::FeatureName::DawnMultiPlanarFormats);
}

void GPUAdapter::OnRequestDeviceCallback(
    GPUDevice* device,
    const GPUDeviceDescriptor* descriptor,
    ScriptPromiseResolver<GPUDevice>* resolver,
    wgpu::RequestDeviceStatus status,
    wgpu::Device dawn_device,
    wgpu::StringView error_message) {
  switch (status) {
    case wgpu::RequestDeviceStatus::Success: {
      DCHECK(dawn_device);

      device->Initialize(dawn_device, descriptor, /*lost_info=*/nullptr);
      resolver->Resolve(device);

      ukm::builders::ClientRenderingAPI(
          device->GetExecutionContext()->UkmSourceID())
          .SetGPUDevice(static_cast<int>(true))
          .Record(device->GetExecutionContext()->UkmRecorder());
      break;
    }

    case wgpu::RequestDeviceStatus::Error:
    case wgpu::RequestDeviceStatus::CallbackCancelled:
      if (dawn_device) {
        // A device provided with an error is already a lost device on the Dawn
        // side, reflect that by resolving the lost property immediately.
        device->Initialize(dawn_device, descriptor,
                           MakeGarbageCollected<GPUDeviceLostInfo>(
                               wgpu::DeviceLostReason::Unknown,
                               StringFromASCIIAndUTF8(error_message)));

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
      script_state, ExceptionContext(v8::ExceptionContext::kOperation,
                                     "GPUAdapter", "requestDevice"));
  auto promise = resolver->Promise();

  wgpu::DeviceDescriptor dawn_desc = {};

  wgpu::DawnConsumeAdapterDescriptor consume_adapter_desc;
  consume_adapter_desc.consumeAdapter = true;
  dawn_desc.nextInChain = &consume_adapter_desc;

  GPUSupportedLimits::ComboLimits required_limits;
  if (descriptor->hasRequiredLimits()) {
    dawn_desc.requiredLimits = required_limits.GetLinked();
    if (!GPUSupportedLimits::Populate(&required_limits,
                                      descriptor->requiredLimits(), resolver)) {
      return promise;
    }
  }

  // Use a set to prevent duplicate features.
  HashSet<wgpu::FeatureName> required_features_set;
  // The ShaderModuleCompilationOptions feature is required only if the adapter
  // has the ShaderModuleCompilationOptions feature and the user has enabled the
  // WebGPUDeveloperFeatures flag. It is needed to control
  // strict math during shader module compilation.
  if (RuntimeEnabledFeatures::WebGPUDeveloperFeaturesEnabled() &&
      GetHandle().HasFeature(
          wgpu::FeatureName::ShaderModuleCompilationOptions)) {
    required_features_set.insert(
        wgpu::FeatureName::ShaderModuleCompilationOptions);
  }
  if (descriptor->hasRequiredFeatures()) {
    for (const V8GPUFeatureName& f : descriptor->requiredFeatures()) {
      // If the feature is not a valid feature reject with a type error.
      if (!features_->Has(f.AsEnum())) {
        resolver->RejectWithTypeError(
            String::Format("Unsupported feature: %s", f.AsCStr()));
        return promise;
      }
      required_features_set.insert(AsDawnEnum(f));
    }
  }

  Vector<wgpu::FeatureName> required_features;
  required_features.AppendRange(required_features_set.begin(),
                                required_features_set.end());
  dawn_desc.requiredFeatures = required_features.data();
  dawn_desc.requiredFeatureCount = required_features.size();

  std::string label = descriptor->label().Utf8();
  if (!label.empty()) {
    dawn_desc.label = label.c_str();
  }

  std::string queueLabel = descriptor->defaultQueue()->label().Utf8();
  if (!queueLabel.empty()) {
    dawn_desc.defaultQueue.label = queueLabel.c_str();
  }

  // Create a GPUDevice without the handle, so that we can set up its callbacks
  // in the wgpu::DeviceDescriptor.
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  auto* device = MakeGarbageCollected<GPUDevice>(
      execution_context, GetDawnControlClient(), this, descriptor->label());
  device->SetDescriptorCallbacks(dawn_desc);

  auto* callback = MakeWGPUOnceCallback(resolver->WrapCallbackInScriptScope(
      BindOnce(&GPUAdapter::OnRequestDeviceCallback, WrapPersistent(this),
               WrapPersistent(device), WrapPersistent(descriptor))));
  GetHandle().RequestDevice(&dawn_desc, wgpu::CallbackMode::AllowProcessEvents,
                            callback->UnboundCallback(),
                            callback->AsUserdata());
  EnsureFlush(ToEventLoop(script_state));

  return promise;
}

void GPUAdapter::Trace(Visitor* visitor) const {
  visitor->Trace(gpu_);
  visitor->Trace(features_);
  visitor->Trace(limits_);
  visitor->Trace(info_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
