// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_BLINK_PLATFORM_IMPL_H_
#define CONTENT_RENDERER_RENDERER_BLINK_PLATFORM_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/id_map.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/child/blink_platform_impl.h"
#include "content/common/content_export.h"
#include "content/renderer/top_level_blame_context.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "components/services/font/public/cpp/font_loader.h"  // nogncheck
#include "third_party/skia/include/core/SkRefCnt.h"           // nogncheck
#endif

namespace blink {
namespace scheduler {
class WebThreadScheduler;
}
class WebGraphicsContext3DProvider;
class WebSecurityOrigin;
}  // namespace blink

namespace media {
class GpuVideoAcceleratorFactories;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace viz {
class RasterContextProvider;
}

namespace content {

class CONTENT_EXPORT RendererBlinkPlatformImpl : public BlinkPlatformImpl {
 public:
  explicit RendererBlinkPlatformImpl(
      blink::scheduler::WebThreadScheduler* main_thread_scheduler);
  ~RendererBlinkPlatformImpl() override;

  blink::scheduler::WebThreadScheduler* main_thread_scheduler() {
    return main_thread_scheduler_;
  }

  // Shutdown must be called just prior to shutting down blink.
  void Shutdown();

  // Platform methods:
  blink::WebSandboxSupport* GetSandboxSupport() override;
  blink::WebThemeEngine* ThemeEngine() override;
  virtual bool sandboxEnabled();
  uint64_t VisitedLinkHash(const char* canonicalURL, size_t length) override;
  bool IsLinkVisited(uint64_t linkHash) override;
  blink::WebString UserAgent() override;
  blink::UserAgentMetadata UserAgentMetadata() override;
  void CacheMetadata(blink::mojom::CodeCacheType cache_type,
                     const blink::WebURL&,
                     base::Time,
                     const uint8_t*,
                     size_t) override;
  void FetchCachedCode(blink::mojom::CodeCacheType cache_type,
                       const blink::WebURL&,
                       FetchCachedCodeCallback) override;
  void ClearCodeCacheEntry(blink::mojom::CodeCacheType cache_type,
                           const GURL&) override;
  void CacheMetadataInCacheStorage(
      const blink::WebURL&,
      base::Time,
      const uint8_t*,
      size_t,
      const blink::WebSecurityOrigin& cacheStorageOrigin,
      const blink::WebString& cacheStorageCacheName) override;
  void PopulateURLResponse(const blink::WebURL& url,
                           const network::mojom::URLResponseHead& head,
                           blink::WebURLResponse* response,
                           bool report_security_info,
                           int request_id) override;
  blink::WebString DefaultLocale() override;
  void SuddenTerminationChanged(bool enabled) override;
  blink::WebString DatabaseCreateOriginIdentifier(
      const blink::WebSecurityOrigin& origin) override;
  viz::FrameSinkId GenerateFrameSinkId() override;
  bool IsLockedToSite() const override;

  blink::WebString FileSystemCreateOriginIdentifier(
      const blink::WebSecurityOrigin& origin) override;

  bool IsThreadedAnimationEnabled() override;
  bool IsGpuCompositingDisabled() override;
#if defined(OS_ANDROID)
  bool IsSynchronousCompositingEnabled() override;
#endif
  bool IsUseZoomForDSFEnabled() override;
  bool IsLcdTextEnabled() override;
  bool IsElasticOverscrollEnabled() override;
  bool IsScrollAnimatorEnabled() override;
  double AudioHardwareSampleRate() override;
  size_t AudioHardwareBufferSize() override;
  unsigned AudioHardwareOutputChannels() override;
  base::TimeDelta GetHungRendererDelay() override;

  std::unique_ptr<blink::WebAudioDevice> CreateAudioDevice(
      unsigned input_channels,
      unsigned channels,
      const blink::WebAudioLatencyHint& latency_hint,
      blink::WebAudioDevice::RenderCallback* callback,
      const blink::WebString& input_device_id) override;

  bool DecodeAudioFileData(blink::WebAudioBus* destination_bus,
                           const char* audio_file_data,
                           size_t data_size) override;

