// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dawn_context_provider.h"

#include <memory>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_arguments.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "gpu/command_buffer/service/dawn_instance.h"
#include "gpu/command_buffer/service/dawn_platform.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_switches.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/dawn/DawnBackendContext.h"
#include "third_party/skia/include/gpu/graphite/dawn/DawnUtils.h"

#if BUILDFLAG(IS_WIN)
#include "third_party/dawn/include/dawn/native/D3D11Backend.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_angle_util_win.h"
#endif

namespace gpu {
namespace {

void LogInfo(WGPULoggingType type, char const* message, void* userdata) {
  VLOG(1) << message;
}

void LogError(WGPUErrorType type, char const* message, void* userdata) {
  LOG(ERROR) << message;
  static crash_reporter::CrashKeyString<1024> error_key(
      "dawn-validation-error");
  error_key.Set(message);
  base::debug::DumpWithoutCrashing();
}

void LogDeviceLost(WGPUDeviceLostReason reason,
                   char const* message,
                   void* userdata) {
  if (reason != WGPUDeviceLostReason_Destroyed) {
    LOG(FATAL) << message;
  }
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

#if BUILDFLAG(IS_WIN)
bool GetANGLED3D11DeviceLUID(LUID* luid) {
  // On Windows, query the LUID of ANGLE's adapter.
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();
  if (!d3d11_device) {
    LOG(ERROR) << "Failed to query ID3D11Device from ANGLE.";
    return false;
  }

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  if (!SUCCEEDED(d3d11_device.As(&dxgi_device))) {
    LOG(ERROR) << "Failed to get IDXGIDevice from ANGLE.";
    return false;
  }

  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  if (!SUCCEEDED(dxgi_device->GetAdapter(&dxgi_adapter))) {
    LOG(ERROR) << "Failed to get IDXGIAdapter from ANGLE.";
    return false;
  }

  DXGI_ADAPTER_DESC adapter_desc;
  if (!SUCCEEDED(dxgi_adapter->GetDesc(&adapter_desc))) {
    LOG(ERROR) << "Failed to get DXGI_ADAPTER_DESC from ANGLE.";
    return false;
  }

  *luid = adapter_desc.AdapterLuid;
  return true;
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

std::unique_ptr<DawnContextProvider> DawnContextProvider::Create(
    const GpuPreferences& gpu_preferences,
    webgpu::DawnCachingInterfaceFactory* caching_interface_factory,
    CacheBlobCallback callback) {
  return DawnContextProvider::CreateWithBackend(
      GetDefaultBackendType(), DefaultForceFallbackAdapter(), gpu_preferences,
      caching_interface_factory, std::move(callback));
}

std::unique_ptr<DawnContextProvider> DawnContextProvider::CreateWithBackend(
    wgpu::BackendType backend_type,
    bool force_fallback_adapter,
    const GpuPreferences& gpu_preferences,
    webgpu::DawnCachingInterfaceFactory* caching_interface_factory,
    CacheBlobCallback callback) {
  auto context_provider =
      base::WrapUnique(new DawnContextProvider(caching_interface_factory));

  // TODO(rivr): This may return a GPU that is not the active one. Currently
  // the only known way to avoid this is platform-specific; e.g. on Mac, create
  // a Dawn device, get the actual Metal device from it, and compare against
  // MTLCreateSystemDefaultDevice().
  if (!context_provider->Initialize(backend_type, force_fallback_adapter,
                                    gpu_preferences, std::move(callback))) {
    context_provider.reset();
  }
  return context_provider;
}
// static
wgpu::BackendType DawnContextProvider::GetDefaultBackendType() {
  const auto switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kSkiaGraphiteBackend);
  if (switch_value == switches::kSkiaGraphiteBackendDawnD3D11) {
    return wgpu::BackendType::D3D11;
  } else if (switch_value == switches::kSkiaGraphiteBackendDawnD3D12) {
    return wgpu::BackendType::D3D12;
  } else if (switch_value == switches::kSkiaGraphiteBackendDawnMetal) {
    return wgpu::BackendType::Metal;
  } else if (switch_value == switches::kSkiaGraphiteBackendDawnSwiftshader ||
             switch_value == switches::kSkiaGraphiteBackendDawnVulkan) {
    return wgpu::BackendType::Vulkan;
  }
#if BUILDFLAG(IS_WIN)
  return base::FeatureList::IsEnabled(features::kSkiaGraphiteDawnUseD3D12)
             ? wgpu::BackendType::D3D12
             : wgpu::BackendType::D3D11;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return wgpu::BackendType::Vulkan;
#elif BUILDFLAG(IS_APPLE)
  return wgpu::BackendType::Metal;
#else
  NOTREACHED();
  return wgpu::BackendType::Null;
#endif
}

// static
bool DawnContextProvider::DefaultForceFallbackAdapter() {
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
             switches::kSkiaGraphiteBackend) ==
         switches::kSkiaGraphiteBackendDawnSwiftshader;
}

DawnContextProvider::DawnContextProvider(
    webgpu::DawnCachingInterfaceFactory* caching_interface_factory)
    : caching_interface_factory_(caching_interface_factory) {}
DawnContextProvider::~DawnContextProvider() = default;

bool DawnContextProvider::Initialize(wgpu::BackendType backend_type,
                                     bool force_fallback_adapter,
                                     const GpuPreferences& gpu_preferences,
                                     CacheBlobCallback callback) {
  std::unique_ptr<webgpu::DawnCachingInterface> caching_interface;
  if (caching_interface_factory_) {
    caching_interface = caching_interface_factory_->CreateInstance(
        kGraphiteDawnGpuDiskCacheHandle, std::move(callback));
  }

  platform_ = std::make_unique<Platform>(std::move(caching_interface));
  instance_ = webgpu::DawnInstance::Create(platform_.get(), gpu_preferences);

  // If a new toggle is added here, ForceDawnTogglesForSkia() which collects
  // info for about:gpu should be updated as well.
  std::vector<const char*> enabled_toggles;
  std::vector<const char*> disabled_toggles;
  for (const auto& toggle : gpu_preferences.enabled_dawn_features_list) {
    enabled_toggles.push_back(toggle.c_str());
  }
  for (const auto& toggle : gpu_preferences.disabled_dawn_features_list) {
    disabled_toggles.push_back(toggle.c_str());
  }
  // The following toggles are all device-scoped toggles so it's not necessary
  // to pass them when creating the Instance above.
#if DCHECK_IS_ON()
  enabled_toggles.push_back("use_user_defined_labels_in_backend");
#else
  if (features::kSkiaGraphiteDawnSkipValidation.Get()) {
    enabled_toggles.push_back("skip_validation");
  }
  enabled_toggles.push_back("disable_robustness");
#endif

#if BUILDFLAG(IS_APPLE)
  if (backend_type == wgpu::BackendType::Vulkan) {
    // Vulkan doesn't support IOSurface image backing, so we need
    // MultiPlanarFormatExtendedUsages to copy to/from multiplanar texture.
    // And this feature is currently experimental.
    enabled_toggles.push_back("allow_unsafe_apis");
  }
#endif  // BUILDFLAG(IS_APPLE)

  wgpu::DawnTogglesDescriptor toggles_desc;
  toggles_desc.enabledToggles = enabled_toggles.data();
  toggles_desc.disabledToggles = disabled_toggles.data();
#ifdef WGPU_BREAKING_CHANGE_COUNT_RENAME
  toggles_desc.enabledToggleCount = enabled_toggles.size();
  toggles_desc.disabledToggleCount = disabled_toggles.size();
#else
  toggles_desc.enabledTogglesCount = enabled_toggles.size();
  toggles_desc.disabledTogglesCount = disabled_toggles.size();
#endif

  wgpu::DeviceDescriptor descriptor;
  descriptor.nextInChain = &toggles_desc;

  // TODO(crbug.com/1456492): verify the required features.
  std::vector<wgpu::FeatureName> features = {
      wgpu::FeatureName::DawnInternalUsages,
      wgpu::FeatureName::DawnMultiPlanarFormats,
      wgpu::FeatureName::ImplicitDeviceSynchronization,
      wgpu::FeatureName::SurfaceCapabilities,
  };

  wgpu::RequestAdapterOptions adapter_options;
  adapter_options.backendType = backend_type;
  adapter_options.forceFallbackAdapter = force_fallback_adapter;
  adapter_options.powerPreference = wgpu::PowerPreference::LowPower;

#if BUILDFLAG(IS_WIN)
  if (adapter_options.backendType == wgpu::BackendType::D3D11) {
    features.push_back(wgpu::FeatureName::D3D11MultithreadProtected);
  }

  // Request the GPU that ANGLE is using if possible.
  dawn::native::d3d::RequestAdapterOptionsLUID adapter_options_luid;
  if (GetANGLED3D11DeviceLUID(&adapter_options_luid.adapterLUID)) {
    adapter_options.nextInChain = &adapter_options_luid;
  }
#endif  // BUILDFLAG(IS_WIN)

  std::vector<dawn::native::Adapter> adapters =
      instance_->EnumerateAdapters(&adapter_options);
  if (adapters.empty()) {
    LOG(ERROR) << "No adapters found.";
    return false;
  }

  wgpu::Adapter adapter(adapters[0].Get());
  if (adapter.HasFeature(wgpu::FeatureName::TransientAttachments)) {
    features.push_back(wgpu::FeatureName::TransientAttachments);
    // Enabling MSAARenderToSingleSampled causes performance regression without
    // TransientAttachments support.
    if (adapter.HasFeature(wgpu::FeatureName::MSAARenderToSingleSampled)) {
      features.push_back(wgpu::FeatureName::MSAARenderToSingleSampled);
    }
  }

  if (adapter.HasFeature(wgpu::FeatureName::Norm16TextureFormats)) {
    features.push_back(wgpu::FeatureName::Norm16TextureFormats);
  }

  if (adapter.HasFeature(wgpu::FeatureName::MultiPlanarFormatP010)) {
    features.push_back(wgpu::FeatureName::MultiPlanarFormatP010);
  }

  if (adapter.HasFeature(wgpu::FeatureName::MultiPlanarFormatExtendedUsages)) {
    features.push_back(wgpu::FeatureName::MultiPlanarFormatExtendedUsages);
  }

  descriptor.requiredFeatures = features.data();
#ifdef WGPU_BREAKING_CHANGE_COUNT_RENAME
  descriptor.requiredFeatureCount = std::size(features);
#else
  descriptor.requiredFeaturesCount = std::size(features);
#endif

  wgpu::Device device = adapter.CreateDevice(&descriptor);
  if (!device) {
    LOG(ERROR) << "Failed to create device.";
    return false;
  }

  device.SetUncapturedErrorCallback(&LogError, nullptr);
  device.SetDeviceLostCallback(&LogDeviceLost, nullptr);
  device.SetLoggingCallback(&LogInfo, nullptr);
  device_ = std::move(device);

  backend_type_ = backend_type;
  is_vulkan_swiftshader_adapter_ =
      backend_type == wgpu::BackendType::Vulkan && force_fallback_adapter;

#if BUILDFLAG(IS_WIN)
  // DirectComposition is initialized in ui/gl/init/gl_initializer_win.cc while
  // initializing GL. So we need to shutdown it and re-initialize it here with
  // the D3D11 device from dawn device.
  // TODO(crbug.com/1469283): avoid initializing DirectComposition twice.
  gl::ShutdownDirectComposition();
  if (auto d3d11_device = GetD3D11Device()) {
    gl::InitializeDirectComposition(std::move(d3d11_device));
  }
#endif  // BUILDFLAG(IS_WIN)

  return true;
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
  if (backend_type() == wgpu::BackendType::D3D11) {
    return dawn::native::d3d11::GetD3D11Device(device_.Get());
  }
  return nullptr;
}
#endif

}  // namespace gpu
