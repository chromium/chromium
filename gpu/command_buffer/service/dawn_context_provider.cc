// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dawn_context_provider.h"

#include <memory>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/dcheck_is_on.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_arguments.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "gpu/command_buffer/service/dawn_instance.h"
#include "gpu/command_buffer/service/dawn_platform.h"
#include "gpu/command_buffer/service/skia_utils.h"
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

// Sets crash key in thread safe manner. This should be used for any crash keys
// set from dawn error or device lost callbacks that may run on multiple
// threads.
template <uint32_t KeySize>
void SetCrashKeyThreadSafe(crash_reporter::CrashKeyString<KeySize>& crash_key,
                           std::string_view message) {
  static base::NoDestructor<base::Lock> lock;
  base::AutoLock auto_lock(*lock.get());
  crash_key.Set(message);
}

void SetDawnErrorCrashKey(std::string_view message) {
  static crash_reporter::CrashKeyString<1024> error_key("dawn-error");
  SetCrashKeyThreadSafe(error_key, message);
}

std::vector<const char*> GetDisabledToggles(
    const GpuPreferences& gpu_preferences) {
  std::vector<const char*> disabled_toggles;
  for (const auto& toggle : gpu_preferences.disabled_dawn_features_list) {
    disabled_toggles.push_back(toggle.c_str());
  }
  return disabled_toggles;
}

