// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_data_manager_impl_private.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "base/version.h"
#include "build/build_config.h"
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
#include "gpu/config/gpu_blacklist.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/gpu_util.h"
#include "gpu/config/software_rendering_list_autogen.h"
#include "gpu/ipc/common/memory_stats.h"
#include "gpu/ipc/host/gpu_memory_buffer_support.h"
#include "gpu/ipc/host/shader_disk_cache.h"
#include "media/media_buildflags.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_switches.h"
#endif
#if defined(OS_MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#endif  // OS_MACOSX
#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif  // OS_WIN

namespace content {

namespace {

#if defined(OS_WIN)
int GetGpuBlacklistHistogramValueWin(gpu::GpuFeatureStatus status) {
  // The enums are defined as:
  //   Enabled VERSION_PRE_XP = 0,
  //   Blacklisted VERSION_PRE_XP = 1,
  //   Disabled VERSION_PRE_XP = 2,
  //   Software VERSION_PRE_XP = 3,
  //   Unknown VERSION_PRE_XP = 4,
  //   Enabled VERSION_XP = 5,
  //   ...
  static const base::win::Version version = base::win::GetVersion();
  if (version == base::win::VERSION_WIN_LAST)
    return -1;
  DCHECK_NE(gpu::kGpuFeatureStatusMax, status);
  int entry_index = static_cast<int>(version) * gpu::kGpuFeatureStatusMax;
  return entry_index + static_cast<int>(status);
}
#endif  // OS_WIN

// Send UMA histograms about the enabled features and GPU properties.
void UpdateFeatureStats(const gpu::GpuFeatureInfo& gpu_feature_info) {
  // Update applied entry stats.
  std::unique_ptr<gpu::GpuBlacklist> blacklist(gpu::GpuBlacklist::Create());
  DCHECK(blacklist.get() && blacklist->max_entry_id() > 0);
  uint32_t max_entry_id = blacklist->max_entry_id();
  // Use entry 0 to capture the total number of times that data
  // was recorded in this histogram in order to have a convenient
  // denominator to compute blacklist percentages for the rest of the
  // entries.
  UMA_HISTOGRAM_EXACT_LINEAR("GPU.BlacklistTestResultsPerEntry", 0,
                             max_entry_id + 1);
  if (!gpu_feature_info.applied_gpu_blacklist_entries.empty()) {
    std::vector<uint32_t> entry_ids = blacklist->GetEntryIDsFromIndices(
        gpu_feature_info.applied_gpu_blacklist_entries);
    DCHECK_EQ(gpu_feature_info.applied_gpu_blacklist_entries.size(),
              entry_ids.size());
    for (auto id : entry_ids) {
      DCHECK_GE(max_entry_id, id);
      UMA_HISTOGRAM_EXACT_LINEAR("GPU.BlacklistTestResultsPerEntry", id,
                                 max_entry_id + 1);
    }
  }

  // Update feature status stats.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  const gpu::GpuFeatureType kGpuFeatures[] = {
      gpu::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
      gpu::GPU_FEATURE_TYPE_GPU_COMPOSITING,
      gpu::GPU_FEATURE_TYPE_GPU_RASTERIZATION,
      gpu::GPU_FEATURE_TYPE_OOP_RASTERIZATION,
      gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
      gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL2};
  const std::string kGpuBlacklistFeatureHistogramNames[] = {
      "GPU.BlacklistFeatureTestResults.Accelerated2dCanvas",
      "GPU.BlacklistFeatureTestResults.GpuCompositing",
      "GPU.BlacklistFeatureTestResults.GpuRasterization",
      "GPU.BlacklistFeatureTestResults.OopRasterization",
      "GPU.BlacklistFeatureTestResults.Webgl",
      "GPU.BlacklistFeatureTestResults.Webgl2"};
  const bool kGpuFeatureUserFlags[] = {
      command_line.HasSwitch(switches::kDisableAccelerated2dCanvas),
      command_line.HasSwitch(switches::kDisableGpu),
      command_line.HasSwitch(switches::kDisableGpuRasterization),
      command_line.HasSwitch(switches::kDisableOopRasterization),
      command_line.HasSwitch(switches::kDisableWebGL),
      (command_line.HasSwitch(switches::kDisableWebGL) ||
       command_line.HasSwitch(switches::kDisableWebGL2))};
#if defined(OS_WIN)
  const std::string kGpuBlacklistFeatureHistogramNamesWin[] = {
      "GPU.BlacklistFeatureTestResultsWindows2.Accelerated2dCanvas",
      "GPU.BlacklistFeatureTestResultsWindows2.GpuCompositing",
      "GPU.BlacklistFeatureTestResultsWindows2.GpuRasterization",
      "GPU.BlacklistFeatureTestResultsWindows2.OopRasterization",
      "GPU.BlacklistFeatureTestResultsWindows2.Webgl",
      "GPU.BlacklistFeatureTestResultsWindows2.Webgl2"};
#endif
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
        kGpuBlacklistFeatureHistogramNames[i], 1, gpu::kGpuFeatureStatusMax,
        gpu::kGpuFeatureStatusMax + 1,
        base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram_pointer->Add(value);
#if defined(OS_WIN)
    int value_win = GetGpuBlacklistHistogramValueWin(value);
    if (value_win >= 0) {
      int32_t max_sample = static_cast<int32_t>(base::win::VERSION_WIN_LAST) *
                           gpu::kGpuFeatureStatusMax;
      histogram_pointer = base::LinearHistogram::FactoryGet(
          kGpuBlacklistFeatureHistogramNamesWin[i], 1, max_sample,
          max_sample + 1, base::HistogramBase::kUmaTargetedHistogramFlag);
      histogram_pointer->Add(value_win);
    }
#endif
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

#if defined(OS_MACOSX)
void DisplayReconfigCallback(CGDirectDisplayID display,
                             CGDisplayChangeSummaryFlags flags,
                             void* gpu_data_manager) {
  if (flags == kCGDisplayBeginConfigurationFlag)
    return;  // This call contains no information about the display change

  GpuDataManagerImpl* manager =
      reinterpret_cast<GpuDataManagerImpl*>(gpu_data_manager);
  DCHECK(manager);

  bool gpu_changed = false;
  if (flags & kCGDisplayAddFlag) {
    gpu::GPUInfo gpu_info;
    if (gpu::CollectBasicGraphicsInfo(&gpu_info)) {
      gpu_changed = manager->UpdateActiveGpu(gpu_info.active_gpu().vendor_id,
                                             gpu_info.active_gpu().device_id);
    }
  }

  if (gpu_changed)
    manager->HandleGpuSwitch();
}
#endif  // OS_MACOSX

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
    const base::Callback<void(const gpu::VideoMemoryUsageStats& stats)>&
        callback,
    const gpu::VideoMemoryUsageStats& stats) {
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(callback, stats));
}

