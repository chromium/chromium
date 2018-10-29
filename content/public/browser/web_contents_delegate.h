// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_DELEGATE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/bluetooth_chooser.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/media_stream_request.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/window_container_type.mojom.h"
#include "third_party/blink/public/common/manifest/web_display_mode.h"
#include "third_party/blink/public/mojom/color_chooser/color_chooser.mojom.h"
#include "third_party/blink/public/platform/web_drag_operation.h"
#include "third_party/blink/public/platform/web_security_style.h"
#include "third_party/blink/public/web/web_fullscreen_options.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

class GURL;

namespace base {
class FilePath;
}

namespace blink {
namespace mojom {
class FileChooserParams;
}
}  // namespace blink

namespace content {
class ColorChooser;
class FileSelectListener;
class JavaScriptDialogManager;
class RenderFrameHost;
class RenderProcessHost;
class RenderWidgetHost;
class SessionStorageNamespace;
class SiteInstance;
class WebContentsImpl;
struct ContextMenuParams;
struct DropData;
struct NativeWebKeyboardEvent;
struct Referrer;
struct SecurityStyleExplanations;
}  // namespace content

namespace gfx {
class Rect;
class Size;
}

namespace url {
class Origin;
}

namespace viz {
class SurfaceId;
}  // namespace viz

namespace blink {
class WebGestureEvent;
}

namespace content {

struct OpenURLParams;

enum class KeyboardEventProcessingResult;

// Objects implement this interface to get notified about changes in the
// WebContents and to provide necessary functionality.
class CONTENT_EXPORT WebContentsDelegate {
 public:
  WebContentsDelegate();

  // Opens a new URL inside the passed in WebContents (if source is 0 open
  // in the current front-most tab), unless |disposition| indicates the url
  // should be opened in a new tab or window.
  //
  // A nullptr source indicates the current tab (callers should probably use
  // OpenURL() for these cases which does it for you).

  // Returns the WebContents the URL is opened in, or nullptr if the URL wasn't
  // opened immediately.
  virtual WebContents* OpenURLFromTab(WebContents* source,
                                      const OpenURLParams& params);

  // Allows the delegate to optionally cancel navigations that attempt to
  // transfer to a different process between the start of the network load and
  // commit.  Defaults to true.
  virtual bool ShouldTransferNavigation(bool is_main_frame_navigation);

  // Called to inform the delegate that the WebContents's navigation state
  // changed. The |changed_flags| indicates the parts of the navigation state
  // that have been updated.
  virtual void NavigationStateChanged(WebContents* source,
                                      InvalidateTypes changed_flags) {}

  // Called to inform the delegate that the WebContent's visible
  // security state changed and that security UI should be updated.
  virtual void VisibleSecurityStateChanged(WebContents* source) {}

  // Creates a new tab with the already-created WebContents 'new_contents'.
  // The window for the added contents should be reparented correctly when this
  // method returns.  If |disposition| is NEW_POPUP, |initial_rect| should hold
  // the initial position and size. If |was_blocked| is non-nullptr, then
  // |*was_blocked| will be set to true if the popup gets blocked, and left
  // unchanged otherwise.
  virtual void AddNewContents(WebContents* source,
                              std::unique_ptr<WebContents> new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_rect,
                              bool user_gesture,
                              bool* was_blocked) {}

  // Selects the specified contents, bringing its container to the front.
  virtual void ActivateContents(WebContents* contents) {}

  // Notifies the delegate that this contents is starting or is done loading
  // some resource. The delegate should use this notification to represent
  // loading feedback. See WebContents::IsLoading()
  // |to_different_document| will be true unless the load is a fragment
  // navigation, or triggered by history.pushState/replaceState.
  virtual void LoadingStateChanged(WebContents* source,
                                   bool to_different_document) {}

  // Notifies the delegate that the page has made some progress loading.
  // |progress| is a value between 0.0 (nothing loaded) to 1.0 (page fully
  // loaded).
  virtual void LoadProgressChanged(WebContents* source,
                                   double progress) {}

  // Request the delegate to close this web contents, and do whatever cleanup
  // it needs to do.
  virtual void CloseContents(WebContents* source) {}

