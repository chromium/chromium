// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_BLINK_PLATFORM_IMPL_H_
#define CONTENT_RENDERER_RENDERER_BLINK_PLATFORM_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/id_map.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/child/blink_platform_impl.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "third_party/blink/public/mojom/peerconnection/webrtc_ip_handling_policy.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_fetch_handler_bypass_option.mojom-shared.h"

namespace blink {
namespace scheduler {
class WebThreadScheduler;
}
class WebGraphicsContext3DProvider;
class WebSecurityOrigin;
enum class ProtocolHandlerSecurityLevel;
struct WebContentSecurityPolicyHeader;
}  // namespace blink

namespace gpu {
struct GPUInfo;
}

namespace media {
class GpuVideoAcceleratorFactories;
}

namespace viz {
class RasterContextProvider;
}

namespace content {

class CONTENT_EXPORT RendererBlinkPlatformImpl : public BlinkPlatformImpl {
 public:
  explicit RendererBlinkPlatformImpl(
      blink::scheduler::WebThreadScheduler* main_thread_scheduler);

  RendererBlinkPlatformImpl(const RendererBlinkPlatformImpl&) = delete;
  RendererBlinkPlatformImpl& operator=(const RendererBlinkPlatformImpl&) =
      delete;

  ~RendererBlinkPlatformImpl() override;

  blink::scheduler::WebThreadScheduler* main_thread_scheduler() {
    return main_thread_scheduler_;
  }

  // Shutdown must be called just prior to shutting down blink.
  void Shutdown();

