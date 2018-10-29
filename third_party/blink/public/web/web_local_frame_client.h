/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_LOCAL_FRAME_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_LOCAL_FRAME_CLIENT_H_

#include <memory>
#include <vector>

#include "base/unguessable_token.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/frame/sandbox_flags.h"
#include "third_party/blink/public/common/frame/user_activation_update_type.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-shared.h"
#include "third_party/blink/public/platform/blame_context.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_provider.h"
#include "third_party/blink/public/platform/web_application_cache_host.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_content_security_policy.h"
#include "third_party/blink/public/platform/web_content_security_policy_struct.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/public/platform/web_file_system_type.h"
#include "third_party/blink/public/platform/web_insecure_request_policy.h"
#include "third_party/blink/public/platform/web_loading_behavior_flag.h"
#include "third_party/blink/public/platform/web_scroll_types.h"
#include "third_party/blink/public/platform/web_set_sink_id_callbacks.h"
#include "third_party/blink/public/platform/web_source_location.h"
#include "third_party/blink/public/platform/web_sudden_termination_disabler_type.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_dom_message_event.h"
#include "third_party/blink/public/web/web_file_chooser_params.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_frame_owner_properties.h"
#include "third_party/blink/public/web/web_global_object_reuse_policy.h"
#include "third_party/blink/public/web/web_history_commit_type.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_icon_url.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "third_party/blink/public/web/web_navigation_type.h"
#include "third_party/blink/public/web/web_text_direction.h"
#include "third_party/blink/public/web/web_triggering_event_info.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "v8/include/v8.h"

namespace service_manager {
class InterfaceProvider;
}

namespace blink {
namespace mojom {
enum class WebFeature : int32_t;
}  // namespace mojom

enum class WebTreeScopeType;
class AssociatedInterfaceProvider;
class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
class WebComputedAXTree;
class WebContentDecryptionModule;
class WebCookieJar;
class WebDocumentLoader;
class WebEncryptedMediaClient;
class WebExternalPopupMenu;
class WebExternalPopupMenuClient;
class WebFileChooserCompletion;
class WebLayerTreeView;
class WebLocalFrame;
class WebMediaPlayer;
class WebMediaPlayerClient;
class WebMediaPlayerEncryptedMediaClient;
class WebMediaPlayerSource;
class WebServiceWorkerProvider;
class WebPlugin;
class WebPushClient;
class WebRTCPeerConnectionHandler;
class WebRelatedAppsFetcher;
class WebSocketHandshakeThrottle;
class WebString;
class WebURL;
class WebURLResponse;
class WebUserMediaClient;
struct WebConsoleMessage;
struct WebContextMenuData;
struct WebFullscreenOptions;
struct WebPluginParams;
struct WebPopupMenuInfo;
struct WebRect;
struct WebResourceTimingInfo;
struct WebScrollIntoViewParams;
struct WebURLError;

class BLINK_EXPORT WebLocalFrameClient {
 public:
  virtual ~WebLocalFrameClient() = default;

  // Initialization ------------------------------------------------------
  // Called exactly once during construction to notify the client about the
  // created WebLocalFrame. Guaranteed to be invoked before any other
  // WebLocalFrameClient callbacks.
  virtual void BindToFrame(WebLocalFrame*) {}

  // Factory methods -----------------------------------------------------

  // May return null.
  virtual WebPlugin* CreatePlugin(const WebPluginParams&) { return nullptr; }

  // May return null.
  // WebContentDecryptionModule* may be null if one has not yet been set.
  virtual WebMediaPlayer* CreateMediaPlayer(const WebMediaPlayerSource&,
                                            WebMediaPlayerClient*,
                                            WebMediaPlayerEncryptedMediaClient*,
                                            WebContentDecryptionModule*,
                                            const WebString& sink_id,
                                            WebLayerTreeView*) {
    return nullptr;
  }

