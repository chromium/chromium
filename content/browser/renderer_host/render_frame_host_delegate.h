// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_DELEGATE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/i18n/rtl.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/media_player_watch_time.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/visibility.h"
#include "content/public/common/javascript_dialog_type.h"
#include "media/mojo/services/media_metrics_provider.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_response_headers.h"
#include "services/device/public/mojom/geolocation_context.mojom.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/choosers/popup_menu.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/frame/blocked_navigation_types.mojom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-forward.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/base/window_open_disposition.h"

#if defined(OS_WIN)
#include "ui/gfx/native_widget_types.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#include "services/device/public/mojom/nfc.mojom.h"
#endif

class GURL;

namespace IPC {
class Message;
}

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace url {
class Origin;
}

namespace blink {
namespace mojom {
class FullscreenOptions;
}
namespace web_pref {
struct WebPreferences;
}
}  // namespace blink

namespace ui {
class ClipboardFormatType;
}

namespace content {
class AgentSchedulingGroupHost;
class FrameTreeNode;
class PageState;
class RenderFrameHostImpl;
class SessionStorageNamespace;
class WebContents;
struct AXEventNotificationDetails;
struct AXLocationChangeNotificationDetails;
struct ContextMenuParams;
struct GlobalRequestID;

namespace mojom {
class CreateNewWindowParams;
}

// An interface implemented by an object interested in knowing about the state
// of the RenderFrameHost.
class CONTENT_EXPORT RenderFrameHostDelegate {
 public:
  // Callback used with HandleClipboardPaste() method.  If the clipboard paste
  // is allowed to proceed, the callback is called with true.  Otherwise the
  // callback is called with false.
  using ClipboardPasteAllowed = RenderFrameHostImpl::ClipboardPasteAllowed;
  using IsClipboardPasteAllowedCallback =
      RenderFrameHostImpl::IsClipboardPasteAllowedCallback;

  using JavaScriptDialogCallback =
      content::JavaScriptDialogManager::DialogClosedCallback;

  // This is used to give the delegate a chance to filter IPC messages.
  virtual bool OnMessageReceived(RenderFrameHostImpl* render_frame_host,
                                 const IPC::Message& message);

  // Allows the delegate to filter incoming associated interface requests.
  virtual void OnAssociatedInterfaceRequest(
      RenderFrameHost* render_frame_host,
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) {}

  // Allows the delegate to filter incoming interface requests.
  virtual void OnInterfaceRequest(
      RenderFrameHost* render_frame_host,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) {}

  // Notification from the renderer host that a suspicious navigation of the
  // main frame has been blocked. Allows the delegate to provide some UI to let
  // the user know about the blocked navigation and give them the option to
  // recover from it.
  // |blocked_url| is the blocked navigation target, |initiator_url| is the URL
  // of the frame initiating the navigation, |reason| specifies why the
  // navigation was blocked.
  virtual void OnDidBlockNavigation(
      const GURL& blocked_url,
      const GURL& initiator_url,
      blink::mojom::NavigationBlockedReason reason) {}

  // Notifies the browser that a frame finished loading.
  virtual void OnDidFinishLoad(RenderFrameHost* render_frame_host,
                               const GURL& url) {}

  // Gets the last committed URL. See WebContents::GetLastCommittedURL for a
  // description of the semantics.
  virtual const GURL& GetMainFrameLastCommittedURL();

  // A message was added to to the console.
  virtual bool DidAddMessageToConsole(
      blink::mojom::ConsoleMessageLevel log_level,
      const base::string16& message,
      int32_t line_no,
      const base::string16& source_id);

  // Called when a RenderFrame for |render_frame_host| is created in the
  // renderer process. Use |RenderFrameDeleted| to listen for when this
  // RenderFrame goes away.
  virtual void RenderFrameCreated(RenderFrameHost* render_frame_host) {}

  // Called when a RenderFrame for |render_frame_host| is deleted or the
  // renderer process in which it runs it has died. Use |RenderFrameCreated| to
  // listen for when RenderFrame objects are created.
  virtual void RenderFrameDeleted(RenderFrameHost* render_frame_host) {}

