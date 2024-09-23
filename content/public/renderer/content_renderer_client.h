// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_CONTENT_RENDERER_CLIENT_H_
#define CONTENT_PUBLIC_RENDERER_CONTENT_RENDERER_CLIENT_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "content/common/content_export.h"
#include "content/public/common/alternative_error_page_override_info.mojom.h"
#include "content/public/common/content_client.h"
#include "media/base/audio_parameters.h"
#include "media/base/key_system_info.h"
#include "media/base/key_systems_support_registration.h"
#include "media/base/supported_types.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/url_loader_throttle_provider.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle_provider.h"
#include "third_party/blink/public/web/web_link_preview_triggerer.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "third_party/blink/public/web/web_navigation_type.h"
#include "ui/base/page_transition_types.h"
#include "v8/include/v8-forward.h"

#if !BUILDFLAG(IS_ANDROID)
#include "media/base/speech_recognition_client.h"
#endif

class GURL;
class SkBitmap;

namespace base {
class FilePath;
class SingleThreadTaskRunner;
}

namespace blink {
class WebElement;
class WebFrame;
class WebLocalFrame;
class WebPlugin;
class WebPrescientNetworking;
class WebServiceWorkerContextProxy;
class WebURL;
class WebURLRequest;
class WebView;
struct WebContentSecurityPolicyHeader;
struct WebPluginParams;
struct WebURLError;
enum class ProtocolHandlerSecurityLevel;
}  // namespace blink

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
namespace cast_streaming {
class ResourceProvider;
}  // namespace cast_streaming
#endif

namespace media {
class DecoderFactory;
class Demuxer;
class ExternalMemoryAllocator;
class GpuVideoAcceleratorFactories;
class MediaLog;
class RendererFactory;
}

namespace mojo {
class BinderMap;
}

namespace url {
class Origin;
}

namespace content {
class RenderFrame;

// Embedder API for participating in renderer logic.
class CONTENT_EXPORT ContentRendererClient {
 public:
  virtual ~ContentRendererClient() {}

  // Notifies us that the RenderThread has been created.
  virtual void RenderThreadStarted() {}

  // Allows the embedder to make Mojo interfaces available to the browser
  // process. Binders can be added to |*binders| to service incoming interface
  // binding requests from RenderProcessHost::BindReceiver().
  virtual void ExposeInterfacesToBrowser(mojo::BinderMap* binders) {}

  // Sets up trap handling for WebAssembly. Default implementation assumes that
  // a crash handler (such as crashpad) is already in use.
  virtual void SetUpWebAssemblyTrapHandler();

  // Notifies that a new RenderFrame has been created.
  virtual void RenderFrameCreated(RenderFrame* render_frame) {}

  // Notifies that a new WebView has been created.
  // `outermost_origin` is only set if it is an extension scheme, otherwise
  // it will be null. It is null to avoid leaking unnecessary information into
  // the renderer.
  virtual void WebViewCreated(blink::WebView* web_view,
                              bool was_created_by_renderer,
                              const url::Origin* outermost_origin) {}

  // Returns the bitmap to show when a plugin crashed, or NULL for none.
  virtual SkBitmap* GetSadPluginBitmap();

  // Returns the bitmap to show when a <webview> guest has crashed, or NULL for
  // none.
  virtual SkBitmap* GetSadWebViewBitmap();

  // Returns true if the embedder renders the contents of the |plugin_element|,
  // using external handlers, in a cross-process frame.
  virtual bool IsPluginHandledExternally(
      RenderFrame* embedder_frame,
      const blink::WebElement& plugin_element,
      const GURL& original_url,
      const std::string& original_mime_type);

  // Specifies whether to disable DOM storage interfaces such as localStorage
  // and sessionStorage.
  virtual bool IsDomStorageDisabled() const;

  // Returns a scriptable object which implements custom javascript API for the
  // given element. This is used for external plugin handlers for providing
  // custom API such as|postMessage| for <embed> and <object>.
  virtual v8::Local<v8::Object> GetScriptableObject(
      const blink::WebElement& plugin_element,
      v8::Isolate* isolate);

  // Allows the embedder to override creating a plugin. If it returns true,
  // then |plugin| will contain the created plugin, although it could be
  // NULL. If it returns false, the content layer will create the plugin.
  virtual bool OverrideCreatePlugin(RenderFrame* render_frame,
                                    const blink::WebPluginParams& params,
                                    blink::WebPlugin** plugin);

  // Creates a replacement plugin that is shown when the plugin at |file_path|
  // couldn't be loaded. This allows the embedder to show a custom placeholder.
  // This may return nullptr. However, if it does return a WebPlugin, it must
  // never fail to initialize.
  virtual blink::WebPlugin* CreatePluginReplacement(
      RenderFrame* render_frame,
      const base::FilePath& plugin_path);