  // May return null.
  virtual std::unique_ptr<WebApplicationCacheHost> CreateApplicationCacheHost(
      WebApplicationCacheHostClient*) {
    return nullptr;
  }

  // May return null.
  virtual std::unique_ptr<WebServiceWorkerProvider>
  CreateServiceWorkerProvider() {
    return nullptr;
  }

  // May return null.
  virtual std::unique_ptr<WebContentSettingsClient>
  CreateWorkerContentSettingsClient() {
    return nullptr;
  }

  // Returns a new WebWorkerFetchContext for a dedicated worker. Ownership of
  // the returned object is transferred to the caller.
  virtual std::unique_ptr<WebWorkerFetchContext> CreateWorkerFetchContext() {
    return nullptr;
  }

  // Create a new WebPopupMenu. In the "createExternalPopupMenu" form, the
  // client is responsible for rendering the contents of the popup menu.
  virtual WebExternalPopupMenu* CreateExternalPopupMenu(
      const WebPopupMenuInfo&,
      WebExternalPopupMenuClient*) {
    return nullptr;
  }

  // Services ------------------------------------------------------------

  // A frame specific cookie jar.  May return null, in which case
  // WebKitPlatformSupport::cookieJar() will be called to access cookies.
  virtual WebCookieJar* CookieJar() { return nullptr; }

  // Returns a blame context for attributing work belonging to this frame.
  virtual BlameContext* GetFrameBlameContext() { return nullptr; }

  // Returns an InterfaceProvider the frame can use to request interfaces from
  // the browser. This method may not return nullptr.
  virtual service_manager::InterfaceProvider* GetInterfaceProvider();

  // Returns an AssociatedInterfaceProvider the frame can use to request
  // navigation-associated interfaces from the browser. See also
  // LocalFrame::GetRemoteNavigationAssociatedInterfaces().
  virtual AssociatedInterfaceProvider*
  GetRemoteNavigationAssociatedInterfaces() {
    return nullptr;
  }

  // General notifications -----------------------------------------------

  // Indicates that another page has accessed the DOM of the initial empty
  // document of a main frame. After this, it is no longer safe to show a
  // pending navigation's URL, because a URL spoof is possible.
  virtual void DidAccessInitialDocument() {}

  // Request the creation of a new child frame. Embedders may return nullptr
  // to prevent the new child frame from being attached. Otherwise, embedders
  // should create a new WebLocalFrame, insert it into the frame tree, and
  // return the created frame.
  virtual WebLocalFrame* CreateChildFrame(
      WebLocalFrame* parent,
      WebTreeScopeType,
      const WebString& name,
      const WebString& fallback_name,
      WebSandboxFlags sandbox_flags,
      const ParsedFeaturePolicy& container_policy,
      const WebFrameOwnerProperties&,
      FrameOwnerElementType) {
    return nullptr;
  }

  // Called when Blink cannot find a frame with the given name in the frame's
  // browsing instance.  This gives the embedder a chance to return a frame
  // from outside of the browsing instance.
  virtual WebFrame* FindFrame(const WebString& name) { return nullptr; }

  // This frame has set its opener to another frame, or disowned the opener
  // if opener is null. See http://html.spec.whatwg.org/#dom-opener.
  virtual void DidChangeOpener(WebFrame*) {}

  // Specifies the reason for the detachment.
  enum class DetachType { kRemove, kSwap };

  // This frame has been detached. Embedders should release any resources
  // associated with this frame. If the DetachType is Remove, the frame should
  // also be removed from the frame tree; otherwise, if the DetachType is
  // Swap, the frame is being replaced in-place by WebFrame::swap().
  virtual void FrameDetached(DetachType) {}

  // This frame has become focused.
  virtual void FrameFocused() {}

  // A provisional load is about to commit.
  virtual void WillCommitProvisionalLoad() {}

  // This frame's name has changed.
  virtual void DidChangeName(const WebString& name) {}

