// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/gpu/gpu_data_manager_impl_private.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <aclapi.h>
#include <sddl.h>
#endif  // BUILDFLAG(IS_WIN)

#include <array>
#include <iterator>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/trace_event/trace_event.h"
#include "base/version.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/switches.h"
#include "components/viz/common/features.h"
#include "content/browser/gpu/gpu_memory_buffer_manager_singleton.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/media/frameless_media_interface_proxy.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/config/gpu_blocklist.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/gpu_util.h"
#include "gpu/config/software_rendering_list_autogen.h"
#include "gpu/ipc/common/memory_stats.h"
#include "gpu/ipc/host/gpu_disk_cache.h"
#include "gpu/vulkan/buildflags.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/media_buildflags.h"
#include "media/mojo/clients/mojo_video_decoder.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/screen.h"
#include "ui/gfx/switches.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_preference.h"
#include "ui/gl/gpu_switching_manager.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/application_status_listener.h"
#endif
#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif
#if BUILDFLAG(IS_MAC)
#include <ApplicationServices/ApplicationServices.h>
#endif  // BUILDFLAG(IS_MAC)
#if BUILDFLAG(IS_WIN)
#include "base/base_paths_win.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/mojom/dxgi_info.mojom.h"
#endif  // BUILDFLAG(IS_WIN)
#if BUILDFLAG(IS_CASTOS)
#include "chromecast/chromecast_buildflags.h"  // nogncheck
#endif                                         // BUILDFLAG(IS_CASTOS)

namespace content {

namespace {

#if BUILDFLAG(IS_ANDROID)
// NOINLINE to ensure this function is used in crash reports.
NOINLINE void FatalGpuProcessLaunchFailureOnBackground() {
  if (!base::android::ApplicationStatusListener::HasVisibleActivities()) {
    // We expect the platform to aggressively kill services when the app is
    // backgrounded. A FATAL error creates a dialog notifying users that the
    // app has crashed which doesn't look good. So we use SIGKILL instead. But
    // still do a crash dump for 1% cases to make sure we're not regressing this
    // case.
    if (base::RandInt(1, 100) == 1)
      base::debug::DumpWithoutCrashing();
    kill(getpid(), SIGKILL);
  }
}
#endif

#if BUILDFLAG(IS_WIN)
// This function checks the created file to ensure it wasn't redirected
// to another location using a symbolic link or a hard link.
bool ValidateFileHandle(HANDLE cache_file_handle,
                        const base::FilePath& cache_file_path) {
  // Check that the file wasn't hardlinked to something else.
  BY_HANDLE_FILE_INFORMATION file_info = {};
  if (!::GetFileInformationByHandle(cache_file_handle, &file_info))
    return false;
  if (file_info.nNumberOfLinks > 1)
    return false;

  // Check the final path matches the expected path.
  wchar_t final_path_buffer[MAX_PATH];
  if (!::GetFinalPathNameByHandle(cache_file_handle, final_path_buffer,
                                  _countof(final_path_buffer),
                                  FILE_NAME_NORMALIZED | VOLUME_NAME_DOS)) {
    return false;
  }
  // Returned string should start with \\?\. If not then fail validation.
  if (!base::StartsWith(final_path_buffer, L"\\\\?\\",
                        base::CompareCase::INSENSITIVE_ASCII)) {
    return false;
  }
  // Expected filename and actual file name must be an exact match.
  return cache_file_path == base::FilePath(&final_path_buffer[4]);
}

// Generate Intel cache file names depending on the app name.
bool GetIntelCacheFileNames(std::vector<base::FilePath::StringType>* names) {
  DCHECK(names);
  DCHECK(names->empty());
  base::FilePath module_path;
  if (!base::PathService::Get(base::FILE_EXE, &module_path))
    return false;
  module_path = module_path.BaseName().RemoveExtension();
  base::FilePath::StringType module_name = module_path.value();
  if (module_name.size() == 0)
    return false;
  // The Intel shader cache files should be appName_[0|1|2].
  names->push_back(module_name + L"_0");
  names->push_back(module_name + L"_1");
  names->push_back(module_name + L"_2");
  return true;
}

void EnableIntelShaderCache() {
  base::FilePath dir;
  if (!base::PathService::Get(base::DIR_COMMON_APP_DATA, &dir))
    return;
  dir = dir.Append(L"Intel").Append(L"ShaderCache");
  if (!base::DirectoryExists(dir))
    return;

  PSECURITY_DESCRIPTOR sd = nullptr;
  ULONG sd_length = 0;
  // Set Full Access to All Users and Administrators, then grant RWX to
  // AppContainers and Low Privilege AppContainers.
  BOOL success = ::ConvertStringSecurityDescriptorToSecurityDescriptor(
      L"D:(A;;FA;;;AU)(A;;FA;;;BA)(A;;GRGWGX;;;S-1-15-2-1)(A;;GRGWGX;;;S-1-15-"
      L"2-2)",
      SDDL_REVISION_1, &sd, &sd_length);
  if (!success)
    return;
  DCHECK(sd);
  DCHECK_LT(0u, sd_length);
  std::unique_ptr<void, decltype(::LocalFree)*> sd_holder(sd, ::LocalFree);
  PACL dacl = nullptr;
  BOOL present = FALSE, defaulted = FALSE;
  success = ::GetSecurityDescriptorDacl(sd, &present, &dacl, &defaulted);
  if (!success)
    return;
  DCHECK(present);
  DCHECK(dacl);
  DCHECK(!defaulted);

  std::vector<base::FilePath::StringType> cache_file_names;
  if (!GetIntelCacheFileNames(&cache_file_names))
    return;
  for (const auto& cache_file_name : cache_file_names) {
    base::FilePath cache_file_path = dir.Append(cache_file_name);
    HANDLE cache_file_handle = ::CreateFileW(
        cache_file_path.value().c_str(), WRITE_DAC,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, 0, nullptr);
    base::win::ScopedHandle handle_holder(cache_file_handle);
    if (cache_file_handle == INVALID_HANDLE_VALUE ||
        !ValidateFileHandle(cache_file_handle, cache_file_path)) {
      continue;
    }

    DWORD result = ::SetSecurityInfo(cache_file_handle, SE_KERNEL_OBJECT,
                                     DACL_SECURITY_INFORMATION, nullptr,
                                     nullptr, dacl, nullptr);
    if (result != ERROR_SUCCESS) {
      LOG(ERROR) << "SetSecurityInfo returned " << result;
    }
  }
}
#endif  // BUILDFLAG(IS_WIN)

// These values are persistent to logs. Entries should not be renumbered and
// numeric values should never be reused.
// This should match enum CanvasOopRasterAndGpuAcceleration in
//  \tools\metrics\histograms\enums.xml
enum class CanvasOopRasterAndGpuAcceleration {
  kAccelOop = 0,
  kAccelNoOop = 1,
  kNoAccelOop = 2,
  kNoAccelNoOop = 3,
  kMaxValue = kNoAccelNoOop,
};

void RecordCanvasAcceleratedOopRasterHistogram(
    const gpu::GpuFeatureInfo& gpu_feature_info,
    bool gpu_compositing_disabled) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  bool accelerated_canvas =
      gpu_feature_info
              .status_values[gpu::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS] ==
          gpu::kGpuFeatureStatusEnabled &&
      !command_line.HasSwitch(switches::kDisableAccelerated2dCanvas);
  bool oopr_canvas =
      gpu_feature_info
          .status_values[gpu::GPU_FEATURE_TYPE_CANVAS_OOP_RASTERIZATION] ==
      gpu::kGpuFeatureStatusEnabled;