  // blink::Platform implementation.
  blink::WebSandboxSupport* GetSandboxSupport() override;
  virtual bool sandboxEnabled();
  uint64_t VisitedLinkHash(std::string_view canonical_url) override;
  uint64_t PartitionedVisitedLinkFingerprint(
      std::string_view canonical_link_url,
      const net::SchemefulSite& top_level_site,
      const blink::WebSecurityOrigin& frame_origin) override;
  bool IsLinkVisited(uint64_t linkHash) override;
  void AddOrUpdateVisitedLinkSalt(const url::Origin& origin,
                                  uint64_t salt) override;
  blink::WebString UserAgent() override;
  blink::UserAgentMetadata UserAgentMetadata() override;
  bool IsRedirectSafe(
      const GURL& from_url,
      const GURL& to_url,
      const std::optional<url::Origin>& request_initiator) override;
  void AppendVariationsThrottles(
      const url::Origin& top_origin,
      std::vector<std::unique_ptr<blink::URLLoaderThrottle>>* throttles)
      override;
  std::unique_ptr<blink::URLLoaderThrottleProvider>
  CreateURLLoaderThrottleProviderForWorker(
      blink::URLLoaderThrottleProviderType provider_type) override;
  void CreateWebGPUGraphicsContext3DProviderAsync(
      const blink::WebURL& document_url,
      base::OnceCallback<
          void(std::unique_ptr<blink::WebGraphicsContext3DProvider>)> callback)
      override;
  void OnGpuChannelEstablished(
      const blink::WebURL& document_url,
      base::OnceCallback<
          void(std::unique_ptr<blink::WebGraphicsContext3DProvider>)> callback,
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host);
  std::unique_ptr<blink::WebSocketHandshakeThrottleProvider>
  CreateWebSocketHandshakeThrottleProvider() override;
  bool ShouldUseCodeCacheWithHashing(
      const blink::WebURL& request_url) const override;
  bool IsolateStartsInBackground() override;
  blink::WebString DefaultLocale() override;
  void SetSuddenTerminationAllowed(bool allowed) override;
  viz::FrameSinkId GenerateFrameSinkId() override;
  bool IsLockedToSite() const override;
  bool IsThreadedAnimationEnabled() override;
  bool IsGpuCompositingDisabled() const override;
#if BUILDFLAG(IS_ANDROID)
  bool IsSynchronousCompositingEnabledForAndroidWebView() override;
  bool IsZeroCopySynchronousSwDrawEnabledForAndroidWebView() override;
  SkCanvas* SynchronousCompositorGetSkCanvasForAndroidWebView() override;
#endif
  bool IsLcdTextEnabled() override;
  bool IsElasticOverscrollEnabled() override;
  bool IsScrollAnimatorEnabled() override;
  double AudioHardwareSampleRate() override;
  size_t AudioHardwareBufferSize() override;
  unsigned AudioHardwareOutputChannels() override;
  base::TimeDelta GetHungRendererDelay() override;
  std::unique_ptr<blink::WebAudioDevice> CreateAudioDevice(
      const blink::WebAudioSinkDescriptor& sink_descriptor,
      unsigned number_of_output_channels,
      const blink::WebAudioLatencyHint& latency_hint,
      std::optional<float> context_sample_rate,
      media::AudioRendererSink::RenderCallback* callback) override;
  bool DecodeAudioFileData(blink::WebAudioBus* destination_bus,
                           base::span<const char> audio_file_data) override;
  scoped_refptr<media::AudioCapturerSource> NewAudioCapturerSource(
      blink::WebLocalFrame* web_frame,
      const media::AudioSourceParameters& params) override;
  scoped_refptr<viz::RasterContextProvider> SharedMainThreadContextProvider()
      override;
  scoped_refptr<viz::RasterContextProvider>
  SharedCompositorWorkerContextProvider(
      cc::RasterDarkModeFilter* dark_mode_filter) override;
  bool IsGpuRemoteDisconnected() override;
  scoped_refptr<gpu::GpuChannelHost> EstablishGpuChannelSync() override;
  void EstablishGpuChannel(EstablishGpuChannelCallback callback) override;
  bool RTCSmoothnessAlgorithmEnabled() override;
  std::optional<double> GetWebRtcMaxCaptureFrameRate() override;
  scoped_refptr<media::AudioRendererSink> NewAudioRendererSink(
      blink::WebAudioDeviceSourceType source_type,
      blink::WebLocalFrame* web_frame,
      const media::AudioSinkParameters& params) override;
  media::AudioLatency::Type GetAudioSourceLatencyType(
      blink::WebAudioDeviceSourceType source_type) override;
  bool ShouldEnforceWebRTCRoutingPreferences() override;
  bool UsesFakeCodecForPeerConnection() override;
  bool IsWebRtcEncryptionEnabled() override;
  media::MediaPermission* GetWebRTCMediaPermission(
      blink::WebLocalFrame* web_frame) override;
  void GetWebRTCRendererPreferences(
      blink::WebLocalFrame* web_frame,
      blink::mojom::WebRtcIpHandlingPolicy* ip_handling_policy,
      uint16_t* udp_min_port,
      uint16_t* udp_max_port,
      bool* allow_mdns_obfuscation) override;
  bool IsWebRtcHWEncodingEnabled() override;
  bool IsWebRtcHWDecodingEnabled() override;
  bool AllowsLoopbackInPeerConnection() override;
  blink::WebVideoCaptureImplManager* GetVideoCaptureImplManager() override;
  std::unique_ptr<blink::WebGraphicsContext3DProvider>
  CreateWebGLGraphicsContextProvider(
      bool prefer_low_power_gpu,
      bool fail_if_major_performance_caveat,
      blink::Platform::WebGLContextType context_type,
      const blink::WebURL& document_url,
      blink::Platform::WebGLContextInfo* gl_info) override;
  std::unique_ptr<blink::WebGraphicsContext3DProvider>
  CreateRasterGraphicsContextProvider(
      const blink::WebURL& document_url,
      blink::Platform::RasterContextType context_type) override;
  std::unique_ptr<blink::WebGraphicsContext3DProvider>
  CreateSharedOffscreenGraphicsContext3DProvider() override;
  std::unique_ptr<blink::WebGraphicsContext3DProvider>
  CreateWebGPUGraphicsContext3DProvider(
      const blink::WebURL& document_url) override;
  blink::WebString ConvertIDNToUnicode(const blink::WebString& host) override;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  void SetThreadType(base::PlatformThreadId thread_id,
                     base::ThreadType) override;
#endif
  std::optional<int> GetWebUIBundledCodeCacheResourceId(
      const GURL& webui_resource_url) override;
  std::unique_ptr<blink::WebDedicatedWorkerHostFactoryClient>
  CreateDedicatedWorkerHostFactoryClient(
      blink::WebDedicatedWorker*,
      const blink::BrowserInterfaceBrokerProxy&) override;
  void DidStartWorkerThread() override;
  void WillStopWorkerThread() override;
  void WorkerContextCreated(const v8::Local<v8::Context>& worker) override;
  bool AllowScriptExtensionForServiceWorker(
      const blink::WebSecurityOrigin& script_origin) override;
  blink::ProtocolHandlerSecurityLevel GetProtocolHandlerSecurityLevel(
      const blink::WebSecurityOrigin& origin) override;
  bool OriginCanAccessServiceWorkers(const GURL& url) override;
  std::tuple<blink::CrossVariantMojoRemote<
                 blink::mojom::ServiceWorkerContainerHostInterfaceBase>,
             blink::CrossVariantMojoRemote<
                 blink::mojom::ServiceWorkerContainerHostInterfaceBase>>
  CloneServiceWorkerContainerHost(
      blink::CrossVariantMojoRemote<
          blink::mojom::ServiceWorkerContainerHostInterfaceBase>
          service_worker_container_host) override;
  void CreateServiceWorkerSubresourceLoaderFactory(
      blink::CrossVariantMojoRemote<
          blink::mojom::ServiceWorkerContainerHostInterfaceBase>
          service_worker_container_host,
      const blink::WebString& client_id,
      std::unique_ptr<network::PendingSharedURLLoaderFactory> fallback_factory,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      scoped_refptr<base::SequencedTaskRunner> task_runner) override;
  std::string GetNameForHistogram(const char* name) override;
  std::unique_ptr<media::MediaLog> GetMediaLog(
      blink::MediaInspectorContext* inspector_context,
      scoped_refptr<base::SingleThreadTaskRunner> owner_task_runner,
      bool is_on_worker) override;
  void AddCreateRemoteChildrenEvent(
      const std::optional<base::UnguessableToken>& navigation_metrics_token,
      const base::TimeTicks& start_time,
      const base::TimeDelta& elapsed_time) override;
  media::GpuVideoAcceleratorFactories* GetGpuFactories() override;
  scoped_refptr<base::SequencedTaskRunner> MediaThreadTaskRunner() override;
  base::WeakPtr<media::DecoderFactory> GetMediaDecoderFactory() override;
  void SetRenderingColorSpace(const gfx::ColorSpace& color_space) override;
  gfx::ColorSpace GetRenderingColorSpace() const override;
  void SetActiveURL(const blink::WebURL& url,
                    const blink::WebString& top_url) override;
  SkBitmap* GetSadPageBitmap() override;
  blink::mojom::PerformanceTier GetCpuPerformanceTier() override;
  std::unique_ptr<blink::WebV8ValueConverter> CreateWebV8ValueConverter()
      override;
  bool DisallowV8FeatureFlagOverrides() const override;
  void AppendContentSecurityPolicy(
      const blink::WebURL& url,
      std::vector<blink::WebContentSecurityPolicyHeader>* csp) override;
  bool IsFilePickerAllowedForCrossOriginSubframe(
      const blink::WebSecurityOrigin& origin) override;
  base::PlatformThreadId GetIOThreadId() const override;
  scoped_refptr<base::SingleThreadTaskRunner> VideoFrameCompositorTaskRunner()
      override;
#if BUILDFLAG(IS_ANDROID)
  void SetPrivateMemoryFootprint(
      uint64_t private_memory_footprint_bytes) override;
  bool IsUserLevelMemoryPressureSignalEnabled() override;
  std::pair<base::TimeDelta, base::TimeDelta>
  InertAndMinimumIntervalOfUserLevelMemoryPressureSignal() override;
#endif  // BUILDFLAG(IS_ANDROID)
  void OnV8HeapLastResortGC() override;

