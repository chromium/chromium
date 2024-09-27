// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_PRIVATE_H_
#define CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_PRIVATE_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list_threadsafe.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/common/content_export.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/display/display_observer.h"
#include "ui/gl/gpu_preference.h"

namespace base {
class CommandLine;
}

namespace content {

class CONTENT_EXPORT GpuDataManagerImplPrivate {
 public:
  explicit GpuDataManagerImplPrivate(GpuDataManagerImpl* owner);

  GpuDataManagerImplPrivate(const GpuDataManagerImplPrivate&) = delete;
  GpuDataManagerImplPrivate& operator=(const GpuDataManagerImplPrivate&) =
      delete;

  virtual ~GpuDataManagerImplPrivate();

  void StartUmaTimer();
  gpu::GPUInfo GetGPUInfo() const;
  gpu::GPUInfo GetGPUInfoForHardwareGpu() const;
  std::vector<std::string> GetDawnInfoList() const;
  bool GpuAccessAllowed(std::string* reason) const;
  bool GpuAccessAllowedForHardwareGpu(std::string* reason) const;
  void RequestDx12VulkanVideoGpuInfoIfNeeded(
      GpuDataManagerImpl::GpuInfoRequest request,
      bool delayed);
  bool IsEssentialGpuInfoAvailable() const;
  bool IsDx12VulkanVersionAvailable() const;
  bool IsGpuFeatureInfoAvailable() const;
  gpu::GpuFeatureStatus GetFeatureStatus(gpu::GpuFeatureType feature) const;
  void RequestVideoMemoryUsageStatsUpdate(
      GpuDataManager::VideoMemoryUsageStatsCallback callback) const;
  void AddObserver(GpuDataManagerObserver* observer);
  void RemoveObserver(GpuDataManagerObserver* observer);
  void UnblockDomainFrom3DAPIs(const GURL& url);
  void DisableHardwareAcceleration();
  bool HardwareAccelerationEnabled() const;

  void UpdateGpuInfo(
      const gpu::GPUInfo& gpu_info,
      const std::optional<gpu::GPUInfo>& optional_gpu_info_for_hardware_gpu);
#if BUILDFLAG(IS_WIN)
  void UpdateDirectXInfo(uint32_t d3d12_feature_level,
                         uint32_t directml_feature_level);
  void UpdateVulkanInfo(uint32_t vulkan_version);
  void UpdateDevicePerfInfo(const gpu::DevicePerfInfo& device_perf_info);

  void UpdateOverlayInfo(const gpu::OverlayInfo& overlay_info);
  void UpdateDXGIInfo(gfx::mojom::DXGIInfoPtr dxgi_info);
  void UpdateDirectXRequestStatus(bool request_continues);
  void UpdateVulkanRequestStatus(bool request_continues);
  bool DirectXRequested() const;
  bool VulkanRequested() const;
  void TerminateInfoCollectionGpuProcess();
#endif
  void PostCreateThreads();
  void UpdateDawnInfo(const std::vector<std::string>& dawn_info_list);

  void UpdateGpuFeatureInfo(const gpu::GpuFeatureInfo& gpu_feature_info,
                            const std::optional<gpu::GpuFeatureInfo>&
                                gpu_feature_info_for_hardware_gpu);
  void UpdateGpuExtraInfo(const gfx::GpuExtraInfo& process_info);
  void UpdateMojoMediaVideoDecoderCapabilities(
      const media::SupportedVideoDecoderConfigs& configs);
  void UpdateMojoMediaVideoEncoderCapabilities(
      const media::VideoEncodeAccelerator::SupportedProfiles&
          supported_profiles);

  gpu::GpuFeatureInfo GetGpuFeatureInfo() const;
  gpu::GpuFeatureInfo GetGpuFeatureInfoForHardwareGpu() const;
  gfx::GpuExtraInfo GetGpuExtraInfo() const;