std::vector<const char*> GetEnabledToggles(
    wgpu::BackendType backend_type,
    bool force_fallback_adapter,
    const GpuPreferences& gpu_preferences) {
  // If a new toggle is added here, ForceDawnTogglesForSkia() which collects
  // info for about:gpu should be updated as well.
  std::vector<const char*> enabled_toggles;
  for (const auto& toggle : gpu_preferences.enabled_dawn_features_list) {
    enabled_toggles.push_back(toggle.c_str());
  }
  // The following toggles are all device-scoped toggles so it's not necessary
  // to pass them when creating the Instance above.

  // Only enable backend labels on Windows or DCHECK builds on other platforms
  // since it can have non-trivial performance overhead e.g. with Metal.
#if DCHECK_IS_ON() || BUILDFLAG(IS_WIN)
  enabled_toggles.push_back("use_user_defined_labels_in_backend");
#endif

#if !DCHECK_IS_ON()
  if (features::kSkiaGraphiteDawnSkipValidation.Get()) {
    enabled_toggles.push_back("skip_validation");
  }
#endif
  enabled_toggles.push_back("disable_robustness");
  enabled_toggles.push_back("disable_lazy_clear_for_mapped_at_creation_buffer");

#if BUILDFLAG(IS_WIN)
  if (backend_type == wgpu::BackendType::D3D11) {
    // Use packed D24_UNORM_S8_UINT DXGI format for Depth24PlusStencil8
    // format.
    enabled_toggles.push_back("use_packed_depth24_unorm_stencil8_format");
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

  return enabled_toggles;
}

std::vector<wgpu::FeatureName> GetRequiredFeatures(
    wgpu::BackendType backend_type,
    wgpu::Adapter adapter) {
  std::vector<wgpu::FeatureName> features = {
      wgpu::FeatureName::DawnInternalUsages,
      wgpu::FeatureName::ImplicitDeviceSynchronization,
#if BUILDFLAG(IS_ANDROID)
      wgpu::FeatureName::TextureCompressionETC2,
#endif
  };

#if BUILDFLAG(IS_ANDROID)
  if (backend_type == wgpu::BackendType::Vulkan) {
    features.push_back(wgpu::FeatureName::StaticSamplers);
    features.push_back(wgpu::FeatureName::YCbCrVulkanSamplers);
  }
#elif BUILDFLAG(IS_WIN)
  if (backend_type == wgpu::BackendType::D3D11) {
    features.push_back(wgpu::FeatureName::D3D11MultithreadProtected);
  }
#endif

  constexpr wgpu::FeatureName kOptionalFeatures[] = {
      wgpu::FeatureName::BGRA8UnormStorage,
      wgpu::FeatureName::BufferMapExtendedUsages,
      wgpu::FeatureName::DawnMultiPlanarFormats,
      wgpu::FeatureName::DualSourceBlending,
      wgpu::FeatureName::FramebufferFetch,
      wgpu::FeatureName::MultiPlanarFormatExtendedUsages,
      wgpu::FeatureName::MultiPlanarFormatNv16,
      wgpu::FeatureName::MultiPlanarFormatNv24,
      wgpu::FeatureName::MultiPlanarFormatP010,
      wgpu::FeatureName::MultiPlanarFormatP210,
      wgpu::FeatureName::MultiPlanarFormatP410,
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

      // The following features are always supported by the the D3D backends.
      wgpu::FeatureName::SharedTextureMemoryD3D11Texture2D,
      wgpu::FeatureName::SharedTextureMemoryDXGISharedHandle,
      wgpu::FeatureName::SharedFenceDXGISharedHandle,

      wgpu::FeatureName::TransientAttachments,

      wgpu::FeatureName::DawnLoadResolveTexture,
      wgpu::FeatureName::DawnPartialLoadResolveTexture,
  };

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

  return features;
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
  NOTREACHED_IN_MIGRATION();
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

// static
bool DawnContextProvider::DefaultValidateAdapterFn(wgpu::BackendType,
                                                   wgpu::Adapter) {
  return true;
}

// Owns the dawn instance/adapter/device so that it's lifetime is not linked to
// a specific DawnContextProvider.
class DawnSharedContext : public base::RefCountedThreadSafe<DawnSharedContext>,
                          public base::trace_event::MemoryDumpProvider {
 public:
  DawnSharedContext() = default;

  bool Initialize(wgpu::BackendType backend_type,
                  bool force_fallback_adapter,
                  const GpuPreferences& gpu_preferences,
                  DawnContextProvider::ValidateAdapterFn validate_adapter_fn,
                  const GpuDriverBugWorkarounds& gpu_driver_workarounds);
  void SetCachingInterface(
      std::unique_ptr<webgpu::DawnCachingInterface> caching_interface);

  wgpu::Device GetDevice() const { return device_; }
  wgpu::BackendType backend_type() const { return backend_type_; }
  bool is_vulkan_swiftshader_adapter() const {
    return is_vulkan_swiftshader_adapter_;
  }
  wgpu::Adapter GetAdapter() const { return adapter_; }
  wgpu::Instance GetInstance() const { return instance_->Get(); }

#if BUILDFLAG(IS_WIN)
  Microsoft::WRL::ComPtr<ID3D11Device> GetD3D11Device() const {
    if (backend_type() == wgpu::BackendType::D3D11) {
      return dawn::native::d3d11::GetD3D11Device(device_.Get());
    }
    return nullptr;
  }
#endif

  std::optional<error::ContextLostReason> GetResetStatus() const;

 private:
  friend class base::RefCountedThreadSafe<DawnSharedContext>;

  // Provided to wgpu::Device as caching callback.
  static size_t LoadCachedData(const void* key,
                               size_t key_size,
                               void* value,
                               size_t value_size,
                               void* userdata) {
    if (auto& caching_interface =
            static_cast<DawnSharedContext*>(userdata)->caching_interface_) {
      return caching_interface->LoadData(key, key_size, value, value_size);
    }
    return 0;
  }

  // Provided to wgpu::Device as caching callback.
  static void StoreCachedData(const void* key,
                              size_t key_size,
                              const void* value,
                              size_t value_size,
                              void* userdata) {
    if (auto& caching_interface =
            static_cast<DawnSharedContext*>(userdata)->caching_interface_) {
      caching_interface->StoreData(key, key_size, value, value_size);
    }
  }

  // Provided to wgpu::Device as logging callback.
  static void LogInfo(WGPULoggingType type,
                      char const* message,
                      void* userdata) {
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

  ~DawnSharedContext() override;

  void OnError(wgpu::ErrorType error_type, const char* message);

  // base::trace_event::MemoryDumpProvider implementation:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  std::unique_ptr<webgpu::DawnCachingInterface> caching_interface_;

  Platform platform_{/*dawn_caching_interface=*/nullptr,
                     /*uma_prefix=*/"GPU.GraphiteDawn.",
                     /*record_cache_count_uma=*/true};
  std::unique_ptr<webgpu::DawnInstance> instance_;
  wgpu::Adapter adapter_;
  wgpu::Device device_;
  wgpu::BackendType backend_type_;
  bool is_vulkan_swiftshader_adapter_ = false;
  bool registered_memory_dump_provider_ = false;

  mutable base::Lock context_lost_lock_;
  std::optional<error::ContextLostReason> context_lost_reason_
      GUARDED_BY(context_lost_lock_);
};

DawnSharedContext::~DawnSharedContext() {
  if (device_) {
    if (registered_memory_dump_provider_) {
      base::trace_event::MemoryDumpManager::GetInstance()
          ->UnregisterDumpProvider(this);
    }
    device_.SetLoggingCallback(nullptr, nullptr);
    // Destroy the device now so that the lost callback, which references this
    // class, is fired now before we clean up the rest of this class.
    device_.Destroy();
  }
  if (instance_) {
    instance_->DisconnectDawnPlatform();
  }
}

bool DawnSharedContext::Initialize(
    wgpu::BackendType backend_type,
    bool force_fallback_adapter,
    const GpuPreferences& gpu_preferences,
    DawnContextProvider::ValidateAdapterFn validate_adapter_fn,
    const GpuDriverBugWorkarounds& gpu_driver_workarounds) {
  // Make Dawn experimental API and WGSL features available since access to this
  // instance doesn't exit the GPU process.
  // LogInfo will be used to receive instance level errors. For example failures
  // of loading libraries, initializing backend, etc
  instance_ = webgpu::DawnInstance::Create(
      &platform_, gpu_preferences, webgpu::SafetyLevel::kUnsafe,
      &DawnSharedContext::LogInfo, nullptr);

  std::vector<const char*> enabled_toggles =
      GetEnabledToggles(backend_type, force_fallback_adapter, gpu_preferences);
  std::vector<const char*> disabled_toggles =
      GetDisabledToggles(gpu_preferences);

  wgpu::DawnTogglesDescriptor toggles_desc;
  toggles_desc.enabledToggles = enabled_toggles.data();
  toggles_desc.disabledToggles = disabled_toggles.data();
  toggles_desc.enabledToggleCount = enabled_toggles.size();
  toggles_desc.disabledToggleCount = disabled_toggles.size();

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
  adapter_ = wgpu::Adapter(adapters[0].Get());

  if (!validate_adapter_fn(backend_type, adapter_)) {
    return false;
  }

  // Start initializing dawn device here.
  wgpu::DawnCacheDeviceDescriptor cache_desc;
  cache_desc.loadDataFunction = &DawnSharedContext::LoadCachedData;
  cache_desc.storeDataFunction = &DawnSharedContext::StoreCachedData;
  // The dawn device is owned by this so a pointer back here is safe.
  cache_desc.functionUserdata = this;
  cache_desc.nextInChain = &toggles_desc;

  wgpu::DeviceDescriptor descriptor;
  descriptor.SetUncapturedErrorCallback(
      [](const wgpu::Device&, wgpu::ErrorType type, const char* message,
         DawnSharedContext* state) {
        if (type != wgpu::ErrorType::NoError) {
          state->OnError(type, message);
        }
      },
      this);
  descriptor.SetDeviceLostCallback(
      wgpu::CallbackMode::AllowSpontaneous,
      [](const wgpu::Device&, wgpu::DeviceLostReason reason,
         const char* message, DawnSharedContext* state) {
        if (reason != wgpu::DeviceLostReason::Destroyed) {
          state->OnError(wgpu::ErrorType::DeviceLost, message);
        }
      },
      this);
  descriptor.nextInChain = &cache_desc;

  std::vector<wgpu::FeatureName> features =
      GetRequiredFeatures(backend_type, adapter_);
  descriptor.requiredFeatures = features.data();
  descriptor.requiredFeatureCount = std::size(features);

  // Use best limits for the device.
  wgpu::SupportedLimits supportedLimits = {};
  if (adapter_.GetLimits(&supportedLimits) != wgpu::Status::Success) {
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
    LOG(ERROR) << "DawnSharedContext creation failed for testing";
    return false;
  }

  // Try create device with backend validation level.
  for (auto it = backend_validation_levels.rbegin();
       it != backend_validation_levels.rend(); ++it) {
    auto level = *it;
    instance_->SetBackendValidationLevel(level);
    device_ = adapter_.CreateDevice(&descriptor);
    if (device_) {
      break;
    }
  }

  if (!device_) {
    LOG(ERROR) << "Failed to create device.";
    return false;
  }

  device_.SetLoggingCallback(&DawnSharedContext::LogInfo, nullptr);

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

  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "DawnSharedContext",
        base::SingleThreadTaskRunner::GetCurrentDefault());
    registered_memory_dump_provider_ = true;
  }

  return true;
}

void DawnSharedContext::SetCachingInterface(
    std::unique_ptr<webgpu::DawnCachingInterface> caching_interface) {
  CHECK(!caching_interface_);
  caching_interface_ = std::move(caching_interface);
}

std::optional<error::ContextLostReason> DawnSharedContext::GetResetStatus()
    const {
  base::AutoLock auto_lock(context_lost_lock_);
  return context_lost_reason_;
}

void DawnSharedContext::OnError(wgpu::ErrorType error_type,
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
      SetCrashKeyThreadSafe(reason_message_key, result_string);
    } else {
      auto unknown_error = base::StringPrintf("Unknown error(0x%08lX)", result);
      LOG(ERROR) << "Device removed reason: " << unknown_error;
      SetCrashKeyThreadSafe(reason_message_key, unknown_error.c_str());
    }
  }