  // This frame has set an insecure request policy.
  virtual void DidEnforceInsecureRequestPolicy(WebInsecureRequestPolicy) {}

  // This frame has set an upgrade insecure navigations set.
  virtual void DidEnforceInsecureNavigationsSet(const std::vector<unsigned>&) {}

  // The sandbox flags or container policy have changed for a child frame of
  // this frame.
  virtual void DidChangeFramePolicy(
      WebFrame* child_frame,
      WebSandboxFlags flags,
      const ParsedFeaturePolicy& container_policy) {}

  // Called when a Feature-Policy or Content-Security-Policy HTTP header (for
  // sandbox flags) is encountered while loading the frame's document.
  virtual void DidSetFramePolicyHeaders(
      WebSandboxFlags flags,
      const ParsedFeaturePolicy& parsed_header) {}

  // Called when a new Content Security Policy is added to the frame's
  // document.  This can be triggered by handling of HTTP headers, handling
  // of <meta> element, or by inheriting CSP from the parent (in case of
  // about:blank).
  virtual void DidAddContentSecurityPolicies(
      const WebVector<WebContentSecurityPolicy>& policies) {}

  // Some frame owner properties have changed for a child frame of this frame.
  // Frame owner properties currently include: scrolling, marginwidth and
  // marginheight.
  virtual void DidChangeFrameOwnerProperties(WebFrame* child_frame,
                                             const WebFrameOwnerProperties&) {}

  // Called when a watched CSS selector matches or stops matching.
  virtual void DidMatchCSS(
      const WebVector<WebString>& newly_matching_selectors,
      const WebVector<WebString>& stopped_matching_selectors) {}

  // Replicate user activation state updates for this frame to the embedder.
  virtual void UpdateUserActivationState(UserActivationUpdateType update_type) {
  }

  // Called if the previous document had a user gesture and is on the same
  // eTLD+1 as the current document.
  virtual void SetHasReceivedUserGestureBeforeNavigation(bool value) {}

  // Called when a frame is capturing mouse input, such as when a scrollbar
  // is being dragged.
  virtual void SetMouseCapture(bool capture) {}

  // Console messages ----------------------------------------------------

  // Whether or not we should report a detailed message for the given source.
  virtual bool ShouldReportDetailedMessageForSource(const WebString& source) {
    return false;
  }

  // A new message was added to the console.
  virtual void DidAddMessageToConsole(const WebConsoleMessage&,
                                      const WebString& source_name,
                                      unsigned source_line,
                                      const WebString& stack_trace) {}

  // Load commands -------------------------------------------------------

  // The client should handle the request as a download.
  // If the request is for a blob: URL, a BlobURLTokenPtr should be provided
  // as |blob_url_token| to ensure the correct blob gets downloaded.
  enum class CrossOriginRedirects { kFollow, kNavigate };
  virtual void DownloadURL(const WebURLRequest&,
                           CrossOriginRedirects cross_origin_redirect_behavior,
                           mojo::ScopedMessagePipeHandle blob_url_token) {}

  // The client should load an error page in the current frame.
  virtual void LoadErrorPage(int reason) {}

  // Navigational queries ------------------------------------------------

  // The client may choose to alter the navigation policy.  Otherwise,
  // defaultPolicy should just be returned.

  struct NavigationPolicyInfo {
    // Note: if browser side navigations are enabled, the client may modify
    // the urlRequest. However, should this happen, the client should change
    // the WebNavigationPolicy to WebNavigationPolicyIgnore, and the load
    // should stop in blink. In all other cases, the urlRequest should not
    // be modified.
    WebURLRequest& url_request;
    WebNavigationType navigation_type;
    WebNavigationPolicy default_policy;
    bool has_user_gesture;
    bool replaces_current_history_item;
    bool is_history_navigation_in_new_child_frame;
    bool is_client_redirect;
    WebTriggeringEventInfo triggering_event_info;
    WebFormElement form;
    WebSourceLocation source_location;
    WebString devtools_initiator_info;
    WebContentSecurityPolicyDisposition
        should_check_main_world_content_security_policy;
    mojo::ScopedMessagePipeHandle blob_url_token;
    base::TimeTicks input_start;

