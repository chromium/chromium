// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dawn_context_provider.h"

#include <memory>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_checker.h"
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
#include "third_party/dawn/include/dawn/webgpu_cpp_print.h"
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

#if BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
#include "third_party/dawn/include/dawn/native/OpenGLBackend.h"
#include "ui/gl/gl_surface_egl.h"
#endif

namespace gpu {
namespace {

// Used as a flag to test dawn initialization failure.
BASE_FEATURE(kForceDawnInitializeFailure, base::FEATURE_DISABLED_BY_DEFAULT);

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

// Different versions of DumpWithoutCrashing for different reasons.
// Deliberately prevent inlining so that the crash report's call stack can
// distinguish between them.
#if BUILDFLAG(IS_WIN)
NOINLINE NOOPT void DumpWithoutCrashingOnDXGIError(wgpu::ErrorType error_type,
                                                   std::string_view message) {
  LOG(ERROR) << "DXGI Error: " << message;

  if (features::kSkiaGraphiteDawnDumpWCOnD3DError.Get()) {
    base::debug::DumpWithoutCrashing();
  }
}

NOINLINE NOOPT void DumpWithoutCrashingOnD3D11DebugLayerError(
    wgpu::ErrorType error_type,
    std::string_view message) {
  LOG(ERROR) << message;
  if (features::kSkiaGraphiteDawnDumpWCOnD3DError.Get()) {
    base::debug::DumpWithoutCrashing();
  }
}
#endif

NOINLINE NOOPT void DumpWithoutCrashingOnGenericError(
    wgpu::ErrorType error_type,
    std::string_view message) {
  LOG(ERROR) << message;
  base::debug::DumpWithoutCrashing();
}

void DumpWithoutCrashingOnError(wgpu::ErrorType error_type,
                                std::string_view message) {
  SetDawnErrorCrashKey(message);
#if BUILDFLAG(IS_WIN)
  if (message.find("DXGI_ERROR") != std::string_view::npos) {
    DumpWithoutCrashingOnDXGIError(error_type, message);
  } else if (message.find("The D3D11 debug layer") != std::string_view::npos) {
    DumpWithoutCrashingOnD3D11DebugLayerError(error_type, message);
  } else
#endif
  {
    DumpWithoutCrashingOnGenericError(error_type, message);
  }
}

// NOTE: Update the toggles in GpuInfoCollector whenever a toggle is disabled
// here.
std::vector<const char*> GetDisabledToggles(
    const GpuPreferences& gpu_preferences) {
  std::vector<const char*> disabled_toggles;
  for (const auto& toggle : gpu_preferences.disabled_dawn_features_list) {
    disabled_toggles.push_back(toggle.c_str());
  }
  return disabled_toggles;
}

// NOTE: Update the toggles in GpuInfoCollector whenever a toggle is enabled
// here.
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
  if (features::kSkiaGraphiteDawnBackendDebugLabels.Get()) {
    enabled_toggles.push_back("use_user_defined_labels_in_backend");
  }

  if (features::kSkiaGraphiteDawnSkipValidation.Get()) {
    enabled_toggles.push_back("skip_validation");
  }

  enabled_toggles.push_back("disable_robustness");
  enabled_toggles.push_back("disable_lazy_clear_for_mapped_at_creation_buffer");

#if BUILDFLAG(IS_WIN)
  if (backend_type == wgpu::BackendType::D3D11) {
    // Use packed D24_UNORM_S8_UINT DXGI format for Depth24PlusStencil8
    // format.
    enabled_toggles.push_back("use_packed_depth24_unorm_stencil8_format");

    if (features::kSkiaGraphiteDawnD3D11DelayFlush.Get()) {
      // Tell Dawn to defer sending commands to GPU until swapchain's Present.
      // This will batch the commands better.
      enabled_toggles.push_back("d3d11_delay_flush_to_gpu");
    }
  }