  // Request the delegate to resize this WebContents to the specified size in
  // screen coordinates. The embedder is free to ignore the request.
  virtual void SetContentsBounds(WebContents* source, const gfx::Rect& bounds) {
  }

  // Notification that the target URL has changed.
  virtual void UpdateTargetURL(WebContents* source,
                               const GURL& url) {}

  // Notification that there was a mouse event, along with the type of event.
  // If |motion| is true, this is a normal motion event. If |exited| is true,
  // the pointer left the contents area.
  virtual void ContentsMouseEvent(WebContents* source,
                                  bool motion,
                                  bool exited) {}

  // Request the delegate to change the zoom level of the current tab.
  virtual void ContentsZoomChange(bool zoom_in) {}

  // Called to determine if the WebContents can be overscrolled with touch/wheel
  // gestures.
  virtual bool CanOverscrollContent() const;

  // Invoked prior to showing before unload handler confirmation dialog.
  virtual void WillRunBeforeUnloadConfirm() {}

  // Returns true if javascript dialogs and unload alerts are suppressed.
  // Default is false.
  virtual bool ShouldSuppressDialogs(WebContents* source);

  // Returns whether pending NavigationEntries for aborted browser-initiated
  // navigations should be preserved (and thus returned from GetVisibleURL).
  // Defaults to false.
  virtual bool ShouldPreserveAbortedURLs(WebContents* source);

  // A message was added to the console of a frame of the page. Returning true
  // indicates that the delegate handled the message. If false is returned the
  // default logging mechanism will be used for the message.
  virtual bool DidAddMessageToConsole(WebContents* source,
                                      int32_t level,
                                      const base::string16& message,
                                      int32_t line_no,
                                      const base::string16& source_id);

  // Tells us that we've finished firing this tab's beforeunload event.
  // The proceed bool tells us whether the user chose to proceed closing the
  // tab. Returns true if the tab can continue on firing its unload event.
  // If we're closing the entire browser, then we'll want to delay firing
  // unload events until all the beforeunload events have fired.
  virtual void BeforeUnloadFired(WebContents* tab,
                                 bool proceed,
                                 bool* proceed_to_fire_unload);

  // Returns true if the location bar should be focused by default rather than
  // the page contents. NOTE: this is only used if WebContents can't determine
  // for itself whether the location bar should be focused by default. For a
  // complete check, you should use WebContents::FocusLocationBarByDefault().
  virtual bool ShouldFocusLocationBarByDefault(WebContents* source);

  // Sets focus to the location bar or some other place that is appropriate.
  // This is called when the tab wants to encourage user input, like for the
  // new tab page.
  virtual void SetFocusToLocationBar(bool select_all) {}

  // Returns whether the page should be focused when transitioning from crashed
  // to live. Default is true.
  virtual bool ShouldFocusPageAfterCrash();

  // Returns whether the page should resume accepting requests for the new
  // window. This is used when window creation is asynchronous
  // and the navigations need to be delayed. Default is true.
  virtual bool ShouldResumeRequestsForCreatedWindow();

  // This is called when WebKit tells us that it is done tabbing through
  // controls on the page. Provides a way for WebContentsDelegates to handle
  // this. Returns true if the delegate successfully handled it.
  virtual bool TakeFocus(WebContents* source,
                         bool reverse);

  // Invoked when the page loses mouse capture.
  virtual void LostCapture() {}

  // Asks the delegate if the given tab can download.
  // Invoking the |callback| synchronously is OK.
  virtual void CanDownload(const GURL& url,
                           const std::string& request_method,
                           const base::Callback<void(bool)>& callback);

  // Returns true if the context menu operation was handled by the delegate.
  virtual bool HandleContextMenu(const content::ContextMenuParams& params);

  // Allows delegates to handle keyboard events before sending to the renderer.
  // See enum for description of return values.
  virtual KeyboardEventProcessingResult PreHandleKeyboardEvent(
      WebContents* source,
      const NativeWebKeyboardEvent& event);

  // Allows delegates to handle unhandled keyboard messages coming back from
  // the renderer. Returns true if the event was handled, false otherwise. A
  // true value means no more processing should happen on the event. The default
  // return value is false
  virtual bool HandleKeyboardEvent(WebContents* source,
                                   const NativeWebKeyboardEvent& event);