#endif

  base::debug::DumpWithoutCrashing();

#if !DCHECK_IS_ON()
  // Do not provoke context loss on validation failures for non-DCHECK builds.
  // We want to capture the above dump on validation errors, but not necessarily
  // restart the GPU process unless we also have a device loss.
  if (error_type == wgpu::ErrorType::Validation) {
    return;
  }
#endif

  base::AutoLock auto_lock(context_lost_lock_);
  if (context_lost_reason_.has_value()) {
    return;
  }

  switch (error_type) {
    case wgpu::ErrorType::OutOfMemory:
      context_lost_reason_ = error::kOutOfMemory;
      break;
    case wgpu::ErrorType::Validation:
      context_lost_reason_ = error::kGuilty;
      break;
    default:
      context_lost_reason_ = error::kUnknown;
      break;
  }
}

namespace {
static constexpr char kDawnMemoryDumpPrefix[] = "gpu/dawn";

class DawnMemoryDump : public dawn::native::MemoryDump {
 public:
  explicit DawnMemoryDump(base::trace_event::ProcessMemoryDump* pmd)
      : pmd_(pmd) {
    CHECK(pmd_);
  }

  ~DawnMemoryDump() override = default;

  void AddScalar(const char* name,
                 const char* key,
                 const char* units,
                 uint64_t value) override {
    pmd_->GetOrCreateAllocatorDump(
            base::JoinString({kDawnMemoryDumpPrefix, name}, "/"))
        ->AddScalar(key, units, value);
  }