  CanvasOopRasterAndGpuAcceleration oop_acceleration_state =
      CanvasOopRasterAndGpuAcceleration::kNoAccelNoOop;
  if (!gpu_compositing_disabled) {
    if (accelerated_canvas && oopr_canvas)
      oop_acceleration_state = CanvasOopRasterAndGpuAcceleration::kAccelOop;
    else if (accelerated_canvas && !oopr_canvas)
      oop_acceleration_state = CanvasOopRasterAndGpuAcceleration::kAccelNoOop;
    else if (!accelerated_canvas && oopr_canvas)
      oop_acceleration_state = CanvasOopRasterAndGpuAcceleration::kNoAccelOop;
  }
  UMA_HISTOGRAM_ENUMERATION("GPU.CanvasOopRaster.OopRasterAndGpuAcceleration",
                            oop_acceleration_state);
}

// Send UMA histograms about the enabled features and GPU properties.
void UpdateFeatureStats(const gpu::GpuFeatureInfo& gpu_feature_info) {
  // Update applied entry stats.
  std::unique_ptr<gpu::GpuBlocklist> blocklist(gpu::GpuBlocklist::Create());
  DCHECK(blocklist.get() && blocklist->max_entry_id() > 0);
  uint32_t max_entry_id = blocklist->max_entry_id();
  // Use entry 0 to capture the total number of times that data
  // was recorded in this histogram in order to have a convenient
  // denominator to compute blocklist percentages for the rest of the
  // entries.
  UMA_HISTOGRAM_EXACT_LINEAR("GPU.BlocklistTestResultsPerEntry", 0,
                             max_entry_id + 1);
  if (!gpu_feature_info.applied_gpu_blocklist_entries.empty()) {
    std::vector<uint32_t> entry_ids = blocklist->GetEntryIDsFromIndices(
        gpu_feature_info.applied_gpu_blocklist_entries);
    DCHECK_EQ(gpu_feature_info.applied_gpu_blocklist_entries.size(),
              entry_ids.size());
    for (auto id : entry_ids) {
      DCHECK_GE(max_entry_id, id);
      UMA_HISTOGRAM_EXACT_LINEAR("GPU.BlocklistTestResultsPerEntry", id,
                                 max_entry_id + 1);
    }
  }

  // Update feature status stats.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  const gpu::GpuFeatureType kGpuFeatures[] = {
      gpu::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
      gpu::GPU_FEATURE_TYPE_ACCELERATED_GL,
      gpu::GPU_FEATURE_TYPE_GPU_TILE_RASTERIZATION,
      gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
      gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL2,
      gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGPU};
  const std::string kGpuBlocklistFeatureHistogramNames[] = {
      "GPU.BlocklistFeatureTestResults.Accelerated2dCanvas",
      "GPU.BlocklistFeatureTestResults.GpuCompositing",
      "GPU.BlocklistFeatureTestResults.GpuRasterization",
      "GPU.BlocklistFeatureTestResults.Webgl",
      "GPU.BlocklistFeatureTestResults.Webgl2",
      "GPU.BlocklistFeatureTestResults.Webgpu"};
  const bool kGpuFeatureUserFlags[] = {
      command_line.HasSwitch(switches::kDisableAccelerated2dCanvas),
      command_line.HasSwitch(switches::kDisableGpu),
      command_line.HasSwitch(switches::kDisableGpuRasterization),
      command_line.HasSwitch(switches::kDisableWebGL),
      (command_line.HasSwitch(switches::kDisableWebGL) ||
       command_line.HasSwitch(switches::kDisableWebGL2)),
      !command_line.HasSwitch(switches::kEnableUnsafeWebGPU)};
  const size_t kNumFeatures =
      sizeof(kGpuFeatures) / sizeof(gpu::GpuFeatureType);
  for (size_t i = 0; i < kNumFeatures; ++i) {
    // We can't use UMA_HISTOGRAM_ENUMERATION here because the same name is
    // expected if the macro is used within a loop.
    gpu::GpuFeatureStatus value =
        gpu_feature_info.status_values[kGpuFeatures[i]];
    if (value == gpu::kGpuFeatureStatusEnabled && kGpuFeatureUserFlags[i])
      value = gpu::kGpuFeatureStatusDisabled;
    base::HistogramBase* histogram_pointer = base::LinearHistogram::FactoryGet(
        kGpuBlocklistFeatureHistogramNames[i], 1, gpu::kGpuFeatureStatusMax,
        gpu::kGpuFeatureStatusMax + 1,
        base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram_pointer->Add(value);
  }
}

void UpdateDriverBugListStats(const gpu::GpuFeatureInfo& gpu_feature_info) {
  // Use entry 0 to capture the total number of times that data was recorded
  // in this histogram in order to have a convenient denominator to compute
  // driver bug list percentages for the rest of the entries.
  base::UmaHistogramSparse("GPU.DriverBugTestResultsPerEntry", 0);

  if (!gpu_feature_info.applied_gpu_driver_bug_list_entries.empty()) {
    std::unique_ptr<gpu::GpuDriverBugList> bug_list(
        gpu::GpuDriverBugList::Create());
    DCHECK(bug_list.get() && bug_list->max_entry_id() > 0);
    std::vector<uint32_t> entry_ids = bug_list->GetEntryIDsFromIndices(
        gpu_feature_info.applied_gpu_driver_bug_list_entries);
    DCHECK_EQ(gpu_feature_info.applied_gpu_driver_bug_list_entries.size(),
              entry_ids.size());
    for (auto id : entry_ids) {
      DCHECK_GE(bug_list->max_entry_id(), id);
      base::UmaHistogramSparse("GPU.DriverBugTestResultsPerEntry", id);
    }
  }
}

#if BUILDFLAG(IS_MAC)
void DisplayReconfigCallback(CGDirectDisplayID display,
                             CGDisplayChangeSummaryFlags flags,
                             void* gpu_data_manager) {
  if (flags == kCGDisplayBeginConfigurationFlag)
    return;  // This call contains no information about the display change

  GpuDataManagerImpl* manager =
      reinterpret_cast<GpuDataManagerImpl*>(gpu_data_manager);
  DCHECK(manager);

  // Notification about "GPU switches" is only necessary on macOS when
  // using ANGLE's OpenGL backend. Short-circuit the dispatches for
  // all other backends.
  gpu::GPUInfo info = manager->GetGPUInfo();
  gl::GLImplementationParts parts = info.gl_implementation_parts;
  if (!(parts.gl == gl::kGLImplementationEGLANGLE &&
        parts.angle == gl::ANGLEImplementation::kOpenGL)) {
    return;
  }

  // Notification is only necessary if the machine actually has more
  // than one GPU - nowadays, defined by it being AMD switchable.
  if (!info.amd_switchable) {
    return;
  }

  // Dispatch the notification through the system.
  manager->HandleGpuSwitch();
}
#endif  // BUILDFLAG(IS_MAC)

void OnVideoMemoryUsageStats(
    GpuDataManager::VideoMemoryUsageStatsCallback callback,
    const gpu::VideoMemoryUsageStats& stats) {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), stats));
}

void RequestVideoMemoryUsageStats(
    GpuDataManager::VideoMemoryUsageStatsCallback callback,
    GpuProcessHost* host) {
  if (!host)
    return;
  host->gpu_service()->GetVideoMemoryUsageStats(
      base::BindOnce(&OnVideoMemoryUsageStats, std::move(callback)));
}

// Determines if SwiftShader is available as a fallback for WebGL.
bool SwiftShaderAllowed() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  return !command_line->HasSwitch(switches::kDisableSoftwareRasterizer) &&
         features::IsSwiftShaderAllowed(command_line);
}

// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with "CompositingMode" in
// src/tools/metrics/histograms/enums.xml.
enum class CompositingMode {
  kSoftware = 0,
  kGL = 1,
  kVulkan = 2,
  kMetal = 3,  // deprecated
  kMaxValue = kMetal
};

// Intentionally crash with a very descriptive name.
NOINLINE void IntentionallyCrashBrowserForUnusableGpuProcess() {
  LOG(FATAL) << "GPU process isn't usable. Goodbye.";
}

#if BUILDFLAG(IS_WIN)
void CollectExtraDevicePerfInfo(const gpu::GPUInfo& gpu_info,
                                gpu::DevicePerfInfo* device_perf_info) {
  device_perf_info->intel_gpu_generation = gpu::GetIntelGpuGeneration(gpu_info);
  const gpu::GPUInfo::GPUDevice& device = gpu_info.active_gpu();
  if (device.vendor_id == 0xffff /* internal flag for software rendering */ ||
      device.vendor_id == 0x15ad /* VMware */ ||
      device.vendor_id == 0x1414 /* Microsoft software renderer */ ||
      gl::IsSoftwareGLImplementation(
          gpu_info.gl_implementation_parts) /* SwiftShader */) {
    device_perf_info->software_rendering = true;
  }
}

// Provides a bridge whereby display::win::ScreenWin can ask the GPU process
// about the HDR status of the system.
class HDRProxy {
 public:
  static void Initialize() {
    display::win::ScreenWin::SetRequestHDRStatusCallback(
        base::BindRepeating(&HDRProxy::RequestHDRStatus));
  }

  static void RequestHDRStatus() {
    auto* gpu_process_host =
        GpuProcessHost::Get(GPU_PROCESS_KIND_SANDBOXED, false);
    if (gpu_process_host) {
      auto* gpu_service = gpu_process_host->gpu_host()->gpu_service();
      gpu_service->RequestDXGIInfo(base::BindOnce(&HDRProxy::GotResult));
    } else {
      GotResult(gfx::mojom::DXGIInfo::New());
    }
  }