    // Specify whether or not a MHTML Archive can be used to load a subframe
    // resource instead of doing a network request.
    enum class ArchiveStatus { Absent, Present };
    ArchiveStatus archive_status;

    explicit NavigationPolicyInfo(WebURLRequest& url_request)
        : url_request(url_request),
          navigation_type(kWebNavigationTypeOther),
          default_policy(kWebNavigationPolicyIgnore),
          has_user_gesture(false),
          replaces_current_history_item(false),
          is_history_navigation_in_new_child_frame(false),
          is_client_redirect(false),
          triggering_event_info(WebTriggeringEventInfo::kUnknown),
          should_check_main_world_content_security_policy(
              kWebContentSecurityPolicyDispositionCheck),
          archive_status(ArchiveStatus::Absent) {}
  };

  virtual WebNavigationPolicy DecidePolicyForNavigation(
      const NavigationPolicyInfo& info) {
    return info.default_policy;
  }

  // Asks the embedder whether the frame is allowed to navigate the main frame
  // to a data URL.
  // TODO(crbug.com/713259): Move renderer side checks to
  //                         RenderFrameImpl::DecidePolicyForNavigation().
  virtual bool AllowContentInitiatedDataUrlNavigations(const WebURL&) {
    return false;
  }

  // Navigational notifications ------------------------------------------

  // These notifications bracket any loading that occurs in the WebFrame.
  virtual void DidStartLoading() {}
  virtual void DidStopLoading() {}

  // Notification that some progress was made loading the current frame.
  // loadProgress is a value between 0 (nothing loaded) and 1.0 (frame fully
  // loaded).
  virtual void DidChangeLoadProgress(double load_progress) {}

  // A form submission has been requested, but the page's submit event handler
  // hasn't yet had a chance to run (and possibly alter/interrupt the submit.)
  virtual void WillSendSubmitEvent(const WebFormElement&) {}

  // A datasource has been created for a new navigation.  The given
  // datasource will become the provisional datasource for the frame.
  virtual void DidCreateDocumentLoader(WebDocumentLoader*) {}

  // A new provisional load has been started.
  virtual void DidStartProvisionalLoad(
      WebDocumentLoader* document_loader,
      WebURLRequest& request,
      mojo::ScopedMessagePipeHandle navigation_initiator_handle) {}

  // The provisional load failed. The WebHistoryCommitType is the commit type
  // that would have been used had the load succeeded.
  virtual void DidFailProvisionalLoad(const WebURLError&,
                                      WebHistoryCommitType) {}

  // The provisional datasource is now committed.  The first part of the
  // response body has been received, and the encoding of the response
  // body is known.
  virtual void DidCommitProvisionalLoad(const WebHistoryItem&,
                                        WebHistoryCommitType,
                                        WebGlobalObjectReusePolicy) {}

  // The frame's document has just been initialized.
  virtual void DidCreateNewDocument() {}

  // The window object for the frame has been cleared of any extra properties
  // that may have been set by script from the previously loaded document. This
  // will get invoked multiple times when navigating from an initial empty
  // document to the actual document.
  virtual void DidClearWindowObject() {}

  // The document element has been created.
  // This method may not invalidate the frame, nor execute JavaScript code.
  virtual void DidCreateDocumentElement() {}

  // Like |didCreateDocumentElement|, except this method may run JavaScript
  // code (and possibly invalidate the frame).
  virtual void RunScriptsAtDocumentElementAvailable() {}

  // The page title is available.
  virtual void DidReceiveTitle(const WebString& title,
                               WebTextDirection direction) {}

  // The icon for the page have changed.
  virtual void DidChangeIcon(WebIconURL::Type) {}