  void AddString(const char* name,
                 const char* key,
                 const std::string& value) override {
    pmd_->GetOrCreateAllocatorDump(
            base::JoinString({kDawnMemoryDumpPrefix, name}, "/"))
        ->AddString(key, "", value);
  }

 private:
  const raw_ptr<base::trace_event::ProcessMemoryDump> pmd_;
};
}  // namespace

bool DawnSharedContext::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  if (args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::kBackground) {
    using base::trace_event::MemoryAllocatorDump;
    pmd->GetOrCreateAllocatorDump(kDawnMemoryDumpPrefix)
        ->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes,
                    dawn::native::ComputeEstimatedMemoryUsage(device_.Get()));
  } else {
    DawnMemoryDump dump(pmd);
    dawn::native::DumpMemoryStatistics(device_.Get(), &dump);
  }
  return true;
}

std::unique_ptr<DawnContextProvider> DawnContextProvider::Create(
    const GpuPreferences& gpu_preferences,
    ValidateAdapterFn validate_adapter_fn,
    const GpuDriverBugWorkarounds& gpu_driver_workarounds) {
  return DawnContextProvider::CreateWithBackend(
      GetDefaultBackendType(), DefaultForceFallbackAdapter(), gpu_preferences,
      validate_adapter_fn, gpu_driver_workarounds);
}