  static void GotResult(gfx::mojom::DXGIInfoPtr dxgi_info) {
    display::win::ScreenWin::SetDXGIInfo(std::move(dxgi_info));
  }
};

#endif  // BUILDFLAG(IS_WIN)
}  // anonymous namespace

GpuDataManagerImplPrivate::GpuDataManagerImplPrivate(GpuDataManagerImpl* owner)
    : owner_(owner),
      observer_list_(base::MakeRefCounted<GpuDataManagerObserverList>()) {
  DCHECK(owner_);
  InitializeGpuModes();
#if BUILDFLAG(IS_WIN)
  EnableIntelShaderCache();
#endif  // BUILDFLAG(IS_WIN)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableGpuCompositing)) {
    SetGpuCompositingDisabled();
  }

  if (command_line->HasSwitch(switches::kSingleProcess) ||
      command_line->HasSwitch(switches::kInProcessGPU)) {
    AppendGpuCommandLine(command_line, GPU_PROCESS_KIND_SANDBOXED);
  }

#if BUILDFLAG(IS_MAC)
  CGDisplayRegisterReconfigurationCallback(DisplayReconfigCallback, owner_);
#endif  // BUILDFLAG(IS_MAC)

  // For testing only.
  if (command_line->HasSwitch(switches::kDisableDomainBlockingFor3DAPIs))
    domain_blocking_enabled_ = false;
}

GpuDataManagerImplPrivate::~GpuDataManagerImplPrivate() {
#if BUILDFLAG(IS_MAC)
  CGDisplayRemoveReconfigurationCallback(DisplayReconfigCallback, owner_);
#endif
}

void GpuDataManagerImplPrivate::StartUmaTimer() {
  // Do not change kTimerInterval without also changing the UMA histogram name,
  // as histogram data from before/after the change will not be comparable.
  constexpr base::TimeDelta kTimerInterval = base::Minutes(5);
  compositing_mode_timer_.Start(
      FROM_HERE, kTimerInterval, this,
      &GpuDataManagerImplPrivate::RecordCompositingMode);
}

void GpuDataManagerImplPrivate::InitializeGpuModes() {
  DCHECK_EQ(gpu::GpuMode::UNKNOWN, gpu_mode_);
  // Android and Chrome OS can't switch to software compositing. If the GPU
  // process initialization fails or GPU process is too unstable then crash the
  // browser process to reset everything.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  fallback_modes_.push_back(gpu::GpuMode::DISPLAY_COMPOSITOR);
  if (SwiftShaderAllowed())
    fallback_modes_.push_back(gpu::GpuMode::SWIFTSHADER);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableGpu)) {
    // Chomecast audio-only builds run with the flag --disable-gpu. The GPU
    // process should not access hardware GPU in this case.
#if BUILDFLAG(IS_CASTOS)
#if BUILDFLAG(IS_CAST_AUDIO_ONLY)
    fallback_modes_.clear();
    fallback_modes_.push_back(gpu::GpuMode::DISPLAY_COMPOSITOR);
#endif  // BUILDFLAG(IS_CAST_AUDIO_ONLY)
#endif  // BUILDFLAG(IS_CASTOS)

#if (BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CAST_ANDROID)) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
    CHECK(false) << "GPU acceleration is required on certain platforms!";
#endif
  } else if (features::IsSkiaGraphiteEnabled(command_line)) {
    // If Graphite is enabled, fall back to Ganesh/GL on platforms that do not
    // support software compositing or sometimes fail dawn initialization.
    // TODO(b/323953910): Eliminate this fallback on each platform once Graphite
    // stability is sufficient on that platform.
    fallback_modes_.push_back(gpu::GpuMode::HARDWARE_GL);
    fallback_modes_.push_back(gpu::GpuMode::HARDWARE_GRAPHITE);
  } else {
    // On Fuchsia Vulkan must be used when it's enabled by the WebEngine
    // embedder. Falling back to SW compositing in that case is not supported.
#if BUILDFLAG(IS_FUCHSIA)
    fallback_modes_.clear();
    fallback_modes_.push_back(gpu::GpuMode::HARDWARE_VULKAN);
#else
    fallback_modes_.push_back(gpu::GpuMode::HARDWARE_GL);
    // Prefer Vulkan over GL if enabled.
    if (features::IsUsingVulkan()) {
      fallback_modes_.push_back(gpu::GpuMode::HARDWARE_VULKAN);
    }
#endif  // BUILDFLAG(IS_FUCHSIA)
  }

  FallBackToNextGpuMode();
}

void GpuDataManagerImplPrivate::BlocklistWebGLForTesting() {
  // This function is for testing only, so disable histograms.
  update_histograms_ = false;

  gpu::GpuFeatureInfo gpu_feature_info;
  for (int ii = 0; ii < gpu::NUMBER_OF_GPU_FEATURE_TYPES; ++ii) {
    if (ii == static_cast<int>(gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL))
      gpu_feature_info.status_values[ii] = gpu::kGpuFeatureStatusBlocklisted;
    else
      gpu_feature_info.status_values[ii] = gpu::kGpuFeatureStatusEnabled;
  }
  UpdateGpuFeatureInfo(gpu_feature_info, std::nullopt);
  NotifyGpuInfoUpdate();
}

void GpuDataManagerImplPrivate::SetSkiaGraphiteEnabledForTesting(bool enabled) {
  // Pretend that HW acceleration is enabled so that we consider GpuFeatureInfo
  // as initialized.
  gpu_feature_info_.status_values[gpu::GPU_FEATURE_TYPE_ACCELERATED_GL] =
      gpu::kGpuFeatureStatusEnabled;
  gpu_feature_info_.status_values[gpu::GPU_FEATURE_TYPE_SKIA_GRAPHITE] =
      enabled ? gpu::kGpuFeatureStatusEnabled : gpu::kGpuFeatureStatusDisabled;
}

gpu::GPUInfo GpuDataManagerImplPrivate::GetGPUInfo() const {
  return gpu_info_;
}

gpu::GPUInfo GpuDataManagerImplPrivate::GetGPUInfoForHardwareGpu() const {
  return gpu_info_for_hardware_gpu_;
}

std::vector<std::string> GpuDataManagerImplPrivate::GetDawnInfoList() const {
  return dawn_info_list_;
}

bool GpuDataManagerImplPrivate::GpuAccessAllowed(std::string* reason) const {
  switch (gpu_mode_) {
    case gpu::GpuMode::HARDWARE_GL:
    case gpu::GpuMode::HARDWARE_GRAPHITE:
    case gpu::GpuMode::HARDWARE_VULKAN:
      return true;
    case gpu::GpuMode::SWIFTSHADER:
      DCHECK(SwiftShaderAllowed());
      return true;
    default:
      if (reason) {
        // If SwiftShader is allowed, then we are here because it was blocked.
        if (SwiftShaderAllowed()) {
          *reason = "GPU process crashed too many times with SwiftShader.";
        } else {
          *reason = "GPU access is disabled ";
          // just running with --disable-gpu only will go to
          // GpuMode::SWIFTSHADER instead. Adding --disable-gpu and
          // --disable-software-rasterizer makes GpuAccessAllowed false and it
          // comes here.
          if (base::CommandLine::ForCurrentProcess()->HasSwitch(
                  switches::kDisableGpu)) {
            *reason +=
                "through commandline switch --disable-gpu and "
                "--disable-software-rasterizer.";
          } else if (hardware_disabled_explicitly_) {
            *reason += "in chrome://settings.";
          } else {
            *reason += "due to frequent crashes.";
          }
        }
      }
      return false;
  }
}

bool GpuDataManagerImplPrivate::GpuAccessAllowedForHardwareGpu(
    std::string* reason) const {
  if (reason)
    *reason = gpu_access_blocked_reason_for_hardware_gpu_;
  return gpu_access_allowed_for_hardware_gpu_;
}

void GpuDataManagerImplPrivate::RequestDx12VulkanVideoGpuInfoIfNeeded(
    GpuDataManagerImpl::GpuInfoRequest request,
    bool delayed) {
  if (request & GpuDataManagerImpl::kGpuInfoRequestDirectX) {
    RequestGpuSupportedDirectXVersion(delayed);
  }

  if (request & GpuDataManagerImpl::kGpuInfoRequestVulkan)
    RequestGpuSupportedVulkanVersion(delayed);

  if (request & GpuDataManagerImpl::kGpuInfoRequestDawnInfo)
    RequestDawnInfo(delayed, /*collect_metrics=*/false);

  if (request & GpuDataManagerImpl::kGpuInfoRequestVideo) {
    DCHECK(!delayed) << "|delayed| is not supported for Mojo Media requests";
    RequestMojoMediaVideoCapabilities();
  }
}

