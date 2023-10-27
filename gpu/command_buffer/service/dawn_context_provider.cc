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
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_arguments.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "gpu/command_buffer/service/dawn_instance.h"
#include "gpu/command_buffer/service/dawn_platform.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_switches.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/dawn/DawnBackendContext.h"
#include "third_party/skia/include/gpu/graphite/dawn/DawnUtils.h"
#include "ui/gl/gl_implementation.h"

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
  if (type != WGPUErrorType_NoError) {
    static_cast<DawnContextProvider*>(userdata)->OnError(type, message);
  }
}

void LogDeviceLost(WGPUDeviceLostReason reason,
                   char const* message,
                   void* userdata) {
  if (reason != WGPUDeviceLostReason_Destroyed) {
    static_cast<DawnContextProvider*>(userdata)->OnError(
        WGPUErrorType_DeviceLost, message);
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

const char* HRESULTToString(HRESULT result) {
  switch (result) {
#define ERROR_CASE(E) \
  case E:             \
    return #E;
    ERROR_CASE(DXGI_ERROR_DEVICE_HUNG)
    ERROR_CASE(DXGI_ERROR_DEVICE_REMOVED)
    ERROR_CASE(DXGI_ERROR_DEVICE_RESET)
    ERROR_CASE(DXGI_ERROR_DRIVER_INTERNAL_ERROR)
    ERROR_CASE(DXGI_ERROR_INVALID_CALL)
    ERROR_CASE(S_OK)
#undef ERROR_CASE
    default:
      return nullptr;
  }
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace

std::unique_ptr<DawnContextProvider> DawnContextProvider::Create(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& gpu_driver_workarounds,
    webgpu::DawnCachingInterfaceFactory* caching_interface_factory,
    CacheBlobCallback callback) {
  return DawnContextProvider::CreateWithBackend(
      GetDefaultBackendType(), DefaultForceFallbackAdapter(), gpu_preferences,
      gpu_driver_workarounds, caching_interface_factory, std::move(callback));
}

std::unique_ptr<DawnContextProvider> DawnContextProvider::CreateWithBackend(
    wgpu::BackendType backend_type,
    bool force_fallback_adapter,
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& gpu_driver_workarounds,
    webgpu::DawnCachingInterfaceFactory* caching_interface_factory,
    CacheBlobCallback callback) {
  auto context_provider =
      base::WrapUnique(new DawnContextProvider(caching_interface_factory));

  // TODO(rivr): This may return a GPU that is not the active one. Currently
  // the only known way to avoid this is platform-specific; e.g. on Mac, create
  // a Dawn device, get the actual Metal device from it, and compare against
  // MTLCreateSystemDefaultDevice().
  if (!context_provider->Initialize(backend_type, force_fallback_adapter,
                                    gpu_preferences, gpu_driver_workarounds,
                                    std::move(callback))) {
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

  if (gl::GetANGLEImplementation() == gl::ANGLEImplementation::kSwiftShader) {
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
             switches::kSkiaGraphiteBackendDawnSwiftshader ||
         gl::GetANGLEImplementation() == gl::ANGLEImplementation::kSwiftShader;
}

DawnContextProvider::DawnContextProvider(
    webgpu::DawnCachingInterfaceFactory* caching_interface_factory)
    : caching_interface_factory_(caching_interface_factory) {}

DawnContextProvider::~DawnContextProvider() {
  if (device_) {
    device_.SetUncapturedErrorCallback(nullptr, nullptr);
    device_.SetDeviceLostCallback(nullptr, nullptr);
    device_.SetLoggingCallback(nullptr, nullptr);
  }
}

bool DawnContextProvider::Initialize(
    wgpu::BackendType backend_type,
    bool force_fallback_adapter,
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& gpu_driver_workarounds,
    CacheBlobCallback callback) {
  std::unique_ptr<webgpu::DawnCachingInterface> caching_interface;
  if (caching_interface_factory_) {
    caching_interface = caching_interface_factory_->CreateInstance(
        kGraphiteDawnGpuDiskCacheHandle, std::move(callback));
  }

  platform_ = std::make_unique<Platform>(std::move(caching_interface),
                                         /*uma_prefix=*/"GPU.GraphiteDawn.");
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
  // We need MultiPlanarFormatExtendedUsages to copy to/from multiplanar
  // texture. And this feature is currently experimental.
  enabled_toggles.push_back("allow_unsafe_apis");
#endif  // BUILDFLAG(IS_APPLE)

  wgpu::DawnTogglesDescriptor toggles_desc;
  toggles_desc.enabledToggles = enabled_toggles.data();
  toggles_desc.disabledToggles = disabled_toggles.data();
  toggles_desc.enabledToggleCount = enabled_toggles.size();
  toggles_desc.disabledToggleCount = disabled_toggles.size();

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
  if (gpu_driver_workarounds.force_high_performance_gpu) {
    adapter_options.powerPreference = wgpu::PowerPreference::HighPerformance;
  } else {
    adapter_options.powerPreference = wgpu::PowerPreference::LowPower;
  }
  adapter_options.nextInChain = &toggles_desc;

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

  const wgpu::FeatureName kOptionalFeatures[] = {
      wgpu::FeatureName::DualSourceBlending,
      wgpu::FeatureName::MultiPlanarFormatExtendedUsages,
      wgpu::FeatureName::MultiPlanarFormatP010,
      wgpu::FeatureName::MultiPlanarRenderTargets,
      wgpu::FeatureName::Norm16TextureFormats,
      wgpu::FeatureName::TransientAttachments,
  };

  wgpu::Adapter adapter(adapters[0].Get());
  for (auto feature : kOptionalFeatures) {
    if (!adapter.HasFeature(feature)) {
      continue;
    }
    features.push_back(feature);

    // Enabling MSAARenderToSingleSampled causes performance regression without
    // TransientAttachments support.
    if (feature == wgpu::FeatureName::TransientAttachments &&
        adapter.HasFeature(wgpu::FeatureName::MSAARenderToSingleSampled)) {
      features.push_back(wgpu::FeatureName::MSAARenderToSingleSampled);
    }
  }

  descriptor.requiredFeatures = features.data();
  descriptor.requiredFeatureCount = std::size(features);

  std::vector<dawn::native::BackendValidationLevel> backend_validation_levels =
      {dawn::native::BackendValidationLevel::Disabled};
  if (features::kSkiaGraphiteDawnBackendValidation.Get()) {
    backend_validation_levels.push_back(
        dawn::native::BackendValidationLevel::Partial);
    backend_validation_levels.push_back(
        dawn::native::BackendValidationLevel::Full);
  }

  wgpu::Device device;
  // Try create device with backend validation level.
  for (auto it = backend_validation_levels.rbegin();
       it != backend_validation_levels.rend(); ++it) {
    instance_->SetBackendValidationLevel(*it);
    device = adapter.CreateDevice(&descriptor);
    if (device) {
      break;
    }
  }

  if (!device) {
    LOG(ERROR) << "Failed to create device.";
    return false;
  }

  device.SetUncapturedErrorCallback(&LogError, static_cast<void*>(this));
  device.SetDeviceLostCallback(&LogDeviceLost, static_cast<void*>(this));
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

bool DawnContextProvider::SupportsFeature(wgpu::FeatureName feature) {
  if (!device_) {
    return false;
  }

  return device_.HasFeature(feature);
}

absl::optional<error::ContextLostReason> DawnContextProvider::GetResetStatus()
    const {
  base::AutoLock auto_lock(context_lost_lock_);
  return context_lost_reason_;
}

void DawnContextProvider::OnError(WGPUErrorType error_type,
                                  const char* message) {
  LOG(ERROR) << message;

  static crash_reporter::CrashKeyString<1024> error_key("dawn-error");
  error_key.Set(message);

#if BUILDFLAG(IS_WIN)
  if (auto d3d11_device = GetD3D11Device()) {
    static crash_reporter::CrashKeyString<64> reason_message_key(
        "d3d11-device-removed-reason");
    HRESULT result = d3d11_device->GetDeviceRemovedReason();

    if (const char* result_string = HRESULTToString(result)) {
      LOG(ERROR) << "Device removed reason: " << result_string;
      reason_message_key.Set(result_string);
    } else {
      auto unknown_error = base::StringPrintf("Unknown error(0x%08lX)", result);
      LOG(ERROR) << "Device removed reason: " << unknown_error;
      reason_message_key.Set(unknown_error.c_str());
    }
  }
#endif

  base::debug::DumpWithoutCrashing();

  base::AutoLock auto_lock(context_lost_lock_);
  if (context_lost_reason_.has_value()) {
    return;
  }

  if (error_type == WGPUErrorType_OutOfMemory) {
    context_lost_reason_ = error::kOutOfMemory;
  } else if (error_type == WGPUErrorType_Validation) {
    context_lost_reason_ = error::kGuilty;
  } else {
    context_lost_reason_ = error::kUnknown;
  }
}

}  // namespace gpu