  // The frame's document finished loading.
  // This method may not execute JavaScript code.
  // TODO(dgozman): rename this to DidFireDOMContentLoadedEvent.
  virtual void DidFinishDocumentLoad() {}

  // Like |didFinishDocumentLoad|, except this method may run JavaScript
  // code (and possibly invalidate the frame).
  virtual void RunScriptsAtDocumentReady(bool document_is_empty) {}

  // The frame's window.onload event is ready to fire. This method may delay
  // window.onload by incrementing LoadEventDelayCount.
  virtual void RunScriptsAtDocumentIdle() {}

  // The 'load' event was dispatched.
  virtual void DidHandleOnloadEvents() {}

  // The frame's document or one of its subresources failed to load. The
  // WebHistoryCommitType is the commit type that would have been used had the
  // load succeeded.
  virtual void DidFailLoad(const WebURLError&, WebHistoryCommitType) {}

  // The frame's document and all of its subresources succeeded to load.
  virtual void DidFinishLoad() {}

  // The navigation resulted in no change to the documents within the page.
  // For example, the navigation may have just resulted in scrolling to a
  // named anchor or a PopState event may have been dispatched.
  virtual void DidFinishSameDocumentNavigation(const WebHistoryItem&,
                                               WebHistoryCommitType,
                                               bool content_initiated) {}

  // Called upon update to scroll position, document state, and other
  // non-navigational events related to the data held by WebHistoryItem.
  // WARNING: This method may be called very frequently.
  virtual void DidUpdateCurrentHistoryItem() {}

  // The frame's manifest has changed.
  virtual void DidChangeManifest() {}

  // The frame's theme color has changed.
  virtual void DidChangeThemeColor() {}

  // Called to report resource timing information for this frame to the parent.
  // Only used when the parent frame is remote.
  virtual void ForwardResourceTimingToParent(const WebResourceTimingInfo&) {}

  // Called to dispatch a load event for this frame in the FrameOwner of an
  // out-of-process parent frame.
  virtual void DispatchLoad() {}

  // Returns the effective connection type when the frame was fetched.
  virtual WebEffectiveConnectionType GetEffectiveConnectionType() {
    return WebEffectiveConnectionType::kTypeUnknown;
  }

  // Overrides the effective connection type for testing.
  virtual void SetEffectiveConnectionTypeForTesting(
      WebEffectiveConnectionType) {}

  virtual WebURLRequest::PreviewsState GetPreviewsStateForFrame() const {
    return WebURLRequest::kPreviewsUnspecified;
  }

  // This frame tried to navigate its top level frame to the given url without
  // ever having received a user gesture.
  virtual void DidBlockFramebust(const WebURL&) {}

  // Returns token to be used as a frame id in the devtools protocol.
  // It is derived from the content's devtools_frame_token, is
  // defined by the browser and passed into Blink upon frame creation.
  virtual base::UnguessableToken GetDevToolsFrameToken() {
    return base::UnguessableToken::Create();
  }

  // When a same-site load fails and the original frame in parent process is
  // owned by an <object> element, this call notifies the owner element that it
  // should render fallback content of its own.
  virtual void RenderFallbackContentInParentProcess() {}

  // PlzNavigate
  // Called to abort a navigation that is being handled by the browser process.
  virtual void AbortClientNavigation() {}

  // Push API ---------------------------------------------------

  // Used to access the embedder for the Push API.
  virtual WebPushClient* PushClient() { return nullptr; }

  // InstalledApp API ----------------------------------------------------

  // Used to access the embedder for the InstalledApp API.
  virtual WebRelatedAppsFetcher* GetRelatedAppsFetcher() { return nullptr; }

  // Editing -------------------------------------------------------------

  // These methods allow the client to intercept and overrule editing
  // operations.
  virtual void DidChangeSelection(bool is_selection_empty) {}
  virtual void DidChangeContents() {}

