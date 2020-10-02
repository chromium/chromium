/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_PLATFORM_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_PLATFORM_H_

#include <memory>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_renderer_sink.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom-shared.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-shared.h"
#include "third_party/blink/public/platform/audio/web_audio_device_source_type.h"
#include "third_party/blink/public/platform/blame_context.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/user_metrics_action.h"
#include "third_party/blink/public/platform/web_audio_device.h"
#include "third_party/blink/public/platform/web_code_cache_loader.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_dedicated_worker_host_factory_client.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/webrtc/api/video/video_codec_type.h"
#include "ui/base/resource/scale_factor.h"

class SkCanvas;

namespace base {
class SingleThreadTaskRunner;
}

namespace gfx {
class ColorSpace;
}

namespace gpu {
class GpuMemoryBufferManager;
}

namespace media {
struct AudioSinkParameters;
struct AudioSourceParameters;
class MediaPermission;
class GpuVideoAcceleratorFactories;
}  // namespace media

namespace network {
namespace mojom {
class URLResponseHead;
}  // namespace mojom
}  // namespace network

namespace v8 {
class Context;
template <class T>
class Local;
}  // namespace v8

namespace viz {
class RasterContextProvider;
}

namespace blink {

class BrowserInterfaceBrokerProxy;
class ThreadSafeBrowserInterfaceBrokerProxy;
class Thread;
struct ThreadCreationParams;
class WebAudioBus;
class WebAudioLatencyHint;
class WebCrypto;
class WebDedicatedWorker;
class WebGraphicsContext3DProvider;
class WebLocalFrame;
class WebMediaCapabilitiesClient;
class WebPublicSuffixList;
class WebSandboxSupport;
class WebSecurityOrigin;
class WebThemeEngine;
class WebURLResponse;
class WebURLResponse;
class WebVideoCaptureImplManager;

namespace scheduler {
class WebThreadScheduler;
}

class BLINK_PLATFORM_EXPORT Platform {
 public:
  // Initialize platform and wtf. If you need to initialize the entire Blink,
  // you should use blink::Initialize. WebThreadScheduler must be owned by
  // the embedder. InitializeBlink must be called before WebThreadScheduler is
  // created and passed to InitializeMainThread.
  static void InitializeBlink();
  static void InitializeMainThread(
      Platform*,
      scheduler::WebThreadScheduler* main_thread_scheduler);
  static Platform* Current();

  // This is another entry point for embedders that only require simple
  // execution environment of Blink. This version automatically sets up Blink
  // with a minimally viable implementation of WebThreadScheduler and
  // blink::Thread for the main thread.
  //
  // TODO(yutak): Fix function name as it seems obsolete at this point.
  static void CreateMainThreadAndInitialize(Platform*);

  // Used to switch the current platform only for testing.
  // You should not pass in a Platform object that is not fully instantiated.
  //
  // NOTE: Instead of calling this directly, us a ScopedTestingPlatformSupport
  // which will restore the previous platform on exit, preventing tests from
  // clobbering each other.
  static void SetCurrentPlatformForTesting(Platform*);

  // This sets up a minimally viable implementation of blink::Thread without
  // changing the current Platform. This is essentially a workaround for the
  // initialization order in ScopedUnittestsEnvironmentSetup, and nobody else
  // should use this.
  static void CreateMainThreadForTesting();

  // These are dirty workaround for tests requiring the main thread task runner
  // from a non-main thread. If your test needs base::TaskEnvironment
  // and a non-main thread may call MainThread()->GetTaskRunner(), call
  // SetMainThreadTaskRunnerForTesting() in your test fixture's SetUp(), and
  // call UnsetMainThreadTaskRunnerForTesting() in TearDown().
  //
  // TODO(yutak): Ideally, these should be packed in a custom test fixture
  // along with TaskEnvironment for reusability.
  static void SetMainThreadTaskRunnerForTesting();
  static void UnsetMainThreadTaskRunnerForTesting();

  Platform();
  virtual ~Platform();

  // May return null if sandbox support is not necessary
  virtual WebSandboxSupport* GetSandboxSupport() { return nullptr; }

  // May return null on some platforms.
  virtual WebThemeEngine* ThemeEngine() { return nullptr; }

