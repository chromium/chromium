// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_data_manager_impl_private.h"

#if defined(OS_WIN)
#include <aclapi.h>
#include <sddl.h>
#include <windows.h>
#endif  // OS_WIN

#include <algorithm>
#include <array>
#include <iterator>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "base/version.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/switches.h"
#include "components/viz/common/features.h"
#include "content/browser/gpu/gpu_memory_buffer_manager_singleton.h"
#include "content/browser/gpu/gpu_process_host.h"
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
#include "gpu/ipc/host/gpu_memory_buffer_support.h"
#include "gpu/ipc/host/shader_disk_cache.h"
#include "gpu/vulkan/buildflags.h"
#include "media/media_buildflags.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/screen.h"
#include "ui/gfx/switches.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_preference.h"
#include "ui/gl/gpu_switching_manager.h"

#if defined(USE_OZONE) || defined(USE_X11)
#include "ui/base/ui_base_features.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#endif
#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif
#if defined(OS_MAC)
#include <ApplicationServices/ApplicationServices.h>
#endif  // OS_MAC
#if defined(OS_WIN)
#include "base/base_paths_win.h"
#include "ui/display/win/screen_win.h"
#endif  // OS_WIN
#if BUILDFLAG(IS_CHROMECAST)
#include "chromecast/chromecast_buildflags.h"
#endif

namespace content {

namespace {

// On X11 (Ozone and non-Ozone), we do not know GpuMemoryBuffer configuration
// support until receiving the initial GPUInfo.
bool CanUpdateGmbGpuPreferences() {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    return !ui::OzonePlatform::GetInstance()
                ->GetPlatformProperties()
                .fetch_buffer_formats_for_gmb_on_gpu;
  }
#endif
#if defined(USE_X11)
  DCHECK(!features::IsUsingOzonePlatform());
  return false;
#endif
  return true;
}

#if defined(OS_ANDROID)
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

#if defined(OS_WIN)
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
#endif  // OS_WIN

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
      gpu::GPU_FEATURE_TYPE_GPU_RASTERIZATION,
      gpu::GPU_FEATURE_TYPE_OOP_RASTERIZATION,
      gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
      gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL2};
  const std::string kGpuBlocklistFeatureHistogramNames[] = {
      "GPU.BlocklistFeatureTestResults.Accelerated2dCanvas",
      "GPU.BlocklistFeatureTestResults.GpuCompositing",
      "GPU.BlocklistFeatureTestResults.GpuRasterization",
      "GPU.BlocklistFeatureTestResults.OopRasterization",
      "GPU.BlocklistFeatureTestResults.Webgl",
      "GPU.BlocklistFeatureTestResults.Webgl2"};
  const bool kGpuFeatureUserFlags[] = {
      command_line.HasSwitch(switches::kDisableAccelerated2dCanvas),
      command_line.HasSwitch(switches::kDisableGpu),
      command_line.HasSwitch(switches::kDisableGpuRasterization),
      command_line.HasSwitch(switches::kDisableOopRasterization),
      command_line.HasSwitch(switches::kDisableWebGL),
      (command_line.HasSwitch(switches::kDisableWebGL) ||
       command_line.HasSwitch(switches::kDisableWebGL2))};
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

#if defined(OS_MAC)
void DisplayReconfigCallback(CGDirectDisplayID display,
                             CGDisplayChangeSummaryFlags flags,
                             void* gpu_data_manager) {
  if (flags == kCGDisplayBeginConfigurationFlag)
    return;  // This call contains no information about the display change

  GpuDataManagerImpl* manager =
      reinterpret_cast<GpuDataManagerImpl*>(gpu_data_manager);
  DCHECK(manager);

  manager->HandleGpuSwitch();
}
#endif  // OS_MAC

// Block all domains' use of 3D APIs for this many milliseconds if
// approaching a threshold where system stability might be compromised.
const int64_t kBlockAllDomainsMs = 10000;
const int kNumResetsWithinDuration = 1;

// Enums for UMA histograms.
enum BlockStatusHistogram {
  BLOCK_STATUS_NOT_BLOCKED,
  BLOCK_STATUS_SPECIFIC_DOMAIN_BLOCKED,
  BLOCK_STATUS_ALL_DOMAINS_BLOCKED,
  BLOCK_STATUS_MAX
};

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
#if BUILDFLAG(ENABLE_SWIFTSHADER)
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableSoftwareRasterizer);
#else
  return false;
#endif
}

// Determines if Vulkan is available for the GPU process.
bool ALLOW_UNUSED_TYPE VulkanAllowed() {
#if BUILDFLAG(ENABLE_VULKAN)
  // Vulkan will be enabled if certain flags are present.
  // --enable-features=Vulkan will cause Vulkan to be used for compositing and
  // rasterization. --use-vulkan by itself will initialize Vulkan so that it can
  // be used for other purposes, such as WebGPU.
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  gpu::VulkanImplementationName use_vulkan =
      gpu::gles2::ParseVulkanImplementationName(command_line);
  return use_vulkan != gpu::VulkanImplementationName::kNone;
#else
  return false;
#endif
}