void GpuDataManagerImplPrivate::RequestGpuSupportedDirectXVersion(
    bool delayed) {
#if BUILDFLAG(IS_WIN)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::TimeDelta delta;
  if (delayed &&
      !command_line->HasSwitch(switches::kNoDelayForDX12VulkanInfoCollection)) {
    delta = base::Seconds(120);
  }

  base::OnceClosure task = base::BindOnce(
      [](base::TimeDelta delta) {
        GpuDataManagerImpl* manager = GpuDataManagerImpl::GetInstance();
        if (manager->DirectXRequested()) {
          return;
        }

        base::CommandLine* command_line =
            base::CommandLine::ForCurrentProcess();
        if (command_line->HasSwitch(
                switches::kDisableGpuProcessForDX12InfoCollection)) {
          manager->UpdateDirectXRequestStatus(false);
          return;
        }

        // No info collection for software GL implementation (id == 0xffff) or
        // abnormal situation (id == 0). There are a few crash reports on
        // exit_or_terminate_process() during process teardown. The GPU ID
        // should be available by the time this task starts to run. In the case
        // of no delay, which is for testing only, don't check the GPU ID
        // because the ID is not available yet.
        const gpu::GPUInfo::GPUDevice& gpu = manager->GetGPUInfo().gpu;
        if ((gpu.vendor_id == 0xffff && gpu.device_id == 0xffff) ||
            (!delta.is_zero() && gpu.vendor_id == 0 && gpu.device_id == 0)) {
          manager->UpdateDirectXRequestStatus(false);
          return;
        }

        GpuProcessHost* host = GpuProcessHost::Get(
            GPU_PROCESS_KIND_INFO_COLLECTION, true /* force_create */);
        if (!host) {
          manager->UpdateDirectXRequestStatus(false);
          return;
        }

        manager->UpdateDirectXRequestStatus(true);
        host->info_collection_gpu_service()
            ->GetGpuSupportedDirectXVersionAndDevicePerfInfo(
                base::BindOnce([](uint32_t d3d12_feature_level,
                                  uint32_t highest_shader_model_version,
                                  uint32_t directml_feature_level,
                                  const gpu::DevicePerfInfo& device_perf_info) {
                  GpuDataManagerImpl* manager =
                      GpuDataManagerImpl::GetInstance();
                  manager->UpdateDirectXInfo(d3d12_feature_level,
                                             directml_feature_level);
                  // UpdateDirectXInfo() needs to be called before
                  // UpdateDevicePerfInfo() because only the latter calls
                  // NotifyGpuInfoUpdate().
                  manager->UpdateDevicePerfInfo(device_perf_info);
                  manager->TerminateInfoCollectionGpuProcess();
                  gpu::RecordGpuSupportedDx12VersionHistograms(
                      d3d12_feature_level, highest_shader_model_version);
                }));
      },
      delta);

  GetUIThreadTaskRunner({})->PostDelayedTask(FROM_HERE, std::move(task), delta);
#endif
}

void GpuDataManagerImplPrivate::RequestGpuSupportedVulkanVersion(bool delayed) {
#if BUILDFLAG(IS_WIN)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::TimeDelta delta;
  if (delayed &&
      !command_line->HasSwitch(switches::kNoDelayForDX12VulkanInfoCollection)) {
    delta = base::Seconds(120);
  }

  base::OnceClosure task = base::BindOnce(
      [](base::TimeDelta delta) {
        GpuDataManagerImpl* manager = GpuDataManagerImpl::GetInstance();
        if (manager->VulkanRequested())
          return;

        // No info collection for software GL implementation (id == 0xffff) or
        // abnormal situation (id == 0). There are a few crash reports on
        // exit_or_terminate_process() during process teardown. The GPU ID
        // should be available by the time this task starts to run. In the case
        // of no delay, which is for testing only, don't check the GPU ID
        // because the ID is not available yet.
        const gpu::GPUInfo::GPUDevice gpu = manager->GetGPUInfo().gpu;
        if ((gpu.vendor_id == 0xffff && gpu.device_id == 0xffff) ||
            (!delta.is_zero() && gpu.vendor_id == 0 && gpu.device_id == 0)) {
          manager->UpdateVulkanRequestStatus(false);
          return;
        }

        GpuProcessHost* host = GpuProcessHost::Get(
            GPU_PROCESS_KIND_INFO_COLLECTION, true /* force_create */);
        if (!host) {
          manager->UpdateVulkanRequestStatus(false);
          return;
        }

        manager->UpdateVulkanRequestStatus(true);
        host->info_collection_gpu_service()->GetGpuSupportedVulkanVersionInfo(
            base::BindOnce([](uint32_t vulkan_version) {
              GpuDataManagerImpl* manager = GpuDataManagerImpl::GetInstance();
              manager->UpdateVulkanInfo(vulkan_version);
              manager->TerminateInfoCollectionGpuProcess();
            }));
      },
      delta);

  GetUIThreadTaskRunner({})->PostDelayedTask(FROM_HERE, std::move(task), delta);
#endif
}

void GpuDataManagerImplPrivate::RequestDawnInfo(bool delayed,
                                                bool collect_metrics) {
  base::TimeDelta delta;
  if (delayed) {
    delta = base::Seconds(120);
  }

  base::OnceClosure task = base::BindOnce(
      [](bool collect_metrics) {
        GpuProcessHost* host = GpuProcessHost::Get(GPU_PROCESS_KIND_SANDBOXED,
                                                   false /* force_create */);
        if (!host) {
          return;
        }

        host->gpu_service()->GetDawnInfo(
            collect_metrics,
            base::BindOnce(
                [](bool collect_metrics,
                   const std::vector<std::string>& dawn_info_list) {
                  if (collect_metrics) {
                    // Metrics collection does not populate the info list.
                    return;
                  }
                  GpuDataManagerImpl* manager =
                      GpuDataManagerImpl::GetInstance();
                  manager->UpdateDawnInfo(dawn_info_list);
                },
                collect_metrics));
      },
      collect_metrics);

  GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostDelayedTask(FROM_HERE, std::move(task), delta);
}

void GpuDataManagerImplPrivate::RequestMojoMediaVideoCapabilities() {
  base::OnceClosure task = base::BindOnce([]() {
    auto media_interface_proxy =
        std::make_unique<FramelessMediaInterfaceProxy>(nullptr);

    mojo::PendingRemote<media::mojom::VideoDecoder> pending_remote_decoder;
    media_interface_proxy->CreateVideoDecoder(
        pending_remote_decoder.InitWithNewPipeAndPassReceiver(),
        /*dst_video_decoder=*/{});
    DCHECK(pending_remote_decoder.is_valid());

    mojo::Remote<media::mojom::VideoDecoder> remote_decoder(
        std::move(pending_remote_decoder));
    DCHECK(remote_decoder.is_connected());

    auto* remote_decoder_ptr = remote_decoder.get();
    DCHECK(remote_decoder_ptr);
    remote_decoder_ptr->GetSupportedConfigs(base::BindOnce(
        [](mojo::Remote<media::mojom::VideoDecoder> /* remote_decoder */,
           std::unique_ptr<
               FramelessMediaInterfaceProxy> /* media_interface_proxy */,
           const media::SupportedVideoDecoderConfigs& configs,
           media::VideoDecoderType /* decoder_type */) {
          GpuDataManagerImpl* manager = GpuDataManagerImpl::GetInstance();
          DCHECK(manager);
          manager->UpdateMojoMediaVideoDecoderCapabilities(configs);
        },
        std::move(remote_decoder), std::move(media_interface_proxy)));
  });

  GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(task));

  // Query VEA profiles to show in chrome://gpu
  auto update_vea_profiles_callback = base::BindPostTask(
      GetUIThreadTaskRunner({}),
      base::BindOnce([](const media::VideoEncodeAccelerator::SupportedProfiles&
                            supported_profiles) {
        GpuDataManagerImpl* manager = GpuDataManagerImpl::GetInstance();
        DCHECK(manager);
        manager->UpdateMojoMediaVideoEncoderCapabilities(supported_profiles);
      }));

  using VEAProfileCallback = base::OnceCallback<void(
      const media::VideoEncodeAccelerator::SupportedProfiles&)>;
  GpuProcessHost::CallOnUI(
      FROM_HERE, GPU_PROCESS_KIND_SANDBOXED, /*force_create=*/false,
      base::BindOnce(
          [](VEAProfileCallback update_vea_profiles_callback,
             GpuProcessHost* host) {
            if (!host)
              return;

            mojo::PendingRemote<media::mojom::VideoEncodeAcceleratorProvider>
                vea_provider_remote;
            host->gpu_service()->CreateVideoEncodeAcceleratorProvider(
                vea_provider_remote.InitWithNewPipeAndPassReceiver());

            mojo::Remote<media::mojom::VideoEncodeAcceleratorProvider>
                vea_provider;
            vea_provider.Bind(std::move(vea_provider_remote));

            // Cache pointer locally since we std::move it into the callback.
            auto* vea_provider_ptr = vea_provider.get();
            vea_provider_ptr->GetVideoEncodeAcceleratorSupportedProfiles(
                base::BindOnce(
                    [](VEAProfileCallback update_vea_profiles_callback,
                       mojo::Remote<
                           media::mojom::VideoEncodeAcceleratorProvider>
                           vea_provider,
                       const media::VideoEncodeAccelerator::SupportedProfiles&
                           supported_profiles) {
                      std::move(update_vea_profiles_callback)
                          .Run(supported_profiles);
                    },
                    std::move(update_vea_profiles_callback),
                    std::move(vea_provider)));
          },
          std::move(update_vea_profiles_callback)));
}