  // AppCache  ----------------------------------------------------------

  virtual bool IsURLSupportedForAppCache(const WebURL& url) { return false; }

  // Audio --------------------------------------------------------------

  virtual double AudioHardwareSampleRate() { return 0; }
  virtual size_t AudioHardwareBufferSize() { return 0; }
  virtual unsigned AudioHardwareOutputChannels() { return 0; }
  virtual base::TimeDelta GetHungRendererDelay() { return base::TimeDelta(); }

  // SavableResource ----------------------------------------------------

  virtual bool IsURLSavableForSavableResource(const WebURL& url) {
    return false;
  }

  // Creates a device for audio I/O.
  // Pass in (number_of_input_channels > 0) if live/local audio input is
  // desired.
  virtual std::unique_ptr<WebAudioDevice> CreateAudioDevice(
      unsigned number_of_input_channels,
      unsigned number_of_channels,
      const WebAudioLatencyHint& latency_hint,
      WebAudioDevice::RenderCallback*,
      const WebString& device_id) {
    return nullptr;
  }

  // Database (WebSQL) ---------------------------------------------------

  // Return a filename-friendly identifier for an origin.
  virtual WebString DatabaseCreateOriginIdentifier(
      const WebSecurityOrigin& origin) {
    return WebString();
  }

  // FileSystem ----------------------------------------------------------

  // Return a filename-friendly identifier for an origin.
  virtual WebString FileSystemCreateOriginIdentifier(
      const WebSecurityOrigin& origin) {
    return WebString();
  }

  // IDN conversion ------------------------------------------------------

  virtual WebString ConvertIDNToUnicode(const WebString& host) { return host; }

  // History -------------------------------------------------------------

  // Returns the hash for the given canonicalized URL for use in visited
  // link coloring.
  virtual uint64_t VisitedLinkHash(const char* canonical_url, size_t length) {
    return 0;
  }

  // Returns whether the given link hash is in the user's history. The
  // hash must have been generated by calling VisitedLinkHash().
  virtual bool IsLinkVisited(uint64_t link_hash) { return false; }

  static const size_t kNoDecodedImageByteLimit = static_cast<size_t>(-1);

  // Returns the maximum amount of memory a decoded image should be allowed.
  // See comments on ImageDecoder::max_decoded_bytes_.
  virtual size_t MaxDecodedImageBytes() { return kNoDecodedImageByteLimit; }

  // See: SysUtils::IsLowEndDevice for the full details of what "low-end" means.
  // This returns true for devices that can use more extreme tradeoffs for
  // performance. Many low memory devices (<=1GB) are not considered low-end.
  virtual bool IsLowEndDevice() { return false; }

  // Process -------------------------------------------------------------

  // Returns a unique FrameSinkID for the current renderer process
  virtual viz::FrameSinkId GenerateFrameSinkId() { return viz::FrameSinkId(); }

  // Returns whether this process is locked to a single site (i.e. a scheme
  // plus eTLD+1, such as https://google.com), or to a more specific origin.
  // This means the process will not be used to load documents or workers from
  // URLs outside that site.
  virtual bool IsLockedToSite() const { return false; }

  // Network -------------------------------------------------------------

  // Returns the WebCodeCacheLoader that is used to fetch data from code caches.
  // It is OK to return a nullptr. When a nullptr is returned, data would not
  // be fetched from code cache.
  virtual std::unique_ptr<WebCodeCacheLoader> CreateCodeCacheLoader() {
    return nullptr;
  }

  // Returns a new WebURLLoaderFactory that wraps the given
  // network::mojom::URLLoaderFactory.
  virtual std::unique_ptr<WebURLLoaderFactory> WrapURLLoaderFactory(
      CrossVariantMojoRemote<network::mojom::URLLoaderFactoryInterfaceBase>
          url_loader_factory) {
    return nullptr;
  }

  // Returns a new WebURLLoaderFactory that wraps the given
  // network::SharedURLLoaderFactory.
  virtual std::unique_ptr<blink::WebURLLoaderFactory>
  WrapSharedURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> factory) {
    return nullptr;
  }

  // Returns the User-Agent string.
  virtual WebString UserAgent() { return WebString(); }