  bool IsGpuCompositingDisabled() const;
  bool IsGpuCompositingDisabledForHardwareGpu() const;

  void SetGpuCompositingDisabled();

  void AppendGpuCommandLine(base::CommandLine* command_line,
                            GpuProcessKind kind) const;

  void UpdateGpuPreferences(gpu::GpuPreferences* gpu_preferences,
                            GpuProcessKind kind) const;

  void AddLogMessage(int level,
                     const std::string& header,
                     const std::string& message);

  void ProcessCrashed();

  base::Value::List GetLogMessages() const;

  void HandleGpuSwitch();

  void BlockDomainsFrom3DAPIs(const std::set<GURL>& urls,
                              gpu::DomainGuilt guilt);
  bool Are3DAPIsBlocked(const GURL& top_origin_url,
                        ThreeDAPIType requester);

  void Notify3DAPIBlocked(const GURL& top_origin_url,
                          int render_process_id,
                          int render_frame_id,
                          ThreeDAPIType requester);

  gpu::GpuMode GetGpuMode() const;
  void FallBackToNextGpuMode();

  bool CanFallback() const { return !fallback_modes_.empty(); }

  bool IsGpuProcessUsingHardwareGpu() const;

  void SetApplicationVisible(bool is_visible);

  void OnDisplayAdded(const display::Display& new_display);
  void OnDisplaysRemoved(const display::Displays& removed_displays);
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics);

#if BUILDFLAG(IS_LINUX)
  bool IsGpuMemoryBufferNV12Supported();
  void SetGpuMemoryBufferNV12Supported(bool supported);
#endif  // BUILDFLAG(IS_LINUX)

  void DisableDomainBlockingFor3DAPIsForTesting();
  void BlocklistWebGLForTesting();
  void SetSkiaGraphiteEnabledForTesting(bool enabled);

 private:
  friend class GpuDataManagerImplPrivateTest;
  friend class GpuDataManagerImplPrivateTestP;

  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           GpuInfoUpdate);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTestP,
                           SingleContextLossDoesNotBlockDomain);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTestP,
                           TwoContextLossesBlockDomain);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTestP,
                           TwoSimultaneousContextLossesDoNotBlockDomain);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTestP, DomainBlockExpires);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTestP, UnblockDomain);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTestP,
                           Domain1DoesNotBlockDomain2);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTestP,
                           UnblockingDomain1DoesNotUnblockDomain2);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTestP,
                           SimultaneousContextLossDoesNotBlock);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTestP,
                           MultipleTDRsBlockAll);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTestP, MultipleTDRsExpire);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTestP,
                           MultipleTDRsCanBeUnblocked);

  // Indicates the reason that access to a given client API (like
  // WebGL or Pepper 3D) was blocked or not. This state is distinct
  // from blocklisting of an entire feature.
  enum class DomainBlockStatus {
    kBlocked,
    kAllDomainsBlocked,
    kNotBlocked,
  };

  using GpuDataManagerObserverList =
      base::ObserverListThreadSafe<GpuDataManagerObserver>;

  struct LogMessage {
    int level;
    std::string header;
    std::string message;

    LogMessage(int _level,
               const std::string& _header,
               const std::string& _message)
        : level(_level),
          header(_header),
          message(_message) { }
  };

  // GPUInfo related data that should stay the same value even after GPUInfo is
  // updated. After GPU process restart different GPUInfo can be sent back to
  // the browser so the values here will be used reset the fixed data.
  struct FixedGpuInfo {
    std::optional<bool> hardware_supports_vulkan;
  };

  // Decide the order of GPU process states, and go to the first one. This
  // should only be called once, during initialization.
  void InitializeGpuModes();

  // Called when GPU access (hardware acceleration and swiftshader) becomes
  // blocked.
  void OnGpuBlocked();

  // Helper to extract the domain from a given URL.
  std::string GetDomainFromURL(const GURL& url) const;

  // Implementation functions for blocking of 3D graphics APIs, used
  // for unit testing.
  void BlockDomainsFrom3DAPIsAtTime(const std::set<GURL>& url,
                                    gpu::DomainGuilt guilt,
                                    base::Time at_time);
  void ExpireOldBlockedDomainsAtTime(base::Time at_time) const;
  DomainBlockStatus Are3DAPIsBlockedAtTime(const GURL& url,
                                           base::Time at_time) const;
  base::TimeDelta GetDomainBlockingExpirationPeriod() const;

  // Notify all observers whenever there is a GPU info update.
  void NotifyGpuInfoUpdate();

  void RequestGpuSupportedDirectXVersion(bool delayed);
  void RequestGpuSupportedVulkanVersion(bool delayed);
  void RequestDawnInfo(bool delayed, bool collect_metrics);
  void RequestMojoMediaVideoCapabilities();

  void RecordCompositingMode();

  const raw_ptr<GpuDataManagerImpl> owner_;

  gpu::GpuFeatureInfo gpu_feature_info_;
  FixedGpuInfo fixed_gpu_info_;
  gpu::GPUInfo gpu_info_;
  gl::GpuPreference active_gpu_heuristic_ = gl::GpuPreference::kDefault;
