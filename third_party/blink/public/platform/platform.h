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

#ifdef WIN32
#include <windows.h>
#endif

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-shared.h"
#include "third_party/blink/public/platform/blame_context.h"
#include "third_party/blink/public/platform/code_cache_loader.h"
#include "third_party/blink/public/platform/user_metrics_action.h"
#include "third_party/blink/public/platform/web_audio_device.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_data_consumer_handle.h"
#include "third_party/blink/public/platform/web_gesture_device.h"
#include "third_party/blink/public/platform/web_localized_string.h"
#include "third_party/blink/public/platform/web_rtc_api_name.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_speech_synthesizer.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace cricket {
class PortAllocator;
}

namespace gpu {
class GpuMemoryBufferManager;
}

namespace rtc {
class Thread;
}

namespace service_manager {
class Connector;
class InterfaceProvider;
}

namespace v8 {
class Context;
template <class T>
class Local;
}

namespace webrtc {
struct RtpCapabilities;
}

namespace blink {

class InterfaceProvider;
class Thread;
struct ThreadCreationParams;
class WebAudioBus;
class WebAudioLatencyHint;
class WebBlobRegistry;
class WebCanvasCaptureHandler;
class WebCookieJar;
class WebCrypto;
class WebDatabaseObserver;
class WebGraphicsContext3DProvider;
class WebImageCaptureFrameGrabber;
class WebLocalFrame;
class WebMIDIAccessor;
class WebMIDIAccessorClient;
class WebMediaCapabilitiesClient;
class WebMediaPlayer;
class WebMediaRecorderHandler;
class WebMediaStream;
class WebMediaStreamCenter;
class WebMediaStreamTrack;
class WebPrescientNetworking;
class WebPublicSuffixList;
class WebPushProvider;
class WebRTCCertificateGenerator;
class WebRTCPeerConnectionHandler;
class WebRTCPeerConnectionHandlerClient;
class WebSandboxSupport;
class WebSecurityOrigin;
class WebSpeechSynthesizer;
class WebSpeechSynthesizerClient;
class WebStorageNamespace;
class WebThemeEngine;
class WebURLLoaderMockFactory;
class WebURLResponse;
class WebURLResponse;
struct WebSize;

namespace scheduler {
class WebThreadScheduler;
}

class BLINK_PLATFORM_EXPORT Platform {
 public:
// HTML5 Database ------------------------------------------------------

#ifdef WIN32
  typedef HANDLE FileHandle;
#else
  typedef int FileHandle;
#endif