  // A context menu should be shown, to be built using the context information
  // provided in the supplied params.
  virtual void ShowContextMenu(RenderFrameHost* render_frame_host,
                               const ContextMenuParams& params) {}

  // A JavaScript alert, confirmation or prompt dialog should be shown.
  virtual void RunJavaScriptDialog(RenderFrameHost* render_frame_host,
                                   const base::string16& message,
                                   const base::string16& default_prompt,
                                   JavaScriptDialogType type,
                                   JavaScriptDialogCallback callback) {}

  virtual void RunBeforeUnloadConfirm(RenderFrameHost* render_frame_host,
                                      bool is_reload,
                                      JavaScriptDialogCallback callback) {}

  // Notifies when new blink::mojom::FaviconURLPtr candidates are received from
  // the renderer process.
  virtual void UpdateFaviconURL(
      RenderFrameHost* source,
      std::vector<blink::mojom::FaviconURLPtr> candidates) {}

  // The pending page load was canceled, so the address bar should be updated.
  virtual void DidCancelLoading() {}

  // Another page accessed the top-level initial empty document, which means it
  // is no longer safe to display a pending URL without risking a URL spoof.
  virtual void DidAccessInitialDocument() {}

  // The frame changed its window.name property.
  virtual void DidChangeName(RenderFrameHost* render_frame_host,
                             const std::string& name) {}

  // The sticky user activation bit has been set on the frame. This will not be
  // called for new RenderFrameHosts whose underlying FrameTreeNode was already
  // activated.
  virtual void DidReceiveFirstUserActivation(
      RenderFrameHost* render_frame_host) {}

  // The display style of the frame has changed.
  virtual void DidChangeDisplayState(RenderFrameHost* render_frame_host,
                                     bool is_display_none) {}

  // The size of the frame has changed.
  virtual void FrameSizeChanged(RenderFrameHost* render_frame_host,
                                const gfx::Size& frame_size) {}

  // The DOMContentLoaded handler in the frame has completed.
  virtual void DOMContentLoaded(RenderFrameHost* render_frame_host) {}

  // The onload handler in the frame has completed. Only called for the top-
  // level frame.
  virtual void DocumentOnLoadCompleted(RenderFrameHost* render_frame_host) {}

  // The state for the page changed and should be updated in session history.
  virtual void UpdateStateForFrame(RenderFrameHost* render_frame_host,
                                   const PageState& page_state) {}

  // The page's title was changed and should be updated. Only called for the
  // top-level frame.
  virtual void UpdateTitle(RenderFrameHost* render_frame_host,
                           const base::string16& title,
                           base::i18n::TextDirection title_direction) {}

  // The destination URL has changed and should be updated.
  virtual void UpdateTargetURL(RenderFrameHost* render_frame_host,
                               const GURL& url) {}

  // Return this object cast to a WebContents, if it is one. If the object is
  // not a WebContents, returns null.
  virtual WebContents* GetAsWebContents();

  // The render frame has requested access to media devices listed in
  // |request|, and the client should grant or deny that permission by
  // calling |callback|.
  virtual void RequestMediaAccessPermission(const MediaStreamRequest& request,
                                            MediaResponseCallback callback);

  // Checks if we have permission to access the microphone or camera. Note that
  // this does not query the user. |type| must be MEDIA_DEVICE_AUDIO_CAPTURE
  // or MEDIA_DEVICE_VIDEO_CAPTURE.
  virtual bool CheckMediaAccessPermission(RenderFrameHost* render_frame_host,
                                          const url::Origin& security_origin,
                                          blink::mojom::MediaStreamType type);

  // Returns the ID of the default device for the given media device |type|.
  // If the returned value is an empty string, it means that there is no
  // default device for the given |type|.
  virtual std::string GetDefaultMediaDeviceID(
      blink::mojom::MediaStreamType type);

  // Get the accessibility mode for the WebContents that owns this frame.
  virtual ui::AXMode GetAccessibilityMode();

  // Called whenever the AXTreeID for the topmost RenderFrameHost has changed.
  virtual void AXTreeIDForMainFrameHasChanged() {}