bool GpuDataManagerImplPrivate::IsEssentialGpuInfoAvailable() const {
  // We always update GPUInfo and GpuFeatureInfo from GPU process together.
  return IsGpuFeatureInfoAvailable();
}

bool GpuDataManagerImplPrivate::IsDx12VulkanVersionAvailable() const {
#if BUILDFLAG(IS_WIN)
  // Certain gpu_integration_test needs dx12/Vulkan info. If this info is
  // needed, --no-delay-for-dx12-vulkan-info-collection should be added to the
  // browser command line, so that the collection of this info isn't delayed.
  // This function returns the status of availability to the tests based on
  // whether gpu info has been requested or not.

  return (gpu_info_dx_valid_ && gpu_info_vulkan_valid_) ||
         (!gpu_info_dx_requested_ || !gpu_info_vulkan_requested_) ||
         (gpu_info_dx_request_failed_ || gpu_info_vulkan_request_failed_);
#else
  return true;
#endif
}

bool GpuDataManagerImplPrivate::IsGpuFeatureInfoAvailable() const {
  return gpu_feature_info_.IsInitialized();
}

gpu::GpuFeatureStatus GpuDataManagerImplPrivate::GetFeatureStatus(
    gpu::GpuFeatureType feature) const {
  DCHECK(feature >= 0 && feature < gpu::NUMBER_OF_GPU_FEATURE_TYPES);
  DCHECK(gpu_feature_info_.IsInitialized());
  return gpu_feature_info_.status_values[feature];
}

void GpuDataManagerImplPrivate::RequestVideoMemoryUsageStatsUpdate(
    GpuDataManager::VideoMemoryUsageStatsCallback callback) const {
  GpuProcessHost::CallOnUI(
      FROM_HERE, GPU_PROCESS_KIND_SANDBOXED, false /* force_create */,
      base::BindOnce(&RequestVideoMemoryUsageStats, std::move(callback)));
}

void GpuDataManagerImplPrivate::AddObserver(GpuDataManagerObserver* observer) {
  observer_list_->AddObserver(observer);
}

void GpuDataManagerImplPrivate::RemoveObserver(
    GpuDataManagerObserver* observer) {
  observer_list_->RemoveObserver(observer);
}

void GpuDataManagerImplPrivate::UnblockDomainFrom3DAPIs(const GURL& url) {
  // Remove all instances of this domain from the recent domain
  // blocking events. This may have the side-effect of removing the
  // kAllDomainsBlocked status.

  // Shortcut in the common case where no blocking has occurred. This
  // is important to not regress navigation performance, since this is
  // now called on every user-initiated navigation.
  if (blocked_domains_.empty())
    return;

  std::string domain = GetDomainFromURL(url);
  auto iter = blocked_domains_.begin();
  while (iter != blocked_domains_.end()) {
    if (domain == iter->second.domain) {
      iter = blocked_domains_.erase(iter);
    } else {
      ++iter;
    }
  }

  // If there are have been enough context loss events spread over a
  // long enough time period, it is possible that a given page will be
  // blocked from using 3D APIs because of other domains' entries, and
  // that reloading this page will not allow 3D APIs to run on this
  // page. Compared to an earlier version of these heuristics, it's
  // not clear whether unblocking a domain that doesn't exist in the
  // blocked_domains_ list should clear out the list entirely.
  // Currently, kBlockedDomainExpirationPeriod is set low enough that
  // this should hopefully not be a problem in practice.
}

void GpuDataManagerImplPrivate::UpdateGpuInfo(
    const gpu::GPUInfo& gpu_info,
    const std::optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu) {
#if BUILDFLAG(IS_WIN)
  // If GPU process crashes and launches again, GPUInfo will be sent back from
  // the new GPU process again, and may overwrite the DX12, Vulkan, info we
  // already collected. This is to make sure it doesn't happen.
  uint32_t directml_feature_level = gpu_info_.directml_feature_level;
  uint32_t d3d12_feature_level = gpu_info_.d3d12_feature_level;
  uint32_t vulkan_version = gpu_info_.vulkan_version;
#endif
  gpu_info_ = gpu_info;
  RecordDiscreteGpuHistograms(gpu_info_);
#if BUILDFLAG(ENABLE_VULKAN)
  // Remember the initial hardware_supports_vulkan value so it doesn't change
  // if GPU process restarts as Vulkan might get disabled by GPU mode fallback.
  if (fixed_gpu_info_.hardware_supports_vulkan.has_value()) {
    gpu_info_.hardware_supports_vulkan =
        *fixed_gpu_info_.hardware_supports_vulkan;
  } else {
    fixed_gpu_info_.hardware_supports_vulkan =
        gpu_info.hardware_supports_vulkan;
  }
#endif
#if BUILDFLAG(IS_WIN)
  if (d3d12_feature_level != 0) {
    gpu_info_.d3d12_feature_level = d3d12_feature_level;
  }
  if (vulkan_version != 0) {
    gpu_info_.vulkan_version = vulkan_version;
  }
  if (directml_feature_level != 0) {
    gpu_info_.directml_feature_level = directml_feature_level;
  }
#endif  // BUILDFLAG(IS_WIN)

  bool needs_to_update_gpu_info_for_hardware_gpu =
      !gpu_info_for_hardware_gpu_.IsInitialized();
  if (!needs_to_update_gpu_info_for_hardware_gpu &&
      !gpu_info_.UsesSwiftShader()) {
    // On multi-GPU system, when switching to a different GPU, we want to reset
    // GPUInfo for hardware GPU, because we want to know on which GPU Chrome
    // crashes multiple times and falls back to SwiftShader.
    const gpu::GPUInfo::GPUDevice& active_gpu = gpu_info_.active_gpu();
    const gpu::GPUInfo::GPUDevice& cached_active_gpu =
        gpu_info_for_hardware_gpu_.active_gpu();
#if BUILDFLAG(IS_WIN)
    if (active_gpu.luid.HighPart != cached_active_gpu.luid.HighPart &&
        active_gpu.luid.LowPart != cached_active_gpu.luid.LowPart) {
      needs_to_update_gpu_info_for_hardware_gpu = true;
    }
#else
    if (active_gpu.vendor_id != cached_active_gpu.vendor_id ||
        active_gpu.device_id != cached_active_gpu.device_id) {
      needs_to_update_gpu_info_for_hardware_gpu = true;
    }
#endif  // BUILDFLAG(IS_WIN)
  }

  if (needs_to_update_gpu_info_for_hardware_gpu) {
    if (gpu_info_for_hardware_gpu.has_value()) {
      DCHECK(gpu_info_for_hardware_gpu->IsInitialized());
      bool valid_info = true;
      if (gpu_info_for_hardware_gpu->UsesSwiftShader()) {
        valid_info = false;
      } else if (gpu_info_for_hardware_gpu->gl_renderer.empty() &&
                 gpu_info_for_hardware_gpu->active_gpu().vendor_id == 0u) {
        valid_info = false;
      }
      if (valid_info)
        gpu_info_for_hardware_gpu_ = gpu_info_for_hardware_gpu.value();
    } else {
      if (!gpu_info_.UsesSwiftShader())
        gpu_info_for_hardware_gpu_ = gpu_info_;
    }
  }

  GetContentClient()->SetGpuInfo(gpu_info_);
  NotifyGpuInfoUpdate();
}

#if BUILDFLAG(IS_WIN)

void GpuDataManagerImplPrivate::UpdateDirectXInfo(
    uint32_t d3d12_feature_level,
    uint32_t directml_feature_level) {
  gpu_info_.d3d12_feature_level = d3d12_feature_level;
  gpu_info_.directml_feature_level = directml_feature_level;
  gpu_info_dx_valid_ = true;
  // No need to call NotifyGpuInfoUpdate() because UpdateDirectXInfo() is
  // always called together with UpdateDevicePerfInfo, which calls
  // NotifyGpuInfoUpdate().
}

