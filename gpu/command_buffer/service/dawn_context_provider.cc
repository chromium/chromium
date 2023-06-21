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
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_arguments.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/dawn_instance.h"
#include "gpu/command_buffer/service/dawn_platform.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"
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

class Platform : public webgpu::DawnPlatform {
 public:
  using webgpu::DawnPlatform::DawnPlatform;

  ~Platform() override = default;

  const unsigned char* GetTraceCategoryEnabledFlag(
      dawn::platform::TraceCategory category) override {
    return TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
        TRACE_DISABLED_BY_DEFAULT("gpu.graphite.dawn"));
  }
};

}  // namespace

std::unique_ptr<DawnContextProvider> DawnContextProvider::Create(
    webgpu::DawnCachingInterfaceFactory* caching_interface_factory,
    CacheBlobCallback callback) {
  auto context_provider =
      base::WrapUnique(new DawnContextProvider(caching_interface_factory));

  // TODO(rivr): This may return a GPU that is not the active one. Currently
  // the only known way to avoid this is platform-specific; e.g. on Mac, create
  // a Dawn device, get the actual Metal device from it, and compare against
  // MTLCreateSystemDefaultDevice().
  if (!context_provider->Initialize(std::move(callback))) {
    context_provider.reset();
  }
  return context_provider;
}

DawnContextProvider::DawnContextProvider(
    webgpu::DawnCachingInterfaceFactory* caching_interface_factory)
    : caching_interface_factory_(caching_interface_factory) {}
DawnContextProvider::~DawnContextProvider() = default;

bool DawnContextProvider::Initialize(CacheBlobCallback callback) {
  std::unique_ptr<webgpu::DawnCachingInterface> caching_interface;
  if (caching_interface_factory_) {
    caching_interface = caching_interface_factory_->CreateInstance(
        kGraphiteDawnGpuDiskCacheHandle, std::move(callback));
  }

  platform_ = std::make_unique<Platform>(std::move(caching_interface));

  GpuPreferences preferences;
#if DCHECK_IS_ON()
  preferences.enable_dawn_backend_validation =
      DawnBackendValidationLevel::kFull;
#else
  preferences.enable_dawn_backend_validation =
      DawnBackendValidationLevel::kDisabled;
#endif

  instance_ = webgpu::DawnInstance::Create(platform_.get(), preferences);
  instance_->DiscoverDefaultPhysicalDevices();

  DawnProcTable backend_procs = dawn::native::GetProcs();
  dawnProcSetProcs(&backend_procs);

  // If a new toggle is added here, ForceDawnTogglesForSkia() which collects
  // info for about:gpu should be updated as well.
  wgpu::DeviceDescriptor descriptor;

  // Disable validation in non-DCHECK builds.
#if !DCHECK_IS_ON()
  // TODO(crbug.com/1456492): check if below toggles are necessary.
  const char* kForceEnabledToggles[] = {"disable_robustness",
                                        "skip_validation"};
  const char* kForceDisabledToggles[] = {"lazy_clear_resource_on_first_use"};

  wgpu::DawnTogglesDescriptor toggles_desc;
  toggles_desc.enabledToggles = kForceEnabledToggles;
  toggles_desc.enabledTogglesCount = std::size(kForceEnabledToggles);
  toggles_desc.disabledToggles = kForceDisabledToggles;
  toggles_desc.disabledTogglesCount = std::size(kForceDisabledToggles);
  descriptor.nextInChain = &toggles_desc;
#endif

  // TODO(crbug.com/1456492): verify the required features.
  wgpu::FeatureName features[] = {
      wgpu::FeatureName::DawnInternalUsages,
      wgpu::FeatureName::DawnMultiPlanarFormats,
      wgpu::FeatureName::DepthClipControl,
      wgpu::FeatureName::Depth32FloatStencil8,
      wgpu::FeatureName::ImplicitDeviceSynchronization,
      wgpu::FeatureName::SurfaceCapabilities,
  };

  descriptor.requiredFeatures = features;
  descriptor.requiredFeaturesCount = std::size(features);

  wgpu::BackendType backend_type = GetDefaultBackendType();
  std::vector<dawn::native::Adapter> adapters = instance_->EnumerateAdapters();
  for (dawn::native::Adapter adapter : adapters) {
    wgpu::AdapterProperties properties;
    adapter.GetProperties(&properties);
    if (properties.backendType == backend_type) {
      wgpu::Device device(adapter.CreateDevice(&descriptor));
      if (device) {
        device.SetUncapturedErrorCallback(&LogError, nullptr);
        device.SetDeviceLostCallback(&LogFatal, nullptr);
        device.SetLoggingCallback(&LogInfo, nullptr);
      }
      device_ = std::move(device);
      return true;
    }
  }

  return false;
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

wgpu::Instance DawnContextProvider::GetInstance() const {
  return instance_->Get();
}

#if BUILDFLAG(IS_WIN)
Microsoft::WRL::ComPtr<ID3D11Device> DawnContextProvider::GetD3D11Device()
    const {
  return dawn::native::d3d11::GetD3D11Device(device_.Get());
}
#endif

}  // namespace gpu
