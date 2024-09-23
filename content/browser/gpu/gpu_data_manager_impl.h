// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_H_
#define CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/no_destructor.h"
#include "base/process/kill.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/three_d_api_types.h"
#include "gpu/config/device_perf_info.h"
#include "gpu/config/gpu_control_list.h"
#include "gpu/config/gpu_domain_guilt.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_mode.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/video/video_encode_accelerator.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/gpu/gpu.mojom.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/gpu_extra_info.h"

#if BUILDFLAG(IS_WIN)
#include "ui/gfx/mojom/dxgi_info.mojom.h"
#endif

class GURL;

namespace gpu {
struct GpuPreferences;
}

namespace content {

class GpuDataManagerImplPrivate;

class CONTENT_EXPORT GpuDataManagerImpl : public GpuDataManager,
                                          public display::DisplayObserver {
 public:
  enum GpuInfoRequest {
    kGpuInfoRequestDirectX = 1 << 0,
    kGpuInfoRequestVulkan = 1 << 1,
    kGpuInfoRequestDawnInfo = 1 << 2,
    kGpuInfoRequestDirectXVulkan =
        kGpuInfoRequestVulkan | kGpuInfoRequestDirectX,
    kGpuInfoRequestVideo = 1 << 3,
    kGpuInfoRequestAll = kGpuInfoRequestDirectX | kGpuInfoRequestVulkan |
                         kGpuInfoRequestDawnInfo | kGpuInfoRequestVideo,
  };

  // Getter for the singleton. This will return NULL on failure.
  static GpuDataManagerImpl* GetInstance();

  GpuDataManagerImpl(const GpuDataManagerImpl&) = delete;
  GpuDataManagerImpl& operator=(const GpuDataManagerImpl&) = delete;

  // This returns true after the first call of GetInstance().
  static bool Initialized();

  // GpuDataManager implementation.
  gpu::GPUInfo GetGPUInfo() override;
  gpu::GpuFeatureStatus GetFeatureStatus(gpu::GpuFeatureType feature) override;
  bool GpuAccessAllowed(std::string* reason) override;
  bool IsEssentialGpuInfoAvailable() override;
  void RequestVideoMemoryUsageStatsUpdate(
      VideoMemoryUsageStatsCallback callback) override;
  // TODO(kbr): the threading model for the GpuDataManagerObservers is
  // not well defined, and it's impossible for callers to correctly
  // delete observers from anywhere except in one of the observer's
  // notification methods. Observer addition and removal, and their
  // callbacks, should probably be required to occur on the UI thread.
  void AddObserver(GpuDataManagerObserver* observer) override;
  void RemoveObserver(GpuDataManagerObserver* observer) override;
  void DisableHardwareAcceleration() override;
  bool HardwareAccelerationEnabled() override;
  void AppendGpuCommandLine(base::CommandLine* command_line,
                            GpuProcessKind kind) override;
  void BlocklistWebGLForTesting() override;
  void SetSkiaGraphiteEnabledForTesting(bool enabled) override;

  // Start a timer that occasionally reports UMA metrics. This is explicitly
  // started because unit tests may create and use a GpuDataManager but they do
  // not want surprise tasks being posted which can interfere with their ability
  // to measure what tasks are in the queue or to move mock time forward.
  void StartUmaTimer();

  // Requests complete GPU info if it has not already been requested
  void RequestDx12VulkanVideoGpuInfoIfNeeded(
      GpuDataManagerImpl::GpuInfoRequest request,
      bool delayed);

  bool IsDx12VulkanVersionAvailable() const;
  bool IsGpuFeatureInfoAvailable() const;

  void UpdateGpuInfo(
      const gpu::GPUInfo& gpu_info,
      const std::optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu);
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
  // Called from BrowserMainLoop::PostCreateThreads().
  // TODO(content/browser/gpu/OWNERS): This should probably use a
  // BrowserMainParts override instead.
  void PostCreateThreads();
  void UpdateDawnInfo(const std::vector<std::string>& dawn_info_list);

  // Update the GPU feature info. This updates the blocklist and enabled status
  // of GPU rasterization. In the future this will be used for more features.
  void UpdateGpuFeatureInfo(const gpu::GpuFeatureInfo& gpu_feature_info,
                            const std::optional<gpu::GpuFeatureInfo>&
                                gpu_feature_info_for_hardware_gpu);
  void UpdateGpuExtraInfo(const gfx::GpuExtraInfo& gpu_extra_info);
  void UpdateMojoMediaVideoDecoderCapabilities(
      const media::SupportedVideoDecoderConfigs& configs);
  void UpdateMojoMediaVideoEncoderCapabilities(
      const media::VideoEncodeAccelerator::SupportedProfiles&
          supported_profiles);

  gpu::GpuFeatureInfo GetGpuFeatureInfo() const;

  // The following functions for cached GPUInfo and GpuFeatureInfo from the
  // hardware GPU even if currently Chrome has fallen back to SwiftShader.
  // Such info are displayed in about:gpu for diagostic purpose.
  gpu::GPUInfo GetGPUInfoForHardwareGpu() const;
  gpu::GpuFeatureInfo GetGpuFeatureInfoForHardwareGpu() const;
  std::vector<std::string> GetDawnInfoList() const;
  bool GpuAccessAllowedForHardwareGpu(std::string* reason);
  bool IsGpuCompositingDisabledForHardwareGpu() const;

  gfx::GpuExtraInfo GetGpuExtraInfo() const;

  bool IsGpuCompositingDisabled() const;

  // This only handles the state of GPU compositing. Instead call
  // ImageTransportFactory::DisableGpuCompositing() to perform a fallback to
  // software compositing.
  void SetGpuCompositingDisabled();

  // Update GpuPreferences based on blocklisting decisions.
  void UpdateGpuPreferences(gpu::GpuPreferences* gpu_preferences,
                            GpuProcessKind kind) const;

  void AddLogMessage(int level,
                     const std::string& header,
                     const std::string& message);

  void ProcessCrashed();

  // Returns a base::Value::List with the log messages.
  base::Value::List GetLogMessages() const;

  // Called when switching GPUs.
  void HandleGpuSwitch();

  // Maintenance of domains requiring explicit user permission before
  // using client-facing 3D APIs (WebGL, Pepper 3D), either because
  // the domain has caused the GPU to reset, or because too many GPU
  // resets have been observed globally recently, and system stability
  // might be compromised. A set of URLs is passed because in the
  // situation where the GPU process crashes, the implementation needs
  // to know that these URLs all came from the same crash.
  //
  // In the set, each URL may be a partial URL (including at least the
  // host) or a full URL to a page.
  void BlockDomainsFrom3DAPIs(const std::set<GURL>& urls,
                              gpu::DomainGuilt guilt);
  bool Are3DAPIsBlocked(const GURL& top_origin_url,
                        ThreeDAPIType requester);
  void UnblockDomainFrom3DAPIs(const GURL& url);

  // Disables domain blocking for 3D APIs. For use only in tests.
  void DisableDomainBlockingFor3DAPIsForTesting();

  // Return mode describing what the GPU process will be launched to run.
  gpu::GpuMode GetGpuMode() const;

  // Called when GPU process initialization failed or the GPU process has
  // crashed repeatedly. This will try to disable hardware acceleration and then
  // SwiftShader WebGL. It will also crash the browser process as a last resort
  // on Android and Chrome OS.
  void FallBackToNextGpuMode();

  // Check if there is at least one fallback option available.
  bool CanFallback() const;

  // Returns false if the latest GPUInfo gl_renderer is from SwiftShader or
  // Disabled (in the viz case).
  bool IsGpuProcessUsingHardwareGpu() const;

  // State tracking allows us to customize GPU process launch depending on
  // whether we are in the foreground or background.
  void SetApplicationVisible(bool is_visible);

  // DisplayObserver overrides.
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplaysRemoved(const display::Displays& removed_displays) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

#if BUILDFLAG(IS_LINUX)
  bool IsGpuMemoryBufferNV12Supported();
  void SetGpuMemoryBufferNV12Supported(bool supported);
#endif  // BUILDFLAG(IS_LINUX)

  // Binds a new Mojo receiver to handle requests from a renderer.
  static void BindReceiver(
      mojo::PendingReceiver<blink::mojom::GpuDataManager> receiver);

 private:
  friend class GpuDataManagerImplPrivate;
  friend class GpuDataManagerImplPrivateTest;
  friend class base::NoDestructor<GpuDataManagerImpl>;

  GpuDataManagerImpl();
  ~GpuDataManagerImpl() override;

  mutable base::Lock lock_;
  std::unique_ptr<GpuDataManagerImplPrivate> private_ GUARDED_BY(lock_)
      PT_GUARDED_BY(lock_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_H_