  // Tells this platform that the renderer is locked to a site (i.e., a scheme
  // plus eTLD+1, such as https://google.com), or to a more specific origin.
  void SetIsLockedToSite();

  void set_webui_resource_to_code_cache_id_map(
      const base::flat_map<GURL, int>& resource_map) {
    webui_resource_to_code_cache_id_map_ = resource_map;
  }

 private:
  bool CheckPreparsedJsCachingEnabled() const;

  void CollectWebGLContextInfo(blink::Platform::WebGLContextInfo* gl_info,
                               const gpu::GPUInfo& gpu_info) const;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
  std::unique_ptr<blink::WebSandboxSupport> sandbox_support_;
#endif

  // Number of active process-level sudden termination disablers.
  int sudden_termination_disables_ = 0;

  // If true, the renderer process is locked to a site.
  bool is_locked_to_site_;

  // NOT OWNED
  raw_ptr<blink::scheduler::WebThreadScheduler> main_thread_scheduler_;

  // Event that signals `io_thread_id_` is set and ready to be read.
  mutable base::WaitableEvent io_thread_id_ready_event_;
  base::PlatformThreadId io_thread_id_ = base::kInvalidThreadId;

  // Thread to run the VideoFrameCompositor on.
  std::unique_ptr<base::Thread> video_frame_compositor_thread_;

  // FrameSinks are generated on both the browser and the renderer. The
  // browser uses routing IDs and will be in the range [0,
  // std::numeric_limits<int32_t>::max()] The renderer has the
  // [std::numeric_limits<int32_t>::max() + 1,
  // std::numeric_limits<uint32_t>::max()] range.
  uint32_t next_frame_sink_id_;

  // Maps WebUI resource URLs to the resource ID of their associated bundled
  // code cache.
  base::flat_map<GURL, int> webui_resource_to_code_cache_id_map_;

  THREAD_CHECKER(main_thread_checker_);

  // Used for callbacks.
  base::WeakPtrFactory<RendererBlinkPlatformImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDERER_BLINK_PLATFORM_IMPL_H_