  // Returns the User Agent metadata. This will replace `UserAgent()` if we
  // end up shipping https://github.com/WICG/ua-client-hints.
  virtual blink::UserAgentMetadata UserAgentMetadata() {
    return blink::UserAgentMetadata();
  }

  // A suggestion to cache this metadata in association with this URL.
  virtual void CacheMetadata(blink::mojom::CodeCacheType cache_type,
                             const WebURL&,
                             base::Time response_time,
                             const uint8_t* data,
                             size_t data_size) {}

  // A request to fetch contents associated with this URL from metadata cache.
  using FetchCachedCodeCallback =
      base::OnceCallback<void(base::Time, mojo_base::BigBuffer)>;
  virtual void FetchCachedCode(blink::mojom::CodeCacheType cache_type,
                               const WebURL&,
                               FetchCachedCodeCallback) {}
  virtual void ClearCodeCacheEntry(blink::mojom::CodeCacheType cache_type,
                                   const GURL&) {}

  // A suggestion to cache this metadata in association with this URL which
  // resource is in CacheStorage.
  virtual void CacheMetadataInCacheStorage(
      const WebURL&,
      base::Time response_time,
      const uint8_t* data,
      size_t data_size,
      const blink::WebSecurityOrigin& cache_storage_origin,
      const WebString& cache_storage_cache_name) {}

  // Converts network::mojom::URLResponseHead to WebURLResponse.
  // TODO(crbug.com/860403): Remove this once it's moved into Blink.
  virtual void PopulateURLResponse(const WebURL& url,
                                   const network::mojom::URLResponseHead& head,
                                   WebURLResponse* response,
                                   bool report_security_info,
                                   int request_id) {}
  // Public Suffix List --------------------------------------------------

  // May return null on some platforms.
  virtual WebPublicSuffixList* PublicSuffixList() { return nullptr; }

  // Resources -----------------------------------------------------------

  // Returns a localized string resource (with substitution parameters).
  virtual WebString QueryLocalizedString(int resource_id) {
    return WebString();
  }
  virtual WebString QueryLocalizedString(int resource_id,
                                         const WebString& parameter) {
    return WebString();
  }
  virtual WebString QueryLocalizedString(int resource_id,
                                         const WebString& parameter1,
                                         const WebString& parameter2) {
    return WebString();
  }

  // Threads -------------------------------------------------------

  // Most of threading functionality has moved to blink::Thread. The functions
  // in Platform are deprecated; use the counterpart in blink::Thread as noted
  // below.

  // DEPRECATED: Use Thread::CreateThread() instead.
  std::unique_ptr<Thread> CreateThread(const ThreadCreationParams&);

  // The two compositor-related functions below are called by the embedder.
  // TODO(yutak): Perhaps we should move these to somewhere else?

  // Create and initialize the compositor thread. After this function
  // completes, you can access CompositorThreadTaskRunner().
  void CreateAndSetCompositorThread();

  // Returns the task runner of the compositor thread. This is available
  // once CreateAndSetCompositorThread() is called.
  scoped_refptr<base::SingleThreadTaskRunner> CompositorThreadTaskRunner();

  // This is called after the compositor thread is created, so the embedder
  // can initiate an IPC to change its thread priority (on Linux we can't
  // increase the nice value, so we need to ask the browser process). This
  // function is only called from the main thread (where InitializeCompositor-
  // Thread() is called).
  virtual void SetDisplayThreadPriority(base::PlatformThreadId) {}

  // Returns a blame context for attributing top-level work which does not
  // belong to a particular frame scope.
  virtual BlameContext* GetTopLevelBlameContext() { return nullptr; }

  // Resources -----------------------------------------------------------

  // Returns a blob of data corresponding to |resource_id|. This should not be
  // used for resources which have compress="gzip" in *.grd.
  virtual WebData GetDataResource(
      int resource_id,
      ui::ScaleFactor scale_factor = ui::SCALE_FACTOR_NONE) {
    return WebData();
  }

  // Gets a blob of data resource corresponding to |resource_id|, then
  // uncompresses it. This should be used for resources which have
  // compress="gzip" in *.grd.
  virtual WebData UncompressDataResource(int resource_id) { return WebData(); }