  if (backend_type == wgpu::BackendType::D3D11 ||
      backend_type == wgpu::BackendType::D3D12) {
    if (features::kSkiaGraphiteDawnDisableD3DShaderOptimizations.Get()) {
      enabled_toggles.push_back("d3d_skip_shader_optimizations");
    }
  }
#endif

  if (backend_type == wgpu::BackendType::Vulkan) {
#if BUILDFLAG(IS_ANDROID)
    // Enable this toggle for all Android devices suspecting vulkan image size
    // mismatch causing SharedTextureMemory creation failures, leading to
    // promise image creation failures. See https://crbug.com/377935752 for
    // details.
    enabled_toggles.push_back(
        "ignore_imported_ahardwarebuffer_vulkan_image_size");
#endif

    // Use a single VkPipelineCache inside dawn.
    enabled_toggles.push_back("vulkan_monolithic_pipeline_cache");
  }

  // Skip expensive swiftshader vkCmdDraw* for tests.
  // TODO(penghuang): rename kDisableGLDrawingForTests to
  // kDisableGpuDrawingForTests
  // TODO(crbug.com/407497928): Enable this toggle over GpuInfoCollector.
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

  if (backend_type == wgpu::BackendType::Vulkan) {
#if BUILDFLAG(IS_ANDROID)
    features.push_back(wgpu::FeatureName::StaticSamplers);
    features.push_back(wgpu::FeatureName::YCbCrVulkanSamplers);
#endif
    features.push_back(wgpu::FeatureName::DawnDeviceAllocatorControl);
  }

#if BUILDFLAG(IS_WIN)
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
      wgpu::FeatureName::SharedFenceSyncFD,

      // The following features are always supported by the the D3D backends.
      wgpu::FeatureName::SharedTextureMemoryD3D11Texture2D,
      wgpu::FeatureName::SharedTextureMemoryDXGISharedHandle,
      wgpu::FeatureName::SharedFenceDXGISharedHandle,

      // The following feature is always supported by the the D3D12 backend.
      wgpu::FeatureName::SharedBufferMemoryD3D12Resource,

      wgpu::FeatureName::TransientAttachments,