  // This method is called in response to handleInputEvent() when the
  // default action for the current keyboard event is not suppressed by the
  // page, to give the embedder a chance to handle the keyboard event
  // specially.
  //
  // Returns true if the keyboard event was handled by the embedder,
  // indicating that the default action should be suppressed.
  virtual bool HandleCurrentKeyboardEvent() { return false; }

  // Dialogs -------------------------------------------------------------

  // Displays a modal alert dialog containing the given message. Returns
  // once the user dismisses the dialog.
  virtual void RunModalAlertDialog(const WebString& message) {}

  // Displays a modal confirmation dialog with the given message as
  // description and OK/Cancel choices. Returns true if the user selects
  // 'OK' or false otherwise.
  virtual bool RunModalConfirmDialog(const WebString& message) { return false; }

  // Displays a modal input dialog with the given message as description
  // and OK/Cancel choices. The input field is pre-filled with
  // defaultValue. Returns true if the user selects 'OK' or false
  // otherwise. Upon returning true, actualValue contains the value of
  // the input field.
  virtual bool RunModalPromptDialog(const WebString& message,
                                    const WebString& default_value,
                                    WebString* actual_value) {
    return false;
  }

  // Displays a modal confirmation dialog with OK/Cancel choices, where 'OK'
  // means that it is okay to proceed with closing the view. Returns true if
  // the user selects 'OK' or false otherwise.
  virtual bool RunModalBeforeUnloadDialog(bool is_reload) { return true; }

  // This method returns immediately after showing the dialog. When the
  // dialog is closed, it should call the WebFileChooserCompletion to
  // pass the results of the dialog. Returns false if
  // WebFileChooseCompletion will never be called.
  virtual bool RunFileChooser(const blink::WebFileChooserParams& params,
                              WebFileChooserCompletion* chooser_completion) {
    return false;
  }

  // UI ------------------------------------------------------------------

  // Shows a context menu with commands relevant to a specific element on
  // the given frame. Additional context data is supplied.
  virtual void ShowContextMenu(const WebContextMenuData&) {}

  // This method is called in response to WebView's saveImageAt(x, y).
  // A data url from <canvas> or <img> is passed to the method's argument.
  virtual void SaveImageFromDataURL(const WebString&) {}

  // Called when the frame rects changed.
  virtual void FrameRectsChanged(const WebRect&) {}

  // Low-level resource notifications ------------------------------------

  // A request is about to be sent out, and the client may modify it.  Request
  // is writable, and changes to the URL, for example, will change the request
  // made.
  virtual void WillSendRequest(WebURLRequest&) {}

  // Response headers have been received.
  virtual void DidReceiveResponse(const WebURLResponse&) {}

  // The specified request was satified from WebCore's memory cache.
  virtual void DidLoadResourceFromMemoryCache(const WebURLRequest&,
                                              const WebURLResponse&) {}

  // This frame has displayed inactive content (such as an image) from an
  // insecure source.  Inactive content cannot spread to other frames.
  virtual void DidDisplayInsecureContent() {}

  // This frame contains a form that submits to an insecure target url.
  virtual void DidContainInsecureFormAction() {}

  // The indicated security origin has run active content (such as a
  // script) from an insecure source.  Note that the insecure content can
  // spread to other frames in the same origin.
  virtual void DidRunInsecureContent(const WebSecurityOrigin&,
                                     const WebURL& insecure_url) {}

  // A PingLoader was created, and a request dispatched to a URL.
  virtual void DidDispatchPingLoader(const WebURL&) {}

  // This frame has displayed inactive content (such as an image) from
  // a connection with certificate errors.
  virtual void DidDisplayContentWithCertificateErrors() {}
  // This frame has run active content (such as a script) from a
  // connection with certificate errors.
  virtual void DidRunContentWithCertificateErrors() {}