// Determines if Metal is available for the GPU process.
bool ALLOW_UNUSED_TYPE MetalAllowed() {
#if defined(OS_MAC)
  return base::FeatureList::IsEnabled(features::kMetal);
#else
  return false;
#endif
}

// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with "CompositingMode" in
// src/tools/metrics/histograms/enums.xml.
enum class CompositingMode {
  kSoftware = 0,
  kGL = 1,
  kVulkan = 2,
  kMetal = 3,
  kMaxValue = kMetal
};

// Intentionally crash with a very descriptive name.
NOINLINE void IntentionallyCrashBrowserForUnusableGpuProcess() {
  LOG(FATAL) << "GPU process isn't usable. Goodbye.";
}

#if defined(OS_WIN)
void CollectExtraDevicePerfInfo(const gpu::GPUInfo& gpu_info,
                                gpu::DevicePerfInfo* device_perf_info) {
  device_perf_info->intel_gpu_generation = gpu::GetIntelGpuGeneration(gpu_info);
  const gpu::GPUInfo::GPUDevice& device = gpu_info.active_gpu();
  if (device.vendor_id == 0xffff /* internal flag for software rendering */ ||
      device.vendor_id == 0x15ad /* VMware */ ||
      device.vendor_id == 0x1414 /* Microsoft software renderer */ ||
      gpu_info.software_rendering /* SwiftShader */) {
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
    // The request must be sent to the GPU process from the IO thread.
    auto task_runner = base::FeatureList::IsEnabled(features::kProcessHostOnUI)
                           ? GetUIThreadTaskRunner({})
                           : GetIOThreadTaskRunner({});
    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(&HDRProxy::RequestOnProcessThread));
  }

  static void GotResultOnProcessThread(bool hdr_enabled) {
    if (base::FeatureList::IsEnabled(features::kProcessHostOnUI)) {
      GotResult(hdr_enabled);
    } else {
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&HDRProxy::GotResult, hdr_enabled));
    }
  }

 private:
  static void RequestOnProcessThread() {
    auto* gpu_process_host =
        GpuProcessHost::Get(GPU_PROCESS_KIND_SANDBOXED, false);
    if (gpu_process_host) {
      auto* gpu_service = gpu_process_host->gpu_host()->gpu_service();
      gpu_service->RequestHDRStatus(
          base::BindOnce(&HDRProxy::GotResultOnProcessThread));
    } else {
      bool hdr_enabled = false;
      GotResultOnProcessThread(hdr_enabled);
    }
  }
  static void GotResult(bool hdr_enabled) {
    display::win::ScreenWin::SetHDREnabled(hdr_enabled);
  }
};

#endif  // OS_WIN
}  // anonymous namespace

GpuDataManagerImplPrivate::GpuDataManagerImplPrivate(GpuDataManagerImpl* owner)
    : owner_(owner),
      observer_list_(base::MakeRefCounted<GpuDataManagerObserverList>()) {
  DCHECK(owner_);
  InitializeGpuModes();
#if defined(OS_WIN)
  EnableIntelShaderCache();
#endif  // OS_WIN
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableGpuCompositing)) {
    SetGpuCompositingDisabled();
  }

  if (command_line->HasSwitch(switches::kSingleProcess) ||
      command_line->HasSwitch(switches::kInProcessGPU)) {
    AppendGpuCommandLine(command_line, GPU_PROCESS_KIND_SANDBOXED);
  }

#if defined(OS_MAC)
  CGDisplayRegisterReconfigurationCallback(DisplayReconfigCallback, owner_);
#endif  // OS_MAC

  // For testing only.
  if (command_line->HasSwitch(switches::kDisableDomainBlockingFor3DAPIs))
    domain_blocking_enabled_ = false;
}

GpuDataManagerImplPrivate::~GpuDataManagerImplPrivate() {
#if defined(OS_MAC)
  CGDisplayRemoveReconfigurationCallback(DisplayReconfigCallback, owner_);
#endif
}

void GpuDataManagerImplPrivate::StartUmaTimer() {
  // Do not change kTimerInterval without also changing the UMA histogram name,
  // as histogram data from before/after the change will not be comparable.
  constexpr base::TimeDelta kTimerInterval = base::TimeDelta::FromMinutes(5);
  compositing_mode_timer_.Start(
      FROM_HERE, kTimerInterval, this,
      &GpuDataManagerImplPrivate::RecordCompositingMode);
}