  // Called when accessibility events or location changes are received
  // from a render frame, when the accessibility mode has the
  // ui::AXMode::kWebContents flag set.
  virtual void AccessibilityEventReceived(
      const AXEventNotificationDetails& details) {}
  virtual void AccessibilityLocationChangesReceived(
      const std::vector<AXLocationChangeNotificationDetails>& details) {}

  // Gets the GeolocationContext associated with this delegate.
  virtual device::mojom::GeolocationContext* GetGeolocationContext();

#if defined(OS_ANDROID)
  // Gets an NFC implementation within the context of this delegate.
  virtual void GetNFC(RenderFrameHost* render_frame_host,
                      mojo::PendingReceiver<device::mojom::NFC> receiver);
#endif

  // Returns whether entering fullscreen with EnterFullscreenMode() is allowed.
  virtual bool CanEnterFullscreenMode();

  // Notification that the frame with the given host wants to enter fullscreen
  // mode. Must only be called if CanEnterFullscreenMode returns true.
  virtual void EnterFullscreenMode(
      RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) {}

  // Notification that the frame wants to go out of fullscreen mode.
  // |will_cause_resize| indicates whether the fullscreen change causes a
  // view resize. e.g. This will be false when going from tab fullscreen to
  // browser fullscreen.
  virtual void ExitFullscreenMode(bool will_cause_resize) {}

  // Notification that this frame has changed fullscreen state.
  virtual void FullscreenStateChanged(RenderFrameHost* rfh,
                                      bool is_fullscreen) {}

#if defined(OS_ANDROID)
  // Updates information to determine whether a user gesture should carryover to
  // future navigations. This is needed so navigations within a certain
  // timeframe of a request initiated by a gesture will be treated as if they
  // were initiated by a gesture too, otherwise the navigation may be blocked.
  virtual void UpdateUserGestureCarryoverInfo() {}
#endif

  // Let the delegate decide whether postMessage should be delivered to
  // |target_rfh| from a source frame in the given SiteInstance.  This defaults
  // to false and overrides the RenderFrameHost's decision if true.
  virtual bool ShouldRouteMessageEvent(
      RenderFrameHost* target_rfh,
      SiteInstance* source_site_instance) const;

  // Ensure that |source_rfh| has swapped-out RenderViews and
  // RenderFrameProxies for itself and for all frames on its opener chain in
  // the current frame's SiteInstance.
  //
  // TODO(alexmos): This method currently supports cross-process postMessage,
  // where we may need to create any missing proxies for the message's source
  // frame and its opener chain. It currently exists in WebContents due to a
  // special case for <webview> guests, but this logic should eventually be
  // moved down into RenderFrameProxyHost::RouteMessageEvent when <webview>
  // refactoring for --site-per-process mode is further along.  See
  // https://crbug.com/330264.
  virtual void EnsureOpenerProxiesExist(RenderFrameHost* source_rfh) {}

  // Set the |node| frame as focused in the current FrameTree as well as
  // possibly changing focus in distinct but related inner/outer WebContents.
  virtual void SetFocusedFrame(FrameTreeNode* node, SiteInstance* source) {}

  // The frame called |window.focus()|.
  virtual void DidCallFocus() {}

  // Searches the WebContents for a focused frame, potentially in an inner
  // WebContents. If this WebContents has no focused frame, returns |nullptr|.
  // If there is no inner WebContents at the focused tree node, returns its
  // RenderFrameHost. If there is an inner WebContents, search it for focused
  // frames and inner contents. If an inner WebContents does not have a focused
  // frame, return its main frame, since the attachment frame in its outer
  // WebContents is not live.
  virtual RenderFrameHost* GetFocusedFrameIncludingInnerWebContents();

  // Returns the main frame for the delegate.
  virtual RenderFrameHostImpl* GetMainFrame();

  // Called by when |source_rfh| advances focus to a RenderFrameProxyHost.
  virtual void OnAdvanceFocus(RenderFrameHostImpl* source_rfh) {}

  // Creates a WebUI object for a frame navigating to |url|. If no WebUI
  // applies, returns null.
  virtual std::unique_ptr<WebUIImpl> CreateWebUIForRenderFrameHost(
      RenderFrameHost* frame_host,
      const GURL& url);