  // Returns the information to display when a navigation error occurs.
  // |error_html| should be set to null if this is a custom error page that will
  // set its own html content, otherwise if |error_html| is not null then it may
  // be set to a HTML page containing the details of the error and maybe links
  // to more info. Note that |error_html| may be not written to in certain cases
  // (lack of information on the error code) so the caller should take care to
  // initialize it with a safe default before the call. |info| contains PWA
  // information used to customise error page, and is set to null if
  // the webpage that goes offline is not within the scope of a PWA.

  virtual void PrepareErrorPage(
      content::RenderFrame* render_frame,
      const blink::WebURLError& error,
      const std::string& http_method,
      mojom::AlternativeErrorPageOverrideInfoPtr alternative_error_page_info,
      std::string* error_html) {}

  virtual void PrepareErrorPageForHttpStatusError(
      content::RenderFrame* render_frame,
      const blink::WebURLError& error,
      const std::string& http_method,
      int http_status,
      mojom::AlternativeErrorPageOverrideInfoPtr alternative_error_page_info,
      std::string* error_html);

  // Allows the embedder to control when media resources are loaded. Embedders
  // can run |closure| immediately if they don't wish to defer media resource
  // loading.  If |has_played_media_before| is true, the render frame has
  // previously started media playback (i.e. played audio and video).
  // Returns true if running of |closure| is deferred; false if run immediately.
  virtual bool DeferMediaLoad(RenderFrame* render_frame,
                              bool has_played_media_before,
                              base::OnceClosure closure);