void GpuDataManagerImplPrivate::InitializeGpuModes() {
  DCHECK_EQ(gpu::GpuMode::UNKNOWN, gpu_mode_);
  // Android and Chrome OS can't switch to software compositing. If the GPU
  // process initialization fails or GPU process is too unstable then crash the
  // browser process to reset everything.
#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  fallback_modes_.push_back(gpu::GpuMode::DISPLAY_COMPOSITOR);
  if (SwiftShaderAllowed())
    fallback_modes_.push_back(gpu::GpuMode::SWIFTSHADER);
#endif  // !OS_ANDROID && !OS_CHROMEOS

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableGpu)) {
    // Chomecast audio-only builds run with the flag --disable-gpu. The GPU
    // process should not access hardware GPU in this case.
#if BUILDFLAG(IS_CHROMECAST)
#if BUILDFLAG(IS_CAST_AUDIO_ONLY)
    fallback_modes_.clear();
    fallback_modes_.push_back(gpu::GpuMode::DISPLAY_COMPOSITOR);
#endif
#elif defined(OS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
    CHECK(false) << "GPU acceleration is required on certain platforms!";
#endif  // IS_CHROMECAST
  } else {
    // On Fuchsia Vulkan must be used when it's enabled by the WebEngine
    // embedder. Falling back to SW compositing in that case is not supported.
#if defined(OS_FUCHSIA)
    fallback_modes_.clear();
    fallback_modes_.push_back(gpu::GpuMode::HARDWARE_VULKAN);
#else
    fallback_modes_.push_back(gpu::GpuMode::HARDWARE_GL);

    if (VulkanAllowed())
      fallback_modes_.push_back(gpu::GpuMode::HARDWARE_VULKAN);
    if (MetalAllowed())
      fallback_modes_.push_back(gpu::GpuMode::HARDWARE_METAL);
#endif  // OS_FUCHSIA
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
  UpdateGpuFeatureInfo(gpu_feature_info, absl::nullopt);
  NotifyGpuInfoUpdate();
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
    case gpu::GpuMode::HARDWARE_METAL:
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
          if (base::CommandLine::ForCurrentProcess()->HasSwitch(
                  switches::kDisableGpu))
            *reason += "through commandline switch --disable-gpu.";
          else if (hardware_disabled_explicitly_)
            *reason += "in chrome://settings.";
          else
            *reason += "due to frequent crashes.";
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

void GpuDataManagerImplPrivate::RequestDxdiagDx12VulkanGpuInfoIfNeeded(
    GpuDataManagerImpl::GpuInfoRequest request,
    bool delayed) {
  if (request & GpuDataManagerImpl::kGpuInfoRequestDxDiag) {
    // Delay is not supported in DxDiag request
    DCHECK(!delayed);
    RequestDxDiagNodeData();
  }

  if (request & GpuDataManagerImpl::kGpuInfoRequestDx12)
    RequestGpuSupportedDx12Version(delayed);

  if (request & GpuDataManagerImpl::kGpuInfoRequestVulkan)
    RequestGpuSupportedVulkanVersion(delayed);

  if (request & GpuDataManagerImpl::kGpuInfoRequestDawnInfo)
    RequestDawnInfo();
}

void GpuDataManagerImplPrivate::RequestDxDiagNodeData() {
#if defined(OS_WIN)
  if (gpu_info_dx_diag_requested_)
    return;
  gpu_info_dx_diag_requested_ = true;

  base::OnceClosure task = base::BindOnce([]() {
    GpuDataManagerImpl* manager = GpuDataManagerImpl::GetInstance();
    // No info collection for software GL implementation (id == 0xffff) or
    // abnormal situation (id == 0). There are a few crash reports on
    // exit_or_terminate_process() during process teardown. The GPU ID
    // should be available by the time this task starts to run.
    // This request comes from chrome://gpu page.
    const gpu::GPUInfo::GPUDevice gpu = manager->GetGPUInfo().gpu;
    if ((gpu.vendor_id == 0xffff && gpu.device_id == 0xffff) ||
        (gpu.vendor_id == 0 && gpu.device_id == 0)) {
      manager->UpdateDxDiagNodeRequestStatus(false);
      return;
    }

    GpuProcessHost* host = GpuProcessHost::Get(GPU_PROCESS_KIND_INFO_COLLECTION,
                                               true /* force_create */);
    if (!host) {
      manager->UpdateDxDiagNodeRequestStatus(false);
      return;
    }

    manager->UpdateDxDiagNodeRequestStatus(true);
    host->info_collection_gpu_service()->RequestDxDiagNodeInfo(
        base::BindOnce([](const gpu::DxDiagNode& dx_diagnostics) {
          GpuDataManagerImpl* manager = GpuDataManagerImpl::GetInstance();
          manager->UpdateDxDiagNode(dx_diagnostics);
          manager->TerminateInfoCollectionGpuProcess();
        }));
  });

  auto task_runner = base::FeatureList::IsEnabled(features::kProcessHostOnUI)
                         ? GetUIThreadTaskRunner({})
                         : GetIOThreadTaskRunner({});
  task_runner->PostTask(FROM_HERE, std::move(task));
#endif
}

void GpuDataManagerImplPrivate::RequestGpuSupportedDx12Version(bool delayed) {
#if defined(OS_WIN)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::TimeDelta delta;
  if (delayed &&
      !command_line->HasSwitch(switches::kNoDelayForDX12VulkanInfoCollection)) {
    delta = base::TimeDelta::FromSeconds(120);
  }

  base::OnceClosure task = base::BindOnce(
      [](base::TimeDelta delta) {
        GpuDataManagerImpl* manager = GpuDataManagerImpl::GetInstance();
        if (manager->Dx12Requested())
          return;

        base::CommandLine* command_line =
            base::CommandLine::ForCurrentProcess();
        if (command_line->HasSwitch(
                switches::kDisableGpuProcessForDX12InfoCollection)) {
          manager->UpdateDx12RequestStatus(false);
          return;
        }

        // No info collection for software GL implementation (id == 0xffff) or
        // abnormal situation (id == 0). There are a few crash reports on
        // exit_or_terminate_process() during process teardown. The GPU ID
        // should be available by the time this task starts to run. In the case
        // of no delay, which is for testing only, don't check the GPU ID
        // because the ID is not available yet.
        const gpu::GPUInfo::GPUDevice gpu = manager->GetGPUInfo().gpu;
        if ((gpu.vendor_id == 0xffff && gpu.device_id == 0xffff) ||
            (!delta.is_zero() && gpu.vendor_id == 0 && gpu.device_id == 0)) {
          manager->UpdateDx12RequestStatus(false);
          return;
        }

        GpuProcessHost* host = GpuProcessHost::Get(
            GPU_PROCESS_KIND_INFO_COLLECTION, true /* force_create */);
        if (!host) {
          manager->UpdateDx12RequestStatus(false);
          return;
        }

        manager->UpdateDx12RequestStatus(true);
        host->info_collection_gpu_service()
            ->GetGpuSupportedDx12VersionAndDevicePerfInfo(
                base::BindOnce([](uint32_t d3d12_feature_level,
                                  uint32_t highest_shader_model_version,
                                  const gpu::DevicePerfInfo& device_perf_info) {
                  GpuDataManagerImpl* manager =
                      GpuDataManagerImpl::GetInstance();
                  manager->UpdateDx12Info(d3d12_feature_level);
                  // UpdateDx1Info() needs to be called before
                  // UpdateDevicePerfInfo() because only the latter calls
                  // NotifyGpuInfoUpdate().
                  manager->UpdateDevicePerfInfo(device_perf_info);
                  manager->TerminateInfoCollectionGpuProcess();
                  gpu::RecordGpuSupportedDx12VersionHistograms(
                      d3d12_feature_level, highest_shader_model_version);
                }));
      },
      delta);

  auto task_runner = base::FeatureList::IsEnabled(features::kProcessHostOnUI)
                         ? GetUIThreadTaskRunner({})
                         : GetIOThreadTaskRunner({});
  task_runner->PostDelayedTask(FROM_HERE, std::move(task), delta);
#endif
}

void GpuDataManagerImplPrivate::RequestGpuSupportedVulkanVersion(bool delayed) {
#if defined(OS_WIN)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::TimeDelta delta;
  if (delayed &&
      !command_line->HasSwitch(switches::kNoDelayForDX12VulkanInfoCollection)) {
    delta = base::TimeDelta::FromSeconds(120);
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

  auto task_runner = base::FeatureList::IsEnabled(features::kProcessHostOnUI)
                         ? GetUIThreadTaskRunner({})
                         : GetIOThreadTaskRunner({});
  task_runner->PostDelayedTask(FROM_HERE, std::move(task), delta);
#endif
}

void GpuDataManagerImplPrivate::RequestDawnInfo() {
  if (gpu_info_dawn_toggles_requested_)
    return;
  gpu_info_dawn_toggles_requested_ = true;

  base::OnceClosure task = base::BindOnce([]() {
    GpuProcessHost* host = GpuProcessHost::Get(GPU_PROCESS_KIND_SANDBOXED,
                                               false /* force_create */);
    if (!host)
      return;

    host->gpu_service()->GetDawnInfo(
        base::BindOnce([](const std::vector<std::string>& dawn_info_list) {
          GpuDataManagerImpl* manager = GpuDataManagerImpl::GetInstance();
          manager->UpdateDawnInfo(dawn_info_list);
        }));
  });

  auto task_runner = base::FeatureList::IsEnabled(features::kProcessHostOnUI)
                         ? GetUIThreadTaskRunner({})
                         : GetIOThreadTaskRunner({});
  task_runner->PostTask(FROM_HERE, std::move(task));
}

bool GpuDataManagerImplPrivate::IsEssentialGpuInfoAvailable() const {
  // We always update GPUInfo and GpuFeatureInfo from GPU process together.
  return IsGpuFeatureInfoAvailable();
}

bool GpuDataManagerImplPrivate::IsDx12VulkanVersionAvailable() const {
#if defined(OS_WIN)
  // Certain gpu_integration_test needs dx12/Vulkan info. If this info is
  // needed, --no-delay-for-dx12-vulkan-info-collection should be added to the
  // browser command line, so that the collection of this info isn't delayed.
  // This function returns the status of availability to the tests based on
  // whether gpu info has been requested or not.

  return (gpu_info_dx12_valid_ && gpu_info_vulkan_valid_) ||
         (!gpu_info_dx12_requested_ || !gpu_info_vulkan_requested_) ||
         (gpu_info_dx12_request_failed_ || gpu_info_vulkan_request_failed_);
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
  GpuProcessHost::CallOnIO(
      GPU_PROCESS_KIND_SANDBOXED, false /* force_create */,
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
  // This method must do two things:
  //
  //  1. If the specific domain is blocked, then unblock it.
  //
  //  2. Reset our notion of how many GPU resets have occurred recently.
  //     This is necessary even if the specific domain was blocked.
  //     Otherwise, if we call Are3DAPIsBlocked with the same domain right
  //     after unblocking it, it will probably still be blocked because of
  //     the recent GPU reset caused by that domain.
  //
  // These policies could be refined, but at a certain point the behavior
  // will become difficult to explain.

  // Shortcut in the common case where no blocking has occurred. This
  // is important to not regress navigation performance, since this is
  // now called on every user-initiated navigation.
  if (blocked_domains_.empty() && timestamps_of_gpu_resets_.empty())
    return;

  std::string domain = GetDomainFromURL(url);

  blocked_domains_.erase(domain);
  timestamps_of_gpu_resets_.clear();
}

void GpuDataManagerImplPrivate::UpdateGpuInfo(
    const gpu::GPUInfo& gpu_info,
    const absl::optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu) {
#if defined(OS_WIN)
  // If GPU process crashes and launches again, GPUInfo will be sent back from
  // the new GPU process again, and may overwrite the DX12, Vulkan, DxDiagNode
  // info we already collected. This is to make sure it doesn't happen.
  gpu::DxDiagNode dx_diagnostics = gpu_info_.dx_diagnostics;
  uint32_t d3d12_feature_level = gpu_info_.d3d12_feature_level;
  uint32_t vulkan_version = gpu_info_.vulkan_version;
#endif
  gpu_info_ = gpu_info;
  base::UmaHistogramCustomMicrosecondsTimes(
      "GPU.GPUInitializationTime.V3", gpu_info_.initialization_time,
      base::TimeDelta::FromMilliseconds(5), base::TimeDelta::FromSeconds(5),
      50);
  UMA_HISTOGRAM_EXACT_LINEAR("GPU.GpuCount", gpu_info_.GpuCount(), 10);
  RecordDiscreteGpuHistograms(gpu_info_);
#if defined(OS_WIN)
  if (!dx_diagnostics.IsEmpty()) {
    gpu_info_.dx_diagnostics = dx_diagnostics;
  }
  if (d3d12_feature_level != 0) {
    gpu_info_.d3d12_feature_level = d3d12_feature_level;
  }
  if (vulkan_version != 0) {
    gpu_info_.vulkan_version = vulkan_version;
  }
#endif  // OS_WIN

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
#if defined(OS_WIN)
    if (active_gpu.luid.HighPart != cached_active_gpu.luid.HighPart &&
        active_gpu.luid.LowPart != cached_active_gpu.luid.LowPart) {
      needs_to_update_gpu_info_for_hardware_gpu = true;
    }
#else
    if (active_gpu.vendor_id != cached_active_gpu.vendor_id ||
        active_gpu.device_id != cached_active_gpu.device_id) {
      needs_to_update_gpu_info_for_hardware_gpu = true;
    }
#endif  // OS_WIN
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

#if defined(OS_WIN)
void GpuDataManagerImplPrivate::UpdateDxDiagNode(
    const gpu::DxDiagNode& dx_diagnostics) {
  gpu_info_.dx_diagnostics = dx_diagnostics;
  // No need to call GetContentClient()->SetGpuInfo().
  NotifyGpuInfoUpdate();
}

void GpuDataManagerImplPrivate::UpdateDx12Info(uint32_t d3d12_feature_level) {
  gpu_info_.d3d12_feature_level = d3d12_feature_level;
  gpu_info_dx12_valid_ = true;
  // No need to call NotifyGpuInfoUpdate() because UpdateDx12Info() is
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

void GpuDataManagerImplPrivate::UpdateHDRStatus(bool hdr_enabled) {
  // This is running on the process thread;
  DCHECK_CURRENTLY_ON(base::FeatureList::IsEnabled(features::kProcessHostOnUI)
                          ? BrowserThread::UI
                          : BrowserThread::IO);
  HDRProxy::GotResultOnProcessThread(hdr_enabled);
}

void GpuDataManagerImplPrivate::UpdateDxDiagNodeRequestStatus(
    bool request_continues) {
  gpu_info_dx_diag_request_failed_ = !request_continues;

  if (gpu_info_dx_diag_request_failed_)
    NotifyGpuInfoUpdate();
}

void GpuDataManagerImplPrivate::UpdateDx12RequestStatus(
    bool request_continues) {
  gpu_info_dx12_requested_ = true;
  gpu_info_dx12_request_failed_ = !request_continues;

  if (gpu_info_dx12_request_failed_) {
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

bool GpuDataManagerImplPrivate::Dx12Requested() const {
  return gpu_info_dx12_requested_;
}

bool GpuDataManagerImplPrivate::VulkanRequested() const {
  return gpu_info_vulkan_requested_;
}

void GpuDataManagerImplPrivate::PostCreateThreads() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kNoDelayForDX12VulkanInfoCollection)) {
    // This is for the info collection test of the gpu integration tests.
    RequestDxdiagDx12VulkanGpuInfoIfNeeded(
        GpuDataManagerImpl::kGpuInfoRequestDx12Vulkan,
        /*delayed=*/false);
  } else {
    // Launch the info collection GPU process to collect DX12 support
    // information for UMA at the start of the browser.
    // Not to affect Chrome startup, this is done in a delayed mode,  i.e., 120
    // seconds after Chrome startup.
    RequestDxdiagDx12VulkanGpuInfoIfNeeded(
        GpuDataManagerImpl::kGpuInfoRequestDx12, /*delayed=*/true);
  }
  // Observer for display change.
  display_observer_.emplace(owner_);

  // Initialization for HDR status update.
  HDRProxy::Initialize();
}

void GpuDataManagerImplPrivate::TerminateInfoCollectionGpuProcess() {
  // Wait until DxDiag, DX12/Vulkan and DevicePerfInfo requests are all
  // complete.
  if (gpu_info_dx_diag_requested_ && !gpu_info_dx_diag_request_failed_ &&
      gpu_info_.dx_diagnostics.IsEmpty())
    return;
  // gpu_info_dx12_valid_ is always updated before device_perf_info
  if (gpu_info_dx12_requested_ && !gpu_info_dx12_request_failed_ &&
      !gpu::GetDevicePerfInfo().has_value())
    return;

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

void GpuDataManagerImplPrivate::UpdateDawnInfo(
    const std::vector<std::string>& dawn_info_list) {
  dawn_info_list_ = dawn_info_list;

  NotifyGpuInfoUpdate();
}

void GpuDataManagerImplPrivate::UpdateGpuFeatureInfo(
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const absl::optional<gpu::GpuFeatureInfo>&
        gpu_feature_info_for_hardware_gpu) {
  gpu_feature_info_ = gpu_feature_info;
#if !defined(OS_FUCHSIA)
  // With Vulkan or Metal, GL might be blocked, so make sure we don't fallback
  // to it later.
  if (HardwareAccelerationEnabled() &&
      gpu_feature_info_.status_values[gpu::GPU_FEATURE_TYPE_ACCELERATED_GL] !=
          gpu::GpuFeatureStatus::kGpuFeatureStatusEnabled) {
    fallback_modes_.erase(
        std::remove(fallback_modes_.begin(), fallback_modes_.end(),
                    gpu::GpuMode::HARDWARE_GL),
        fallback_modes_.end());
  }
  // If Vulkan initialization fails, the GPU process can silently fallback to
  // GL.
  if (gpu_mode_ == gpu::GpuMode::HARDWARE_VULKAN &&
      gpu_feature_info_.status_values[gpu::GPU_FEATURE_TYPE_VULKAN] !=
          gpu::GpuFeatureStatus::kGpuFeatureStatusEnabled) {
    // TODO(rivr): The GpuMode in GpuProcessHost will still be
    // HARDWARE_VULKAN. This isn't a big issue right now because both GPU modes
    // report to the same histogram. The first fallback will occur after 4
    // crashes, instead of 3.
    FallBackToNextGpuMode();
  }
#endif  // !OS_FUCHSIA
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
  }
}

void GpuDataManagerImplPrivate::UpdateGpuExtraInfo(
    const gfx::GpuExtraInfo& gpu_extra_info) {
  gpu_extra_info_ = gpu_extra_info;
  observer_list_->Notify(FROM_HERE,
                         &GpuDataManagerObserver::OnGpuExtraInfoUpdate);
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
    case gpu::GpuMode::HARDWARE_METAL:
    case gpu::GpuMode::HARDWARE_VULKAN:
      use_gl = browser_command_line->GetSwitchValueASCII(switches::kUseGL);
      break;
    case gpu::GpuMode::SWIFTSHADER: {
      bool legacy_software_gl = true;
#if (defined(OS_LINUX) && !defined(USE_OZONE)) || defined(OS_WIN)
      // This setting makes WebGL run on SwANGLE instead of SwiftShader GL.
      legacy_software_gl = false;
#endif
      gl::SetSoftwareWebGLCommandLineSwitches(command_line, legacy_software_gl);
    } break;
    default:
      use_gl = gl::kGLImplementationDisabledName;
  }
  if (!use_gl.empty()) {
    command_line->AppendSwitchASCII(switches::kUseGL, use_gl);
  }

#if !defined(OS_MAC)
  // MacOSX bots use real GPU in tests.
  if (browser_command_line->HasSwitch(switches::kHeadless)) {
    if (command_line->HasSwitch(switches::kUseGL)) {
      use_gl = command_line->GetSwitchValueASCII(switches::kUseGL);
      // Don't append kOverrideUseSoftwareGLForHeadless when we need to enable
      // GPU hardware for headless chromium.
      if (use_gl != gl::kGLImplementationEGLName)
        command_line->AppendSwitch(switches::kOverrideUseSoftwareGLForHeadless);
    }
  }
#endif  // !OS_MAC
}

void GpuDataManagerImplPrivate::UpdateGpuPreferences(
    gpu::GpuPreferences* gpu_preferences,
    GpuProcessKind kind) const {
  DCHECK(gpu_preferences);

  // For performance reasons, discourage storing VideoFrames in a biplanar
  // GpuMemoryBuffer if this is not native, see https://crbug.com/791676.
  auto* gpu_memory_buffer_manager =
      GpuMemoryBufferManagerSingleton::GetInstance();
  if (gpu_memory_buffer_manager && CanUpdateGmbGpuPreferences()) {
    gpu_preferences->disable_biplanar_gpu_memory_buffers_for_video_frames =
        !gpu_memory_buffer_manager->IsNativeGpuMemoryBufferConfiguration(
            gfx::BufferFormat::YUV_420_BIPLANAR,
            gfx::BufferUsage::GPU_READ_CPU_READ_WRITE);
  }

  gpu_preferences->gpu_program_cache_size =
      gpu::ShaderDiskCache::CacheSizeBytes();

  gpu_preferences->texture_target_exception_list =
      gpu::CreateBufferUsageAndFormatExceptionList();

  gpu_preferences->watchdog_starts_backgrounded = !application_is_visible_;

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  gpu_preferences->gpu_startup_dialog =
#if defined(OS_WIN)
      (kind == GPU_PROCESS_KIND_INFO_COLLECTION &&
       command_line->HasSwitch(switches::kGpu2StartupDialog)) ||
#endif
      (kind == GPU_PROCESS_KIND_SANDBOXED &&
       command_line->HasSwitch(switches::kGpuStartupDialog));

#if defined(OS_WIN)
  if (kind == GPU_PROCESS_KIND_INFO_COLLECTION) {
    gpu_preferences->disable_gpu_watchdog = true;
    gpu_preferences->enable_perf_data_collection = true;
  }
#endif

#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    gpu_preferences->message_pump_type = ui::OzonePlatform::GetInstance()
                                             ->GetPlatformProperties()
                                             .message_pump_type_for_gpu;
  }
#endif

#if defined(OS_MAC)
  if (gpu_mode_ != gpu::GpuMode::HARDWARE_METAL)
    gpu_preferences->enable_metal = false;
#elif BUILDFLAG(ENABLE_VULKAN)
  if (gpu_mode_ != gpu::GpuMode::HARDWARE_VULKAN)
    gpu_preferences->use_vulkan = gpu::VulkanImplementationName::kNone;
#endif
}

void GpuDataManagerImplPrivate::DisableHardwareAcceleration() {
  hardware_disabled_explicitly_ = true;
  while (HardwareAccelerationEnabled())
    FallBackToNextGpuMode();
}

bool GpuDataManagerImplPrivate::HardwareAccelerationEnabled() const {
  switch (gpu_mode_) {
    case gpu::GpuMode::HARDWARE_GL:
    case gpu::GpuMode::HARDWARE_METAL:
    case gpu::GpuMode::HARDWARE_VULKAN:
      return true;
    default:
      return false;
  }
}

void GpuDataManagerImplPrivate::OnGpuBlocked() {
  absl::optional<gpu::GpuFeatureInfo> gpu_feature_info_for_hardware_gpu;
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

void GpuDataManagerImplPrivate::ProcessCrashed(
    base::TerminationStatus exit_code) {
  observer_list_->Notify(
      FROM_HERE, &GpuDataManagerObserver::OnGpuProcessCrashed, exit_code);
}

std::unique_ptr<base::ListValue> GpuDataManagerImplPrivate::GetLogMessages()
    const {
  auto value = std::make_unique<base::ListValue>();
  for (size_t ii = 0; ii < log_messages_.size(); ++ii) {
    auto dict = std::make_unique<base::DictionaryValue>();
    dict->SetInteger("level", log_messages_[ii].level);
    dict->SetString("header", log_messages_[ii].header);
    dict->SetString("message", log_messages_[ii].message);
    value->Append(std::move(dict));
  }
  return value;
}

void GpuDataManagerImplPrivate::HandleGpuSwitch() {
  base::AutoUnlock unlock(owner_->lock_);
  // Notify observers in the browser process.
  ui::GpuSwitchingManager::GetInstance()->NotifyGpuSwitched(
      active_gpu_heuristic_);
  // Pass the notification to the GPU process to notify observers there.
  GpuProcessHost::CallOnIO(
      GPU_PROCESS_KIND_SANDBOXED, false /* force_create */,
      base::BindOnce(
          [](gl::GpuPreference active_gpu, GpuProcessHost* host) {
            if (host)
              host->gpu_service()->GpuSwitched(active_gpu);
          },
          active_gpu_heuristic_));
}

void GpuDataManagerImplPrivate::OnDisplayAdded(
    const display::Display& new_display) {
#if defined(OS_WIN)
  if (gpu_info_dx_diag_requested_) {
    // Reset DxDiag flags so the data can be updated again
    gpu_info_dx_diag_requested_ = false;
    gpu_info_.dx_diagnostics = gpu::DxDiagNode();
    // This DxDiag request goes to the unsandboxed GPU info collection GPU
    // process while the notification below goes to the sandboxed GPU process.
    RequestDxDiagNodeData();
  }
#endif

  base::AutoUnlock unlock(owner_->lock_);

  // Notify observers in the browser process.
  ui::GpuSwitchingManager::GetInstance()->NotifyDisplayAdded();
  // Pass the notification to the GPU process to notify observers there.
  GpuProcessHost::CallOnIO(GPU_PROCESS_KIND_SANDBOXED, false /* force_create */,
                           base::BindOnce([](GpuProcessHost* host) {
                             if (host)
                               host->gpu_service()->DisplayAdded();
                           }));
}

void GpuDataManagerImplPrivate::OnDisplayRemoved(
    const display::Display& old_display) {
#if defined(OS_WIN)
  if (gpu_info_dx_diag_requested_) {
    // Reset DxDiag flags so the data can be updated again
    gpu_info_dx_diag_requested_ = false;
    gpu_info_.dx_diagnostics = gpu::DxDiagNode();
    // This DxDiag request goes to the unsandboxed GPU info collection GPU
    // process while the notification below goes to the sandboxed GPU process.
    RequestDxDiagNodeData();
  }
#endif

  base::AutoUnlock unlock(owner_->lock_);

  // Notify observers in the browser process.
  ui::GpuSwitchingManager::GetInstance()->NotifyDisplayRemoved();
  // Pass the notification to the GPU process to notify observers there.
  GpuProcessHost::CallOnIO(GPU_PROCESS_KIND_SANDBOXED, false /* force_create */,
                           base::BindOnce([](GpuProcessHost* host) {
                             if (host)
                               host->gpu_service()->DisplayRemoved();
                           }));
}

void GpuDataManagerImplPrivate::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
#if defined(OS_WIN)
  if (gpu_info_dx_diag_requested_) {
    // Reset DxDiag flags so the data can be updated again
    gpu_info_dx_diag_requested_ = false;
    gpu_info_.dx_diagnostics = gpu::DxDiagNode();
    // This DxDiag request goes to the unsandboxed GPU info collection GPU
    // process while the notification below goes to the sandboxed GPU process.
    RequestDxDiagNodeData();
  }
#endif

  base::AutoUnlock unlock(owner_->lock_);

  // Notify observers in the browser process.
  ui::GpuSwitchingManager::GetInstance()->NotifyDisplayMetricsChanged();
  // Pass the notification to the GPU process to notify observers there.
  GpuProcessHost::CallOnIO(GPU_PROCESS_KIND_SANDBOXED, false /* force_create */,
                           base::BindOnce([](GpuProcessHost* host) {
                             if (host)
                               host->gpu_service()->DisplayMetricsChanged();
                           }));
}

void GpuDataManagerImplPrivate::BlockDomainFrom3DAPIs(const GURL& url,
                                                      gpu::DomainGuilt guilt) {
  BlockDomainFrom3DAPIsAtTime(url, guilt, base::Time::Now());
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
                       base::CompareCase::SENSITIVE))
    return false;
  if (base::StartsWith(gpu_info_.gl_renderer, "ANGLE",
                       base::CompareCase::SENSITIVE) &&
      gpu_info_.gl_renderer.find("SwiftShader Device") != std::string::npos)
    return false;
  if (gpu_info_.gl_renderer == "Disabled")
    return false;
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

void GpuDataManagerImplPrivate::BlockDomainFrom3DAPIsAtTime(
    const GURL& url,
    gpu::DomainGuilt guilt,
    base::Time at_time) {
  if (!domain_blocking_enabled_)
    return;

  std::string domain = GetDomainFromURL(url);

  blocked_domains_[domain] = guilt;
  timestamps_of_gpu_resets_.push_back(at_time);
}

GpuDataManagerImplPrivate::DomainBlockStatus
GpuDataManagerImplPrivate::Are3DAPIsBlockedAtTime(const GURL& url,
                                                  base::Time at_time) const {
  if (!domain_blocking_enabled_)
    return DomainBlockStatus::kNotBlocked;

  // Note: adjusting the policies in this code will almost certainly
  // require adjusting the associated unit tests.
  std::string domain = GetDomainFromURL(url);

  {
    if (blocked_domains_.find(domain) != blocked_domains_.end()) {
      // Err on the side of caution, and assume that if a particular
      // domain shows up in the block map, it's there for a good
      // reason and don't let its presence there automatically expire.
      return DomainBlockStatus::kBlocked;
    }
  }

  // Look at the timestamps of the recent GPU resets to see if there are
  // enough within the threshold which would cause us to blocklist all
  // domains. This doesn't need to be overly precise -- if time goes
  // backward due to a system clock adjustment, that's fine.
  //
  // TODO(kbr): make this pay attention to the TDR thresholds in the
  // Windows registry, but make sure it continues to be testable.
  {
    auto iter = timestamps_of_gpu_resets_.begin();
    int num_resets_within_timeframe = 0;
    while (iter != timestamps_of_gpu_resets_.end()) {
      base::Time time = *iter;
      base::TimeDelta delta_t = at_time - time;

      // If this entry has "expired", just remove it.
      if (delta_t.InMilliseconds() > kBlockAllDomainsMs) {
        iter = timestamps_of_gpu_resets_.erase(iter);
        continue;
      }

      ++num_resets_within_timeframe;
      ++iter;
    }

    if (num_resets_within_timeframe >= kNumResetsWithinDuration) {
      return DomainBlockStatus::kAllDomainsBlocked;
    }
  }

  return DomainBlockStatus::kNotBlocked;
}

int64_t GpuDataManagerImplPrivate::GetBlockAllDomainsDurationInMs() const {
  return kBlockAllDomainsMs;
}

gpu::GpuMode GpuDataManagerImplPrivate::GetGpuMode() const {
  return gpu_mode_;
}

void GpuDataManagerImplPrivate::FallBackToNextGpuMode() {
  if (fallback_modes_.empty()) {
#if defined(OS_ANDROID)
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

}  // namespace content
