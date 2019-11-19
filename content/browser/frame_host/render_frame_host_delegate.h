// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_DELEGATE_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_DELEGATE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/media_player_watch_time.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/visibility.h"
#include "content/public/common/javascript_dialog_type.h"
#include "content/public/common/resource_load_info.mojom.h"
#include "content/public/common/resource_type.h"
#include "media/mojo/services/media_metrics_provider.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_response_headers.h"
#include "services/device/public/mojom/geolocation_context.mojom.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/frame/blocked_navigation_types.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
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
class FileChooserParams;
class FullscreenOptions;
}
}  // namespace blink

namespace content {
class FileSelectListener;
class FrameTreeNode;
class InterstitialPage;
class PageState;
class RenderFrameHost;
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
  // This is used to give the delegate a chance to filter IPC messages.
  virtual bool OnMessageReceived(RenderFrameHostImpl* render_frame_host,
                                 const IPC::Message& message);

  // Allows the delegate to filter incoming associated inteface requests.
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
  virtual void OnDidBlockNavigation(const GURL& blocked_url,
                                    const GURL& initiator_url,
                                    blink::NavigationBlockedReason reason) {}

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
                                   IPC::Message* reply_msg) {}

  virtual void RunBeforeUnloadConfirm(RenderFrameHost* render_frame_host,
                                      bool is_reload,
                                      IPC::Message* reply_msg) {}

  // Called when a file selection is to be done.
  // Overrides of this function must call either listener->FileSelected() or
  // listener->FileSelectionCanceled().
  virtual void RunFileChooser(
      RenderFrameHost* render_frame_host,
      std::unique_ptr<content::FileSelectListener> listener,
      const blink::mojom::FileChooserParams& params);

  // Request to enumerate a directory.  This is equivalent to running the file
  // chooser in directory-enumeration mode and having the user select the given
  // directory.
  // Overrides of this function must call either listener->FileSelected() or
  // listener->FileSelectionCanceled().
  virtual void EnumerateDirectory(RenderFrameHost* render_frame_host,
                                  std::unique_ptr<FileSelectListener> listener,
                                  const base::FilePath& directory_path);

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

  // Return this object cast to a WebContents, if it is one. If the object is
  // not a WebContents, returns NULL.
  virtual WebContents* GetAsWebContents();

  // Returns this object cast to an InterstitialPage if it is one. Returns
  // nullptr otherwise.
  virtual InterstitialPage* GetAsInterstitialPage();

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

  // Called when accessibility events or location changes are received
  // from a render frame, when the accessibility mode has the
  // ui::AXMode::kWebContents flag set.
  virtual void AccessibilityEventReceived(
      const AXEventNotificationDetails& details) {}
  virtual void AccessibilityLocationChangesReceived(
      const std::vector<AXLocationChangeNotificationDetails>& details) {}

  // Find a guest RenderFrameHost by its parent |render_frame_host| and
  // |browser_plugin_instance_id|.
  virtual RenderFrameHost* GetGuestByInstanceID(
      RenderFrameHost* render_frame_host,
      int browser_plugin_instance_id);

  // Gets the GeolocationContext associated with this delegate.
  virtual device::mojom::GeolocationContext* GetGeolocationContext();

#if defined(OS_ANDROID)
  // Gets an NFC implementation within the context of this delegate.
  virtual void GetNFC(mojo::PendingReceiver<device::mojom::NFC> receiver);
#endif

  // Notification that the frame wants to go into fullscreen mode.
  // |origin| represents the origin of the frame that requests fullscreen.
  virtual void EnterFullscreenMode(
      const GURL& origin,
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
      const GURL& url);

  // Called by |frame| to notify that it has received an update on focused
  // element. |bounds_in_root_view| is the rectangle containing the element that
  // is focused and is with respect to root frame's RenderWidgetHost's
  // coordinate space.
  virtual void OnFocusedElementChangedInFrame(
      RenderFrameHostImpl* frame,
      const gfx::Rect& bounds_in_root_view) {}

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
  // Note: this is not called "ShowWindow" because that will clash with
  // the Windows function which is actually a #define.
  virtual void ShowCreatedWindow(int process_id,
                                 int main_frame_widget_route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_rect,
                                 bool user_gesture) {}

  // Notifies that mixed content was displayed or ran.
  virtual void DidDisplayInsecureContent() {}
  virtual void DidContainInsecureFormAction() {}
  virtual void DidRunInsecureContent(const GURL& security_origin,
                                     const GURL& target_url) {}

  // Reports that passive mixed content was found at the specified url.
  virtual void PassiveInsecureContentFound(const GURL& resource_url) {}

  // Checks if running of active mixed content is allowed for the specified
  // WebContents/tab.
  virtual bool ShouldAllowRunningInsecureContent(WebContents* web_contents,
                                                 bool allowed_per_prefs,
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

  // Notifies that the render frame started loading a subresource.
  virtual void SubresourceResponseStarted(
      const url::Origin& origin_of_final_response_url,
      net::CertStatus cert_status) {}

  // Notifies that the render finished loading a subresource for the frame
  // associated with |render_frame_host|.
  virtual void ResourceLoadComplete(
      RenderFrameHost* render_frame_host,
      const GlobalRequestID& request_id,
      mojom::ResourceLoadInfoPtr resource_load_info) {}

  // Request to print a frame that is in a different process than its parent.
  virtual void PrintCrossProcessSubframe(const gfx::Rect& rect,
                                         int document_cookie,
                                         RenderFrameHost* render_frame_host) {}

  // Updates the Picture-in-Picture controller with the relevant viz::SurfaceId
  // of the video to be in Picture-in-Picture mode.
  virtual void UpdatePictureInPictureSurfaceId(const viz::SurfaceId& surface_id,
                                               const gfx::Size& natural_size) {}

  // Returns the visibility of the delegate.
  virtual Visibility GetVisibility();

  // Get the UKM source ID for current content from the last committed
  // cross-document navigation. This is for providing data about the
  // content to the URL-keyed metrics service. Use this method if UKM events
  // should be attributed to the navigation that led to the creation of this
  // document, that is, attribute events following navigations within the same
  // document to the same source. Note: This is also exposed by the
  // RenderWidgetHostDelegate class.
  virtual ukm::SourceId GetUkmSourceIdForLastCommittedSource() const;

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

  virtual void MediaWatchTimeChanged(
      const content::MediaPlayerWatchTime& watch_time) {}

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

  virtual media::MediaMetricsProvider::RecordAggregateWatchTimeCallback
  GetRecordAggregateWatchTimeCallback();

 protected:
  virtual ~RenderFrameHostDelegate() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_DELEGATE_H_