void GpuDataManagerImplPrivate::UpdateVulkanInfo(uint32_t vulkan_version) {
  gpu_info_.vulkan_version = vulkan_version;
  gpu_info_vulkan_valid_ = true;
  NotifyGpuInfoUpdate();
}

void GpuDataManagerImplPrivate::UpdateDevicePerfInfo(
    const gpu::DevicePerfInfo& device_perf_info) {
  gpu::DevicePerfInfo mutable_device_perf_info = device_perf_info;
  CollectExtraDevicePerfInfo(gpu_info_, &mutable_device_perf_info);
  gpu::SetDevicePerfInfo(mutable_device_perf_info);
  // No need to call GetContentClient()->SetGpuInfo().
  NotifyGpuInfoUpdate();
}

void GpuDataManagerImplPrivate::UpdateOverlayInfo(
    const gpu::OverlayInfo& overlay_info) {
  gpu_info_.overlay_info = overlay_info;

  // No need to call GetContentClient()->SetGpuInfo().
  NotifyGpuInfoUpdate();
}

void GpuDataManagerImplPrivate::UpdateDXGIInfo(
    gfx::mojom::DXGIInfoPtr dxgi_info) {
  // Calling out into HDRProxy::GotResult may end up re-entering us via
  // GpuDataManagerImpl::OnDisplayRemoved/OnDisplayAdded. Both of these
  // take the owner's lock. To avoid recursive locks, we PostTask
  // HDRProxy::GotResult so that it runs outside of the lock.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&HDRProxy::GotResult, std::move(dxgi_info)));
}

void GpuDataManagerImplPrivate::UpdateDirectXRequestStatus(
    bool request_continues) {
  gpu_info_dx_requested_ = true;
  gpu_info_dx_request_failed_ = !request_continues;

  if (gpu_info_dx_request_failed_) {
    gpu::DevicePerfInfo device_perf_info;
    gpu::CollectDevicePerfInfo(&device_perf_info, /*in_browser_process=*/true);
    UpdateDevicePerfInfo(device_perf_info);
  }
}

void GpuDataManagerImplPrivate::UpdateVulkanRequestStatus(
    bool request_continues) {
  gpu_info_vulkan_requested_ = true;
  gpu_info_vulkan_request_failed_ = !request_continues;
}

bool GpuDataManagerImplPrivate::DirectXRequested() const {
  return gpu_info_dx_requested_;
}

bool GpuDataManagerImplPrivate::VulkanRequested() const {
  return gpu_info_vulkan_requested_;
}

void GpuDataManagerImplPrivate::TerminateInfoCollectionGpuProcess() {
  // Wait until DX12/Vulkan and DevicePerfInfo requests are all complete.
  // gpu_info_dx12_valid_ is always updated before device_perf_info
  if (gpu_info_dx_requested_ && !gpu_info_dx_request_failed_ &&
      !gpu::GetDevicePerfInfo().has_value()) {
    return;
  }

  if (gpu_info_vulkan_requested_ && !gpu_info_vulkan_request_failed_ &&
      !gpu_info_vulkan_valid_)
    return;

  // GpuProcessHost::Get() calls GpuDataManagerImpl functions and causes a
  // re-entry of lock.
  base::AutoUnlock unlock(owner_->lock_);
  // GpuProcessHost::Get() only runs on the IO thread. Get() can be called
  // directly here from TerminateInfoCollectionGpuProcess(), which also runs on
  // the IO thread.
  GpuProcessHost* host = GpuProcessHost::Get(GPU_PROCESS_KIND_INFO_COLLECTION,
                                             false /* force_create */);
  if (host)
    host->ForceShutdown();
}
#endif

void GpuDataManagerImplPrivate::PostCreateThreads() {
  // Launch the info collection GPU process to collect Dawn info.
  // Not to affect Chrome startup, this is done in a delayed mode, i.e., 120
  // seconds after Chrome startup.
  RequestDawnInfo(/*delayed=*/true, /*collect_metrics=*/true);

#if BUILDFLAG(IS_WIN)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kNoDelayForDX12VulkanInfoCollection)) {
    // This is for the info collection test of the gpu integration tests.
    RequestDx12VulkanVideoGpuInfoIfNeeded(
        GpuDataManagerImpl::kGpuInfoRequestDirectXVulkan,
        /*delayed=*/false);
  } else {
    // Launch the info collection GPU process to collect DX12 and DirectML
    // support information for UMA at the start of the browser. Not to affect
    // Chrome startup, this is done in a delayed mode,  i.e., 120 seconds after
    // Chrome startup.
    RequestDx12VulkanVideoGpuInfoIfNeeded(
        GpuDataManagerImpl::kGpuInfoRequestDirectX, /*delayed=*/true);
  }

  // Observer for display change.
  display_observer_.emplace(owner_);

  // Initialization for HDR status update.
  HDRProxy::Initialize();
#endif  // BUILDFLAG(IS_WIN)
}

void GpuDataManagerImplPrivate::UpdateDawnInfo(
    const std::vector<std::string>& dawn_info_list) {
  dawn_info_list_ = dawn_info_list;

  NotifyGpuInfoUpdate();
}

void GpuDataManagerImplPrivate::UpdateGpuFeatureInfo(
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const std::optional<gpu::GpuFeatureInfo>&
        gpu_feature_info_for_hardware_gpu) {
  gpu_feature_info_ = gpu_feature_info;
#if !BUILDFLAG(IS_FUCHSIA)
  // With Vulkan or Graphite, GL might be blocked so don't fallback to it later.
  if (HardwareAccelerationEnabled() &&
      gpu_feature_info_.status_values[gpu::GPU_FEATURE_TYPE_ACCELERATED_GL] !=
          gpu::GpuFeatureStatus::kGpuFeatureStatusEnabled) {
    fallback_modes_.erase(
        std::remove(fallback_modes_.begin(), fallback_modes_.end(),
                    gpu::GpuMode::HARDWARE_GL),
        fallback_modes_.end());
  }

  // If Vulkan or Graphite initialization fails, the GPU process can silently
  // fallback to GL.
  if (gpu_mode_ == gpu::GpuMode::HARDWARE_VULKAN &&
      gpu_feature_info_.status_values[gpu::GPU_FEATURE_TYPE_VULKAN] !=
          gpu::GpuFeatureStatus::kGpuFeatureStatusEnabled) {
    // TODO(rivr): The GpuMode in GpuProcessHost will still be
    // HARDWARE_VULKAN. This isn't a big issue right now because both GPU modes
    // report to the same histogram. The first fallback will occur after 4
    // crashes, instead of 3.
    FallBackToNextGpuMode();
  } else if (gpu_mode_ == gpu::GpuMode::HARDWARE_GRAPHITE &&
             gpu_feature_info_
                     .status_values[gpu::GPU_FEATURE_TYPE_SKIA_GRAPHITE] !=
                 gpu::GpuFeatureStatus::kGpuFeatureStatusEnabled) {
    FallBackToNextGpuMode();
  }
#endif  // !BUILDFLAG(IS_FUCHSIA)
  if (!gpu_feature_info_for_hardware_gpu_.IsInitialized()) {
    if (gpu_feature_info_for_hardware_gpu.has_value()) {
      DCHECK(gpu_feature_info_for_hardware_gpu->IsInitialized());
      gpu_feature_info_for_hardware_gpu_ =
          gpu_feature_info_for_hardware_gpu.value();
    } else {
      gpu_feature_info_for_hardware_gpu_ = gpu_feature_info_;
    }
    is_gpu_compositing_disabled_for_hardware_gpu_ = IsGpuCompositingDisabled();
    gpu_access_allowed_for_hardware_gpu_ =
        GpuAccessAllowed(&gpu_access_blocked_reason_for_hardware_gpu_);
  }
  if (update_histograms_) {
    UpdateFeatureStats(gpu_feature_info_);
    UpdateDriverBugListStats(gpu_feature_info_);
    RecordCanvasAcceleratedOopRasterHistogram(gpu_feature_info_,
                                              IsGpuCompositingDisabled());
  }
}

void GpuDataManagerImplPrivate::UpdateGpuExtraInfo(
    const gfx::GpuExtraInfo& gpu_extra_info) {
  gpu_extra_info_ = gpu_extra_info;
  observer_list_->Notify(FROM_HERE,
                         &GpuDataManagerObserver::OnGpuExtraInfoUpdate);
}