      wgpu::FeatureName::DawnLoadResolveTexture,
      wgpu::FeatureName::DawnPartialLoadResolveTexture,
      wgpu::FeatureName::DawnTexelCopyBufferRowAlignment,
      wgpu::FeatureName::FlexibleTextureViews,
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
  HRESULT hr = d3d11_device.As(&dxgi_device);
  CHECK_EQ(hr, S_OK);

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

const char* BackendTypeToString(wgpu::BackendType backend_type) {
  switch (backend_type) {
    case wgpu::BackendType::D3D11:
      return "D3D11";
    case wgpu::BackendType::D3D12:
      return "D3D12";
    case wgpu::BackendType::Metal:
      return "Metal";
    case wgpu::BackendType::OpenGL:
      return "OpenGL";
    case wgpu::BackendType::OpenGLES:
      return "OpenGLES";
    case wgpu::BackendType::Vulkan:
      return "Vulkan";
    default:
      CHECK(false);
  }
}

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
  } else if (switch_value == switches::kSkiaGraphiteBackendDawnOpenGLES) {
    return wgpu::BackendType::OpenGLES;
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
  DawnSharedContext(gl::ProgressReporter* progress_reporter,
                    bool thread_safe_graphite_context);

  bool Initialize(wgpu::BackendType backend_type,
                  bool force_fallback_adapter,
                  const GpuPreferences& gpu_preferences,
                  const GpuDriverBugWorkarounds& workarounds,
                  DawnContextProvider::ValidateAdapterFn validate_adapter_fn);
  void SetCachingInterface(
      std::unique_ptr<webgpu::DawnCachingInterface> dawn_caching_interface);
  void SetCachingInterface(scoped_refptr<GpuPersistentCache> persistent_cache);

  wgpu::Device GetDevice() const { return device_; }
  wgpu::BackendType backend_type() const { return backend_type_; }
  bool is_vulkan_swiftshader_adapter() const {
    return is_vulkan_swiftshader_adapter_;
  }
  wgpu::Adapter GetAdapter() const { return adapter_; }
  wgpu::Instance GetInstance() const { return instance_->Get(); }

  webgpu::DawnPlatform* GetDawnPlatform() { return &platform_; }

#if BUILDFLAG(IS_WIN)
  Microsoft::WRL::ComPtr<ID3D11Device> GetD3D11Device() const {
    if (backend_type() == wgpu::BackendType::D3D11) {
      return dawn::native::d3d11::GetD3D11Device(device_.Get());
    }
    return nullptr;
  }

  void FlushD3D11CommandsIfDelayed() const {
    if (backend_type() != wgpu::BackendType::D3D11) {
      return;
    }

    // This function is meant for delayed flush option.
    if (!features::kSkiaGraphiteDawnD3D11DelayFlush.Get()) {
      return;
    }

    TRACE_EVENT0("gpu", "DawnSharedContext::FlushD3D11Commands");

    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
        dawn::native::d3d11::GetD3D11Device(device_.Get());
    if (!d3d11_device) {
      return;
    }

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    d3d11_device->GetImmediateContext(&context);
    context->Flush();
  }
#endif

  std::optional<error::ContextLostReason> GetResetStatus() const;

  std::unique_ptr<GraphiteSharedContext> CreateGraphiteSharedContext(
      const skgpu::graphite::ContextOptions& options,
      GpuProcessShmCount* use_shader_cache_shm_count,
      bool is_thread_safe) {
    if (!device_) {
      return nullptr;
    }

    skgpu::graphite::DawnBackendContext backend_context;
    backend_context.fInstance = GetInstance();
    backend_context.fDevice = device_;
    backend_context.fQueue = device_.GetQueue();

    std::unique_ptr<skgpu::graphite::Context> graphite_context =
        skgpu::graphite::ContextFactory::MakeDawn(backend_context, options);
    if (!graphite_context) {
      return nullptr;
    }

    return std::make_unique<GraphiteSharedContext>(
        std::move(graphite_context), use_shader_cache_shm_count, is_thread_safe,
        features::kSkiaGraphiteMaxPendingRecordings.Get(),
        GetBackendFlushCallback());
  }

  bool use_thread_safe_graphite_context() const {
    return use_thread_safe_graphite_context_;
  }

  GraphiteSharedContext* GetThreadSafeGraphiteSharedContext() const {
    CHECK(use_thread_safe_graphite_context());
    return thread_safe_graphite_shared_context_.get();
  }

  bool InitializeThreadSafeGraphiteContext(
      const skgpu::graphite::ContextOptions& options,
      GpuProcessShmCount* use_shader_cache_shm_count) {
    DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
    CHECK(use_thread_safe_graphite_context());
    CHECK(!thread_safe_graphite_shared_context_);
    thread_safe_graphite_shared_context_ = CreateGraphiteSharedContext(
        options, use_shader_cache_shm_count, /*is_thread_safe=*/true);
    return !!thread_safe_graphite_shared_context_;
  }

 private:
  friend class base::RefCountedThreadSafe<DawnSharedContext>;

  // Provided to wgpu::Device as logging callback.
  static void DeviceLogInfo(wgpu::LoggingType type,
                            wgpu::StringView message,
                            DawnSharedContext* shared_context) {
    CHECK(shared_context);

    std::string_view view = {message.data, message.length};

    switch (static_cast<wgpu::LoggingType>(type)) {
      case wgpu::LoggingType::Warning:
        LOG(WARNING) << view;
        break;
      case wgpu::LoggingType::Error:
        // Trigger context loss.
        shared_context->OnError(wgpu::ErrorType::Internal, view);
        break;
      default:
        break;
    }
  }

  // Provided to wgpu::Instance as logging callback.
  static void InstanceLogInfo(wgpu::LoggingType type,
                              wgpu::StringView message,
                              DawnSharedContext* shared_context) {
    CHECK(shared_context);

    std::string_view view = {message.data, message.length};

    if (!shared_context->device_) {
      // If device hasn't been created yet. Saving the message so that if there
      // is any init failure afterward, we can include the messages in the
      // LogInitFailure()'s report.
      shared_context->StoreInitLoggingMessage(view);
    }

    switch (static_cast<wgpu::LoggingType>(type)) {
      case wgpu::LoggingType::Warning:
        LOG(WARNING) << view;
        break;
      case wgpu::LoggingType::Error:
        LOG(ERROR) << view;
        if (shared_context->device_) {
          // Only DwC if the device is already created.
          // We don't need to DwC for any error before the device is initialized
          // because LogInitFailure() would handle them instead.
          // Note: We don't trigger context loss here for now since most of the
          // errors reported via instance callback is related to Surface
          // creation. In that case, instead of triggering context loss, we
          // should let the call sites handle them gracefully.
          SetDawnErrorCrashKey(view);
          base::debug::DumpWithoutCrashing();
        }
        break;
      default:
        break;
    }
  }

  ~DawnSharedContext() override;

  GraphiteSharedContext::FlushCallback GetBackendFlushCallback() {
#if BUILDFLAG(IS_WIN)
    return base::BindRepeating(&DawnSharedContext::FlushD3D11CommandsIfDelayed,
                               base::RetainedRef(this));
#else
    return {};
#endif
  }

  void OnError(wgpu::ErrorType error_type, wgpu::StringView message);

  void StoreInitLoggingMessage(std::string_view message) {
    init_logging_msgs_.append(message);
    init_logging_msgs_.append("\n");
  }

  void LogInitFailure(std::string_view reason,
                      bool generate_crash_report,
                      wgpu::BackendType backend_type,
                      bool force_fallback_adapter) {
    LOG(ERROR) << reason;

    if (!generate_crash_report) {
      return;
    }

    SCOPED_CRASH_KEY_STRING256("dawn-shared-context", "init-failure", reason);
    SCOPED_CRASH_KEY_STRING32("dawn-shared-context", "backend-type",
                              BackendTypeToString(backend_type));
    SCOPED_CRASH_KEY_BOOL("dawn-shared-context", "fallback-adapter",
                          force_fallback_adapter);
    // Also include any logging messages collected during the initialization.
    SCOPED_CRASH_KEY_STRING1024("dawn-shared-context", "init-logging-msgs",
                                init_logging_msgs_);
    init_logging_msgs_.clear();
    base::debug::DumpWithoutCrashing();
  }

  // base::trace_event::MemoryDumpProvider implementation:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // caching_interface_ is null or set to either dawn_caching_interface_ or
  // persistent_cache_
  std::unique_ptr<webgpu::DawnCachingInterface> dawn_caching_interface_;
  scoped_refptr<GpuPersistentCache> persistent_cache_;
  raw_ptr<dawn::platform::CachingInterface> caching_interface_ = nullptr;

  Platform platform_;
  std::unique_ptr<webgpu::DawnInstance> instance_;
  wgpu::Adapter adapter_;
  wgpu::Device device_;
  wgpu::BackendType backend_type_;
  // Store logging messages collected during device initialization.
  std::string init_logging_msgs_;
  bool is_vulkan_swiftshader_adapter_ = false;
  bool registered_memory_dump_provider_ = false;

  // If true, both GpuMain and CompositorGpuThread share the same
  // GraphiteSharedContext which is created lazily. If false,
  // DawnContextProvider owns GraphiteSharedContext and each DawnContextProvider
  // (i.e. each thread) has its own GraphiteSharedContext.
  const bool use_thread_safe_graphite_context_;
  std::unique_ptr<GraphiteSharedContext> thread_safe_graphite_shared_context_;

  mutable base::Lock context_lost_lock_;
  std::optional<error::ContextLostReason> context_lost_reason_
      GUARDED_BY(context_lost_lock_);

  THREAD_CHECKER(main_thread_checker_);
};

DawnSharedContext::DawnSharedContext(gl::ProgressReporter* progress_reporter,
                                     bool use_thread_safe_graphite_context)
    : platform_(/*dawn_caching_interface=*/nullptr,
                progress_reporter,
                /*uma_prefix=*/"GPU.GraphiteDawn.",
                /*record_cache_count_uma=*/true),
      use_thread_safe_graphite_context_(use_thread_safe_graphite_context) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
}

