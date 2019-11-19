// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_PRIVATE_H_
#define CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_PRIVATE_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/observer_list_threadsafe.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "ui/gl/gpu_preference.h"

namespace base {
class CommandLine;
}

namespace gpu {
struct GpuPreferences;
}

namespace content {

class CONTENT_EXPORT GpuDataManagerImplPrivate {
 public:
  explicit GpuDataManagerImplPrivate(GpuDataManagerImpl* owner);
  virtual ~GpuDataManagerImplPrivate();

  void BlacklistWebGLForTesting();
  gpu::GPUInfo GetGPUInfo() const;
  gpu::GPUInfo GetGPUInfoForHardwareGpu() const;
  bool GpuAccessAllowed(std::string* reason) const;
  bool GpuProcessStartAllowed() const;
  void RequestDxdiagDx12VulkanGpuInfoIfNeeded(GpuInfoRequest request,
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
      const base::Optional<gpu::GPUInfo>& optional_gpu_info_for_hardware_gpu);
#if defined(OS_WIN)
  void UpdateDxDiagNode(const gpu::DxDiagNode& dx_diagnostics);
  void UpdateDx12VulkanInfo(
      const gpu::Dx12VulkanVersionInfo& dx12_vulkan_version_info);
  void UpdateDx12VulkanRequestStatus(bool request_continues);
  void UpdateDxDiagNodeRequestStatus(bool request_continues);
  bool Dx12VulkanRequested() const;
#endif
  void UpdateGpuFeatureInfo(const gpu::GpuFeatureInfo& gpu_feature_info,
                            const base::Optional<gpu::GpuFeatureInfo>&
                                gpu_feature_info_for_hardware_gpu);
  void UpdateGpuExtraInfo(const gpu::GpuExtraInfo& process_info);

  gpu::GpuFeatureInfo GetGpuFeatureInfo() const;
  gpu::GpuFeatureInfo GetGpuFeatureInfoForHardwareGpu() const;
  gpu::GpuExtraInfo GetGpuExtraInfo() const;

  bool IsGpuCompositingDisabled() const;

  void SetGpuCompositingDisabled();

  void AppendGpuCommandLine(base::CommandLine* command_line,
                            GpuProcessKind kind) const;

  void UpdateGpuPreferences(gpu::GpuPreferences* gpu_preferences,
                            GpuProcessKind kind) const;

  void AddLogMessage(int level,
                     const std::string& header,
                     const std::string& message);

  void ProcessCrashed(base::TerminationStatus exit_code);

  std::unique_ptr<base::ListValue> GetLogMessages() const;

  void HandleGpuSwitch();

  void BlockDomainFrom3DAPIs(const GURL& url, gpu::DomainGuilt guilt);
  bool Are3DAPIsBlocked(const GURL& top_origin_url,
                        int render_process_id,
                        int render_frame_id,
                        ThreeDAPIType requester);

  void DisableDomainBlockingFor3DAPIsForTesting();

  void Notify3DAPIBlocked(const GURL& top_origin_url,
                          int render_process_id,
                          int render_frame_id,
                          ThreeDAPIType requester);

  bool UpdateActiveGpu(uint32_t vendor_id, uint32_t device_id);

  gpu::GpuMode GetGpuMode() const;
  void FallBackToNextGpuMode();

  bool IsGpuProcessUsingHardwareGpu() const;

  void SetApplicationVisible(bool is_visible);

 private:
  friend class GpuDataManagerImplPrivateTest;

  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           GpuInfoUpdate);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           BlockAllDomainsFrom3DAPIs);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           UnblockGuiltyDomainFrom3DAPIs);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           UnblockDomainOfUnknownGuiltFrom3DAPIs);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           UnblockOtherDomainFrom3DAPIs);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           UnblockThisDomainFrom3DAPIs);

  // Indicates the reason that access to a given client API (like
  // WebGL or Pepper 3D) was blocked or not. This state is distinct
  // from blacklisting of an entire feature.
  enum class DomainBlockStatus {
    kBlocked,
    kAllDomainsBlocked,
    kNotBlocked,
  };

  using DomainGuiltMap = std::map<std::string, gpu::DomainGuilt>;

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

  // Called when GPU access (hardware acceleration and swiftshader) becomes
  // blocked.
  void OnGpuBlocked();

  // Helper to extract the domain from a given URL.
  std::string GetDomainFromURL(const GURL& url) const;

  // Implementation functions for blocking of 3D graphics APIs, used
  // for unit testing.
  void BlockDomainFrom3DAPIsAtTime(const GURL& url,
                                   gpu::DomainGuilt guilt,
                                   base::Time at_time);
  DomainBlockStatus Are3DAPIsBlockedAtTime(const GURL& url,
                                           base::Time at_time) const;
  int64_t GetBlockAllDomainsDurationInMs() const;

  // Notify all observers whenever there is a GPU info update.
  void NotifyGpuInfoUpdate();

  void RequestDxDiagNodeData();
  void RequestGpuSupportedRuntimeVersion(bool delayed);

  void RecordCompositingMode();

  GpuDataManagerImpl* const owner_;

  gpu::GpuFeatureInfo gpu_feature_info_;
  gpu::GPUInfo gpu_info_;
  gl::GpuPreference active_gpu_heuristic_ = gl::GpuPreference::kDefault;
#if defined(OS_WIN)
  bool gpu_info_dx_diag_requested_ = false;
  bool gpu_info_dx_diag_request_failed_ = false;
  bool gpu_info_dx12_vulkan_valid_ = false;
  bool gpu_info_dx12_vulkan_requested_ = false;
  bool gpu_info_dx12_vulkan_request_failed_ = false;
#endif

  // What we would have gotten if we haven't fallen back to SwiftShader or
  // pure software (in the viz case).
  gpu::GpuFeatureInfo gpu_feature_info_for_hardware_gpu_;
  gpu::GPUInfo gpu_info_for_hardware_gpu_;

  gpu::GpuExtraInfo gpu_extra_info_;

  const scoped_refptr<GpuDataManagerObserverList> observer_list_;

  // Periodically calls RecordCompositingMode() for compositing mode UMA.
  base::RepeatingTimer compositing_mode_timer_;

  // Contains the 1000 most recent log messages.
  std::vector<LogMessage> log_messages_;

  // What the gpu process is being run for.
  gpu::GpuMode gpu_mode_ = gpu::GpuMode::HARDWARE_ACCELERATED;

  // Used to tell if the gpu was disabled due to process crashes.
  bool hardware_disabled_by_fallback_ = false;

  // We disable histogram stuff in testing, especially in unit tests because
  // they cause random failures.
  bool update_histograms_ = true;

  DomainGuiltMap blocked_domains_;
  mutable std::list<base::Time> timestamps_of_gpu_resets_;
  bool domain_blocking_enabled_ = true;

  bool application_is_visible_ = true;

  bool disable_gpu_compositing_ = false;

  DISALLOW_COPY_AND_ASSIGN(GpuDataManagerImplPrivate);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_PRIVATE_H_