  // Initialize platform and wtf. If you need to initialize the entire Blink,
  // you should use blink::Initialize. WebThreadScheduler must be owned by
  // the embedder.
  static void Initialize(Platform*,
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
  static void SetCurrentPlatformForTesting(Platform*);

  // This sets up a minimally viable implementation of blink::Thread without
  // changing the current Platform. This is essentially a workaround for the
  // initialization order in ScopedUnittestsEnvironmentSetup, and nobody else
  // should use this.
  static void CreateMainThreadForTesting();

  // These are dirty workaround for tests requiring the main thread task runner
  // from a non-main thread. If your test needs base::ScopedTaskEnvironment
  // and a non-main thread may call MainThread()->GetTaskRunner(), call
  // SetMainThreadTaskRunnerForTesting() in your test fixture's SetUp(), and
  // call UnsetMainThreadTaskRunnerForTesting() in TearDown().
  //
  // TODO(yutak): Ideally, these should be packed in a custom test fixture
  // along with ScopedTaskEnvironment for reusability.
  static void SetMainThreadTaskRunnerForTesting();
  static void UnsetMainThreadTaskRunnerForTesting();

  Platform();
  virtual ~Platform();

  // May return null.
  virtual WebCookieJar* CookieJar() { return nullptr; }

  // May return null if sandbox support is not necessary
  virtual WebSandboxSupport* GetSandboxSupport() { return nullptr; }

  // May return null on some platforms.
  virtual WebThemeEngine* ThemeEngine() { return nullptr; }

  // May return null.
  virtual std::unique_ptr<WebSpeechSynthesizer> CreateSpeechSynthesizer(
      WebSpeechSynthesizerClient*) {
    return nullptr;
  }

  // Audio --------------------------------------------------------------

  virtual double AudioHardwareSampleRate() { return 0; }
  virtual size_t AudioHardwareBufferSize() { return 0; }
  virtual unsigned AudioHardwareOutputChannels() { return 0; }

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

  // MIDI ----------------------------------------------------------------

  // Creates a platform dependent WebMIDIAccessor. MIDIAccessor under platform
  // creates and owns it.
  virtual std::unique_ptr<WebMIDIAccessor> CreateMIDIAccessor(
      WebMIDIAccessorClient*);

  // Blob ----------------------------------------------------------------

  // Must return non-null.
  virtual WebBlobRegistry* GetBlobRegistry() { return nullptr; }

  // Database ------------------------------------------------------------

  // Opens a database file.
  virtual FileHandle DatabaseOpenFile(const WebString& vfs_file_name,
                                      int desired_flags) {
    return FileHandle();
  }

  // Deletes a database file and returns the error code.
  virtual int DatabaseDeleteFile(const WebString& vfs_file_name,
                                 bool sync_dir) {
    return 0;
  }

  // Returns the attributes of the given database file.
  virtual long DatabaseGetFileAttributes(const WebString& vfs_file_name) {
    return 0;
  }

  // Returns the size of the given database file.
  virtual long long DatabaseGetFileSize(const WebString& vfs_file_name) {
    return 0;
  }

  // Returns the space available for the given origin.
  virtual long long DatabaseGetSpaceAvailableForOrigin(
      const WebSecurityOrigin& origin) {
    return 0;
  }

  // Set the size of the given database file.
  virtual bool DatabaseSetFileSize(const WebString& vfs_file_name,
                                   long long size) {
    return false;
  }

  // Return a filename-friendly identifier for an origin.
  virtual WebString DatabaseCreateOriginIdentifier(
      const WebSecurityOrigin& origin) {
    return WebString();
  }

  // DOM Storage --------------------------------------------------

  // Return a LocalStorage namespace
  virtual std::unique_ptr<WebStorageNamespace> CreateLocalStorageNamespace();

  // Return a SessionStorage namespace
  virtual std::unique_ptr<WebStorageNamespace> CreateSessionStorageNamespace(
      base::StringPiece namespace_id);

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
  virtual unsigned long long VisitedLinkHash(const char* canonical_url,
                                             size_t length) {
    return 0;
  }

  // Returns whether the given link hash is in the user's history. The
  // hash must have been generated by calling VisitedLinkHash().
  virtual bool IsLinkVisited(unsigned long long link_hash) { return false; }

  static const size_t kNoDecodedImageByteLimit = static_cast<size_t>(-1);

  // Returns the maximum amount of memory a decoded image should be allowed.
  // See comments on ImageDecoder::max_decoded_bytes_.
  virtual size_t MaxDecodedImageBytes() { return kNoDecodedImageByteLimit; }

  // Returns true if this is a low-end device.
  // This is the same as base::SysInfo::IsLowEndDevice.
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

  // Returns the platform's default URLLoaderFactory. It is expected that the
  // returned value is stored and to be used for all the CreateURLLoader
  // requests for the same loading context.
  //
  // WARNING: This factory understands http(s) and blob URLs, but it does not
  // understand URLs like chrome-extension:// and file:// as those are provided
  // by the browser process on a per-frame or per-worker basis. If you require
  // support for such URLs, you must add that support manually. Typically you
  // get a factory bundle from the browser process, and compose a new factory
  // using both the bundle and this default.
  //
  // TODO(kinuko): See if we can deprecate this too.
  virtual std::unique_ptr<WebURLLoaderFactory> CreateDefaultURLLoaderFactory() {
    return nullptr;
  }

  // Returns the CodeCacheLoader that is used to fetch data from code caches.
  // It is OK to return a nullptr. When a nullptr is returned, data would not
  // be fetched from code cache.
  virtual std::unique_ptr<CodeCacheLoader> CreateCodeCacheLoader() {
    return nullptr;
  }

  // Returns a new WebURLLoaderFactory that wraps the given
  // network::mojom::URLLoaderFactory.
  virtual std::unique_ptr<WebURLLoaderFactory> WrapURLLoaderFactory(
      mojo::ScopedMessagePipeHandle url_loader_factory_handle) {
    return nullptr;
  }

  // Returns a new WebURLLoaderFactory that wraps the given
  // network::SharedURLLoaderFactory.
  virtual std::unique_ptr<blink::WebURLLoaderFactory>
  WrapSharedURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> factory) {
    return nullptr;
  }

  // Returns a WebDataConsumerHandle for a given mojo data pipe endpoint.
  virtual std::unique_ptr<WebDataConsumerHandle> CreateDataConsumerHandle(
      mojo::ScopedDataPipeConsumerHandle handle) {
    return nullptr;
  }

  // May return null.
  virtual WebPrescientNetworking* PrescientNetworking() { return nullptr; }

  // Returns the User-Agent string.
  virtual WebString UserAgent() { return WebString(); }

  // A suggestion to cache this metadata in association with this URL.
  virtual void CacheMetadata(blink::mojom::CodeCacheType cache_type,
                             const WebURL&,
                             base::Time response_time,
                             const char* data,
                             size_t data_size) {}