DawnSharedContext::~DawnSharedContext() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (device_) {
    if (registered_memory_dump_provider_) {
      base::trace_event::MemoryDumpManager::GetInstance()
          ->UnregisterDumpProvider(this);
    }
    device_.SetLoggingCallback([](wgpu::LoggingType, wgpu::StringView) {});

    // Destroy GraphiteSharedContext and skgpu::graphite::Context before
    // device_, on which skgpu::graphite::Context is created.
    thread_safe_graphite_shared_context_ = nullptr;

    // Destroy the device now so that the lost callback, which references this
    // class, is fired now before we clean up the rest of this class.
    device_.Destroy();
  }
  if (instance_) {
    instance_->DisconnectDawnPlatform();
  }
}

namespace {
// Dawn Graphite adapter feature level for metrics.
//
// See also: webgpu.h:WGPUFeatureLevel
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(DawnAdapterFeatureLevel)
enum class DawnAdapterFeatureLevel {
  kUnknown = 0,
  kCompatibility = 1,
  kCore = 2,
  kMaxValue = kCore,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/gpu/enums.xml:DawnAdapterFeatureLevel)

DawnAdapterFeatureLevel DawnAdapterFeatureLevelFromWGPU(
    wgpu::FeatureLevel level) {
  switch (level) {
    case wgpu::FeatureLevel::Compatibility:
      return DawnAdapterFeatureLevel::kCompatibility;
    case wgpu::FeatureLevel::Core:
      return DawnAdapterFeatureLevel::kCore;
    default:
      return DawnAdapterFeatureLevel::kUnknown;
  }
}
}  // namespace