std::unique_ptr<DawnContextProvider> DawnContextProvider::CreateWithBackend(
    wgpu::BackendType backend_type,
    bool force_fallback_adapter,
    const GpuPreferences& gpu_preferences,
    ValidateAdapterFn validate_adapter_fn,
    const GpuDriverBugWorkarounds& gpu_driver_workarounds) {
  auto dawn_shared_context = base::MakeRefCounted<DawnSharedContext>();
  if (!dawn_shared_context->Initialize(backend_type, force_fallback_adapter,
                                       gpu_preferences, validate_adapter_fn,
                                       gpu_driver_workarounds)) {
    return nullptr;
  }

  return base::WrapUnique(
      new DawnContextProvider(std::move(dawn_shared_context)));
}

std::unique_ptr<DawnContextProvider>
DawnContextProvider::CreateWithSharedDevice(
    const DawnContextProvider* existing) {
  CHECK(existing);
  CHECK(existing->dawn_shared_context_);
  return base::WrapUnique(
      new DawnContextProvider(existing->dawn_shared_context_));
}

DawnContextProvider::DawnContextProvider(
    scoped_refptr<DawnSharedContext> dawn_shared_context)
    : dawn_shared_context_(std::move(dawn_shared_context)) {
  CHECK(dawn_shared_context_);
}

DawnContextProvider::~DawnContextProvider() = default;

wgpu::Device DawnContextProvider::GetDevice() const {
  return dawn_shared_context_->GetDevice();
}

wgpu::BackendType DawnContextProvider::backend_type() const {
  return dawn_shared_context_->backend_type();
}

bool DawnContextProvider::is_vulkan_swiftshader_adapter() const {
  return dawn_shared_context_->is_vulkan_swiftshader_adapter();
}

wgpu::Adapter DawnContextProvider::GetAdapter() const {
  return dawn_shared_context_->GetAdapter();
}

wgpu::Instance DawnContextProvider::GetInstance() const {
  return dawn_shared_context_->GetInstance();
}

bool DawnContextProvider::InitializeGraphiteContext(
    const skgpu::graphite::ContextOptions& context_options) {
  CHECK(!graphite_context_);

  if (auto device = GetDevice()) {
    skgpu::graphite::DawnBackendContext backend_context;
    backend_context.fInstance = GetInstance();
    backend_context.fDevice = device;
    backend_context.fQueue = device.GetQueue();

    graphite_context_ = skgpu::graphite::ContextFactory::MakeDawn(
        backend_context, context_options);
  }

  return !!graphite_context_;
}

void DawnContextProvider::SetCachingInterface(
    std::unique_ptr<webgpu::DawnCachingInterface> caching_interface) {
  CHECK(dawn_shared_context_->HasOneRef());
  CHECK(!graphite_context_);
  dawn_shared_context_->SetCachingInterface(std::move(caching_interface));
}

#if BUILDFLAG(IS_WIN)
Microsoft::WRL::ComPtr<ID3D11Device> DawnContextProvider::GetD3D11Device()
    const {
  return dawn_shared_context_->GetD3D11Device();
}
#endif

bool DawnContextProvider::SupportsFeature(wgpu::FeatureName feature) {
  return dawn_shared_context_->GetDevice().HasFeature(feature);
}

std::optional<error::ContextLostReason> DawnContextProvider::GetResetStatus()
    const {
  return dawn_shared_context_->GetResetStatus();
}

}  // namespace gpu