  // Decodes the in-memory audio file data and returns the linear PCM audio data
  // in the |destination_bus|.
  // Returns true on success.
  virtual bool DecodeAudioFileData(WebAudioBus* destination_bus,
                                   const char* audio_file_data,
                                   size_t data_size) {
    return false;
  }

  // Process lifetime management -----------------------------------------

  // Disable/Enable sudden termination on a process level. When possible, it
  // is preferable to disable sudden termination on a per-frame level via
  // mojom::LocalFrameHost::SuddenTerminationDisablerChanged.
  // This method should only be called on the main thread.
  virtual void SuddenTerminationChanged(bool enabled) {}

  // System --------------------------------------------------------------

  // Returns a value such as "en-US".
  virtual WebString DefaultLocale() { return WebString(); }

  // Returns an interface to the IO task runner.
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() const {
    return nullptr;
  }

  // Returns an interface to run nested message loop. Used for debugging.
  class NestedMessageLoopRunner {
   public:
    virtual ~NestedMessageLoopRunner() = default;
    virtual void Run() = 0;
    virtual void QuitNow() = 0;
  };
  virtual std::unique_ptr<NestedMessageLoopRunner>
  CreateNestedMessageLoopRunner() const {
    return nullptr;
  }
  // Testing -------------------------------------------------------------

  // Record a UMA sequence action.  The UserMetricsAction construction must
  // be on a single line for extract_actions.py to find it.  Please see
  // that script for more details.  Intended use is:
  // RecordAction(UserMetricsAction("MyAction"))
  virtual void RecordAction(const UserMetricsAction&) {}

  typedef uint64_t WebMemoryAllocatorDumpGuid;

  // GPU ----------------------------------------------------------------
  //
  enum ContextType {
    kWebGL1ContextType,  // WebGL 1.0 context, use only for WebGL canvases
    kWebGL2ContextType,  // WebGL 2.0 context, use only for WebGL canvases
    kWebGL2ComputeContextType,  // WebGL 2.0 Compute context, use only for WebGL
                                // canvases
    kGLES2ContextType,   // GLES 2.0 context, default, good for using skia
    kGLES3ContextType,   // GLES 3.0 context
    kWebGPUContextType,  // WebGPU context
  };
  struct ContextAttributes {
    bool prefer_low_power_gpu = false;
    bool fail_if_major_performance_caveat = false;
    ContextType context_type = kGLES2ContextType;
    // Offscreen contexts usually share a surface for the default frame buffer
    // since they aren't rendering to it. Setting any of the following
    // attributes causes creation of a custom surface owned by the context.
    bool support_alpha = false;
    bool support_depth = false;
    bool support_antialias = false;
    bool support_stencil = false;

    // Offscreen contexts created for WebGL should not need the RasterInterface
    // or GrContext. If either of these are set to false, it will not be
    // possible to use the corresponding interface for the lifetime of the
    // context.
    bool enable_raster_interface = false;
    bool support_grcontext = false;
  };
  struct GraphicsInfo {
    unsigned vendor_id = 0;
    unsigned device_id = 0;
    unsigned reset_notification_strategy = 0;
    bool sandboxed = false;
    bool amd_switchable = false;
    bool optimus = false;
    WebString vendor_info;
    WebString renderer_info;
    WebString driver_version;
    WebString error_message;
  };
  // Returns a newly allocated and initialized offscreen context provider,
  // backed by an independent context. Returns null if the context cannot be
  // created or initialized.
  virtual std::unique_ptr<WebGraphicsContext3DProvider>
  CreateOffscreenGraphicsContext3DProvider(const ContextAttributes&,
                                           const WebURL& top_document_url,
                                           GraphicsInfo*);

  // Returns a newly allocated and initialized offscreen context provider,
  // backed by the process-wide shared main thread context. Returns null if
  // the context cannot be created or initialized.
  virtual std::unique_ptr<WebGraphicsContext3DProvider>
  CreateSharedOffscreenGraphicsContext3DProvider();