void RequestVideoMemoryUsageStats(
    const base::Callback<void(const gpu::VideoMemoryUsageStats& stats)>&
        callback,
    GpuProcessHost* host) {
  if (!host)
    return;
  host->gpu_service()->GetVideoMemoryUsageStats(
      base::BindOnce(&OnVideoMemoryUsageStats, callback));
}

#if defined(OS_WIN)
void UpdateDxDiagNodeOnIO(const gpu::DxDiagNode& dx_diagnostics) {
  // This function is called on the IO thread, but GPUInfo on GpuDataManagerImpl
  // should be updated on the UI thread since it can call into functions that
  // expect to run in the UI thread.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          [](const gpu::DxDiagNode& dx_diagnostics) {
            GpuDataManagerImpl::GetInstance()->UpdateDxDiagNode(dx_diagnostics);
          },
          dx_diagnostics));
}

void UpdateDx12VulkanInfoOnIO(
    const gpu::Dx12VulkanVersionInfo& dx12_vulkan_version_info) {
  // This function is called on the IO thread, but GPUInfo on GpuDataManagerImpl
  // should be updated on the UI thread since it can call into functions that
  // expect to run in the UI thread.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          [](const gpu::Dx12VulkanVersionInfo& dx12_vulkan_version_info) {
            GpuDataManagerImpl::GetInstance()->UpdateDx12VulkanInfo(
                dx12_vulkan_version_info);
          },
          dx12_vulkan_version_info));
}
#endif
}  // anonymous namespace