  scoped_refptr<media::AudioCapturerSource> NewAudioCapturerSource(
      blink::WebLocalFrame* web_frame,
      const media::AudioSourceParameters& params) override;
  viz::RasterContextProvider* SharedMainThreadContextProvider() override;
  bool RTCSmoothnessAlgorithmEnabled() override;
  base::Optional<double> GetWebRtcMaxCaptureFrameRate() override;
  scoped_refptr<media::AudioRendererSink> NewAudioRendererSink(
      blink::WebAudioDeviceSourceType source_type,
      blink::WebLocalFrame* web_frame,
      const media::AudioSinkParameters& params) override;
  media::AudioLatency::LatencyType GetAudioSourceLatencyType(
      blink::WebAudioDeviceSourceType source_type) override;
  base::Optional<std::string> GetWebRTCAudioProcessingConfiguration() override;
  bool ShouldEnforceWebRTCRoutingPreferences() override;
  bool UsesFakeCodecForPeerConnection() override;
  bool IsWebRtcEncryptionEnabled() override;
  bool IsWebRtcStunOriginEnabled() override;
  base::Optional<blink::WebString> WebRtcStunProbeTrialParameter() override;
  media::MediaPermission* GetWebRTCMediaPermission(
      blink::WebLocalFrame* web_frame) override;
  void GetWebRTCRendererPreferences(blink::WebLocalFrame* web_frame,
                                    blink::WebString* ip_handling_policy,
                                    uint16_t* udp_min_port,
                                    uint16_t* udp_max_port,
                                    bool* allow_mdns_obfuscation) override;
  base::Optional<int> GetAgcStartupMinimumVolume() override;
  bool IsWebRtcHWH264DecodingEnabled(
      webrtc::VideoCodecType video_coded_type) override;
  bool IsWebRtcHWEncodingEnabled() override;
  bool IsWebRtcHWDecodingEnabled() override;
  bool IsWebRtcSrtpAesGcmEnabled() override;
  bool IsWebRtcSrtpEncryptedHeadersEnabled() override;
  bool AllowsLoopbackInPeerConnection() override;

  blink::WebVideoCaptureImplManager* GetVideoCaptureImplManager() override;

  std::unique_ptr<blink::WebGraphicsContext3DProvider>
  CreateOffscreenGraphicsContext3DProvider(
      const blink::Platform::ContextAttributes& attributes,
      const blink::WebURL& top_document_web_url,
      blink::Platform::GraphicsInfo* gl_info) override;
  std::unique_ptr<blink::WebGraphicsContext3DProvider>
  CreateSharedOffscreenGraphicsContext3DProvider() override;
  std::unique_ptr<blink::WebGraphicsContext3DProvider>
  CreateWebGPUGraphicsContext3DProvider(
      const blink::WebURL& top_document_web_url) override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  blink::WebString ConvertIDNToUnicode(const blink::WebString& host) override;
  void SetDisplayThreadPriority(base::PlatformThreadId thread_id) override;
  blink::BlameContext* GetTopLevelBlameContext() override;

  std::unique_ptr<blink::WebDedicatedWorkerHostFactoryClient>
  CreateDedicatedWorkerHostFactoryClient(
      blink::WebDedicatedWorker*,
      const blink::BrowserInterfaceBrokerProxy&) override;
  void DidStartWorkerThread() override;
  void WillStopWorkerThread() override;
  void WorkerContextCreated(const v8::Local<v8::Context>& worker) override;
  bool AllowScriptExtensionForServiceWorker(
      const blink::WebSecurityOrigin& script_origin) override;
  bool IsExcludedHeaderForServiceWorkerFetchEvent(
      const blink::WebString& header_name) override;

  void RecordMetricsForBackgroundedRendererPurge() override;

  std::unique_ptr<blink::WebCodeCacheLoader> CreateCodeCacheLoader() override;

  std::unique_ptr<blink::WebURLLoaderFactory> WrapURLLoaderFactory(
      blink::CrossVariantMojoRemote<
          network::mojom::URLLoaderFactoryInterfaceBase> url_loader_factory)
      override;
  std::unique_ptr<blink::WebURLLoaderFactory> WrapSharedURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> factory) override;

  media::GpuVideoAcceleratorFactories* GetGpuFactories() override;
  void SetRenderingColorSpace(const gfx::ColorSpace& color_space) override;

  // Tells this platform that the renderer is locked to a site (i.e., a scheme
  // plus eTLD+1, such as https://google.com), or to a more specific origin.
  void SetIsLockedToSite();

 private:
  bool CheckPreparsedJsCachingEnabled() const;

  // Return the mojo interface for making CodeCache calls.
  blink::mojom::CodeCacheHost& GetCodeCacheHost();

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC)
  std::unique_ptr<blink::WebSandboxSupport> sandbox_support_;
#endif

  // This counter keeps track of the number of times sudden termination is
  // enabled or disabled. It starts at 0 (enabled) and for every disable
  // increments by 1, for every enable decrements by 1. When it reaches 0,
  // we tell the browser to enable fast termination.
  int sudden_termination_disables_;

  // If true, the renderer process is locked to a site.
  bool is_locked_to_site_;

  // NOT OWNED
  blink::scheduler::WebThreadScheduler* main_thread_scheduler_;

  TopLevelBlameContext top_level_blame_context_;

  mojo::PendingRemote<blink::mojom::CodeCacheHost> code_cache_host_remote_;
  mojo::SharedRemote<blink::mojom::CodeCacheHost> code_cache_host_;

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  sk_sp<font_service::FontLoader> font_loader_;
#endif

  THREAD_CHECKER(main_thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(RendererBlinkPlatformImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDERER_BLINK_PLATFORM_IMPL_H_