  // This frame loaded a resource with an otherwise-valid legacy Symantec
  // certificate that is slated for distrust (|did_fail|=false) or has already
  // been distrusted (|did_fail|=true).
  virtual void ReportLegacySymantecCert(const WebURL&, bool did_fail) {}

  // A performance timing event (e.g. first paint) occurred
  virtual void DidChangePerformanceTiming() {}

  // UseCounter ----------------------------------------------------------
  // Blink exhibited a certain loading behavior that the browser process will
  // use for segregated histograms.
  virtual void DidObserveLoadingBehavior(WebLoadingBehaviorFlag) {}
  // Blink UseCounter should only track feature usage for non NTP activities.
  // ShouldTrackUseCounter checks the url of a page's main frame is not a new
  // tab page url.
  virtual bool ShouldTrackUseCounter(const WebURL&) { return true; }

  // Blink hits the code path for a certain web feature for the first time on
  // this frame. As a performance optimization, features already hit on other
  // frames associated with the same page in the renderer are not currently
  // reported. This is used for reporting UseCounter features histograms.
  virtual void DidObserveNewFeatureUsage(mojom::WebFeature) {}
  // Blink hits the code path for a certain CSS property (either an animated CSS
  // property or not) for the first time on this frame. As a performance
  // optimization, features already hit on other frames associated with the same
  // page in the renderer are not currently reported. This is used for reporting
  // UseCounter CSS histograms.
  virtual void DidObserveNewCssPropertyUsage(int /*css_property*/,
                                             bool /*is_animated*/) {}

  // Reports that visible elements in the frame shifted (bit.ly/lsm-explainer).
  virtual void DidObserveLayoutJank(double jank_fraction) {}

  // Script notifications ------------------------------------------------

  // Notifies that a new script context has been created for this frame.
  // This is similar to didClearWindowObject but only called once per
  // frame context.
  virtual void DidCreateScriptContext(v8::Local<v8::Context>, int world_id) {}

  // WebKit is about to release its reference to a v8 context for a frame.
  virtual void WillReleaseScriptContext(v8::Local<v8::Context>, int world_id) {}

  // Geometry notifications ----------------------------------------------

  // The main frame scrolled.
  virtual void DidChangeScrollOffset() {}

  // Informs the browser that the draggable regions have been updated.
  virtual void DraggableRegionsChanged() {}

  // Scrolls a local frame in its remote process. Called on the
  // WebLocalFrameClient of a local frame only.
  virtual void ScrollRectToVisibleInParentFrame(
      const WebRect&,
      const WebScrollIntoViewParams&) {}

  // When the bubbling of a logical scroll reaches a local root, bubbling
  // will be continued in the parent process.
  virtual void BubbleLogicalScrollInParentFrame(
      WebScrollDirection direction,
      WebScrollGranularity granularity) {}

  // MediaStream -----------------------------------------------------

  // A new WebRTCPeerConnectionHandler is created.
  virtual void WillStartUsingPeerConnectionHandler(
      WebRTCPeerConnectionHandler*) {}

  virtual WebUserMediaClient* UserMediaClient() { return nullptr; }

  // Encrypted Media -------------------------------------------------

  virtual WebEncryptedMediaClient* EncryptedMediaClient() { return nullptr; }

  // User agent ------------------------------------------------------

  // Asks the embedder if a specific user agent should be used. Non-empty
  // strings indicate an override should be used. Otherwise,
  // Platform::current()->userAgent() will be called to provide one.
  virtual WebString UserAgentOverride() { return WebString(); }

  // Do not track ----------------------------------------------------

  // Asks the embedder what value the network stack will send for the DNT
  // header. An empty string indicates that no DNT header will be send.
  virtual WebString DoNotTrackValue() { return WebString(); }

  // WebGL ------------------------------------------------------

  // Asks the embedder whether WebGL is blocked for the WebFrame. This call is
  // placed here instead of WebContentSettingsClient because this class is
  // implemented in content/, and putting it here avoids adding more public
  // content/ APIs.
  virtual bool ShouldBlockWebGL() { return false; }