GpuDataManagerImplPrivate::GpuDataManagerImplPrivate(GpuDataManagerImpl* owner)
    : owner_(owner),
      observer_list_(base::MakeRefCounted<GpuDataManagerObserverList>()) {
  DCHECK(owner_);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableGpu))
    DisableHardwareAcceleration();

  if (command_line->HasSwitch(switches::kSingleProcess) ||
      command_line->HasSwitch(switches::kInProcessGPU)) {
    in_process_gpu_ = true;
    AppendGpuCommandLine(command_line);
  }

#if defined(OS_MACOSX)
  CGDisplayRegisterReconfigurationCallback(DisplayReconfigCallback, owner_);
#endif  // OS_MACOSX

  // For testing only.
  if (command_line->HasSwitch(switches::kDisableDomainBlockingFor3DAPIs))
    domain_blocking_enabled_ = false;
}

GpuDataManagerImplPrivate::~GpuDataManagerImplPrivate() {
#if defined(OS_MACOSX)
  CGDisplayRemoveReconfigurationCallback(DisplayReconfigCallback, owner_);
#endif
}

void GpuDataManagerImplPrivate::BlacklistWebGLForTesting() {
  // This function is for testing only, so disable histograms.
  update_histograms_ = false;

  gpu::GpuFeatureInfo gpu_feature_info;
  for (int ii = 0; ii < gpu::NUMBER_OF_GPU_FEATURE_TYPES; ++ii) {
    if (ii == static_cast<int>(gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL))
      gpu_feature_info.status_values[ii] = gpu::kGpuFeatureStatusBlacklisted;
    else
      gpu_feature_info.status_values[ii] = gpu::kGpuFeatureStatusEnabled;
  }
  UpdateGpuFeatureInfo(gpu_feature_info, base::nullopt);
  NotifyGpuInfoUpdate();
}

gpu::GPUInfo GpuDataManagerImplPrivate::GetGPUInfo() const {
  return gpu_info_;
}

gpu::GPUInfo GpuDataManagerImplPrivate::GetGPUInfoForHardwareGpu() const {
  return gpu_info_for_hardware_gpu_;
}

bool GpuDataManagerImplPrivate::GpuAccessAllowed(std::string* reason) const {
  bool swiftshader_available = false;
#if BUILDFLAG(ENABLE_SWIFTSHADER)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableSoftwareRasterizer)) {
    swiftshader_available = true;
  }
#endif
  if (swiftshader_blocked_) {
    if (reason) {
      *reason = "GPU process crashed too many times with SwiftShader.";
    }
    return false;
  }
  if (swiftshader_available)
    return true;

  if (card_disabled_) {
    if (reason) {
      *reason = "GPU access is disabled ";
      if (base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kDisableGpu))
        *reason += "through commandline switch --disable-gpu.";
      else
        *reason += "in chrome://settings.";
    }
    return false;
  }
  return true;
}

bool GpuDataManagerImplPrivate::GpuProcessStartAllowed() const {
  if (GpuAccessAllowed(nullptr))
    return true;

#if defined(USE_X11) || defined(OS_MACOSX)
  // If GPU access is disabled with OOP-D we run the display compositor in:
  //   Browser process: Windows
  //   GPU process: Linux and Mac
  //   N/A: Android and Chrome OS (GPU access can't be disabled)
  if (base::FeatureList::IsEnabled(features::kVizDisplayCompositor))
    return true;
#endif

  return false;
}

void GpuDataManagerImplPrivate::RequestCompleteGpuInfoIfNeeded() {
  if (complete_gpu_info_already_requested_)
    return;
  if (!NeedsCompleteGpuInfoCollection())
    return;

#if defined(OS_WIN)
  if (!GpuAccessAllowed(nullptr))
    return;
  if (in_process_gpu_)
    return;
  complete_gpu_info_already_requested_ = true;
  GpuProcessHost::CallOnIO(GpuProcessHost::GPU_PROCESS_KIND_UNSANDBOXED_NO_GL,
                           true /* force_create */,
                           base::Bind([](GpuProcessHost* host) {
                             if (!host)
                               return;
                             host->gpu_service()->RequestCompleteGpuInfo(
                                 base::BindOnce(&UpdateDxDiagNodeOnIO));
                           }));
#else
  // NeedsCompleteGpuInfoCollection() always returns false on platforms other
  // than Windows.
  NOTREACHED();
#endif
}