  // Allows the embedder to override the Demuxer used for certain URLs.
  // If a non-null value is returned, the object will be used as the source of
  // media data by the media player instance for which this method was called.
  virtual std::unique_ptr<media::Demuxer> OverrideDemuxerForUrl(
      RenderFrame* render_frame,
      const GURL& url,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Allows the embedder to provide a WebSocketHandshakeThrottleProvider. If it
  // returns NULL then none will be used.
  virtual std::unique_ptr<blink::WebSocketHandshakeThrottleProvider>
  CreateWebSocketHandshakeThrottleProvider();

  // Allows the embedder to control whether the renderer should leverage the
  // compiled code cache with hashing for a given `request_url`.
  virtual bool ShouldUseCodeCacheWithHashing(
      const blink::WebURL& request_url) const;

  // Called immediately after the sandbox is initialized on the main thread.
  // (If the renderer is run with --no-sandbox, it is still called in
  // RendererMain at about the same time.)
  virtual void PostSandboxInitialized() {}

  // Called on the main-thread immediately after the io thread is
  // created.
  virtual void PostIOThreadCreated(
      base::SingleThreadTaskRunner* io_thread_task_runner);

  // Called on the main-thread immediately after the compositor thread is
  // created.
  virtual void PostCompositorThreadCreated(
      base::SingleThreadTaskRunner* compositor_thread_task_runner);

  // Returns true if the renderer process should schedule the idle handler when
  // all widgets are hidden.
  virtual bool RunIdleHandlerWhenWidgetsHidden();

  // Returns true if a popup window should be allowed.
  virtual bool AllowPopup();

  // Service worker may react on the activity. For example, reset the idle
  // timer.
  virtual bool ShouldNotifyServiceWorkerOnWebSocketActivity(
      v8::Local<v8::Context> context);

  // Returns the security level to use for Navigator.RegisterProtocolHandler().
  virtual blink::ProtocolHandlerSecurityLevel GetProtocolHandlerSecurityLevel(
      const url::Origin& origin);

#if BUILDFLAG(IS_ANDROID)
  // TODO(sgurun) This callback is deprecated and will be removed as soon
  // as android webview completes implementation of a resource throttle based
  // shouldoverrideurl implementation. See crbug.com/325351
  //
  // Returns true if the navigation was handled by the embedder and should be
  // ignored by WebKit. This method is used by CEF and android_webview.
  virtual bool HandleNavigation(RenderFrame* render_frame,
                                blink::WebFrame* frame,
                                const blink::WebURLRequest& request,
                                blink::WebNavigationType type,
                                blink::WebNavigationPolicy default_policy,
                                bool is_redirect);
#endif

  // Notifies the embedder that the given frame is requesting the resource at
  // `target_url`. If the function returns a valid `new_url`, the request must
  // be updated to use it.
  //
  // `upstream_url`: URL of the frame that initiated the request.
  // `target_url`: URL being requested by `upstream_url`.
  // `site_for_cookies`: Approximately the URL of the request of the main
  // frame. It is empty in the case of cross-site iframes.
  virtual void WillSendRequest(blink::WebLocalFrame* frame,
                               ui::PageTransition transition_type,
                               const blink::WebURL& upstream_url,
                               const blink::WebURL& target_url,
                               const net::SiteForCookies& site_for_cookies,
                               const url::Origin* initiator_origin,
                               GURL* new_url);

  // Returns true if the render frame is used for NoStatePrefetch and will not
  // be rendered.
  virtual bool IsPrefetchOnly(RenderFrame* render_frame);

  // See blink::Platform.
  virtual uint64_t VisitedLinkHash(std::string_view canonical_url);
  virtual uint64_t PartitionedVisitedLinkFingerprint(
      std::string_view canonical_link_url,
      const net::SchemefulSite& top_level_site,
      const url::Origin& frame_origin);
  virtual bool IsLinkVisited(uint64_t link_hash);
  virtual void AddOrUpdateVisitedLinkSalt(const url::Origin& origin,
                                          uint64_t salt);

  // Creates a WebPrescientNetworking instance for |render_frame|. The returned
  // instance is owned by the frame. May return null.
  virtual std::unique_ptr<blink::WebPrescientNetworking>
  CreatePrescientNetworking(RenderFrame* render_frame);

  // Returns true if the given Pepper plugin is external (requiring special
  // startup steps).
  virtual bool IsExternalPepperPlugin(const std::string& module_name);

  // Returns true if the given Pepper plugin should process content from
  // different origins in different PPAPI processes. This is generally a
  // worthwhile precaution when the plugin provides an active scripting
  // language.
  virtual bool IsOriginIsolatedPepperPlugin(const base::FilePath& plugin_path);

  // Allows embedder to register the key system(s) it supports.
  virtual std::unique_ptr<media::KeySystemSupportRegistration>
  GetSupportedKeySystems(RenderFrame* render_frame,
                         media::GetSupportedKeySystemsCB cb);

  // Allows embedder to describe customized audio capabilities.
  virtual bool IsSupportedAudioType(const media::AudioType& type);

  // Allows embedder to describe customized video capabilities.
  virtual bool IsSupportedVideoType(const media::VideoType& type);

  // Return true if the bitstream format |codec| is supported by the audio sink.
  virtual bool IsSupportedBitstreamAudioCodec(media::AudioCodec codec);

  // Returns custom allocator if exists, else nullptr
  // Allocator will live as long as ContentRendererClient.
  virtual media::ExternalMemoryAllocator* GetMediaAllocator();

  // Returns true if we should report a detailed message (including a stack
  // trace) for console [logs|errors|exceptions]. |source| is the WebKit-
  // reported source for the error; this can point to a page or a script,
  // and can be external or internal.
  virtual bool ShouldReportDetailedMessageForSource(
      const std::u16string& source);

  // Creates a permission client for in-renderer worker.
  virtual std::unique_ptr<blink::WebContentSettingsClient>
  CreateWorkerContentSettingsClient(RenderFrame* render_frame);

#if !BUILDFLAG(IS_ANDROID)
  // Creates a speech recognition client used to transcribe audio into captions.
  virtual std::unique_ptr<media::SpeechRecognitionClient>
  CreateSpeechRecognitionClient(RenderFrame* render_frame);
#endif

  // Returns true if the page at |url| can use Pepper CameraDevice APIs.
  virtual bool IsPluginAllowedToUseCameraDeviceAPI(const GURL& url);

  // Notifies that a document element has been inserted in the frame's document.
  // This may be called multiple times for the same document. This method may
  // invalidate the frame.
  virtual void RunScriptsAtDocumentStart(RenderFrame* render_frame) {}

  // Notifies that the DOM is ready in the frame's document.
  // This method may invalidate the frame.
  virtual void RunScriptsAtDocumentEnd(RenderFrame* render_frame) {}

  // Notifies that the window.onload event is about to fire.
  // This method may invalidate the frame.
  virtual void RunScriptsAtDocumentIdle(RenderFrame* render_frame) {}

  // Allows subclasses to enable some runtime features before Blink has
  // started.
  virtual void SetRuntimeFeaturesDefaultsBeforeBlinkInitialization() {}

  // Returns whether or not V8 script extensions should be allowed for a
  // service worker.
  virtual bool AllowScriptExtensionForServiceWorker(
      const url::Origin& script_origin);

  // Notifies that a service worker context is going to be initialized. No
  // meaningful task has run on the worker thread at this point. This
  // function is called from the worker thread.
  virtual void WillInitializeServiceWorkerContextOnWorkerThread() {}

  // Notifies that a service worker context has been created. This function is
  // called from the worker thread.
  // |context_proxy| is valid until
  // WillDestroyServiceWorkerContextOnWorkerThread() is called.
  virtual void DidInitializeServiceWorkerContextOnWorkerThread(
      blink::WebServiceWorkerContextProxy* context_proxy,
      const GURL& service_worker_scope,
      const GURL& script_url) {}

  // Notifies that the main script of a service worker is about to evaluate.
  // This function is called from the worker thread.
  // |context_proxy| is valid until
  // WillDestroyServiceWorkerContextOnWorkerThread() is called.
  virtual void WillEvaluateServiceWorkerOnWorkerThread(
      blink::WebServiceWorkerContextProxy* context_proxy,
      v8::Local<v8::Context> v8_context,
      int64_t service_worker_version_id,
      const GURL& service_worker_scope,
      const GURL& script_url,
      const blink::ServiceWorkerToken& service_worker_token) {}

  // Notifies that a service worker context has finished executing its top-level
  // JavaScript. This function is called from the worker thread.
  virtual void DidStartServiceWorkerContextOnWorkerThread(
      int64_t service_worker_version_id,
      const GURL& service_worker_scope,
      const GURL& script_url) {}

  // Notifies that a service worker context will be destroyed. This function
  // is called from the worker thread.
  virtual void WillDestroyServiceWorkerContextOnWorkerThread(
      v8::Local<v8::Context> context,
      int64_t service_worker_version_id,
      const GURL& service_worker_scope,
      const GURL& script_url) {}

  // Whether this renderer should enforce preferences related to the WebRTC
  // routing logic, i.e. allowing multiple routes and non-proxied UDP.
  virtual bool ShouldEnforceWebRTCRoutingPreferences();

  // Notifies that a worker context has been created. This function is called
  // from the worker thread.
  virtual void DidInitializeWorkerContextOnWorkerThread(
      v8::Local<v8::Context> context) {}

  // Overwrites the given URL to use an HTML5 embed if possible.
  // An empty URL is returned if the URL is not overriden.
  virtual GURL OverrideFlashEmbedWithHTML(const GURL& url);

  // Whether the renderer allows idle media players to be automatically
  // suspended after a period of inactivity.
  virtual bool IsIdleMediaSuspendEnabled();

  // Allows the embedder to return a (possibly null)
  // blink::URLLoaderThrottleProvider for a frame or worker. For frames this is
  // called on the main thread, and for workers it's called on the main or
  // worker threads depending on http://crbug.com/692909.
  virtual std::unique_ptr<blink::URLLoaderThrottleProvider>
  CreateURLLoaderThrottleProvider(
      blink::URLLoaderThrottleProviderType provider_type);

  // Called when Blink cannot find a frame with the given name in the frame's
  // browsing instance.  This gives the embedder a chance to return a frame
  // from outside of the browsing instance.
  virtual blink::WebFrame* FindFrame(blink::WebLocalFrame* relative_to_frame,
                                     const std::string& name);

  // Returns true only if it's safe to redirect `from_url` to `to_url`.
  virtual bool IsSafeRedirectTarget(const GURL& from_url, const GURL& to_url);

  // The user agent string is given from the browser process. This is called at
  // most once.
  virtual void DidSetUserAgent(const std::string& user_agent);

  // Optionally returns audio renderer algorithm parameters.
  virtual std::optional<::media::AudioRendererAlgorithmParameters>
  GetAudioRendererAlgorithmParameters(
      ::media::AudioParameters audio_parameters);

  // Appends to `csp`, the default CSP which should be applied to the given
  // `url`. This allows the embedder to customize the applied CSP.
  virtual void AppendContentSecurityPolicy(
      const blink::WebURL& url,
      blink::WebVector<blink::WebContentSecurityPolicyHeader>* csp);

  // Returns a RendererFactory to use as the "base" for a
  // RendererFactorySelector. Returns `nullptr` to get the default behaviour.
  // The arguments will outlive the returned factory.
  virtual std::unique_ptr<media::RendererFactory> GetBaseRendererFactory(
      content::RenderFrame* render_frame,
      media::MediaLog* media_log,
      media::DecoderFactory* decoder_factory,
      base::RepeatingCallback<media::GpuVideoAcceleratorFactories*()>
          get_gpu_factories_cb,
      int element_id);

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
  // Creates a new cast_streaming::ResourceProvider. Will only be called once
  // per RenderFrame.
  virtual std::unique_ptr<cast_streaming::ResourceProvider>
  CreateCastStreamingResourceProvider();
#endif

  // Creates a WebLinkPreviewTriggerer if an embedder wants to observe events
  // and trigger preview. It is allowed to return nullptr.
  //
  // See blink::WebLinkPreviewTriggerer for more details.
  virtual std::unique_ptr<blink::WebLinkPreviewTriggerer>
  CreateLinkPreviewTriggerer();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_CONTENT_RENDERER_CLIENT_H_