bool DawnSharedContext::Initialize(
    wgpu::BackendType backend_type,
    bool force_fallback_adapter,
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    DawnContextProvider::ValidateAdapterFn validate_adapter_fn) {
  // Make Dawn experimental API and WGSL features available since access to this
  // instance doesn't exit the GPU process.
  // LogInfo will be used to receive instance level errors. For example failures
  // of loading libraries, initializing backend, etc
  dawn::native::DawnInstanceDescriptor dawn_instance_desc;
  dawn_instance_desc.SetLoggingCallback(&DawnSharedContext::InstanceLogInfo,
                                        this);
  instance_ = webgpu::DawnInstance::Create(&platform_, gpu_preferences,
                                           webgpu::SafetyLevel::kUnsafe,
                                           &dawn_instance_desc);

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
  if (workarounds.force_high_performance_gpu) {
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

#if BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
  dawn::native::opengl::RequestAdapterOptionsGetGLProc
      adapter_options_get_gl_proc = {};
  if (adapter_options.backendType == wgpu::BackendType::OpenGLES) {
    adapter_options_get_gl_proc.getProc = gl::GetGLProcAddress;
    gl::GLDisplayEGL* gl_display = gl::GLSurfaceEGL::GetGLDisplayEGL();
    if (gl_display) {
      adapter_options_get_gl_proc.display = gl_display->GetDisplay();
    } else {
      adapter_options_get_gl_proc.display = EGL_NO_DISPLAY;
    }
    adapter_options_get_gl_proc.nextInChain = adapter_options.nextInChain;
    adapter_options.nextInChain = &adapter_options_get_gl_proc;
  }
#endif

  adapter_options.featureLevel = wgpu::FeatureLevel::Core;
  std::vector<dawn::native::Adapter> adapters =
      instance_->EnumerateAdapters(&adapter_options);

  if (adapters.empty()) {
    LOG(ERROR) << "No adapters found for non compatibility mode.";
    adapter_options.featureLevel = wgpu::FeatureLevel::Compatibility;
    adapters = instance_->EnumerateAdapters(&adapter_options);
  }

  if (adapters.empty()) {
    // On Android, it's expected that some devices might not support Dawn atm.
    // So don't generate report for it.
    LogInitFailure("No adapters found.",
                   /*generate_crash_report=*/!BUILDFLAG(IS_ANDROID),
                   backend_type, force_fallback_adapter);
    return false;
  }
  adapter_ = wgpu::Adapter(adapters[0].Get());

  if (!validate_adapter_fn(backend_type, adapter_)) {
    LogInitFailure("Validate adapter failed.",
                   /*generate_crash_report=*/!BUILDFLAG(IS_ANDROID),
                   backend_type, force_fallback_adapter);
    return false;
  }

  // Start initializing dawn device here.
  wgpu::DawnCacheDeviceDescriptor cache_desc;
  cache_desc.loadDataFunction = [](const void* key, size_t key_size,
                                   void* value, size_t value_size,
                                   void* userdata) -> size_t {
    if (auto caching_interface =
            static_cast<DawnSharedContext*>(userdata)->caching_interface_) {
      return caching_interface->LoadData(key, key_size, value, value_size);
    }
    return 0;
  };
  cache_desc.storeDataFunction = [](const void* key, size_t key_size,
                                    const void* value, size_t value_size,
                                    void* userdata) {
    if (auto caching_interface =
            static_cast<DawnSharedContext*>(userdata)->caching_interface_) {
      caching_interface->StoreData(key, key_size, value, value_size);
    }
  };
  // The dawn device is owned by this so a pointer back here is safe.
  cache_desc.functionUserdata = this;
  cache_desc.nextInChain = &toggles_desc;

  wgpu::DawnDeviceAllocatorControl allocator_desc;
  wgpu::DeviceDescriptor descriptor;
  if (backend_type == wgpu::BackendType::Vulkan) {
    // Use a 256kb heap block size in the Vulkan backend to minimize
    // fragmentation.
    allocator_desc.allocatorHeapBlockSize = 256 * 1024;
    allocator_desc.nextInChain = &cache_desc;
    descriptor.nextInChain = &allocator_desc;
  } else {
    descriptor.nextInChain = &cache_desc;
  }

  descriptor.SetUncapturedErrorCallback(
      [](const wgpu::Device&, wgpu::ErrorType type, wgpu::StringView message,
         DawnSharedContext* state) {
        if (type != wgpu::ErrorType::NoError) {
          state->OnError(type, message);
        }
      },
      this);
  descriptor.SetDeviceLostCallback(
      wgpu::CallbackMode::AllowSpontaneous,
      [](const wgpu::Device&, wgpu::DeviceLostReason reason,
         wgpu::StringView message, DawnSharedContext* state) {
        if (reason != wgpu::DeviceLostReason::Destroyed) {
          state->OnError(wgpu::ErrorType::Unknown, message);
        }
      },
      this);

  std::vector<wgpu::FeatureName> features =
      GetRequiredFeatures(backend_type, adapter_);
  descriptor.requiredFeatures = features.data();
  descriptor.requiredFeatureCount = std::size(features);

  // Use best limits for the device.
  wgpu::Limits supportedLimits = {};
  if (adapter_.GetLimits(&supportedLimits) != wgpu::Status::Success) {
    LogInitFailure("Failed to call adapter.GetLimits().",
                   /*generate_crash_report=*/true, backend_type,
                   force_fallback_adapter);
    return false;
  }
  descriptor.requiredLimits = &supportedLimits;

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
    LogInitFailure("Failed to create device.", /*generate_crash_report=*/true,
                   backend_type, force_fallback_adapter);
    return false;
  }

  device_.SetLoggingCallback(&DawnSharedContext::DeviceLogInfo, this);

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

  base::UmaHistogramEnumeration(
      "GPU.Dawn.AdapterFeatureLevel",
      DawnAdapterFeatureLevelFromWGPU(adapter_options.featureLevel));
  return true;
}