  // A request to fetch contents associated with this URL from metadata cache.
  virtual void FetchCachedCode(
      blink::mojom::CodeCacheType cache_type,
      const GURL&,
      base::OnceCallback<void(base::Time, const std::vector<uint8_t>&)>) {}
  virtual void ClearCodeCacheEntry(blink::mojom::CodeCacheType cache_type,
                                   const GURL&) {}

  // A suggestion to cache this metadata in association with this URL which
  // resource is in CacheStorage.
  virtual void CacheMetadataInCacheStorage(
      const WebURL&,
      base::Time response_time,
      const char* data,
      size_t data_size,
      const blink::WebSecurityOrigin& cache_storage_origin,
      const WebString& cache_storage_cache_name) {}

  // Public Suffix List --------------------------------------------------

  // May return null on some platforms.
  virtual WebPublicSuffixList* PublicSuffixList() { return nullptr; }

  // Resources -----------------------------------------------------------

  // Returns a localized string resource (with substitution parameters).
  virtual WebString QueryLocalizedString(WebLocalizedString::Name) {
    return WebString();
  }
  virtual WebString QueryLocalizedString(WebLocalizedString::Name,
                                         const WebString& parameter) {
    return WebString();
  }
  virtual WebString QueryLocalizedString(WebLocalizedString::Name,
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

  // DEPRECATED: Use Thread::CreateWebAudioThread() instead.
  std::unique_ptr<Thread> CreateWebAudioThread();

  // DEPRECATED: Use Thread::Current() instead.
  Thread* CurrentThread();

  // DEPRECATED: Use Thread::MainThread() instead.
  Thread* MainThread();

  // DEPRECATED: Use Thread::CompositorThread() instead.
  Thread* CompositorThread();

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

  // Returns a blob of data corresponding to the named resource.
  virtual WebData GetDataResource(const char* name) { return WebData(); }

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
  // WebLocalFrameClient::SuddenTerminationDisablerChanged.
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

  // Gets a pointer to URLLoaderMockFactory for testing. Will not be available
  // in production builds.
  // TODO(kinuko,toyoshim): Deprecate this one. (crbug.com/751425)
  virtual WebURLLoaderMockFactory* GetURLLoaderMockFactory() { return nullptr; }

  // Record to a RAPPOR privacy-preserving metric, see:
  // https://www.chromium.org/developers/design-documents/rappor.
  // RecordRappor records a sample string, while RecordRapporURL records the
  // eTLD+1 of a url.
  virtual void RecordRappor(const char* metric, const WebString& sample) {}
  virtual void RecordRapporURL(const char* metric, const blink::WebURL& url) {}

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
  CreateOffscreenGraphicsContext3DProvider(
      const ContextAttributes&,
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
  CreateWebGPUGraphicsContext3DProvider(const WebURL& top_document_url,
                                        GraphicsInfo*);

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

  // WebRTC ----------------------------------------------------------

  // Creates a WebRTCPeerConnectionHandler for RTCPeerConnection.
  // May return null if WebRTC functionality is not avaliable or if it's out of
  // resources.
  virtual std::unique_ptr<WebRTCPeerConnectionHandler>
  CreateRTCPeerConnectionHandler(WebRTCPeerConnectionHandlerClient*,
                                 scoped_refptr<base::SingleThreadTaskRunner>);

  // Creates a WebMediaRecorderHandler to record MediaStreams.
  // May return null if the functionality is not available or out of resources.
  virtual std::unique_ptr<WebMediaRecorderHandler> CreateMediaRecorderHandler(
      scoped_refptr<base::SingleThreadTaskRunner>);

  // May return null if WebRTC functionality is not available or out of
  // resources.
  virtual std::unique_ptr<WebRTCCertificateGenerator>
  CreateRTCCertificateGenerator();

  // May return null if WebRTC functionality is not available or out of
  // resources.
  virtual std::unique_ptr<WebMediaStreamCenter> CreateMediaStreamCenter();

  // Returns the SingleThreadTaskRunner suitable for running WebRTC networking.
  // An rtc::Thread will have already been created.
  // May return null if WebRTC functionality is not implemented.
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetWebRtcWorkerThread() {
    return nullptr;
  }

  // Returns the rtc::Thread instance associated with the WebRTC worker thread.
  // TODO(bugs.webrtc.org/9419): Remove once WebRTC can be built as a component.
  // May return null if WebRTC functionality is not implemented.
  virtual rtc::Thread* GetWebRtcWorkerThreadRtcThread() { return nullptr; }

  // May return null if WebRTC functionality is not implemented.
  virtual std::unique_ptr<cricket::PortAllocator> CreateWebRtcPortAllocator(
      WebLocalFrame* frame);

  // Creates a WebCanvasCaptureHandler to capture Canvas output.
  virtual std::unique_ptr<WebCanvasCaptureHandler>
  CreateCanvasCaptureHandler(const WebSize&, double, WebMediaStreamTrack*);

  // Fills in the WebMediaStream to capture from the WebMediaPlayer identified
  // by the second parameter.
  virtual void CreateHTMLVideoElementCapturer(
      WebMediaStream*,
      WebMediaPlayer*,
      scoped_refptr<base::SingleThreadTaskRunner>) {}
  virtual void CreateHTMLAudioElementCapturer(WebMediaStream*,
                                              WebMediaPlayer*) {}

  // Creates a WebImageCaptureFrameGrabber to take a snapshot of a Video Tracks.
  // May return null if the functionality is not available.
  virtual std::unique_ptr<WebImageCaptureFrameGrabber>
  CreateImageCaptureFrameGrabber();

  // Returns the most optimistic view of the capabilities of the system for
  // sending or receiving media of the given kind ("audio" or "video").
  virtual std::unique_ptr<webrtc::RtpCapabilities> GetRtpSenderCapabilities(
      const WebString& kind);
  virtual std::unique_ptr<webrtc::RtpCapabilities> GetRtpReceiverCapabilities(
      const WebString& kind);

  virtual void UpdateWebRTCAPICount(WebRTCAPIName api_name) {}

  // WebWorker ----------------------------------------------------------

  virtual void DidStartWorkerThread() {}
  virtual void WillStopWorkerThread() {}
  virtual void WorkerContextCreated(const v8::Local<v8::Context>& worker) {}
  virtual bool AllowScriptExtensionForServiceWorker(const WebURL& script_url) {
    return false;
  }

  // WebCrypto ----------------------------------------------------------

  virtual WebCrypto* Crypto() { return nullptr; }

  // Mojo ---------------------------------------------------------------

  virtual service_manager::Connector* GetConnector();

  virtual InterfaceProvider* GetInterfaceProvider();

  virtual const char* GetBrowserServiceName() const { return ""; }

  // This method converts from the supplied DOM code enum to the
  // embedder's DOM code value for the key pressed. |dom_code| values are
  // based on the value defined in
  // ui/events/keycodes/dom4/keycode_converter_data.h.
  // Returns null string, if DOM code value is not found.
  virtual WebString DomCodeStringFromEnum(int dom_code) { return WebString(); }

  // This method converts from the suppled DOM code value to the
  // embedder's DOM code enum for the key pressed. |code_string| is defined in
  // ui/events/keycodes/dom4/keycode_converter_data.h.
  // Returns 0, if DOM code enum is not found.
  virtual int DomEnumFromCodeString(const WebString& code_string) { return 0; }

  // This method converts from the supplied DOM |key| enum to the
  // corresponding DOM |key| string value for the key pressed. |dom_key| values
  // are based on the value defined in ui/events/keycodes/dom3/dom_key_data.h.
  // Returns empty string, if DOM key value is not found.
  virtual WebString DomKeyStringFromEnum(int dom_key) { return WebString(); }

  // This method converts from the suppled DOM |key| value to the
  // embedder's DOM |key| enum for the key pressed. |key_string| is defined in
  // ui/events/keycodes/dom3/dom_key_data.h.
  // Returns 0 if DOM key enum is not found.
  virtual int DomKeyEnumFromString(const WebString& key_string) { return 0; }

  // This method returns whether the specified |dom_key| is a modifier key.
  // |dom_key| values are based on the value defined in
  // ui/events/keycodes/dom3/dom_key_data.h.
  virtual bool IsDomKeyForModifier(int dom_key) { return false; }

  // WebDatabase --------------------------------------------------------

  virtual WebDatabaseObserver* DatabaseObserver() { return nullptr; }

  // Push API------------------------------------------------------------

  virtual WebPushProvider* PushProvider() { return nullptr; }

  // Media Capabilities --------------------------------------------------

  virtual WebMediaCapabilitiesClient* MediaCapabilitiesClient() {
    return nullptr;
  }

  // Memory ------------------------------------------------------------

  // Requests purging memory. The platform may or may not purge memory,
  // depending on memory pressure.
  virtual void RequestPurgeMemory() {}

  virtual void SetMemoryPressureNotificationsSuppressed(bool suppressed) {}

  // V8 Context Snapshot --------------------------------------------------

  // This method returns true only when
  // tools/v8_context_snapshot/v8_context_snapshot_generator is running (which
  // runs during Chromium's build step).
  virtual bool IsTakingV8ContextSnapshot() { return false; }

 private:
  static void InitializeCommon(Platform* platform,
                               std::unique_ptr<Thread> main_thread);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_PLATFORM_H_