  // Allows delegates to handle gesture events before sending to the renderer.
  // Returns true if the |event| was handled and thus shouldn't be processed
  // by the renderer's event handler. Note that the touch events that create
  // the gesture are always passed to the renderer since the gesture is created
  // and dispatched after the touches return without being "preventDefault()"ed.
  virtual bool PreHandleGestureEvent(
      WebContents* source,
      const blink::WebGestureEvent& event);

  // Called when an external drag event enters the web contents window. Return
  // true to allow dragging and dropping on the web contents window or false to
  // cancel the operation. This method is used by Chromium Embedded Framework.
  virtual bool CanDragEnter(WebContents* source,
                            const DropData& data,
                            blink::WebDragOperationsMask operations_allowed);

  // Shows the repost form confirmation dialog box.
  virtual void ShowRepostFormWarningDialog(WebContents* source) {}

  // Allows delegate to override navigation to the history entries.
  // Returns true to allow WebContents to continue with the default processing.
  virtual bool OnGoToEntryOffset(int offset);

  // Allows delegate to control whether a new WebContents can be created by
  // |web_contents|.
  //
  // The route ID parameters passed to this method are associated with the
  // |source_site_instance|'s RenderProcessHost. They may also be
  // MSG_ROUTING_NONE. If they are valid, they correspond to a trio of
  // RenderView, RenderFrame, and RenderWidget objects that have been created in
  // the renderer, but not yet assigned a WebContents, RenderViewHost,
  // RenderFrameHost, or RenderWidgetHost.
  //
  // The return value is interpreted as follows:
  //
  //   Return true: |web_contents| should create a WebContents.
  //   Return false: |web_contents| should not create a WebContents. The
  //       provisionally-created RenderView (if it exists) in the renderer
  //       process will be destroyed, UNLESS the delegate, during this method,
  //       itself creates a WebContents using |source_site_instance|,
  //       |route_id|, |main_frame_route_id|, and |main_frame_widget_route_id|
  //       as creation parameters. If this happens, the delegate assumes
  //       ownership of the corresponding RenderView, etc. |web_contents| will
  //       detect that this has happened by looking for the existence of a
  //       RenderViewHost in |source_site_instance| with |route_id|.
  virtual bool ShouldCreateWebContents(
      WebContents* web_contents,
      RenderFrameHost* opener,
      SiteInstance* source_site_instance,
      int32_t route_id,
      int32_t main_frame_route_id,
      int32_t main_frame_widget_route_id,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url,
      const std::string& partition_id,
      SessionStorageNamespace* session_storage_namespace);

  // Notifies the delegate about the creation of a new WebContents. This
  // typically happens when popups are created.
  virtual void WebContentsCreated(WebContents* source_contents,
                                  int opener_render_process_id,
                                  int opener_render_frame_id,
                                  const std::string& frame_name,
                                  const GURL& target_url,
                                  WebContents* new_contents) {}

  // Notifies the embedder that a Portal WebContents was created.
  virtual void PortalWebContentsCreated(WebContents* portal_web_contents) {}

  // Notification that one of the frames in the WebContents is hung. |source| is
  // the WebContents that is hung, and |render_widget_host| is the
  // RenderWidgetHost that, while routing events to it, discovered the hang.
  //
  // |hang_monitor_restarter| can be used to restart the timer used to
  // detect the hang.  The timer is typically restarted when the renderer has
  // become active, the tab got hidden, or the user has chosen to wait some
  // more.
  //
  // Useful member functions on |render_widget_host|:
  // - Getting the hung render process: GetProcess()
  // - Querying whether the process is still hung: IsCurrentlyUnresponsive()
  virtual void RendererUnresponsive(
      WebContents* source,
      RenderWidgetHost* render_widget_host,
      base::RepeatingClosure hang_monitor_restarter) {}

  // Notification that a process in the WebContents is no longer hung. |source|
  // is the WebContents that was hung, and |render_widget_host| is the
  // RenderWidgetHost that was passed in an earlier call to
  // RendererUnresponsive().
  virtual void RendererResponsive(WebContents* source,
                                  RenderWidgetHost* render_widget_host) {}