void DawnSharedContext::SetCachingInterface(
    std::unique_ptr<webgpu::DawnCachingInterface> dawn_caching_interface) {
  CHECK(!caching_interface_);
  dawn_caching_interface_ = std::move(dawn_caching_interface);
  caching_interface_ = dawn_caching_interface_.get();
}

void DawnSharedContext::SetCachingInterface(
    scoped_refptr<GpuPersistentCache> persistent_cache) {
  CHECK(!caching_interface_);
  persistent_cache_ = std::move(persistent_cache);
  caching_interface_ = persistent_cache_.get();
}

std::optional<error::ContextLostReason> DawnSharedContext::GetResetStatus()
    const {
  base::AutoLock auto_lock(context_lost_lock_);
  return context_lost_reason_;
}

void DawnSharedContext::OnError(wgpu::ErrorType error_type,
                                wgpu::StringView message) {
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

  DumpWithoutCrashingOnError(error_type,
                             static_cast<std::string_view>(message));

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

static constexpr char kAllocatorMemoryDumpPrefix[] =
    "gpu/vulkan/graphite_allocator";

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
  using base::trace_event::MemoryAllocatorDump;
  if (args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::kBackground) {
    const dawn::native::MemoryUsageInfo mem_usage =
        dawn::native::ComputeEstimatedMemoryUsageInfo(device_.Get());

    pmd->GetOrCreateAllocatorDump(kDawnMemoryDumpPrefix)
        ->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes, mem_usage.totalUsage);
    pmd->GetOrCreateAllocatorDump(
           base::JoinString({kDawnMemoryDumpPrefix, "textures"}, "/"))
        ->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes, mem_usage.texturesUsage);
    pmd
        ->GetOrCreateAllocatorDump(base::JoinString(
            {kDawnMemoryDumpPrefix, "textures/depth_stencil"}, "/"))
        ->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes,
                    mem_usage.depthStencilTexturesUsage);
    auto* msaa_dump = pmd->GetOrCreateAllocatorDump(
        base::JoinString({kDawnMemoryDumpPrefix, "textures/msaa"}, "/"));
    msaa_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                         MemoryAllocatorDump::kUnitsBytes,
                         mem_usage.msaaTexturesUsage);
    msaa_dump->AddScalar(MemoryAllocatorDump::kNameObjectCount,
                         MemoryAllocatorDump::kUnitsObjects,
                         mem_usage.msaaTexturesCount);
    msaa_dump->AddScalar("biggest_size", MemoryAllocatorDump::kUnitsBytes,
                         mem_usage.largestMsaaTextureUsage);
    pmd->GetOrCreateAllocatorDump(
           base::JoinString({kDawnMemoryDumpPrefix, "buffers"}, "/"))
        ->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes, mem_usage.buffersUsage);
  } else {
    DawnMemoryDump dawnMemoryDump(pmd);
    dawn::native::DumpMemoryStatistics(device_.Get(), &dawnMemoryDump);
  }

  if (backend_type() == wgpu::BackendType::Vulkan) {
    // For Graphite-Vulkan backend, report vulkan allocator dumps and
    // statistics.
    auto* dump = pmd->GetOrCreateAllocatorDump(kAllocatorMemoryDumpPrefix);
    const dawn::native::AllocatorMemoryInfo allocator_usage =
        dawn::native::GetAllocatorMemoryInfo(device_.Get());
    // `allocated_size` is memory allocated from the device, used is what is
    // actually used.
    dump->AddScalar("allocated_size", MemoryAllocatorDump::kUnitsBytes,
                    allocator_usage.totalAllocatedMemory -
                        allocator_usage.totalLazyAllocatedMemory);
    dump->AddScalar(
        "used_size", MemoryAllocatorDump::kUnitsBytes,
        allocator_usage.totalUsedMemory - allocator_usage.totalLazyUsedMemory);
    dump->AddScalar(
        "fragmentation_size", MemoryAllocatorDump::kUnitsBytes,
        allocator_usage.totalAllocatedMemory - allocator_usage.totalUsedMemory);
    dump->AddScalar("lazy_allocated_size", MemoryAllocatorDump::kUnitsBytes,
                    allocator_usage.totalLazyAllocatedMemory);
    dump->AddScalar("lazy_used_size", MemoryAllocatorDump::kUnitsBytes,
                    allocator_usage.totalLazyUsedMemory);
  }

  return true;
}