  // Called by |frame| to notify that it has received an update on focused
  // element. |bounds_in_root_view| is the rectangle containing the element that
  // is focused and is with respect to root frame's RenderWidgetHost's
  // coordinate space.
  virtual void OnFocusedElementChangedInFrame(
      RenderFrameHostImpl* frame,
      const gfx::Rect& bounds_in_root_view,
      blink::mojom::FocusType focus_type) {}

  // The page is trying to open a new page (e.g. a popup window). The window
  // should be created associated the process of |opener|, but it should not
  // be shown yet. That should happen in response to ShowCreatedWindow.
  // |params.window_container_type| describes the type of RenderViewHost
  // container that is requested -- in particular, the window.open call may
  // have specified 'background' and 'persistent' in the feature string.
  //
  // The passed |opener| is the RenderFrameHost initiating the window creation.
  // It will never be null, even if the opener is suppressed via |params|.
  //
  // The passed |params.frame_name| parameter is the name parameter that was
  // passed to window.open(), and will be empty if none was passed.
  //
  // Note: this is not called "CreateWindow" because that will clash with
  // the Windows function which is actually a #define.
  //
  // On success, a non-owning pointer to the new RenderFrameHostDelegate is
  // returned.
  //
  // The caller is expected to handle cleanup if this operation fails or is
  // suppressed by checking if the return value is null.
  virtual RenderFrameHostDelegate* CreateNewWindow(
      RenderFrameHost* opener,
      const mojom::CreateNewWindowParams& params,
      bool is_new_browsing_instance,
      bool has_user_gesture,
      SessionStorageNamespace* session_storage_namespace);

  // Show a previously created page with the specified disposition and bounds.
  // The window is identified by the |main_frame_widget_route_id| passed to
  // CreateNewWindow.
  //
  // The passed |opener| is the RenderFrameHost initiating the window creation.
  // It will never be null, even if the opener is suppressed via |params|.
  //
  // Note: this is not called "ShowWindow" because that will clash with
  // the Windows function which is actually a #define.
  virtual void ShowCreatedWindow(RenderFrameHost* opener,
                                 int main_frame_widget_route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_rect,
                                 bool user_gesture) {}

  // Notified that mixed content was displayed or ran.
  virtual void DidDisplayInsecureContent() {}
  virtual void DidContainInsecureFormAction() {}
  // The main frame document element is ready. This happens when the document
  // has finished parsing.
  virtual void DocumentAvailableInMainFrame() {}
  virtual void DidRunInsecureContent(const GURL& security_origin,
                                     const GURL& target_url) {}

  // Reports that passive mixed content was found at the specified url.
  virtual void PassiveInsecureContentFound(const GURL& resource_url) {}

  // Checks if running of active mixed content is allowed in the current tab.
  virtual bool ShouldAllowRunningInsecureContent(bool allowed_per_prefs,
                                                 const url::Origin& origin,
                                                 const GURL& resource_url);

  // Opens a new view-source tab for the last committed document in |frame|.
  virtual void ViewSource(RenderFrameHostImpl* frame) {}

#if defined(OS_ANDROID)
  virtual base::android::ScopedJavaLocalRef<jobject>
  GetJavaRenderFrameHostDelegate();
#endif

  // Whether the delegate is being destroyed, in which case the RenderFrameHost
  // should not be asked to create a RenderFrame.
  virtual bool IsBeingDestroyed();

  // Notified that the render frame started loading a subresource.
  virtual void SubresourceResponseStarted(const GURL& url,
                                          net::CertStatus cert_status) {}

  // Notified that the render finished loading a subresource for the frame
  // associated with |render_frame_host|.
  virtual void ResourceLoadComplete(
      RenderFrameHost* render_frame_host,
      const GlobalRequestID& request_id,
      blink::mojom::ResourceLoadInfoPtr resource_load_info) {}

  // Request to print a frame that is in a different process than its parent.
  virtual void PrintCrossProcessSubframe(const gfx::Rect& rect,
                                         int document_cookie,
                                         RenderFrameHost* render_frame_host) {}