  // Accessibility -------------------------------------------------------

  // Notifies embedder about an accessibility event.
  virtual void PostAccessibilityEvent(const WebAXObject&, ax::mojom::Event) {}

  // Notifies embedder that a WebAXObject is dirty and its state needs
  // to be serialized again. If |subtree| is true, the entire subtree is
  // dirty.
  virtual void MarkWebAXObjectDirty(const WebAXObject&, bool subtree) {}

  // Provides accessibility information about a find in page result.
  virtual void HandleAccessibilityFindInPageResult(int identifier,
                                                   int match_index,
                                                   const WebNode& start_node,
                                                   int start_offset,
                                                   const WebNode& end_node,
                                                   int end_offset) {}

  // Fullscreen ----------------------------------------------------------

  // Called to enter/exit fullscreen mode.
  // After calling enterFullscreen or exitFullscreen,
  // WebWidget::didEnterFullscreen or WebWidget::didExitFullscreen
  // respectively will be called once the fullscreen mode has changed.
  virtual void EnterFullscreen(const blink::WebFullscreenOptions& options) {}
  virtual void ExitFullscreen() {}

  // Called when this frame changes fullscreen state.
  virtual void FullscreenStateChanged(bool is_fullscreen) {}

  // Sudden termination --------------------------------------------------

  // Called when elements preventing the sudden termination of the frame
  // become present or stop being present. |type| is the type of element
  // (BeforeUnload handler, Unload handler).
  virtual void SuddenTerminationDisablerChanged(
      bool present,
      WebSuddenTerminationDisablerType) {}

  // Navigator Content Utils  --------------------------------------------

  // Registers a new URL handler for the given protocol.
  virtual void RegisterProtocolHandler(const WebString& scheme,
                                       const WebURL& url,
                                       const WebString& title) {}

  // Unregisters a given URL handler for the given protocol.
  virtual void UnregisterProtocolHandler(const WebString& scheme,
                                         const WebURL& url) {}

  // Audio Output Devices API --------------------------------------------

  // Checks that the given audio sink exists and is authorized. The result is
  // provided via the callbacks.
  virtual void CheckIfAudioSinkExistsAndIsAuthorized(
      const WebString& sink_id,
      std::unique_ptr<WebSetSinkIdCallbacks> callbacks) {
    callbacks->OnError(WebSetSinkIdError::kNotSupported);
  }

  // Visibility ----------------------------------------------------------

  // Returns the current visibility of the WebFrame.
  virtual mojom::PageVisibilityState VisibilityState() const {
    return mojom::PageVisibilityState::kVisible;
  }

  // Overwrites the given URL to use an HTML5 embed if possible.
  // An empty URL is returned if the URL is not overriden.
  virtual WebURL OverrideFlashEmbedWithHTML(const WebURL& url) {
    return WebURL();
  }

  // Loading --------------------------------------------------------------

  virtual std::unique_ptr<blink::WebURLLoaderFactory> CreateURLLoaderFactory() {
    NOTREACHED();
    return nullptr;
  }

  // Accessibility Object Model -------------------------------------------

  // This method is used to expose the AX Tree stored in content/renderer to the
  // DOM as part of AOM Phase 4.
  virtual WebComputedAXTree* GetOrCreateWebComputedAXTree() { return nullptr; }

  // WebSocket -----------------------------------------------------------
  virtual std::unique_ptr<WebSocketHandshakeThrottle>
  CreateWebSocketHandshakeThrottle() {
    return nullptr;
  }

  // Returns true when the contents of plugin are handled externally. This means
  // the plugin element will own a content frame but the frame is than used
  // externally to load the required handelrs.
  virtual bool IsPluginHandledExternally(const WebElement& plugin_element,
                                         const WebURL& url,
                                         const WebString& suggested_mime_type) {
    return false;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_LOCAL_FRAME_CLIENT_H_