std::unique_ptr<DawnContextProvider> DawnContextProvider::Create(
    const GpuPreferences& gpu_preferences,
    const GpuFeatureInfo& gpu_feature_info,
    gl::ProgressReporter* progress_reporter,
    ValidateAdapterFn validate_adapter_fn) {
  return DawnContextProvider::CreateWithBackend(
      GetDefaultBackendType(), DefaultForceFallbackAdapter(), gpu_preferences,
      gpu_feature_info, progress_reporter, validate_adapter_fn);
}

std::unique_ptr<DawnContextProvider> DawnContextProvider::CreateWithBackend(
    wgpu::BackendType backend_type,
    bool force_fallback_adapter,
    const GpuPreferences& gpu_preferences,
    const GpuFeatureInfo& gpu_feature_info,
    gl::ProgressReporter* progress_reporter,
    ValidateAdapterFn validate_adapter_fn) {
  bool use_thread_safe_graphite_context =
      features::IsDrDcEnabled(gpu_feature_info) &&
      features::IsGraphiteContextThreadSafe();
  auto dawn_shared_context = base::MakeRefCounted<DawnSharedContext>(
      progress_reporter, use_thread_safe_graphite_context);
  GpuDriverBugWorkarounds workarounds(
      gpu_feature_info.enabled_gpu_driver_bug_workarounds);
  if (!dawn_shared_context->Initialize(backend_type, force_fallback_adapter,
                                       gpu_preferences, workarounds,
                                       validate_adapter_fn)) {
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

bool DawnContextProvider::use_thread_safe_shared_context() const {
  return dawn_shared_context_->use_thread_safe_graphite_context();
}

void DawnContextProvider::InitializeThreadSafeGraphiteContext(
    const skgpu::graphite::ContextOptions& options,
    GpuProcessShmCount* use_shader_cache_shm_count) {
  dawn_shared_context_->InitializeThreadSafeGraphiteContext(
      options, use_shader_cache_shm_count);
}

bool DawnContextProvider::InitializeGraphiteContext(
    const skgpu::graphite::ContextOptions& options,
    GpuProcessShmCount* use_shader_cache_shm_count) {
  if (dawn_shared_context_->use_thread_safe_graphite_context()) {
    return !!dawn_shared_context_->GetThreadSafeGraphiteSharedContext();
  }
  graphite_shared_context_ = dawn_shared_context_->CreateGraphiteSharedContext(
      options, use_shader_cache_shm_count, /*is_thread_safe=*/false);
  return !!graphite_shared_context_;
}

void DawnContextProvider::SetCachingInterface(
    std::unique_ptr<webgpu::DawnCachingInterface> dawn_caching_interface) {
  CHECK(dawn_shared_context_->HasOneRef());
  CHECK(!graphite_shared_context_);
  dawn_shared_context_->SetCachingInterface(std::move(dawn_caching_interface));
}

void DawnContextProvider::SetCachingInterface(
    scoped_refptr<GpuPersistentCache> persistent_cache) {
  CHECK(dawn_shared_context_->HasOneRef());
  CHECK(!graphite_shared_context_);
  dawn_shared_context_->SetCachingInterface(std::move(persistent_cache));
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

GraphiteSharedContext* DawnContextProvider::GetGraphiteSharedContext() const {
  if (dawn_shared_context_->use_thread_safe_graphite_context()) {
    // Both threads shares the same GraphiteSharedContext. DawnSharedContext
    // owns GraphiteSharedContext
    return dawn_shared_context_->GetThreadSafeGraphiteSharedContext();
  } else {
    // Each DawnContextProvider owns its own GraphiteSharedContext and
    // skgpu::graphite::Context
    return graphite_shared_context_.get();
  }
}

webgpu::DawnPlatform* DawnContextProvider::GetDawnPlatform() {
  return dawn_shared_context_->GetDawnPlatform();
}

}  // namespace gpu