  // Returns a newly allocated and initialized WebGPU context provider,
  // backed by an independent context. Returns null if the context cannot be
  // created or initialized.
  virtual std::unique_ptr<WebGraphicsContext3DProvider>
  CreateWebGPUGraphicsContext3DProvider(const WebURL& top_document_url);

  virtual gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() {
    return nullptr;
  }

  // When true, animations will run on a compositor thread independently from
  // the blink main thread.
  // This is true when there exists a renderer compositor in this process. But
  // for unit tests, a single-threaded compositor may be used so it may remain
  // false.
  virtual bool IsThreadedAnimationEnabled() { return false; }

  // Whether the compositor is using gpu and expects gpu resources as inputs,
  // or software based resources.
  // NOTE: This function should not be called from core/ and modules/, but
  // called by platform/graphics/ is fine.
  virtual bool IsGpuCompositingDisabled() { return true; }

#if defined(OS_ANDROID)
  // Returns if synchronous compositing is enabled. Only used for Android
  // webview.
  virtual bool IsSynchronousCompositingEnabledForAndroidWebView() {
    return false;
  }

  // Returns if zero copy synchronouse software draw is enabled. Only used
  // when SynchronousCompositing is enabled and only when in single process
  // mode.
  virtual bool IsZeroCopySynchronousSwDrawEnabledForAndroidWebView() {
    return false;
  }

  // Return the SkCanvas that is to be used if
  // ZeroCopySynchronousSwDrawEnabled returns true.
  virtual SkCanvas* SynchronousCompositorGetSkCanvasForAndroidWebView() {
    return nullptr;
  }
#endif

  // Whether zoom for dsf is enabled. When true, inputs to blink would all be
  // scaled by the device scale factor so that layout is done in device pixel
  // space.
  virtual bool IsUseZoomForDSFEnabled() { return false; }

  // Whether LCD text is enabled.
  virtual bool IsLcdTextEnabled() { return false; }

  // Whether rubberbanding/elatic on overscrolling is enabled. This usually
  // varies between each OS and can be configured via user settings in the OS.
  virtual bool IsElasticOverscrollEnabled() { return false; }

  // Whether the scroll animator that produces smooth scrolling is enabled.
  virtual bool IsScrollAnimatorEnabled() { return true; }

  // Returns a context provider that will be bound on the main thread
  // thread.
  virtual scoped_refptr<viz::RasterContextProvider>
  SharedMainThreadContextProvider();

  // Returns a worker context provider that will be bound on the compositor
  // thread.
  virtual scoped_refptr<viz::RasterContextProvider>
  SharedCompositorWorkerContextProvider();

  // Synchronously establish a channel to the GPU plugin if not previously
  // established or if it has been lost (for example if the GPU plugin crashed).
  // If there is a pending asynchronous request, it will be completed by the
  // time this routine returns.
  virtual scoped_refptr<gpu::GpuChannelHost> EstablishGpuChannelSync();

  // Media stream ----------------------------------------------------
  virtual scoped_refptr<media::AudioCapturerSource> NewAudioCapturerSource(
      blink::WebLocalFrame* web_frame,
      const media::AudioSourceParameters& params) {
    return nullptr;
  }

  virtual bool RTCSmoothnessAlgorithmEnabled() { return true; }

  // WebRTC ----------------------------------------------------------

  virtual base::Optional<double> GetWebRtcMaxCaptureFrameRate() {
    return base::nullopt;
  }

  virtual scoped_refptr<media::AudioRendererSink> NewAudioRendererSink(
      blink::WebAudioDeviceSourceType source_type,
      blink::WebLocalFrame* web_frame,
      const media::AudioSinkParameters& params) {
    return nullptr;
  }
  virtual media::AudioLatency::LatencyType GetAudioSourceLatencyType(
      blink::WebAudioDeviceSourceType source_type) {
    return media::AudioLatency::LATENCY_PLAYBACK;
  }

  virtual base::Optional<std::string> GetWebRTCAudioProcessingConfiguration() {
    return base::nullopt;
  }

  virtual bool ShouldEnforceWebRTCRoutingPreferences() { return true; }

  virtual media::MediaPermission* GetWebRTCMediaPermission(
      WebLocalFrame* web_frame) {
    return nullptr;
  }

  virtual bool UsesFakeCodecForPeerConnection() { return false; }

