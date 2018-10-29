// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_CONTENT_RENDERER_CLIENT_H_
#define CONTENT_PUBLIC_RENDERER_CONTENT_RENDERER_CLIENT_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "build/build_config.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/url_loader_throttle_provider.h"
#include "content/public/renderer/websocket_handshake_throttle_provider.h"
#include "media/base/decode_capabilities.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "third_party/blink/public/web/web_navigation_type.h"
#include "ui/base/page_transition_types.h"
#include "v8/include/v8.h"

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
class WebMIDIAccessor;
class WebMIDIAccessorClient;
class WebPlugin;
class WebPrescientNetworking;
class WebSpeechSynthesizer;
class WebSpeechSynthesizerClient;
class WebThemeEngine;
class WebURL;
class WebURLRequest;
class WebURLResponse;
struct WebPluginParams;
struct WebURLError;
}  // namespace blink

namespace media {
class KeySystemProperties;
}

namespace content {
class BrowserPluginDelegate;
class MediaStreamRendererFactory;
class RenderFrame;
class RenderView;
struct WebPluginInfo;

// Embedder API for participating in renderer logic.
class CONTENT_EXPORT ContentRendererClient {
 public:
  virtual ~ContentRendererClient() {}

  // Notifies us that the RenderThread has been created.
  virtual void RenderThreadStarted() {}

  // Notifies that a new RenderFrame has been created.
  virtual void RenderFrameCreated(RenderFrame* render_frame) {}

  // Notifies that a new RenderView has been created.
  virtual void RenderViewCreated(RenderView* render_view) {}

  // Returns the bitmap to show when a plugin crashed, or NULL for none.
  virtual SkBitmap* GetSadPluginBitmap();

  // Returns the bitmap to show when a <webview> guest has crashed, or NULL for
  // none.
  virtual SkBitmap* GetSadWebViewBitmap();

  // Returns true if the embedder renders the contents of the |plugin_element|
  // in a cross-process frame using MimeHandlerView.
  virtual bool MaybeCreateMimeHandlerView(
      RenderFrame* embedder_frame,
      const blink::WebElement& plugin_element,
      const GURL& original_url,
      const std::string& original_mime_type,
      int32_t instance_id_to_use);

  // Allows the embedder to override creating a plugin. If it returns true, then
  // |plugin| will contain the created plugin, although it could be NULL. If it
  // returns false, the content layer will create the plugin.
  virtual bool OverrideCreatePlugin(
      RenderFrame* render_frame,
      const blink::WebPluginParams& params,
      blink::WebPlugin** plugin);

  // Creates a replacement plugin that is shown when the plugin at |file_path|
  // couldn't be loaded. This allows the embedder to show a custom placeholder.
  // This may return nullptr. However, if it does return a WebPlugin, it must
  // never fail to initialize.
  virtual blink::WebPlugin* CreatePluginReplacement(
      RenderFrame* render_frame,
      const base::FilePath& plugin_path);

  // Creates a delegate for browser plugin.
  virtual BrowserPluginDelegate* CreateBrowserPluginDelegate(
      RenderFrame* render_frame,
      const WebPluginInfo& info,
      const std::string& mime_type,
      const GURL& original_url);

  // Returns true if the embedder has an error page to show for the given http
  // status code.
  virtual bool HasErrorPage(int http_status_code);

  // Returns true if the embedder prefers not to show an error page for a failed
  // navigation to |url| in |render_frame|.
  virtual bool ShouldSuppressErrorPage(RenderFrame* render_frame,
                                       const GURL& url);

  // Returns false for new tab page activities, which should be filtered out in
  // UseCounter; returns true otherwise.
  virtual bool ShouldTrackUseCounter(const GURL& url);

  // Returns the information to display when a navigation error occurs.
  // If |error_html| is not null then it may be set to a HTML page
  // containing the details of the error and maybe links to more info.
  // If |error_description| is not null it may be set to contain a brief
  // message describing the error that has occurred.
  // Either of the out parameters may be not written to in certain cases
  // (lack of information on the error code) so the caller should take care to
  // initialize the string values with safe defaults before the call.
  virtual void PrepareErrorPage(content::RenderFrame* render_frame,
                                const blink::WebURLRequest& failed_request,
                                const blink::WebURLError& error,
                                std::string* error_html,
                                base::string16* error_description) {}
  virtual void PrepareErrorPageForHttpStatusError(
      content::RenderFrame* render_frame,
      const blink::WebURLRequest& failed_request,
      const GURL& unreachable_url,
      int http_status,
      std::string* error_html,
      base::string16* error_description) {}

  // Returns as |error_description| a brief description of the error that
  // ocurred. The out parameter may be not written to in certain cases (lack of
  // information on the error code)
  virtual void GetErrorDescription(const blink::WebURLRequest& failed_request,
                                   const blink::WebURLError& error,
                                   base::string16* error_description) {}