  // Invoked when a main fram navigation occurs.
  virtual void DidNavigateMainFramePostCommit(WebContents* source) {}

  // Returns a pointer to a service to manage JavaScript dialogs. May return
  // nullptr in which case dialogs aren't shown.
  virtual JavaScriptDialogManager* GetJavaScriptDialogManager(
      WebContents* source);

  // Called when color chooser should open. Returns the opened color chooser.
  // Returns nullptr if we failed to open the color chooser (e.g. when there is
  // a ColorChooserDialog already open on Windows). Ownership of the returned
  // pointer is transferred to the caller.
  virtual ColorChooser* OpenColorChooser(
      WebContents* web_contents,
      SkColor color,
      const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions);

  // Called when a file selection is to be done.
  // This function is responsible for calling listener->FileSelected() or
  // listener->FileSelectionCanceled().
  virtual void RunFileChooser(RenderFrameHost* render_frame_host,
                              std::unique_ptr<FileSelectListener> listener,
                              const blink::mojom::FileChooserParams& params);

  // Request to enumerate a directory.  This is equivalent to running the file
  // chooser in directory-enumeration mode and having the user select the given
  // directory.
  // This function is responsible for calling listener->FileSelected() or
  // listener->FileSelectionCanceled().
  virtual void EnumerateDirectory(WebContents* web_contents,
                                  std::unique_ptr<FileSelectListener> listener,
                                  const base::FilePath& path);

  // Shows a chooser for the user to select a nearby Bluetooth device. The
  // observer must live at least as long as the returned chooser object.
  virtual std::unique_ptr<BluetoothChooser> RunBluetoothChooser(
      RenderFrameHost* frame,
      const BluetoothChooser::EventHandler& event_handler);

  // Returns true if the delegate will embed a WebContents-owned fullscreen
  // render widget.  In this case, the delegate may access the widget by calling
  // WebContents::GetFullscreenRenderWidgetHostView().  If false is returned,
  // WebContents will be responsible for showing the fullscreen widget.
  virtual bool EmbedsFullscreenWidget() const;

  // Called when the renderer puts a tab into fullscreen mode.
  // |origin| is the origin of the initiating frame inside the |web_contents|.
  // |origin| can be empty in which case the |web_contents| last committed
  // URL's origin should be used.
  virtual void EnterFullscreenModeForTab(
      WebContents* web_contents,
      const GURL& origin,
      const blink::WebFullscreenOptions& options) {}

  // Called when the renderer puts a tab out of fullscreen mode.
  virtual void ExitFullscreenModeForTab(WebContents*) {}

  virtual bool IsFullscreenForTabOrPending(
      const WebContents* web_contents) const;

  // Returns the actual display mode of the top-level browsing context.
  // For example, it should return 'blink::WebDisplayModeFullscreen' whenever
  // the browser window is put to fullscreen mode (either by the end user,
  // or HTML API or from a web manifest setting).
  // See http://w3c.github.io/manifest/#dfn-display-mode
  virtual blink::WebDisplayMode GetDisplayMode(
      const WebContents* web_contents) const;

  // Register a new handler for URL requests with the given scheme.
  // |user_gesture| is true if the registration is made in the context of a user
  // gesture.
  virtual void RegisterProtocolHandler(WebContents* web_contents,
                                       const std::string& protocol,
                                       const GURL& url,
                                       bool user_gesture) {}

  // Unregister the registered handler for URL requests with the given scheme.
  // |user_gesture| is true if the registration is made in the context of a user
  // gesture.
  virtual void UnregisterProtocolHandler(WebContents* web_contents,
                                         const std::string& protocol,
                                         const GURL& url,
                                         bool user_gesture) {}

  // Result of string search in the page. This includes the number of matches
  // found and the selection rect (in screen coordinates) for the string found.
  // If |final_update| is false, it indicates that more results follow.
  virtual void FindReply(WebContents* web_contents,
                         int request_id,
                         int number_of_matches,
                         const gfx::Rect& selection_rect,
                         int active_match_ordinal,
                         bool final_update) {}

#if defined(OS_ANDROID)
  // Provides the rects of the current find-in-page matches.
  // Sent as a reply to RequestFindMatchRects.
  virtual void FindMatchRectsReply(WebContents* web_contents,
                                   int version,
                                   const std::vector<gfx::RectF>& rects,
                                   const gfx::RectF& active_rect) {}
#endif