void GpuDataManagerImplPrivate::RequestGpuSupportedRuntimeVersion() {
#if defined(OS_WIN)
  if (in_process_gpu_)
    return;
  base::OnceClosure task = base::BindOnce([]() {
    GpuProcessHost* host =
        GpuProcessHost::Get(GpuProcessHost::GPU_PROCESS_KIND_UNSANDBOXED_NO_GL,
                            true /* force_create */);
    if (!host)
      return;
    host->gpu_service()->GetGpuSupportedRuntimeVersion(
        base::BindOnce(&UpdateDx12VulkanInfoOnIO));
  });

  base::PostDelayedTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                                  std::move(task),
                                  base::TimeDelta::FromMilliseconds(15000));
#else
  NOTREACHED();
#endif
}

bool GpuDataManagerImplPrivate::IsEssentialGpuInfoAvailable() const {
  // We always update GPUInfo and GpuFeatureInfo from GPU process together.
  return IsGpuFeatureInfoAvailable();
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
    const base::Callback<void(const gpu::VideoMemoryUsageStats& stats)>&
        callback) const {
  GpuProcessHost::CallOnIO(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                           false /* force_create */,
                           base::Bind(&RequestVideoMemoryUsageStats, callback));
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
    const base::Optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu) {
#if defined(OS_WIN)
  // If GPU process crashes and launches again, GPUInfo will be sent back from
  // the new GPU process again, and may overwrite the DX12, Vulkan, DxDiagNode
  // info we already collected. This is to make sure it doesn't happen.
  gpu::DxDiagNode dx_diagnostics = gpu_info_.dx_diagnostics;
  gpu::Dx12VulkanVersionInfo dx12_vulkan_version_info =
      gpu_info_.dx12_vulkan_version_info;
#endif
  gpu_info_ = gpu_info;
#if defined(OS_WIN)
  if (!dx_diagnostics.IsEmpty()) {
    gpu_info_.dx_diagnostics = dx_diagnostics;
  }
  if (!dx12_vulkan_version_info.IsEmpty()) {
    gpu_info_.dx12_vulkan_version_info = dx12_vulkan_version_info;
  }
#endif  // OS_WIN

  if (!gpu_info_for_hardware_gpu_.IsInitialized()) {
    if (gpu_info_for_hardware_gpu) {
      DCHECK(gpu_info_for_hardware_gpu->IsInitialized());
      gpu_info_for_hardware_gpu_ = gpu_info_for_hardware_gpu.value();
    } else {
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
  if (complete_gpu_info_already_requested_)
    complete_gpu_info_already_requested_ = false;
  // No need to call GetContentClient()->SetGpuInfo().
  NotifyGpuInfoUpdate();
}

void GpuDataManagerImplPrivate::UpdateDx12VulkanInfo(
    const gpu::Dx12VulkanVersionInfo& dx12_vulkan_version_info) {
  gpu_info_.dx12_vulkan_version_info = dx12_vulkan_version_info;
  // No need to call GetContentClient()->SetGpuInfo().
  NotifyGpuInfoUpdate();
}
#endif

void GpuDataManagerImplPrivate::UpdateGpuFeatureInfo(
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const base::Optional<gpu::GpuFeatureInfo>&
        gpu_feature_info_for_hardware_gpu) {
  gpu_feature_info_ = gpu_feature_info;
  if (!gpu_feature_info_for_hardware_gpu_.IsInitialized()) {
    if (gpu_feature_info_for_hardware_gpu.has_value()) {
      DCHECK(gpu_feature_info_for_hardware_gpu->IsInitialized());
      gpu_feature_info_for_hardware_gpu_ =
          gpu_feature_info_for_hardware_gpu.value();
    } else {
      gpu_feature_info_for_hardware_gpu_ = gpu_feature_info;
    }
  }
  if (update_histograms_) {
    UpdateFeatureStats(gpu_feature_info);
    UpdateDriverBugListStats(gpu_feature_info);
  }
}

gpu::GpuFeatureInfo GpuDataManagerImplPrivate::GetGpuFeatureInfo() const {
  return gpu_feature_info_;
}

gpu::GpuFeatureInfo GpuDataManagerImplPrivate::GetGpuFeatureInfoForHardwareGpu()
    const {
  return gpu_feature_info_for_hardware_gpu_;
}

void GpuDataManagerImplPrivate::AppendGpuCommandLine(
    base::CommandLine* command_line) const {
  DCHECK(command_line);
  const base::CommandLine* browser_command_line =
      base::CommandLine::ForCurrentProcess();

  gpu::GpuPreferences gpu_prefs = GetGpuPreferencesFromCommandLine();
  UpdateGpuPreferences(&gpu_prefs);
  command_line->AppendSwitchASCII(switches::kGpuPreferences,
                                  gpu_prefs.ToSwitchValue());

  std::string use_gl;
  if (card_disabled_ && SwiftShaderAllowed()) {
    use_gl = gl::kGLImplementationSwiftShaderForWebGLName;
  } else if (card_disabled_) {
    use_gl = gl::kGLImplementationDisabledName;
  } else {
    use_gl = browser_command_line->GetSwitchValueASCII(switches::kUseGL);
  }
  if (!use_gl.empty()) {
    command_line->AppendSwitchASCII(switches::kUseGL, use_gl);
  }

#if !defined(OS_MACOSX)
  // MacOSX bots use real GPU in tests.
  if (browser_command_line->HasSwitch(
          switches::kOverrideUseSoftwareGLForTests) ||
      browser_command_line->HasSwitch(switches::kHeadless)) {
    // TODO(zmo): We should also pass in kUseGL here.
    // See https://crbug.com/805204.
    command_line->AppendSwitch(switches::kOverrideUseSoftwareGLForTests);
  }
#endif  // !OS_MACOSX
}

void GpuDataManagerImplPrivate::UpdateGpuPreferences(
    gpu::GpuPreferences* gpu_preferences) const {
  DCHECK(gpu_preferences);

  // For performance reasons, discourage storing VideoFrames in a biplanar
  // GpuMemoryBuffer if this is not native, see https://crbug.com/791676.
  if (auto* gpu_memory_buffer_manager =
          GpuMemoryBufferManagerSingleton::GetInstance()) {
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
}

void GpuDataManagerImplPrivate::DisableHardwareAcceleration() {
  card_disabled_ = true;
  if (!SwiftShaderAllowed())
    OnGpuBlocked();
}

bool GpuDataManagerImplPrivate::HardwareAccelerationEnabled() const {
  return !card_disabled_;
}

bool GpuDataManagerImplPrivate::SwiftShaderAllowed() const {
#if !BUILDFLAG(ENABLE_SWIFTSHADER)
  return false;
#else
  return !swiftshader_blocked_ &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableSoftwareRasterizer);
#endif
}

void GpuDataManagerImplPrivate::OnGpuBlocked() {
  base::Optional<gpu::GpuFeatureInfo> gpu_feature_info_for_hardware_gpu;
  if (gpu_feature_info_.IsInitialized())
    gpu_feature_info_for_hardware_gpu = gpu_feature_info_;
  gpu::GpuFeatureInfo gpu_feature_info = gpu::ComputeGpuFeatureInfoWithNoGpu();
  UpdateGpuFeatureInfo(gpu_feature_info, gpu_feature_info_for_hardware_gpu);

  // Some observers might be waiting.
  NotifyGpuInfoUpdate();
}

void GpuDataManagerImplPrivate::AddLogMessage(
    int level, const std::string& header, const std::string& message) {
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
  ui::GpuSwitchingManager::GetInstance()->NotifyGpuSwitched();
  // Pass the notification to the GPU process to notify observers there.
  GpuProcessHost::CallOnIO(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                           false /* force_create */,
                           base::Bind([](GpuProcessHost* host) {
                             if (host)
                               host->gpu_service()->GpuSwitched();
                           }));
}

bool GpuDataManagerImplPrivate::UpdateActiveGpu(uint32_t vendor_id,
                                                uint32_t device_id) {
  if (gpu_info_.gpu.vendor_id == vendor_id &&
      gpu_info_.gpu.device_id == device_id) {
    // The primary GPU is active.
    if (gpu_info_.gpu.active)
      return false;
    gpu_info_.gpu.active = true;
    for (size_t ii = 0; ii < gpu_info_.secondary_gpus.size(); ++ii)
      gpu_info_.secondary_gpus[ii].active = false;
  } else {
    // A secondary GPU is active.
    for (size_t ii = 0; ii < gpu_info_.secondary_gpus.size(); ++ii) {
      if (gpu_info_.secondary_gpus[ii].vendor_id == vendor_id &&
          gpu_info_.secondary_gpus[ii].device_id == device_id) {
        if (gpu_info_.secondary_gpus[ii].active)
          return false;
        gpu_info_.secondary_gpus[ii].active = true;
      } else {
        gpu_info_.secondary_gpus[ii].active = false;
      }
    }
    gpu_info_.gpu.active = false;
  }
  GetContentClient()->SetGpuInfo(gpu_info_);
  NotifyGpuInfoUpdate();
  return true;
}

void GpuDataManagerImplPrivate::BlockDomainFrom3DAPIs(const GURL& url,
                                                      gpu::DomainGuilt guilt) {
  BlockDomainFrom3DAPIsAtTime(url, guilt, base::Time::Now());
}

bool GpuDataManagerImplPrivate::Are3DAPIsBlocked(const GURL& top_origin_url,
                                                 int render_process_id,
                                                 int render_frame_id,
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
  // enough within the threshold which would cause us to blacklist all
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
      UMA_HISTOGRAM_ENUMERATION("GPU.BlockStatusForClient3DAPIs",
                                BLOCK_STATUS_ALL_DOMAINS_BLOCKED,
                                BLOCK_STATUS_MAX);

      return DomainBlockStatus::kAllDomainsBlocked;
    }
  }

  UMA_HISTOGRAM_ENUMERATION("GPU.BlockStatusForClient3DAPIs",
                            BLOCK_STATUS_NOT_BLOCKED,
                            BLOCK_STATUS_MAX);

  return DomainBlockStatus::kNotBlocked;
}

