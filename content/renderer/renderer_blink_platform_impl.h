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
#include "content/common/possibly_associated_interface_ptr.h"
#include "content/renderer/top_level_blame_context.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/screen_orientation/web_screen_orientation_type.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom.h"
#include "third_party/blink/public/platform/modules/cache_storage/cache_storage.mojom.h"
#include "third_party/blink/public/platform/modules/webdatabase/web_database.mojom.h"

#if defined(OS_LINUX)
#include "components/services/font/public/cpp/font_loader.h"  // nogncheck
#include "third_party/skia/include/core/SkRefCnt.h"           // nogncheck
#endif

namespace blink {
namespace scheduler {
class WebThreadScheduler;
}
class WebCanvasCaptureHandler;
class WebGraphicsContext3DProvider;
class WebMediaPlayer;
class WebMediaRecorderHandler;
class WebMediaStream;
class WebSecurityOrigin;
}  // namespace blink

namespace network {
class SharedURLLoaderFactory;
}

namespace content {
class BlinkInterfaceProviderImpl;
class ChildURLLoaderFactoryBundle;
class LocalStorageCachedAreas;
class ThreadSafeSender;
class WebDatabaseObserverImpl;

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
  blink::WebCookieJar* CookieJar() override;
  blink::WebThemeEngine* ThemeEngine() override;
  std::unique_ptr<blink::WebSpeechSynthesizer> CreateSpeechSynthesizer(
      blink::WebSpeechSynthesizerClient* client) override;
  virtual bool sandboxEnabled();
  unsigned long long VisitedLinkHash(const char* canonicalURL,
                                     size_t length) override;
  bool IsLinkVisited(unsigned long long linkHash) override;
  blink::WebPrescientNetworking* PrescientNetworking() override;
  blink::WebString UserAgent() override;
  void CacheMetadata(blink::mojom::CodeCacheType cache_type,
                     const blink::WebURL&,
                     base::Time,
                     const char*,
                     size_t) override;
  void FetchCachedCode(
      blink::mojom::CodeCacheType cache_type,
      const GURL&,
      base::OnceCallback<void(base::Time, const std::vector<uint8_t>&)>)
      override;
  void ClearCodeCacheEntry(blink::mojom::CodeCacheType cache_type,
                           const GURL&) override;
  void CacheMetadataInCacheStorage(
      const blink::WebURL&,
      base::Time,
      const char*,
      size_t,
      const blink::WebSecurityOrigin& cacheStorageOrigin,
      const blink::WebString& cacheStorageCacheName) override;
  blink::WebString DefaultLocale() override;
  void SuddenTerminationChanged(bool enabled) override;
  std::unique_ptr<blink::WebStorageNamespace> CreateLocalStorageNamespace()
      override;
  std::unique_ptr<blink::WebStorageNamespace> CreateSessionStorageNamespace(
      base::StringPiece namespace_id) override;
  blink::Platform::FileHandle DatabaseOpenFile(
      const blink::WebString& vfs_file_name,
      int desired_flags) override;
  int DatabaseDeleteFile(const blink::WebString& vfs_file_name,
                         bool sync_dir) override;
  long DatabaseGetFileAttributes(
      const blink::WebString& vfs_file_name) override;
  long long DatabaseGetFileSize(const blink::WebString& vfs_file_name) override;
  long long DatabaseGetSpaceAvailableForOrigin(
      const blink::WebSecurityOrigin& origin) override;
  bool DatabaseSetFileSize(const blink::WebString& vfs_file_name,
                           long long size) override;
  blink::WebString DatabaseCreateOriginIdentifier(
      const blink::WebSecurityOrigin& origin) override;
  viz::FrameSinkId GenerateFrameSinkId() override;
  bool IsLockedToSite() const override;

  blink::WebString FileSystemCreateOriginIdentifier(
      const blink::WebSecurityOrigin& origin) override;

  bool IsThreadedAnimationEnabled() override;
  bool IsGpuCompositingDisabled() override;
  double AudioHardwareSampleRate() override;
  size_t AudioHardwareBufferSize() override;
  unsigned AudioHardwareOutputChannels() override;
  blink::WebDatabaseObserver* DatabaseObserver() override;

