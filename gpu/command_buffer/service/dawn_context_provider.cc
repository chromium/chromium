// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dawn_context_provider.h"

#include <memory>
#include <vector>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "gpu/config/gpu_finch_features.h"
#include "third_party/dawn/include/dawn/dawn_proc.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/dawn/DawnBackendContext.h"
#include "third_party/skia/include/gpu/graphite/dawn/DawnUtils.h"

#if BUILDFLAG(IS_WIN)
#include "third_party/dawn/include/dawn/native/D3D11Backend.h"
#endif

namespace gpu {

namespace {

void LogInfo(WGPULoggingType type, char const* message, void* userdata) {
  VLOG(1) << message;
}

void LogError(WGPUErrorType type, char const* message, void* userdata) {
  LOG(ERROR) << message;
}

void LogFatal(WGPUDeviceLostReason reason,
              char const* message,
              void* userdata) {
  LOG(FATAL) << message;
}

wgpu::BackendType GetDefaultBackendType() {
#if BUILDFLAG(IS_WIN)
  return base::FeatureList::IsEnabled(features::kSkiaGraphiteDawnUseD3D12)
             ? wgpu::BackendType::D3D12
             : wgpu::BackendType::D3D11;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return wgpu::BackendType::Vulkan;
#elif BUILDFLAG(IS_MAC)
  return wgpu::BackendType::Metal;
#else
  NOTREACHED();
  return wgpu::BackendType::Null;
#endif
}

}  // namespace

std::unique_ptr<DawnContextProvider> DawnContextProvider::Create() {
  auto context_provider = base::WrapUnique(new DawnContextProvider());
  if (!context_provider->GetDevice()) {
    return nullptr;
  }
  return context_provider;
}

DawnContextProvider::DawnContextProvider() {
  // TODO(rivr): This may return a GPU that is not the active one. Currently
  // the only known way to avoid this is platform-specific; e.g. on Mac, create
  // a Dawn device, get the actual Metal device from it, and compare against
  // MTLCreateSystemDefaultDevice().
  device_ = CreateDevice(GetDefaultBackendType());
}

DawnContextProvider::~DawnContextProvider() = default;

wgpu::Device DawnContextProvider::CreateDevice(wgpu::BackendType type) {
#if DCHECK_IS_ON()
  instance_.EnableBackendValidation(true);
#endif

  instance_.DiscoverDefaultPhysicalDevices();
  DawnProcTable backend_procs = dawn::native::GetProcs();
  dawnProcSetProcs(&backend_procs);

  // If a new toggle is added here, ForceDawnTogglesForSkia() which collects
  // info for about:gpu should be updated as well.
  wgpu::DeviceDescriptor descriptor;

  // Disable validation in non-DCHECK builds.
#if !DCHECK_IS_ON()
  std::vector<const char*> force_enabled_toggles;
  std::vector<const char*> force_disabled_toggles;

  force_enabled_toggles.push_back("disable_robustness");
  force_enabled_toggles.push_back("skip_validation");
  force_disabled_toggles.push_back("lazy_clear_resource_on_first_use");

  wgpu::DawnTogglesDescriptor toggles_desc;
  toggles_desc.enabledToggles = force_enabled_toggles.data();
  toggles_desc.enabledTogglesCount = force_enabled_toggles.size();
  toggles_desc.disabledToggles = force_disabled_toggles.data();
  toggles_desc.disabledTogglesCount = force_disabled_toggles.size();
  descriptor.nextInChain = &toggles_desc;
#endif

  std::vector<wgpu::FeatureName> features;
  features.push_back(wgpu::FeatureName::DawnInternalUsages);
  features.push_back(wgpu::FeatureName::DawnMultiPlanarFormats);
  features.push_back(wgpu::FeatureName::DepthClipControl);
  features.push_back(wgpu::FeatureName::Depth32FloatStencil8);
  features.push_back(wgpu::FeatureName::ImplicitDeviceSynchronization);
  features.push_back(wgpu::FeatureName::SurfaceCapabilities);

  descriptor.requiredFeatures = features.data();
  descriptor.requiredFeaturesCount = features.size();

  std::vector<dawn::native::Adapter> adapters = instance_.EnumerateAdapters();
  for (dawn::native::Adapter adapter : adapters) {
    wgpu::AdapterProperties properties;
    adapter.GetProperties(&properties);
    if (properties.backendType == type) {
      wgpu::Device device(adapter.CreateDevice(&descriptor));
      if (device) {
        device.SetUncapturedErrorCallback(&LogError, nullptr);
        device.SetDeviceLostCallback(&LogFatal, nullptr);
        device.SetLoggingCallback(&LogInfo, nullptr);
      }
      return device;
    }
  }
  return nullptr;
}

bool DawnContextProvider::InitializeGraphiteContext(
    const skgpu::graphite::ContextOptions& options) {
  CHECK(!graphite_context_);

  if (device_) {
    skgpu::graphite::DawnBackendContext backend_context;
    backend_context.fDevice = device_;
    backend_context.fQueue = device_.GetQueue();
    graphite_context_ =
        skgpu::graphite::ContextFactory::MakeDawn(backend_context, options);
  }

  return !!graphite_context_;
}

#if BUILDFLAG(IS_WIN)
Microsoft::WRL::ComPtr<ID3D11Device> DawnContextProvider::GetD3D11Device()
    const {
  return dawn::native::d3d11::GetD3D11Device(device_.Get());
}
#endif

}  // namespace gpu