  // Allows the embedder to control when media resources are loaded. Embedders
  // can run |closure| immediately if they don't wish to defer media resource
  // loading.  If |has_played_media_before| is true, the render frame has
  // previously started media playback (i.e. played audio and video).
  // Returns true if running of |closure| is deferred; false if run immediately.
  virtual bool DeferMediaLoad(RenderFrame* render_frame,
                              bool has_played_media_before,
                              base::OnceClosure closure);

  // Allows the embedder to override creating a WebMIDIAccessor.  If it
  // returns NULL the content layer will create the MIDI accessor.
  virtual std::unique_ptr<blink::WebMIDIAccessor> OverrideCreateMIDIAccessor(
      blink::WebMIDIAccessorClient* client);

  // Allows the embedder to override the WebThemeEngine used. If it returns NULL
  // the content layer will provide an engine.
  virtual blink::WebThemeEngine* OverrideThemeEngine();

  // Allows the embedder to provide a WebSocketHandshakeThrottleProvider. If it
  // returns NULL then none will be used.
  virtual std::unique_ptr<WebSocketHandshakeThrottleProvider>
  CreateWebSocketHandshakeThrottleProvider();

  // Allows the embedder to override the WebSpeechSynthesizer used.
  // If it returns NULL the content layer will provide an engine.
  virtual std::unique_ptr<blink::WebSpeechSynthesizer>
  OverrideSpeechSynthesizer(blink::WebSpeechSynthesizerClient* client);

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

#if defined(OS_ANDROID)
  // TODO(sgurun) This callback is deprecated and will be removed as soon
  // as android webview completes implementation of a resource throttle based
  // shouldoverrideurl implementation. See crbug.com/325351
  //
  // Returns true if the navigation was handled by the embedder and should be
  // ignored by WebKit. This method is used by CEF and android_webview.
  virtual bool HandleNavigation(RenderFrame* render_frame,
                                bool is_content_initiated,
                                bool render_view_was_created_by_renderer,
                                blink::WebFrame* frame,
                                const blink::WebURLRequest& request,
                                blink::WebNavigationType type,
                                blink::WebNavigationPolicy default_policy,
                                bool is_redirect);
#endif

  // Returns true if we should fork a new process for the given navigation.
  virtual bool ShouldFork(blink::WebLocalFrame* frame,
                          const GURL& url,
                          const std::string& http_method,
                          bool is_initial_navigation,
                          bool is_server_redirect);

  // Notifies the embedder that the given frame is requesting the resource at
  // |url|. If the function returns a valid |new_url|, the request must be
  // updated to use it. The |attach_same_site_cookies| output parameter
  // determines whether SameSite cookies should be attached to the request.
  // TODO(nasko): When moved over to Network Service, find a way to perform
  // this check on the browser side, so untrusted renderer processes cannot
  // influence whether SameSite cookies are attached.
  virtual void WillSendRequest(blink::WebLocalFrame* frame,
                               ui::PageTransition transition_type,
                               const blink::WebURL& url,
                               const url::Origin* initiator_origin,
                               GURL* new_url,
                               bool* attach_same_site_cookies);

  // Returns true if the request is associated with a document that is in
  // ""prefetch only" mode, and will not be rendered.
  virtual bool IsPrefetchOnly(RenderFrame* render_frame,
                              const blink::WebURLRequest& request);

  // See blink::Platform.
  virtual unsigned long long VisitedLinkHash(const char* canonical_url,
                                             size_t length);
  virtual bool IsLinkVisited(unsigned long long link_hash);
  virtual blink::WebPrescientNetworking* GetPrescientNetworking();
  virtual bool ShouldOverridePageVisibilityState(
      const RenderFrame* render_frame,
      blink::mojom::PageVisibilityState* override_state);

  // Returns true if the given Pepper plugin is external (requiring special
  // startup steps).
  virtual bool IsExternalPepperPlugin(const std::string& module_name);

  // Returns true if the given Pepper plugin should process content from
  // different origins in different PPAPI processes. This is generally a
  // worthwhile precaution when the plugin provides an active scripting
  // language.
  virtual bool IsOriginIsolatedPepperPlugin(const base::FilePath& plugin_path);

  // Allows an embedder to provide a MediaStreamRendererFactory.
  virtual std::unique_ptr<MediaStreamRendererFactory>
  CreateMediaStreamRendererFactory();

  // Allows embedder to register the key system(s) it supports by populating
  // |key_systems|.
  virtual void AddSupportedKeySystems(
      std::vector<std::unique_ptr<media::KeySystemProperties>>* key_systems);

  // Signal that embedder has changed key systems.
  // TODO(chcunningham): Refactor this to a proper change "observer" API that is
  // less fragile (don't assume AddSupportedKeySystems has just one caller).
  virtual bool IsKeySystemsUpdateNeeded();

  // Allows embedder to describe customized audio capabilities.
  virtual bool IsSupportedAudioConfig(const media::AudioConfig& config);

  // Allows embedder to describe customized video capabilities.
  virtual bool IsSupportedVideoConfig(const media::VideoConfig& config);

  // Return true if the bitstream format |codec| is supported by the audio sink.
  virtual bool IsSupportedBitstreamAudioCodec(media::AudioCodec codec);