int64_t GpuDataManagerImplPrivate::GetBlockAllDomainsDurationInMs() const {
  return kBlockAllDomainsMs;
}

bool GpuDataManagerImplPrivate::NeedsCompleteGpuInfoCollection() const {
#if defined(OS_WIN)
  return gpu_info_.dx_diagnostics.IsEmpty();
#else
  return false;
#endif
}

gpu::GpuMode GpuDataManagerImplPrivate::GetGpuMode() const {
  if (HardwareAccelerationEnabled()) {
    return gpu::GpuMode::HARDWARE_ACCELERATED;
  } else if (SwiftShaderAllowed()) {
    return gpu::GpuMode::SWIFTSHADER;
  } else if (base::FeatureList::IsEnabled(features::kVizDisplayCompositor)) {
    return gpu::GpuMode::DISPLAY_COMPOSITOR;
  } else {
    return gpu::GpuMode::DISABLED;
  }
}

void GpuDataManagerImplPrivate::FallBackToNextGpuMode() {
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  // Android and Chrome OS can't switch to software compositing. If the GPU
  // process initialization fails or GPU process is too unstable then crash the
  // browser process to reset everything.
  LOG(FATAL) << "GPU process isn't usable. Goodbye.";
#else
  // TODO(kylechar): Use GpuMode to store the current mode instead of
  // multiple bools.

  if (!card_disabled_) {
    DisableHardwareAcceleration();
  } else if (SwiftShaderAllowed()) {
    swiftshader_blocked_ = true;
    OnGpuBlocked();
  } else if (base::FeatureList::IsEnabled(features::kVizDisplayCompositor)) {
    // The GPU process is frequently crashing with only the display compositor
    // running. This should never happen so something is wrong. Crash the
    // browser process to reset everything.
    LOG(FATAL) << "The display compositor is frequently crashing. Goodbye.";
  } else {
    // We are already at GpuMode::DISABLED. We shouldn't be launching the GPU
    // process for it to fail.
    NOTREACHED();
  }
#endif
}

}  // namespace content