  std::unique_ptr<blink::WebAudioDevice> CreateAudioDevice(
      unsigned input_channels,
      unsigned channels,
      const blink::WebAudioLatencyHint& latency_hint,
      blink::WebAudioDevice::RenderCallback* callback,
      const blink::WebString& input_device_id) override;

  bool DecodeAudioFileData(blink::WebAudioBus* destination_bus,
                           const char* audio_file_data,
                           size_t data_size) override;

  std::unique_ptr<blink::WebMIDIAccessor> CreateMIDIAccessor(
      blink::WebMIDIAccessorClient* client) override;

  blink::WebBlobRegistry* GetBlobRegistry() override;
  std::unique_ptr<blink::WebRTCPeerConnectionHandler>
  CreateRTCPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandlerClient* client,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  std::unique_ptr<blink::WebRTCCertificateGenerator>
  CreateRTCCertificateGenerator() override;
  std::unique_ptr<blink::WebMediaRecorderHandler> CreateMediaRecorderHandler(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  std::unique_ptr<blink::WebMediaStreamCenter> CreateMediaStreamCenter()
      override;
  scoped_refptr<base::SingleThreadTaskRunner> GetWebRtcWorkerThread() override;
  rtc::Thread* GetWebRtcWorkerThreadRtcThread() override;
  std::unique_ptr<cricket::PortAllocator> CreateWebRtcPortAllocator(
      blink::WebLocalFrame* frame) override;
  std::unique_ptr<blink::WebCanvasCaptureHandler> CreateCanvasCaptureHandler(
      const blink::WebSize& size,
      double frame_rate,
      blink::WebMediaStreamTrack* track) override;
  void CreateHTMLVideoElementCapturer(
      blink::WebMediaStream* web_media_stream,
      blink::WebMediaPlayer* web_media_player,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  void CreateHTMLAudioElementCapturer(
      blink::WebMediaStream* web_media_stream,
      blink::WebMediaPlayer* web_media_player) override;
  std::unique_ptr<blink::WebImageCaptureFrameGrabber>
  CreateImageCaptureFrameGrabber() override;
  std::unique_ptr<webrtc::RtpCapabilities> GetRtpSenderCapabilities(
      const blink::WebString& kind) override;
  std::unique_ptr<webrtc::RtpCapabilities> GetRtpReceiverCapabilities(
      const blink::WebString& kind) override;
  void UpdateWebRTCAPICount(blink::WebRTCAPIName api_name) override;
  std::unique_ptr<blink::WebGraphicsContext3DProvider>
  CreateOffscreenGraphicsContext3DProvider(
      const blink::Platform::ContextAttributes& attributes,
      const blink::WebURL& top_document_web_url,
      blink::Platform::GraphicsInfo* gl_info) override;
  std::unique_ptr<blink::WebGraphicsContext3DProvider>
  CreateSharedOffscreenGraphicsContext3DProvider() override;
  std::unique_ptr<blink::WebGraphicsContext3DProvider>
  CreateWebGPUGraphicsContext3DProvider(
      const blink::WebURL& top_document_web_url,
      blink::Platform::GraphicsInfo* gl_info) override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  blink::WebString ConvertIDNToUnicode(const blink::WebString& host) override;
  service_manager::Connector* GetConnector() override;
  blink::InterfaceProvider* GetInterfaceProvider() override;
  void SetDisplayThreadPriority(base::PlatformThreadId thread_id) override;
  blink::BlameContext* GetTopLevelBlameContext() override;
  void RecordRappor(const char* metric,
                    const blink::WebString& sample) override;
  void RecordRapporURL(const char* metric, const blink::WebURL& url) override;
  blink::WebPushProvider* PushProvider() override;
  void DidStartWorkerThread() override;
  void WillStopWorkerThread() override;
  void WorkerContextCreated(const v8::Local<v8::Context>& worker) override;

  // Disables the WebSandboxSupport implementation for testing.
  // Tests that do not set up a full sandbox environment should call
  // SetSandboxEnabledForTesting(false) _before_ creating any instances
  // of this class, to ensure that we don't attempt to use sandbox-related
  // file descriptors or other resources.
  //
  // Returns the previous |enable| value.
  static bool SetSandboxEnabledForTesting(bool enable);

  WebDatabaseObserverImpl* web_database_observer_impl() {
    return web_database_observer_impl_.get();
  }

  std::unique_ptr<blink::WebURLLoaderFactory> CreateDefaultURLLoaderFactory()
      override;
  std::unique_ptr<blink::CodeCacheLoader> CreateCodeCacheLoader() override;

  std::unique_ptr<blink::WebURLLoaderFactory> WrapURLLoaderFactory(
      mojo::ScopedMessagePipeHandle url_loader_factory_handle) override;
  std::unique_ptr<blink::WebURLLoaderFactory> WrapSharedURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> factory) override;
  std::unique_ptr<blink::WebDataConsumerHandle> CreateDataConsumerHandle(
      mojo::ScopedDataPipeConsumerHandle handle) override;
  void RequestPurgeMemory() override;
  void SetMemoryPressureNotificationsSuppressed(bool suppressed) override;

  // Returns non-null.
  // It is invalid to call this in an incomplete env where
  // RenderThreadImpl::current() returns nullptr (e.g. in some tests).
  scoped_refptr<ChildURLLoaderFactoryBundle>
  CreateDefaultURLLoaderFactoryBundle();

  PossiblyAssociatedInterfacePtr<network::mojom::URLLoaderFactory>
  CreateNetworkURLLoaderFactory();

  // Clones the source namespace to the destination namespace.
  void CloneSessionStorageNamespace(const std::string& source_namespace,
                                    const std::string& destination_namespace);

  // Tells this platform that the renderer is locked to a site (i.e., a scheme
  // plus eTLD+1, such as https://google.com), or to a more specific origin.
  void SetIsLockedToSite();

 private:
  bool CheckPreparsedJsCachingEnabled() const;

  // Ensure that the WebDatabaseHost has been initialized.
  void InitializeWebDatabaseHostIfNeeded();

  // Return the mojo interface for making WebDatabaseHost calls.
  blink::mojom::WebDatabaseHost& GetWebDatabaseHost();

  // Return the mojo interface for making CodeCache calls.
  blink::mojom::CodeCacheHost& GetCodeCacheHost();

  std::unique_ptr<service_manager::Connector> connector_;
  scoped_refptr<base::SingleThreadTaskRunner> io_runner_;

#if !defined(OS_ANDROID) && !defined(OS_WIN) && !defined(OS_FUCHSIA)
  class SandboxSupport;
  std::unique_ptr<SandboxSupport> sandbox_support_;
#endif

  // This counter keeps track of the number of times sudden termination is
  // enabled or disabled. It starts at 0 (enabled) and for every disable
  // increments by 1, for every enable decrements by 1. When it reaches 0,
  // we tell the browser to enable fast termination.
  int sudden_termination_disables_;

  // If true, the renderer process is locked to a site.
  bool is_locked_to_site_;

  std::unique_ptr<blink::WebBlobRegistry> blob_registry_;

  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  std::unique_ptr<WebDatabaseObserverImpl> web_database_observer_impl_;

  // NOT OWNED
  blink::scheduler::WebThreadScheduler* main_thread_scheduler_;

  TopLevelBlameContext top_level_blame_context_;

  std::unique_ptr<LocalStorageCachedAreas> local_storage_cached_areas_;

  std::unique_ptr<BlinkInterfaceProviderImpl> blink_interface_provider_;

  blink::mojom::WebDatabaseHostPtrInfo web_database_host_info_;
  scoped_refptr<blink::mojom::ThreadSafeWebDatabaseHostPtr> web_database_host_;

  blink::mojom::CodeCacheHostPtrInfo code_cache_host_info_;
  scoped_refptr<blink::mojom::ThreadSafeCodeCacheHostPtr> code_cache_host_;

#if defined(OS_LINUX)
  sk_sp<font_service::FontLoader> font_loader_;
#endif

  THREAD_CHECKER(main_thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(RendererBlinkPlatformImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDERER_BLINK_PLATFORM_IMPL_H_