#if BUILDFLAG(IS_WIN)
  bool gpu_info_dx_valid_ = false;
  bool gpu_info_dx_requested_ = false;
  bool gpu_info_dx_request_failed_ = false;
  bool gpu_info_vulkan_valid_ = false;
  bool gpu_info_vulkan_requested_ = false;
  bool gpu_info_vulkan_request_failed_ = false;
#endif
  // The Dawn info queried from the GPU process.
  std::vector<std::string> dawn_info_list_;

  // What we would have gotten if we haven't fallen back to SwiftShader or
  // pure software (in the viz case).
  gpu::GpuFeatureInfo gpu_feature_info_for_hardware_gpu_;
  gpu::GPUInfo gpu_info_for_hardware_gpu_;
  bool is_gpu_compositing_disabled_for_hardware_gpu_ = false;
  bool gpu_access_allowed_for_hardware_gpu_ = true;
  std::string gpu_access_blocked_reason_for_hardware_gpu_;

  gfx::GpuExtraInfo gpu_extra_info_;

  const scoped_refptr<GpuDataManagerObserverList> observer_list_;

  // Periodically calls RecordCompositingMode() for compositing mode UMA.
  base::RepeatingTimer compositing_mode_timer_;

  // Contains the 1000 most recent log messages.
  std::vector<LogMessage> log_messages_;

  // What the gpu process is being run for.
  gpu::GpuMode gpu_mode_ = gpu::GpuMode::UNKNOWN;

  // Order of gpu process fallback states, used as a stack.
  std::vector<gpu::GpuMode> fallback_modes_;

  std::optional<display::ScopedOptionalDisplayObserver> display_observer_;

  // Used to tell if the gpu was disabled by an explicit call to
  // DisableHardwareAcceleration(), rather than by fallback.
  bool hardware_disabled_explicitly_ = false;

  // We disable histogram stuff in testing, especially in unit tests because
  // they cause random failures.
  bool update_histograms_ = true;

  struct DomainBlockingEntry {
    DomainBlockingEntry(const std::string& domain, gpu::DomainGuilt guilt)
        : domain(domain), guilt(guilt) {}

    std::string domain;
    gpu::DomainGuilt guilt;
  };

  // Implicitly sorted by increasing timestamp.
  mutable std::multimap<base::Time, DomainBlockingEntry> blocked_domains_;
  bool domain_blocking_enabled_ = true;

  bool application_is_visible_ = true;

  bool disable_gpu_compositing_ = false;
#if BUILDFLAG(IS_LINUX)
  bool is_gpu_memory_buffer_NV12_supported_ = false;
#endif  // BUILDFLAG(IS_LINUX)
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_PRIVATE_H_