  // Request to paint preview a frame that is in a different process that its
  // parent.
  virtual void CapturePaintPreviewOfCrossProcessSubframe(
      const gfx::Rect& rect,
      const base::UnguessableToken& guid,
      RenderFrameHost* render_frame_host) {}

  // Updates the Picture-in-Picture controller with the relevant viz::SurfaceId
  // of the video to be in Picture-in-Picture mode.
  virtual void UpdatePictureInPictureSurfaceId(const viz::SurfaceId& surface_id,
                                               const gfx::Size& natural_size) {}

  // Returns a copy of the current WebPreferences associated with this
  // RenderFrameHost's WebContents. If it does not exist, this will create one
  // and send the newly computed value to all renderers.
  // Note that this will not trigger a recomputation of WebPreferences if it
  // already exists - this will return the last computed/set value of
  // WebPreferences. If we want to guarantee that the value reflects the current
  // state of the WebContents, NotifyPreferencesChanged() should be called
  // before calling this.
  virtual const blink::web_pref::WebPreferences&
  GetOrCreateWebPreferences() = 0;

  // Returns the visibility of the delegate.
  virtual Visibility GetVisibility();

  // Get the UKM source ID for current content from the last committed
  // navigation, either a cross-document or same-document navigation. This is
  // for providing data about the content to the URL-keyed metrics service.
  // Use this method if UKM events should be attributed to the latest
  // navigation, that is, attribute events to the new source after each
  // same-document navigation, if any.
  virtual ukm::SourceId
  GetUkmSourceIdForLastCommittedSourceIncludingSameDocument() const;

  // Notify observers if WebAudio AudioContext has started (or stopped) playing
  // audible sounds.
  virtual void AudioContextPlaybackStarted(RenderFrameHost* host,
                                           int context_id) {}
  virtual void AudioContextPlaybackStopped(RenderFrameHost* host,
                                           int context_id) {}

  // Notifies observers if the frame has changed audible state.
  virtual void OnFrameAudioStateChanged(RenderFrameHost* host,
                                        bool is_audible) {}

  // Returns the main frame of the inner delegate that is attached to this
  // delegate using |frame_tree_node|. Returns nullptr if no such inner delegate
  // exists.
  virtual RenderFrameHostImpl* GetMainFrameForInnerDelegate(
      FrameTreeNode* frame_tree_node);

  // Determine if the frame is of a low priority.
  virtual bool IsFrameLowPriority(const RenderFrameHost* render_frame_host);

  // Registers a new URL handler for the given protocol.
  virtual void RegisterProtocolHandler(RenderFrameHostImpl* host,
                                       const std::string& scheme,
                                       const GURL& url,
                                       const base::string16& title,
                                       bool user_gesture) {}

  // Unregisters a given URL handler for the given protocol.
  virtual void UnregisterProtocolHandler(RenderFrameHostImpl* host,
                                         const std::string& scheme,
                                         const GURL& url,
                                         bool user_gesture) {}

  // Go to the session history entry at the given offset (ie, -1 will return the
  // "back" item).
  virtual void OnGoToEntryAtOffset(RenderFrameHostImpl* source,
                                   int32_t offset,
                                   bool has_user_gesture) {}

  virtual media::MediaMetricsProvider::RecordAggregateWatchTimeCallback
  GetRecordAggregateWatchTimeCallback();

  // Determines if a clipboard paste using |data| of type |data_type| is allowed
  // in this renderer frame.  Possible data types supported for paste can be
  // seen in the ClipboardHostImpl class.  Text based formats will use the
  // data_type ui::ClipboardFormatType::GetPlainTextType() unless it is known
  // to be of a more specific type, like RTF or HTML, in which case a type
  // such as ui::ClipboardFormatType::GetRtfType() or
  // ui::ClipboardFormatType::GetHtmlType() is used.
  //
  // It is also possible for the data type to be
  // ui::ClipboardFormatType::GetWebCustomDataType() indicating that the paste
  // uses a custom data format.  It is up to the implementation to attempt to
  // understand the type if possible.  It is acceptable to deny pastes of
  // unknown data types.
  //
  // The implementation is expected to show UX to the user if needed.  If
  // shown, the UX should be associated with the specific render frame host.
  //
  // The callback is called, possibly asynchronously, with a status indicating
  // whether the operation is allowed or not.
  virtual void IsClipboardPasteAllowed(
      const GURL& url,
      const ui::ClipboardFormatType& data_type,
      const std::string& data,
      IsClipboardPasteAllowedCallback callback);