  // Invoked when the preferred size of the contents has been changed.
  virtual void UpdatePreferredSize(WebContents* web_contents,
                                   const gfx::Size& pref_size) {}

  // Invoked when the contents auto-resized and the container should match it.
  virtual void ResizeDueToAutoResize(WebContents* web_contents,
                                     const gfx::Size& new_size) {}

  // Requests to lock the mouse. Once the request is approved or rejected,
  // GotResponseToLockMouseRequest() will be called on the requesting tab
  // contents.
  virtual void RequestToLockMouse(WebContents* web_contents,
                                  bool user_gesture,
                                  bool last_unlocked_by_target) {}

  // Notification that the page has lost the mouse lock.
  virtual void LostMouseLock() {}

  // Requests keyboard lock. Once the request is approved or rejected,
  // GotResponseToKeyboardLockRequest() will be called on |web_contents|.
  virtual void RequestKeyboardLock(WebContents* web_contents,
                                   bool esc_key_locked) {}

  // Notification that the keyboard lock request has been canceled.
  virtual void CancelKeyboardLockRequest(WebContents* web_contents) {}

  // Asks permission to use the camera and/or microphone. If permission is
  // granted, a call should be made to |callback| with the devices. If the
  // request is denied, a call should be made to |callback| with an empty list
  // of devices. |request| has the details of the request (e.g. which of audio
  // and/or video devices are requested, and lists of available devices).
  virtual void RequestMediaAccessPermission(WebContents* web_contents,
                                            const MediaStreamRequest& request,
                                            MediaResponseCallback callback);

  // Checks if we have permission to access the microphone or camera. Note that
  // this does not query the user. |type| must be MEDIA_DEVICE_AUDIO_CAPTURE
  // or MEDIA_DEVICE_VIDEO_CAPTURE.
  virtual bool CheckMediaAccessPermission(RenderFrameHost* render_frame_host,
                                          const GURL& security_origin,
                                          MediaStreamType type);

  // Returns the ID of the default device for the given media device |type|.
  // If the returned value is an empty string, it means that there is no
  // default device for the given |type|.
  virtual std::string GetDefaultMediaDeviceID(WebContents* web_contents,
                                              MediaStreamType type);

#if defined(OS_ANDROID)
  // Returns true if the given media should be blocked to load.
  virtual bool ShouldBlockMediaRequest(const GURL& url);

  // Tells the delegate to enter overlay mode.
  // Overlay mode means that we are currently using AndroidOverlays to display
  // video, and that the compositor's surface should support alpha and not be
  // marked as opaque. See media/base/android/android_overlay.h.
  virtual void SetOverlayMode(bool use_overlay_mode);
#endif

  // Requests permission to access the PPAPI broker. The delegate should return
  // true and call the passed in |callback| with the result, or return false
  // to indicate that it does not support asking for permission.
  virtual bool RequestPpapiBrokerPermission(
      WebContents* web_contents,
      const GURL& url,
      const base::FilePath& plugin_path,
      const base::Callback<void(bool)>& callback);

  // Returns the size for the new render view created for the pending entry in
  // |web_contents|; if there's no size, returns an empty size.
  // This is optional for implementations of WebContentsDelegate; if the
  // delegate doesn't provide a size, the current WebContentsView's size will be
  // used.
  virtual gfx::Size GetSizeForNewRenderView(WebContents* web_contents) const;

  // Returns true if the WebContents is never visible.
  virtual bool IsNeverVisible(WebContents* web_contents);

  // Called in response to a request to save a frame. If this returns true, the
  // default behavior is suppressed.
  virtual bool SaveFrame(const GURL& url, const Referrer& referrer);

  // Can be overridden by a delegate to return the security style of the
  // given |web_contents|, populating |security_style_explanations| to
  // explain why the SecurityStyle was downgraded. Returns
  // WebSecurityStyleUnknown if not overriden.
  virtual blink::WebSecurityStyle GetSecurityStyle(
      WebContents* web_contents,
      SecurityStyleExplanations* security_style_explanations);