  // Returns true if we should report a detailed message (including a stack
  // trace) for console [logs|errors|exceptions]. |source| is the WebKit-
  // reported source for the error; this can point to a page or a script,
  // and can be external or internal.
  virtual bool ShouldReportDetailedMessageForSource(
      const base::string16& source) const;

  // Creates a permission client for in-renderer worker.
  virtual std::unique_ptr<blink::WebContentSettingsClient>
  CreateWorkerContentSettingsClient(RenderFrame* render_frame);

  // Returns true if the page at |url| can use Pepper CameraDevice APIs.
  virtual bool IsPluginAllowedToUseCameraDeviceAPI(const GURL& url);

  // Returns true if the page at |url| can use Pepper Compositor APIs.
  virtual bool IsPluginAllowedToUseCompositorAPI(const GURL& url);

  // Returns true if dev channel APIs are available for plugins.
  virtual bool IsPluginAllowedToUseDevChannelAPIs();

  // Records a sample string to a Rappor privacy-preserving metric.
  // See: https://www.chromium.org/developers/design-documents/rappor
  virtual void RecordRappor(const std::string& metric,
                            const std::string& sample) {}

  // Records a domain and registry of a url to a Rappor privacy-preserving
  // metric. See: https://www.chromium.org/developers/design-documents/rappor
  virtual void RecordRapporURL(const std::string& metric, const GURL& url) {}

  // Gives the embedder a chance to add properties to the context menu.
  // Currently only called when the context menu is for an image.
  virtual void AddImageContextMenuProperties(
      const blink::WebURLResponse& response,
      bool is_image_in_context_a_placeholder_image,
      std::map<std::string, std::string>* properties) {}

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

  // Notifies that a service worker context has been created. This function
  // is called from the worker thread.
  virtual void DidInitializeServiceWorkerContextOnWorkerThread(
      v8::Local<v8::Context> context,
      int64_t service_worker_version_id,
      const GURL& service_worker_scope,
      const GURL& script_url) {}

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

  // Asks the embedder whether to exclude the given header from service worker
  // fetch events. This is useful if the embedder injects headers that it wants
  // to go to network but not to the service worker. This function is called
  // from the worker thread.
  virtual bool IsExcludedHeaderForServiceWorkerFetchEvent(
      const std::string& header_name);

  // Whether this renderer should enforce preferences related to the WebRTC
  // routing logic, i.e. allowing multiple routes and non-proxied UDP.
  virtual bool ShouldEnforceWebRTCRoutingPreferences();

  // Provides a default configuration of WebRTC audio processing, in JSON format
  // with fields corresponding to webrtc::AudioProcessing::Config. Allows for a
  // more functional tuning on platforms with known implementation and hardware
  // limitations.
  // This is currently not supported when running the Chrome audio service.
  virtual base::Optional<std::string>
  WebRTCPlatformSpecificAudioProcessingConfiguration();

  // Notifies that a worker context has been created. This function is called
  // from the worker thread.
  virtual void DidInitializeWorkerContextOnWorkerThread(
      v8::Local<v8::Context> context) {}

  // Overwrites the given URL to use an HTML5 embed if possible.
  // An empty URL is returned if the URL is not overriden.
  virtual GURL OverrideFlashEmbedWithHTML(const GURL& url);

  // Provides parameters for initializing the global task scheduler. Default
  // params are used if this returns nullptr.
  virtual std::unique_ptr<base::TaskScheduler::InitParams>
  GetTaskSchedulerInitParams();

  // Whether the renderer allows idle media players to be automatically
  // suspended after a period of inactivity.
  virtual bool IsIdleMediaSuspendEnabled();

  // Whether the renderer should automatically suspend media playback on
  // background tabs for given |render_frame|.
  virtual bool IsBackgroundMediaSuspendEnabled(RenderFrame* render_frame);

  // Called when a resource at |url| is loaded using an otherwise-valid legacy
  // Symantec certificate that will be distrusted in future. Allows the embedder
  // to override the message that is added to the console to inform developers
  // that their certificate will be distrusted in future. If the method returns
  // true, then |*console_message| will be printed to the console; otherwise a
  // generic mesage will be used.
  virtual bool OverrideLegacySymantecCertConsoleMessage(
      const GURL& url,
      std::string* console_messsage);

  // Asks the embedder to bind |service_request| to its renderer-side service
  // implementation.
  virtual void CreateRendererService(
      service_manager::mojom::ServiceRequest service_request) {}

  virtual std::unique_ptr<URLLoaderThrottleProvider>
  CreateURLLoaderThrottleProvider(URLLoaderThrottleProviderType provider_type);

  // Called when Blink cannot find a frame with the given name in the frame's
  // browsing instance.  This gives the embedder a chance to return a frame
  // from outside of the browsing instance.
  virtual blink::WebFrame* FindFrame(blink::WebLocalFrame* relative_to_frame,
                                     const std::string& name);

  // Returns true if it is safe to redirect to |url|, otherwise returns false.
  virtual bool IsSafeRedirectTarget(const GURL& url);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_CONTENT_RENDERER_CLIENT_H_