  // Notified when the main frame adjusts the page scale.
  virtual void OnPageScaleFactorChanged(RenderFrameHostImpl* source,
                                        float page_scale_factor) {}

  virtual void OnTextAutosizerPageInfoChanged(
      RenderFrameHostImpl* source,
      blink::mojom::TextAutosizerPageInfoPtr page_info) {}

  // Return true if we have seen a recent orientation change, which is used to
  // decide if we should consume user activation when entering fullscreen.
  virtual bool HasSeenRecentScreenOrientationChange();

  // Return true if the page has a transient affordance to enter fullscreen
  // without consuming user activation.
  virtual bool IsTransientAllowFullscreenActive() const;

  // The page is trying to open a new widget (e.g. a select popup). The
  // widget should be created associated with the given
  // |agent_scheduling_group|, but it should not be shown yet. That should
  // happen in response to ShowCreatedWidget.
  virtual void CreateNewWidget(
      AgentSchedulingGroupHost& agent_scheduling_group,
      int32_t route_id,
      mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost>
          blink_widget_host,
      mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget) {}

  // Creates a full screen RenderWidget. Similar to above.
  virtual void CreateNewFullscreenWidget(
      AgentSchedulingGroupHost& agent_scheduling_group,
      int32_t route_id,
      mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost>
          blink_widget_host,
      mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget) {}

  // Return true if the popup is shown through WebContentsObserver.
  // BrowserPluginGuest for the guest WebContents will show the popup on Mac,
  // then, we should skip to show the popup at RenderViewHostDelegateView.
  virtual bool ShowPopupMenu(
      RenderFrameHostImpl* render_frame_host,
      mojo::PendingRemote<blink::mojom::PopupMenuClient>* popup_client,
      const gfx::Rect& bounds,
      int32_t item_height,
      double font_size,
      int32_t selected_item,
      std::vector<blink::mojom::MenuItemPtr>* menu_items,
      bool right_aligned,
      bool allow_multiple_selection);

  virtual void DidLoadResourceFromMemoryCache(
      RenderFrameHostImpl* source,
      const GURL& url,
      const std::string& http_request,
      const std::string& mime_type,
      network::mojom::RequestDestination request_destination) {}

  // Called when the renderer sends a response via DomAutomationController.
  // For example, `window.domAutomationController.send(foo())` sends the result
  // of foo() here.
  virtual void DomOperationResponse(const std::string& json_string) {}

  virtual void OnCookiesAccessed(RenderFrameHostImpl* render_frame_host,
                                 const CookieAccessDetails& details) {}

  // Notified that the renderer responded after calling GetSavableResourceLinks.
  virtual void SavableResourceLinksResponse(
      RenderFrameHostImpl* source,
      const std::vector<GURL>& resources_list,
      blink::mojom::ReferrerPtr referrer,
      const std::vector<blink::mojom::SavableSubframePtr>& subframes) {}

  // Notified that the renderer returned an error after calling
  // GetSavableResourceLinks in case the frame contains non-savable content
  // (i.e. from a non-savable scheme) or if there were errors gathering the
  // links.
  virtual void SavableResourceLinksError(RenderFrameHostImpl* source) {}

  // Called when |RenderFrameHostImpl::lifecycle_state()| changes i.e., when
  // RenderFrameHost LifecycleState changes from old_state to new_state.
  virtual void RenderFrameHostStateChanged(
      RenderFrameHost* host,
      RenderFrameHostImpl::LifecycleState old_state,
      RenderFrameHostImpl::LifecycleState new_state) {}

 protected:
  virtual ~RenderFrameHostDelegate() = default;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_DELEGATE_H_
