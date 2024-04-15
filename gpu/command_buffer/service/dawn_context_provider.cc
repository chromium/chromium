// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dawn_context_provider.h"

#include <memory>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
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
#include "gpu/config/gpu_util.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/dawn/DawnBackendContext.h"
#include "third_party/skia/include/gpu/graphite/dawn/DawnUtils.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(IS_WIN)
#include <d3d11_4.h>

#include "third_party/dawn/include/dawn/native/D3D11Backend.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_angle_util_win.h"
#endif

namespace gpu {
namespace {

// Used as a flag to test dawn initialization failure.
BASE_FEATURE(kForceDawnInitializeFailure,
             "ForceDawnInitializeFailure",
             base::FEATURE_DISABLED_BY_DEFAULT);

void SetDawnErrorCrashKey(std::string_view message) {
  static crash_reporter::CrashKeyString<1024> error_key("dawn-error");
  error_key.Set(message);
}

void LogInfo(WGPULoggingType type, char const* message, void* userdata) {
  switch (static_cast<wgpu::LoggingType>(type)) {
    case wgpu::LoggingType::Warning:
      LOG(WARNING) << message;
      break;
    case wgpu::LoggingType::Error:
      LOG(ERROR) << message;
      SetDawnErrorCrashKey(message);
      base::debug::DumpWithoutCrashing();
      break;
    default:
      break;
  }
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

bool IsD3D11DebugLayerEnabled(
    const Microsoft::WRL::ComPtr<ID3D11Device>& d3d11_device) {
  Microsoft::WRL::ComPtr<ID3D11Debug> d3d11_debug;
  return SUCCEEDED(d3d11_device.As(&d3d11_debug));
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
    const GpuDriverBugWorkarounds& gpu_driver_workarounds) {
  return DawnContextProvider::CreateWithBackend(
      GetDefaultBackendType(), DefaultForceFallbackAdapter(), gpu_preferences,
      gpu_driver_workarounds);
}

std::unique_ptr<DawnContextProvider> DawnContextProvider::CreateWithBackend(
    wgpu::BackendType backend_type,
    bool force_fallback_adapter,
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& gpu_driver_workarounds) {
  auto context_provider = base::WrapUnique(new DawnContextProvider());

  // TODO(rivr): This may return a GPU that is not the active one. Currently
  // the only known way to avoid this is platform-specific; e.g. on Mac, create
  // a Dawn device, get the actual Metal device from it, and compare against
  // MTLCreateSystemDefaultDevice().
  if (!context_provider->Initialize(backend_type, force_fallback_adapter,
                                    gpu_preferences, gpu_driver_workarounds)) {
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
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
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

DawnContextProvider::DawnContextProvider() = default;

DawnContextProvider::~DawnContextProvider() {
  if (device_) {
    device_.SetUncapturedErrorCallback(nullptr, nullptr);
    device_.SetDeviceLostCallback(nullptr, nullptr);
    device_.SetLoggingCallback(nullptr, nullptr);
  }
  if (instance_) {
    instance_->DisconnectDawnPlatform();
  }
}

bool DawnContextProvider::Initialize(
    wgpu::BackendType backend_type,
    bool force_fallback_adapter,
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& gpu_driver_workarounds) {
  platform_ = std::make_unique<Platform>(nullptr,
                                         /*uma_prefix=*/"GPU.GraphiteDawn.");

  // Make Dawn experimental API and WGSL features available since access to this
  // instance doesn't exit the GPU process.
  // LogInfo will be used to receive instance level errors. For example failures
  // of loading libraries, initializing backend, etc
  instance_ = webgpu::DawnInstance::Create(platform_.get(), gpu_preferences,
                                           webgpu::SafetyLevel::kUnsafe,
                                           LogInfo, nullptr);

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
  enabled_toggles.push_back("disable_lazy_clear_for_mapped_at_creation_buffer");

#if BUILDFLAG(IS_WIN)
  // ClearRenderTargetView() is buggy with some GPUs, so use draw instead.
  // TODO(crbug.com/329702368): only enable color_clear_with_draw for GPUs with
  // the issue.
  if (backend_type == wgpu::BackendType::D3D11) {
    enabled_toggles.push_back("clear_color_with_draw");
  }
#endif

  // Skip expensive swiftshader vkCmdDraw* for tests.
  // TODO(penghuang): rename kDisableGLDrawingForTests to
  // kDisableGpuDrawingForTests
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (backend_type == wgpu::BackendType::Vulkan && force_fallback_adapter &&
      command_line->HasSwitch(switches::kDisableGLDrawingForTests)) {
    enabled_toggles.push_back("vulkan_skip_draw");
  }

  wgpu::DawnTogglesDescriptor toggles_desc;
  toggles_desc.enabledToggles = enabled_toggles.data();
  toggles_desc.disabledToggles = disabled_toggles.data();
  toggles_desc.enabledToggleCount = enabled_toggles.size();
  toggles_desc.disabledToggleCount = disabled_toggles.size();

  wgpu::DawnCacheDeviceDescriptor cache_desc;
  cache_desc.loadDataFunction = &DawnContextProvider::LoadCachedData;
  cache_desc.storeDataFunction = &DawnContextProvider::StoreCachedData;
  // The dawn device is owned by this so a pointer back here is safe.
  cache_desc.functionUserdata = this;
  cache_desc.nextInChain = &toggles_desc;

  wgpu::DeviceDescriptor descriptor;
  descriptor.nextInChain = &cache_desc;

  std::vector<wgpu::FeatureName> features = {
      wgpu::FeatureName::DawnInternalUsages,
      wgpu::FeatureName::ImplicitDeviceSynchronization,
      wgpu::FeatureName::SurfaceCapabilities,
#if BUILDFLAG(IS_ANDROID)
      wgpu::FeatureName::TextureCompressionETC2,
#endif
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

  dawn::native::d3d::RequestAdapterOptionsLUID adapter_options_luid;
  if ((adapter_options.backendType == wgpu::BackendType::D3D11 ||
       adapter_options.backendType == wgpu::BackendType::D3D12) &&
      GetANGLED3D11DeviceLUID(&adapter_options_luid.adapterLUID)) {
    // Request the GPU that ANGLE is using if possible.
    adapter_options_luid.nextInChain = adapter_options.nextInChain;
    adapter_options.nextInChain = &adapter_options_luid;
  }

  // Share D3D11 device with ANGLE to reduce synchronization overhead.
  dawn::native::d3d11::RequestAdapterOptionsD3D11Device
      adapter_options_d3d11_device;
  if (adapter_options.backendType == wgpu::BackendType::D3D11) {
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
        gl::QueryD3D11DeviceObjectFromANGLE();
    CHECK(d3d11_device) << "Query d3d11 device from ANGLE failed.";

    static crash_reporter::CrashKeyString<16> feature_level_key(
        "d3d11-feature-level");
    std::string feature_level =
        D3DFeatureLevelToString(d3d11_device->GetFeatureLevel());
    feature_level_key.Set(feature_level.c_str());

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_device_context;
    d3d11_device->GetImmediateContext(&d3d11_device_context);

    Microsoft::WRL::ComPtr<ID3D11Multithread> d3d11_multithread;
    HRESULT hr = d3d11_device_context.As(&d3d11_multithread);
    CHECK(SUCCEEDED(hr)) << "Query ID3D11Multithread interface failed: 0x"
                         << std::hex << hr;

    // Dawn requires enable multithread protection for d3d11 device.
    d3d11_multithread->SetMultithreadProtected(TRUE);
    adapter_options_d3d11_device.device = std::move(d3d11_device);
    adapter_options_d3d11_device.nextInChain = adapter_options.nextInChain;
    adapter_options.nextInChain = &adapter_options_d3d11_device;
  }
#endif  // BUILDFLAG(IS_WIN)

  adapter_options.compatibilityMode = false;
  std::vector<dawn::native::Adapter> adapters =
      instance_->EnumerateAdapters(&adapter_options);

#if !BUILDFLAG(IS_WIN)
  // Not fallback to compatibility mode due to rendering issue with d3d11
  // feature level 11.0
  if (adapters.empty()) {
    LOG(ERROR) << "No adapters found for non compatibility mode.";
    adapter_options.compatibilityMode = true;
    adapters = instance_->EnumerateAdapters(&adapter_options);
  }
#endif

  if (adapters.empty()) {
    LOG(ERROR) << "No adapters found.";
    return false;
  }

  const wgpu::FeatureName kOptionalFeatures[] = {
      wgpu::FeatureName::BGRA8UnormStorage,
      wgpu::FeatureName::BufferMapExtendedUsages,
      wgpu::FeatureName::DawnMultiPlanarFormats,
      wgpu::FeatureName::DualSourceBlending,
      wgpu::FeatureName::FramebufferFetch,
      wgpu::FeatureName::MultiPlanarFormatExtendedUsages,
      wgpu::FeatureName::MultiPlanarFormatP010,
      wgpu::FeatureName::MultiPlanarFormatNv12a,
      wgpu::FeatureName::MultiPlanarRenderTargets,
      wgpu::FeatureName::Unorm16TextureFormats,

      // The following features are always supported by the the Metal backend on
      // the Mac versions on which Chrome runs.
      wgpu::FeatureName::SharedTextureMemoryIOSurface,
      wgpu::FeatureName::SharedFenceMTLSharedEvent,

      // The following features are always supported when running on the Vulkan
      // backend on Android.
      wgpu::FeatureName::SharedTextureMemoryAHardwareBuffer,
      wgpu::FeatureName::SharedFenceVkSemaphoreSyncFD,

      // The following features are always supported when running on the Vulkan
      // backend on Ozone.
      wgpu::FeatureName::SharedTextureMemoryDmaBuf,
      wgpu::FeatureName::SharedFenceVkSemaphoreOpaqueFD,

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

  // Use best limits for the device.
  wgpu::SupportedLimits supportedLimits = {};
  if (!adapter.GetLimits(&supportedLimits)) {
    LOG(ERROR) << "Failed to call adapter.GetLimits().";
    return false;
  }

  wgpu::RequiredLimits deviceCreationLimits = {};
  deviceCreationLimits.limits = supportedLimits.limits;
  descriptor.requiredLimits = &deviceCreationLimits;

  // ANGLE always tries creating D3D11 device with debug layer when dcheck is
  // on, so tries creating dawn device with backend validation as well.
  constexpr bool enable_backend_validation =
      DCHECK_IS_ON() && BUILDFLAG(IS_WIN);

  std::vector<dawn::native::BackendValidationLevel> backend_validation_levels =
      {dawn::native::BackendValidationLevel::Disabled};
  if (features::kSkiaGraphiteDawnBackendValidation.Get() ||
      enable_backend_validation) {
    backend_validation_levels.push_back(
        dawn::native::BackendValidationLevel::Partial);
    backend_validation_levels.push_back(
        dawn::native::BackendValidationLevel::Full);
  }

  if (base::FeatureList::IsEnabled(kForceDawnInitializeFailure)) {
    LOG(ERROR) << "DawnContextProvider creation failed for testing";
    return false;
  }

  wgpu::Device device;
  // Try create device with backend validation level.
  for (auto it = backend_validation_levels.rbegin();
       it != backend_validation_levels.rend(); ++it) {
    auto level = *it;
    instance_->SetBackendValidationLevel(level);
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

  adapter_ = std::move(adapter);
  device_ = std::move(device);

  backend_type_ = backend_type;
  is_vulkan_swiftshader_adapter_ =
      backend_type == wgpu::BackendType::Vulkan && force_fallback_adapter;

#if BUILDFLAG(IS_WIN)
  if (auto d3d11_device = GetD3D11Device()) {
    static auto* crash_key = base::debug::AllocateCrashKeyString(
        "d3d11-debug-layer", base::debug::CrashKeySize::Size32);
    const bool enabled = IsD3D11DebugLayerEnabled(d3d11_device);
    base::debug::SetCrashKeyString(crash_key, enabled ? "enabled" : "disabled");
  }
#endif  // BUILDFLAG(IS_WIN)

  return true;
}

bool DawnContextProvider::InitializeGraphiteContext(
    const skgpu::graphite::ContextOptions& options) {
  CHECK(!graphite_context_);

  if (device_) {
    skgpu::graphite::DawnBackendContext backend_context;
    backend_context.fInstance = GetInstance();
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

void DawnContextProvider::SetCachingInterface(
    std::unique_ptr<webgpu::DawnCachingInterface> caching_interface) {
  CHECK(!caching_interface_);
  caching_interface_ = std::move(caching_interface);
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

std::optional<error::ContextLostReason> DawnContextProvider::GetResetStatus()
    const {
  base::AutoLock auto_lock(context_lost_lock_);
  return context_lost_reason_;
}

void DawnContextProvider::OnError(WGPUErrorType error_type,
                                  const char* message) {
  LOG(ERROR) << message;
  SetDawnErrorCrashKey(message);

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

// static
size_t DawnContextProvider::LoadCachedData(const void* key,
                                           size_t key_size,
                                           void* value,
                                           size_t value_size,
                                           void* userdata) {
  auto* context_provider = static_cast<DawnContextProvider*>(userdata);
  if (!context_provider->caching_interface_) {
    return 0;
  }

  return context_provider->caching_interface_->LoadData(key, key_size, value,
                                                        value_size);
}

// static
void DawnContextProvider::StoreCachedData(const void* key,
                                          size_t key_size,
                                          const void* value,
                                          size_t value_size,
                                          void* userdata) {
  auto* context_provider = static_cast<DawnContextProvider*>(userdata);
  if (context_provider->caching_interface_) {
    context_provider->caching_interface_->StoreData(key, key_size, value,
                                                    value_size);
  }
}

}  // namespace gpu