  // Requests the app banner. This method is called from the DevTools.
  virtual void RequestAppBannerFromDevTools(content::WebContents* web_contents);

  // Called when a suspicious navigation of the main frame has been blocked.
  // Allows the delegate to provide some UI to let the user know about the
  // blocked navigation and give them the option to recover from it. The given
  // URL is the blocked navigation target.
  virtual void OnDidBlockFramebust(content::WebContents* web_contents,
                                   const GURL& url) {}

  // Reports that passive mixed content was found at the specified url.
  virtual void PassiveInsecureContentFound(const GURL& resource_url) {}

  // Checks if running of active mixed content is allowed for the specified
  // WebContents/tab.
  virtual bool ShouldAllowRunningInsecureContent(WebContents* web_contents,
                                                 bool allowed_per_prefs,
                                                 const url::Origin& origin,
                                                 const GURL& resource_url);

  virtual void SetTopControlsShownRatio(WebContents* web_contents,
                                        float ratio) {}

  // Requests to get browser controls info such as the height of the top/bottom
  // controls, and whether they will shrink the Blink's view size.
  // Note that they are not complete in the sense that there is no API to tell
  // content to poll these values again, except part of resize. But this is not
  // needed by embedder because it's always accompanied by view size change.
  virtual int GetTopControlsHeight() const;
  virtual int GetBottomControlsHeight() const;
  virtual bool DoBrowserControlsShrinkRendererSize(
      const WebContents* web_contents) const;

  // Propagates to the browser that gesture scrolling has changed state. This is
  // used by the browser to assist in controlling the behavior of sliding the
  // top controls as a result of page gesture scrolling while in tablet mode.
  virtual void SetTopControlsGestureScrollInProgress(bool in_progress);

  // Give WebContentsDelegates the opportunity to adjust the previews state.
  virtual void AdjustPreviewsStateForNavigation(
      content::WebContents* web_contents,
      PreviewsState* previews_state) {}

  // Requests to print an out-of-process subframe for the specified WebContents.
  // |rect| is the rectangular area where its content resides in its parent
  // frame. |document_cookie| is a unique id for a printed document associated
  // with
  //                   a print job.
  // |subframe_host| is the render frame host of the subframe to be printed.
  virtual void PrintCrossProcessSubframe(WebContents* web_contents,
                                         const gfx::Rect& rect,
                                         int document_cookie,
                                         RenderFrameHost* subframe_host) const {
  }

  // Notifies the Picture-in-Picture controller that there is a new player
  // entering Picture-in-Picture.
  // Returns the size of the Picture-in-Picture window.
  virtual gfx::Size EnterPictureInPicture(const viz::SurfaceId&,
                                          const gfx::Size& natural_size);

  // Updates the Picture-in-Picture controller with a signal that
  // Picture-in-Picture mode has ended.
  virtual void ExitPictureInPicture();

#if defined(OS_ANDROID)
  // Updates information to determine whether a user gesture should carryover to
  // future navigations. This is needed so navigations within a certain
  // timeframe of a request initiated by a gesture will be treated as if they
  // were initiated by a gesture too, otherwise the navigation may be blocked.
  virtual void UpdateUserGestureCarryoverInfo(WebContents* web_contents) {}
#endif

  // Requests the delegate to replace |old_contents| with |new_contents| in the
  // container that holds |old_contents|. If the  delegate successfully replaces
  // |old_contents|, the return parameter passes ownership of |old_contents|.
  // Otherwise, |new_contents| is returned.
  // |did_finish_load| is true if WebContentsObserver::DidFinishLoad() has
  // already been called for |new_contents|.
  virtual std::unique_ptr<content::WebContents> SwapWebContents(
      content::WebContents* old_contents,
      std::unique_ptr<content::WebContents> new_contents,
      bool did_start_load,
      bool did_finish_load);

 protected:
  virtual ~WebContentsDelegate();

 private:
  friend class WebContentsImpl;

  // Called when |this| becomes the WebContentsDelegate for |source|.
  void Attach(WebContents* source);

  // Called when |this| is no longer the WebContentsDelegate for |source|.
  void Detach(WebContents* source);

  // The WebContents that this is currently a delegate for.
  std::set<WebContents*> attached_contents_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_DELEGATE_H_