void GpuDataManagerImplPrivate::UpdateMojoMediaVideoDecoderCapabilities(
    const media::SupportedVideoDecoderConfigs& configs) {
  gpu_info_.video_decode_accelerator_supported_profiles =
      media::GpuVideoAcceleratorUtil::ConvertMediaConfigsToGpuDecodeProfiles(
          configs);
  NotifyGpuInfoUpdate();
}

void GpuDataManagerImplPrivate::UpdateMojoMediaVideoEncoderCapabilities(
    const media::VideoEncodeAccelerator::SupportedProfiles& profiles) {
  gpu_info_.video_encode_accelerator_supported_profiles =
      media::GpuVideoAcceleratorUtil::ConvertMediaToGpuEncodeProfiles(profiles);
  NotifyGpuInfoUpdate();
}

gpu::GpuFeatureInfo GpuDataManagerImplPrivate::GetGpuFeatureInfo() const {
  return gpu_feature_info_;
}

gpu::GpuFeatureInfo GpuDataManagerImplPrivate::GetGpuFeatureInfoForHardwareGpu()
    const {
  return gpu_feature_info_for_hardware_gpu_;
}

gfx::GpuExtraInfo GpuDataManagerImplPrivate::GetGpuExtraInfo() const {
  return gpu_extra_info_;
}

bool GpuDataManagerImplPrivate::IsGpuCompositingDisabled() const {
  return disable_gpu_compositing_ || !HardwareAccelerationEnabled();
}

bool GpuDataManagerImplPrivate::IsGpuCompositingDisabledForHardwareGpu() const {
  return is_gpu_compositing_disabled_for_hardware_gpu_;
}

void GpuDataManagerImplPrivate::SetGpuCompositingDisabled() {
  if (!IsGpuCompositingDisabled()) {
    disable_gpu_compositing_ = true;
    if (gpu_feature_info_.IsInitialized())
      NotifyGpuInfoUpdate();
  }
}

void GpuDataManagerImplPrivate::AppendGpuCommandLine(
    base::CommandLine* command_line,
    GpuProcessKind kind) const {
  DCHECK(command_line);
  const base::CommandLine* browser_command_line =
      base::CommandLine::ForCurrentProcess();

  gpu::GpuPreferences gpu_prefs = GetGpuPreferencesFromCommandLine();
  UpdateGpuPreferences(&gpu_prefs, kind);

  command_line->AppendSwitchASCII(switches::kGpuPreferences,
                                  gpu_prefs.ToSwitchValue());

  std::string use_gl;
  switch (gpu_mode_) {
    case gpu::GpuMode::HARDWARE_GL:
    case gpu::GpuMode::HARDWARE_GRAPHITE:
    case gpu::GpuMode::HARDWARE_VULKAN:
      use_gl = browser_command_line->GetSwitchValueASCII(switches::kUseGL);
      break;
    case gpu::GpuMode::SWIFTSHADER:
      gl::SetSoftwareWebGLCommandLineSwitches(command_line);
      break;
    default:
      use_gl = gl::kGLImplementationDisabledName;
  }
  if (!use_gl.empty()) {
    command_line->AppendSwitchASCII(switches::kUseGL, use_gl);
  }
}

void GpuDataManagerImplPrivate::UpdateGpuPreferences(
    gpu::GpuPreferences* gpu_preferences,
    GpuProcessKind kind) const {
  DCHECK(gpu_preferences);

  gpu_preferences->gpu_program_cache_size = gpu::GetDefaultGpuDiskCacheSize();

  gpu_preferences->watchdog_starts_backgrounded = !application_is_visible_;

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  gpu_preferences->gpu_startup_dialog =
#if BUILDFLAG(IS_WIN)
      (kind == GPU_PROCESS_KIND_INFO_COLLECTION &&
       command_line->HasSwitch(switches::kGpu2StartupDialog)) ||
#endif
      (kind == GPU_PROCESS_KIND_SANDBOXED &&
       command_line->HasSwitch(switches::kGpuStartupDialog));

#if BUILDFLAG(IS_WIN)
  if (kind == GPU_PROCESS_KIND_INFO_COLLECTION) {
    gpu_preferences->disable_gpu_watchdog = true;
    gpu_preferences->enable_perf_data_collection = true;
  }
#endif

#if BUILDFLAG(IS_OZONE)
  gpu_preferences->message_pump_type = ui::OzonePlatform::GetInstance()
                                           ->GetPlatformProperties()
                                           .message_pump_type_for_gpu;
#endif

  // Disable loading VulkanImplementation if not using Ganesh/Vulkan.
  if (gpu_mode_ != gpu::GpuMode::HARDWARE_VULKAN) {
    gpu_preferences->use_vulkan = gpu::VulkanImplementationName::kNone;
  }

  if (!HardwareAccelerationEnabled()) {
    gpu_preferences->gr_context_type = gpu::GrContextType::kNone;
  } else if (gpu_mode_ != gpu::GpuMode::HARDWARE_GRAPHITE) {
    // Recompute the `gr_context_type` pref with Graphite explicitly disabled,
    // as it may currently be set to Graphite.
    auto command_line_with_graphite_disabled(*command_line);
    command_line_with_graphite_disabled.AppendSwitch(
        switches::kDisableSkiaGraphite);
    gpu_preferences->gr_context_type =
        gpu::gles2::ParseGrContextType(&command_line_with_graphite_disabled);
  }
}

void GpuDataManagerImplPrivate::DisableHardwareAcceleration() {
  hardware_disabled_explicitly_ = true;
  while (HardwareAccelerationEnabled())
    FallBackToNextGpuMode();
}

bool GpuDataManagerImplPrivate::HardwareAccelerationEnabled() const {
  switch (gpu_mode_) {
    case gpu::GpuMode::HARDWARE_GL:
    case gpu::GpuMode::HARDWARE_GRAPHITE:
    case gpu::GpuMode::HARDWARE_VULKAN:
      return true;
    default:
      return false;
  }
}

void GpuDataManagerImplPrivate::OnGpuBlocked() {
  std::optional<gpu::GpuFeatureInfo> gpu_feature_info_for_hardware_gpu;
  if (gpu_feature_info_.IsInitialized())
    gpu_feature_info_for_hardware_gpu = gpu_feature_info_;
  gpu::GpuFeatureInfo gpu_feature_info = gpu::ComputeGpuFeatureInfoWithNoGpu();
  UpdateGpuFeatureInfo(gpu_feature_info, gpu_feature_info_for_hardware_gpu);

  // Some observers might be waiting.
  NotifyGpuInfoUpdate();
}

void GpuDataManagerImplPrivate::AddLogMessage(int level,
                                              const std::string& header,
                                              const std::string& message) {
  // Some clients emit many log messages. This has been observed to consume GBs
  // of memory in the wild
  // https://bugs.chromium.org/p/chromium/issues/detail?id=798012. Use a limit
  // of 1000 messages to prevent excess memory usage.
  const int kLogMessageLimit = 1000;

  log_messages_.push_back(LogMessage(level, header, message));
  if (log_messages_.size() > kLogMessageLimit)
    log_messages_.erase(log_messages_.begin());
}

void GpuDataManagerImplPrivate::ProcessCrashed() {
  observer_list_->Notify(FROM_HERE,
                         &GpuDataManagerObserver::OnGpuProcessCrashed);
}

base::Value::List GpuDataManagerImplPrivate::GetLogMessages() const {
  base::Value::List value;
  for (const auto& log_message : log_messages_) {
    base::Value::Dict dict;
    dict.Set("level", log_message.level);
    dict.Set("header", log_message.header);
    dict.Set("message", log_message.message);
    value.Append(std::move(dict));
  }
  return value;
}

void GpuDataManagerImplPrivate::HandleGpuSwitch() {
  base::AutoUnlock unlock(owner_->lock_);
  // Notify observers in the browser process.
  ui::GpuSwitchingManager::GetInstance()->NotifyGpuSwitched(
      active_gpu_heuristic_);
  // Pass the notification to the GPU process to notify observers there.
  GpuProcessHost::CallOnUI(
      FROM_HERE, GPU_PROCESS_KIND_SANDBOXED, false /* force_create */,
      base::BindOnce(
          [](gl::GpuPreference active_gpu, GpuProcessHost* host) {
            if (host)
              host->gpu_service()->GpuSwitched(active_gpu);
          },
          active_gpu_heuristic_));
}

void GpuDataManagerImplPrivate::OnDisplayAdded(
    const display::Display& new_display) {
  base::AutoUnlock unlock(owner_->lock_);

  // Notify observers in the browser process.
  ui::GpuSwitchingManager::GetInstance()->NotifyDisplayAdded();
  // Pass the notification to the GPU process to notify observers there.
  GpuProcessHost::CallOnUI(FROM_HERE, GPU_PROCESS_KIND_SANDBOXED,
                           false /* force_create */,
                           base::BindOnce([](GpuProcessHost* host) {
                             if (host)
                               host->gpu_service()->DisplayAdded();
                           }));
}