  virtual bool IsWebRtcEncryptionEnabled() { return true; }

  virtual bool IsWebRtcStunOriginEnabled() { return false; }

  virtual bool IsWebRtcSrtpAesGcmEnabled() { return false; }

  virtual bool IsWebRtcSrtpEncryptedHeadersEnabled() { return false; }

  virtual base::Optional<WebString> WebRtcStunProbeTrialParameter() {
    return base::nullopt;
  }

  // TODO(qingsi): Consolidate the legacy |ip_handling_policy| with
  // |allow_mdns_obfuscation| following the latest spec on IP handling modes
  // with mDNS introduced
  // (https://tools.ietf.org/html/draft-ietf-rtcweb-ip-handling-12);
  virtual void GetWebRTCRendererPreferences(WebLocalFrame* web_frame,
                                            WebString* ip_handling_policy,
                                            uint16_t* udp_min_port,
                                            uint16_t* udp_max_port,
                                            bool* allow_mdns_obfuscation) {}

  virtual base::Optional<int> GetAgcStartupMinimumVolume() {
    return base::nullopt;
  }

  virtual bool IsWebRtcHWH264DecodingEnabled(
      webrtc::VideoCodecType video_coded_type) {
    return true;
  }

  virtual bool IsWebRtcHWEncodingEnabled() { return true; }

  virtual bool IsWebRtcHWDecodingEnabled() { return true; }

  virtual bool AllowsLoopbackInPeerConnection() { return false; }

  // VideoCapture -------------------------------------------------------

  virtual WebVideoCaptureImplManager* GetVideoCaptureImplManager() {
    return nullptr;
  }

  // WebWorker ----------------------------------------------------------

  virtual std::unique_ptr<WebDedicatedWorkerHostFactoryClient>
  CreateDedicatedWorkerHostFactoryClient(WebDedicatedWorker*,
                                         const BrowserInterfaceBrokerProxy&) {
    return nullptr;
  }
  virtual void DidStartWorkerThread() {}
  virtual void WillStopWorkerThread() {}
  virtual void WorkerContextCreated(const v8::Local<v8::Context>& worker) {}
  virtual bool AllowScriptExtensionForServiceWorker(
      const WebSecurityOrigin& script_origin) {
    return false;
  }
  virtual bool IsExcludedHeaderForServiceWorkerFetchEvent(
      const WebString& header_name) {
    return false;
  }

  // WebCrypto ----------------------------------------------------------

  virtual WebCrypto* Crypto() { return nullptr; }

  // Mojo ---------------------------------------------------------------

  // Callable from any thread. Asks the browser to bind an interface receiver on
  // behalf of this renderer.
  //
  // Note that all GetInterface requests made on this object will hop to the IO
  // thread before being passed to the browser process.
  //
  // Callers should consider scoping their interfaces to a more specific context
  // before resorting to use of process-scoped interface bindings. Frames and
  // workers have their own contexts, and their BrowserInterfaceBrokerProxy
  // instances have less overhead since they don't need to be thread-safe.
  // Using a more narrowly defined scope when possible is also generally better
  // for security.
  virtual ThreadSafeBrowserInterfaceBrokerProxy* GetBrowserInterfaceBroker();

  // Media Capabilities --------------------------------------------------

  virtual WebMediaCapabilitiesClient* MediaCapabilitiesClient() {
    return nullptr;
  }

  // GpuVideoAcceleratorFactories --------------------------------------

  virtual media::GpuVideoAcceleratorFactories* GetGpuFactories() {
    return nullptr;
  }

  virtual void SetRenderingColorSpace(const gfx::ColorSpace& color_space) {}

  // Renderer Memory Metrics ----------------------------------------------

  virtual void RecordMetricsForBackgroundedRendererPurge() {}

  // V8 Context Snapshot --------------------------------------------------

  // This method returns true only when
  // tools/v8_context_snapshot/v8_context_snapshot_generator is running (which
  // runs during Chromium's build step).
  virtual bool IsTakingV8ContextSnapshot() { return false; }

 private:
  static void InitializeMainThreadCommon(Platform* platform,
                                         std::unique_ptr<Thread> main_thread);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_PLATFORM_H_