void GpuDataManagerImplPrivate::OnDisplaysRemoved(
    const display::Displays& removed_displays) {
  base::AutoUnlock unlock(owner_->lock_);

  // Notify observers in the browser process.
  ui::GpuSwitchingManager::GetInstance()->NotifyDisplayRemoved();
  // Pass the notification to the GPU process to notify observers there.
  GpuProcessHost::CallOnUI(FROM_HERE, GPU_PROCESS_KIND_SANDBOXED,
                           false /* force_create */,
                           base::BindOnce([](GpuProcessHost* host) {
                             if (host)
                               host->gpu_service()->DisplayRemoved();
                           }));
}

void GpuDataManagerImplPrivate::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  base::AutoUnlock unlock(owner_->lock_);

  // Notify observers in the browser process.
  ui::GpuSwitchingManager::GetInstance()->NotifyDisplayMetricsChanged();
  // Pass the notification to the GPU process to notify observers there.
  GpuProcessHost::CallOnUI(FROM_HERE, GPU_PROCESS_KIND_SANDBOXED,
                           false /* force_create */,
                           base::BindOnce([](GpuProcessHost* host) {
                             if (host)
                               host->gpu_service()->DisplayMetricsChanged();
                           }));
}

void GpuDataManagerImplPrivate::BlockDomainsFrom3DAPIs(
    const std::set<GURL>& urls,
    gpu::DomainGuilt guilt) {
  BlockDomainsFrom3DAPIsAtTime(urls, guilt, base::Time::Now());
}

bool GpuDataManagerImplPrivate::Are3DAPIsBlocked(const GURL& top_origin_url,
                                                 ThreeDAPIType requester) {
  return Are3DAPIsBlockedAtTime(top_origin_url, base::Time::Now()) !=
         DomainBlockStatus::kNotBlocked;
}

void GpuDataManagerImplPrivate::DisableDomainBlockingFor3DAPIsForTesting() {
  domain_blocking_enabled_ = false;
}

void GpuDataManagerImplPrivate::NotifyGpuInfoUpdate() {
  observer_list_->Notify(FROM_HERE, &GpuDataManagerObserver::OnGpuInfoUpdate);
}

bool GpuDataManagerImplPrivate::IsGpuProcessUsingHardwareGpu() const {
  if (base::StartsWith(gpu_info_.gl_renderer, "Google SwiftShader",
                       base::CompareCase::SENSITIVE)) {
    return false;
  }
  if (base::StartsWith(gpu_info_.gl_renderer, "ANGLE",
                       base::CompareCase::SENSITIVE) &&
      gpu_info_.gl_renderer.find("SwiftShader Device") != std::string::npos) {
    return false;
  }
  if (gpu_info_.gl_renderer == "Disabled") {
    return false;
  }
  return true;
}

void GpuDataManagerImplPrivate::SetApplicationVisible(bool is_visible) {
  application_is_visible_ = is_visible;
}

std::string GpuDataManagerImplPrivate::GetDomainFromURL(const GURL& url) const {
  // For the moment, we just use the host, or its IP address, as the
  // entry in the set, rather than trying to figure out the top-level
  // domain. This does mean that a.foo.com and b.foo.com will be
  // treated independently in the blocking of a given domain, but it
  // would require a third-party library to reliably figure out the
  // top-level domain from a URL.
  if (!url.has_host()) {
    return std::string();
  }

  return url.host();
}

void GpuDataManagerImplPrivate::BlockDomainsFrom3DAPIsAtTime(
    const std::set<GURL>& urls,
    gpu::DomainGuilt guilt,
    base::Time at_time) {
  if (!domain_blocking_enabled_)
    return;

  // The coalescing of multiple entries for the same blocking event is
  // crucially important for the algorithm. Coalescing based on timestamp
  // would introduce flakiness.
  std::set<std::string> domains;
  for (const auto& url : urls) {
    domains.insert(GetDomainFromURL(url));
  }

  for (const auto& domain : domains) {
    blocked_domains_.insert({at_time, {domain, guilt}});
  }
}

static const base::TimeDelta kBlockedDomainExpirationPeriod = base::Minutes(2);

void GpuDataManagerImplPrivate::ExpireOldBlockedDomainsAtTime(
    base::Time at_time) const {
  // After kBlockedDomainExpirationPeriod, un-block a domain previously
  // blocked due to context loss.

  // Uses the fact that "blocked_domains_" is mutable to perform a cleanup.
  base::Time everything_expired_before =
      at_time - kBlockedDomainExpirationPeriod;
  blocked_domains_.erase(
      blocked_domains_.begin(),
      std::lower_bound(blocked_domains_.begin(), blocked_domains_.end(),
                       everything_expired_before,
                       [](const auto& elem, const base::Time& t) {
                         return elem.first < t;
                       }));
}

GpuDataManagerImplPrivate::DomainBlockStatus
GpuDataManagerImplPrivate::Are3DAPIsBlockedAtTime(const GURL& url,
                                                  base::Time at_time) const {
  if (!domain_blocking_enabled_)
    return DomainBlockStatus::kNotBlocked;

  // Note: adjusting the policies in this code will almost certainly
  // require adjusting the associated unit tests.

  // First expire old domain blocks.
  ExpireOldBlockedDomainsAtTime(at_time);

  std::string domain = GetDomainFromURL(url);
  size_t losses_for_domain = base::ranges::count(
      blocked_domains_, domain,
      [](const auto& entry) { return entry.second.domain; });
  // Allow one context loss per domain, so block if there are two or more.
  if (losses_for_domain > 1)
    return DomainBlockStatus::kBlocked;

  // Look at and cluster the timestamps of recent domain blocking events to
  // see if there are more than the threshold which would cause us to
  // blocklist all domains. GPU process crashes or TDR events are
  // discovered because the blocked domain entries all have the same
  // timestamp.
  //
  // TODO(kbr): make this pay attention to the TDR thresholds in the
  // Windows registry, but make sure it continues to be testable.
  {
    int num_event_clusters = 0;
    base::Time last_time;  // Initialized to the "zero" time.

    // Relies on the domain blocking events being sorted by increasing
    // timestamp.
    for (const auto& elem : blocked_domains_) {
      if (last_time.is_null() || elem.first != last_time) {
        last_time = elem.first;
        ++num_event_clusters;
      }
    }

    const int kMaxNumResetsWithinDuration = 2;

    if (num_event_clusters > kMaxNumResetsWithinDuration)
      return DomainBlockStatus::kAllDomainsBlocked;
  }

  return DomainBlockStatus::kNotBlocked;
}

base::TimeDelta GpuDataManagerImplPrivate::GetDomainBlockingExpirationPeriod()
    const {
  return kBlockedDomainExpirationPeriod;
}

gpu::GpuMode GpuDataManagerImplPrivate::GetGpuMode() const {
  return gpu_mode_;
}

void GpuDataManagerImplPrivate::FallBackToNextGpuMode() {
  if (fallback_modes_.empty()) {
#if BUILDFLAG(IS_ANDROID)
    FatalGpuProcessLaunchFailureOnBackground();
#endif
    IntentionallyCrashBrowserForUnusableGpuProcess();
  }

  gpu_mode_ = fallback_modes_.back();
  fallback_modes_.pop_back();
  DCHECK_NE(gpu_mode_, gpu::GpuMode::UNKNOWN);
  if (gpu_mode_ == gpu::GpuMode::DISPLAY_COMPOSITOR)
    OnGpuBlocked();
}

void GpuDataManagerImplPrivate::RecordCompositingMode() {
  CompositingMode compositing_mode;
  if (IsGpuCompositingDisabled()) {
    compositing_mode = CompositingMode::kSoftware;
  } else {
    // TODO(penghuang): Record kVulkan here if we're using Vulkan.
    compositing_mode = CompositingMode::kGL;
  }

  UMA_HISTOGRAM_ENUMERATION("GPU.CompositingMode", compositing_mode);
}

#if BUILDFLAG(IS_LINUX)
bool GpuDataManagerImplPrivate::IsGpuMemoryBufferNV12Supported() {
  return is_gpu_memory_buffer_NV12_supported_;
}
void GpuDataManagerImplPrivate::SetGpuMemoryBufferNV12Supported(
    bool supported) {
  is_gpu_memory_buffer_NV12_supported_ = supported;
}
#endif  // BUILDFLAG(IS_LINUX)

}  // namespace content
