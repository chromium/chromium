// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_DELEGATE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/eye_dropper.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/serial_chooser.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/window_container_type.mojom-forward.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "third_party/blink/public/mojom/choosers/color_chooser.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/blocked_navigation_types.mojom.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

class GURL;

namespace base {
class FilePath;
template <typename T>
class WeakPtr;
}

namespace blink {
namespace mojom {
class FileChooserParams;
class WindowFeatures;
}
}  // namespace blink

namespace content {
class ColorChooser;
class EyeDropperListener;
class FileSelectListener;
class JavaScriptDialogManager;
class RenderFrameHost;
class RenderWidgetHost;
class SessionStorageNamespace;
class SiteInstance;
struct ContextMenuParams;
struct DropData;
struct MediaPlayerWatchTime;
struct NativeWebKeyboardEvent;
struct Referrer;
}  // namespace content

namespace device {
namespace mojom {
class GeolocationContext;
}
}  // namespace device

namespace gfx {
class Rect;
class Size;
}

namespace url {
class Origin;
}

namespace blink {
class WebGestureEvent;
enum class ProtocolHandlerSecurityLevel;
}

namespace content {

class AudioStreamBrokerFactory;
struct OpenURLParams;

enum class KeyboardEventProcessingResult;

// Result of an EnterPictureInPicture request.
enum class PictureInPictureResult {
  // The request was successful.
  kSuccess,

  // Picture-in-Picture is not supported by the embedder.
  kNotSupported,
};

// Objects implement this interface to get notified about changes in the
// WebContents and to provide necessary functionality. If a method doesn't
// change state, e.g. has no return value, then it can move to
// WebContentsObserver if many places want to observe the change. If the
// implementation of one of the methods below would be shared by many or all of
// WebContentsDelegate implementations then it can go on ContentBrowserClient.
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
  virtual bool ShouldAllowRendererInitiatedCrossProcessNavigation(
      bool is_outermost_main_frame_navigation);

  // Called to inform the delegate that the WebContents's navigation state
  // changed. The |changed_flags| indicates the parts of the navigation state
  // that have been updated.
  virtual void NavigationStateChanged(WebContents* source,
                                      InvalidateTypes changed_flags) {}

  // Called to inform the delegate that the WebContent's visible
  // security state changed and that security UI should be updated.
  virtual void VisibleSecurityStateChanged(WebContents* source) {}

  // Creates a new tab with the already-created WebContents `new_contents`.
  // The window for the added contents should be reparented correctly when this
  // method returns. `target_url` is set to the value provided when
  // `new_contents` was created. If `disposition` is NEW_POPUP,
  // `window_features` should hold the initial position, size and other
  // properties of the window. If `was_blocked` is non-nullptr, then
  // `*was_blocked` will be set to true if the popup gets blocked, and left
  // unchanged otherwise.
  virtual void AddNewContents(
      WebContents* source,
      std::unique_ptr<WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) {}

  // Selects the specified contents, bringing its container to the front.
  virtual void ActivateContents(WebContents* contents) {}

  // Notifies the delegate that this contents is starting or is done loading
  // some resource. The delegate should use this notification to represent
  // loading feedback. See WebContents::IsLoading()
  // |should_show_loading_ui| indicates whether a load start should be visible
  // in UI elements. It is generally true for different-document navigations and
  // false for most same-document navigations (because same-documents are
  // typically instantaneous so there's no point in flickering the UI). The
  // exception is the navigation API's intercept(), which is the sole type
  // of same-document navigation that is asynchronous, and therefore a UI change
  // is sensible.
  virtual void LoadingStateChanged(WebContents* source,
                                   bool should_show_loading_ui) {}

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
  virtual bool CanOverscrollContent();

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
  // NOTE: If you only need to monitor messages added to the console, rather
  // than change the behavior when they are added, prefer using
  // WebContentsObserver::OnDidAddMessageToConsole().
  virtual bool DidAddMessageToConsole(
      WebContents* source,
      blink::mojom::ConsoleMessageLevel log_level,
      const std::u16string& message,
      int32_t line_no,
      const std::u16string& source_id);

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
  virtual void SetFocusToLocationBar() {}

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

  // Asks the delegate if the given tab can download.
  // Invoking the |callback| synchronously is OK.
  virtual void CanDownload(const GURL& url,
                           const std::string& request_method,
                           base::OnceCallback<void(bool)> callback);

  // Asks the delegate to open/show the context menu based on `params`.
  //
  // The `render_frame_host` represents the frame that requests the context menu
  // (typically this frame is focused, but this is not necessarily the case -
  // see https://crbug.com/1257907#c14).
  //
  // Returns true if the context menu operation was handled by the delegate.
  virtual bool HandleContextMenu(RenderFrameHost& render_frame_host,
                                 const ContextMenuParams& params);

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
                            blink::DragOperationsMask operations_allowed);

  // Shows the repost form confirmation dialog box.
  virtual void ShowRepostFormWarningDialog(WebContents* source) {}

  // Allows delegate to override navigation to the history entries.
  // Returns true to allow WebContents to continue with the default processing.
  virtual bool OnGoToEntryOffset(int offset);

  // Allows delegate to control whether a new WebContents can be created by
  // the WebContents itself.
  //
  // If an delegate returns true, it can optionally also override
  // CreateCustomWebContents() below to provide their own WebContents.
  virtual bool IsWebContentsCreationOverridden(
      SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url);

  // Allow delegate to creates a custom WebContents when
  // WebContents::CreateNewWindow() is called. This function is only called
  // when IsWebContentsCreationOverridden() returns true.
  //
  // In general, a delegate should return a pointer to a created WebContents
  // so that the opener can be given a references to it as appropriate.
  // Returning nullptr also makes sense if the delegate wishes to suppress
  // all window creation, or if the delegate wants to ensure the opener
  // cannot get a reference effectively creating a new browsing instance.
  virtual WebContents* CreateCustomWebContents(
      RenderFrameHost* opener,
      SiteInstance* source_site_instance,
      bool is_new_browsing_instance,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url,
      const StoragePartitionConfig& partition_config,
      SessionStorageNamespace* session_storage_namespace);

  // Notifies the delegate about the creation of a new WebContents. This
  // typically happens when popups are created.
  virtual void WebContentsCreated(WebContents* source_contents,
                                  int opener_render_process_id,
                                  int opener_render_frame_id,
                                  const std::string& frame_name,
                                  const GURL& target_url,
                                  WebContents* new_contents) {}

  // Notifies the embedder that a new WebContents has been created to contain
  // the contents of a portal.
  virtual void PortalWebContentsCreated(WebContents* portal_web_contents) {}

  // Notifies the embedder that an existing WebContents that it manages (e.g., a
  // browser tab) has become the contents of a portal.
  //
  // During portal activation, WebContentsDelegate::ActivatePortalWebContents
  // will be called to release the delegate's management of a WebContents.
  // Shortly afterward, the portal will assume ownership of the contents and
  // call this function to indicate that this is complete, passing the
  // swapped-out contents as |portal_web_contents|.
  //
  // Implementations will likely want to apply changes analogous to those they
  // would apply to a new WebContents in PortalWebContentsCreated.
  virtual void WebContentsBecamePortal(WebContents* portal_web_contents) {}

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

  // Returns a pointer to a service to manage JavaScript dialogs. May return
  // nullptr in which case dialogs aren't shown.
  virtual JavaScriptDialogManager* GetJavaScriptDialogManager(
      WebContents* source);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC)
  // Called when color chooser should open. Returns the opened color chooser.
  // Returns nullptr if we failed to open the color chooser. The color chooser
  // is only supported/required for Android.
  virtual std::unique_ptr<ColorChooser> OpenColorChooser(
      WebContents* web_contents,
      SkColor color,
      const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC)

  // Called when an eye dropper should open. Returns the eye dropper window.
  // The eye dropper is responsible for calling listener->ColorSelected() or
  // listener->ColorSelectionCanceled().
  // The ownership of the returned pointer is transferred to the caller.
  virtual std::unique_ptr<EyeDropper> OpenEyeDropper(
      RenderFrameHost* frame,
      EyeDropperListener* listener);

  // Called when a file selection is to be done.
  // This function is responsible for calling listener->FileSelected() or
  // listener->FileSelectionCanceled().
  virtual void RunFileChooser(RenderFrameHost* render_frame_host,
                              scoped_refptr<FileSelectListener> listener,
                              const blink::mojom::FileChooserParams& params);

  // Request to enumerate a directory.  This is equivalent to running the file
  // chooser in directory-enumeration mode and having the user select the given
  // directory.
  // This function is responsible for calling listener->FileSelected() or
  // listener->FileSelectionCanceled().
  virtual void EnumerateDirectory(WebContents* web_contents,
                                  scoped_refptr<FileSelectListener> listener,
                                  const base::FilePath& path);

  // Creates an info bar for the user to control the receiving of the SMS.
  virtual void CreateSmsPrompt(RenderFrameHost*,
                               const std::vector<url::Origin>&,
                               const std::string& one_time_code,
                               base::OnceCallback<void()> on_confirm,
                               base::OnceCallback<void()> on_cancel);

  // Returns whether entering fullscreen with |EnterFullscreenModeForTab()| is
  // allowed.
  virtual bool CanEnterFullscreenModeForTab(
      RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options);

  // Called when the renderer puts a tab into fullscreen mode.
  // |requesting_frame| is the specific content frame requesting fullscreen.
  // |CanEnterFullscreenModeForTab()| must return true on entry.
  virtual void EnterFullscreenModeForTab(
      RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) {}

  virtual void FullscreenStateChangedForTab(
      RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) {}

  // Called when the renderer puts a tab out of fullscreen mode.
  virtual void ExitFullscreenModeForTab(WebContents*) {}

  // Returns true if the given `web_contents` is, or is transitioning to
  // tab-fullscreen.
  virtual bool IsFullscreenForTabOrPending(const WebContents* web_contents);

  // Overload of IsFullscreenForTabOrPending which also outputs the current or
  // target display of the fullscreen tab. If the function returns true and
  // `display_id` is not nullptr, the target display ID of the tab will be
  // written to `display_id`.
  virtual bool IsFullscreenForTabOrPending(const WebContents* web_contents,
                                           int64_t* display_id);

  // Returns the actual display mode of the top-level browsing context.
  // For example, it should return 'blink::mojom::DisplayModeFullscreen'
  // whenever the browser window is put to fullscreen mode (either by the end
  // user, or HTML API or from a web manifest setting). See
  // http://w3c.github.io/manifest/#dfn-display-mode
  virtual blink::mojom::DisplayMode GetDisplayMode(
      const WebContents* web_contents);

  // Returns the security level to use for Navigator.RegisterProtocolHandler().
  virtual blink::ProtocolHandlerSecurityLevel GetProtocolHandlerSecurityLevel(
      RenderFrameHost* requesting_frame);

  // Register a new handler for URL requests with the given scheme.
  // |user_gesture| is true if the registration is made in the context of a user
  // gesture.
  virtual void RegisterProtocolHandler(RenderFrameHost* requesting_frame,
                                       const std::string& protocol,
                                       const GURL& url,
                                       bool user_gesture) {}

  // Unregister the registered handler for URL requests with the given scheme.
  // |user_gesture| is true if the registration is made in the context of a user
  // gesture.
  virtual void UnregisterProtocolHandler(RenderFrameHost* requesting_frame,
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

#if BUILDFLAG(IS_ANDROID)
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
                                  bool last_unlocked_by_target);

  // Notification that the page has lost the mouse lock.
  virtual void LostMouseLock() {}

  // Requests keyboard lock. Once the request is approved or rejected,
  // GotResponseToKeyboardLockRequest() will be called on |web_contents|.
  virtual void RequestKeyboardLock(WebContents* web_contents,
                                   bool esc_key_locked);

  // Notification that the keyboard lock request has been canceled.
  virtual void CancelKeyboardLockRequest(WebContents* web_contents) {}

  // Asks permission to use the camera and/or microphone. If permission is
  // granted, a call should be made to |callback| with the devices. If the
  // request is denied, a call should be made to |callback| with an empty list
  // of devices. |request| has the details of the request (e.g. which of audio
  // and/or video devices are requested, and lists of available devices).
  virtual void RequestMediaAccessPermission(
      WebContents* web_contents,
      const MediaStreamRequest& request,
      content::MediaResponseCallback callback);

  // Checks if we have permission to access the microphone or camera. Note that
  // this does not query the user. |type| must be MEDIA_DEVICE_AUDIO_CAPTURE
  // or MEDIA_DEVICE_VIDEO_CAPTURE.
  virtual bool CheckMediaAccessPermission(RenderFrameHost* render_frame_host,
                                          const GURL& security_origin,
                                          blink::mojom::MediaStreamType type);

  // Returns the ID of the default device for the given media device |type|.
  // If the returned value is an empty string, it means that there is no
  // default device for the given |type|.
  virtual std::string GetDefaultMediaDeviceID(
      WebContents* web_contents,
      blink::mojom::MediaStreamType type);

  // Returns the human-readable name for title in Media Controls.
  // If the returned value is an empty string, it means that there is no
  // human-readable name.
  // For example, this returns an extension name for title instead of extension
  // url.
  virtual std::string GetTitleForMediaControls(WebContents* web_contents);

  // Returns AudioStreamBrokerFactory to use to create AudioStreamBroker when
  // creating audio I/O streams. Returned `AudioStreamBrokerFactory` is used and
  // deleted on the IO thread.
  virtual std::unique_ptr<AudioStreamBrokerFactory>
  CreateAudioStreamBrokerFactory(WebContents* web_contents);

#if BUILDFLAG(IS_ANDROID)
  // Returns true if the given media should be blocked to load.
  virtual bool ShouldBlockMediaRequest(const GURL& url);

  // Tells the delegate to enter overlay mode.
  // Overlay mode means that we are currently using AndroidOverlays to display
  // video, and that the compositor's surface should support alpha and not be
  // marked as opaque. See media/base/android/android_overlay.h.
  virtual void SetOverlayMode(bool use_overlay_mode) {}
#endif

  // Returns the size for the new render view created for the pending entry in
  // |web_contents|; if there's no size, returns an empty size.
  // This is optional for implementations of WebContentsDelegate; if the
  // delegate doesn't provide a size, the current WebContentsView's size will be
  // used.
  virtual gfx::Size GetSizeForNewRenderView(WebContents* web_contents);

  // Returns true if the WebContents is never user-visible, thus the renderer
  // never needs to produce pixels for display.
  virtual bool IsNeverComposited(WebContents* web_contents);

  // Askss |guest_web_contents| to perform the same. If this returns true, the
  // default behavior is suppressed.
  virtual bool GuestSaveFrame(WebContents* guest_web_contents);

  // Called in response to a request to save a frame. If this returns true, the
  // default behavior is suppressed.
  virtual bool SaveFrame(const GURL& url,
                         const Referrer& referrer,
                         content::RenderFrameHost* rfh);

  // Called when a suspicious navigation of the main frame has been blocked.
  // Allows the delegate to provide some UI to let the user know about the
  // blocked navigation and give them the option to recover from it.
  // |blocked_url| is the blocked navigation target, |initiator_url| is the URL
  // of the frame initiating the navigation, |reason| specifies why the
  // navigation was blocked.
  virtual void OnDidBlockNavigation(
      WebContents* web_contents,
      const GURL& blocked_url,
      const GURL& initiator_url,
      blink::mojom::NavigationBlockedReason reason) {}

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

  // Requests to get browser controls info such as the height/min height of the
  // top/bottom controls, and whether to animate these changes to height or
  // whether they will shrink the Blink's view size. Note that they are not
  // complete in the sense that there is no API to tell content to poll these
  // values again, except part of resize. But this is not needed by embedder
  // because it's always accompanied by view size change.
  virtual int GetTopControlsHeight();
  virtual int GetTopControlsMinHeight();
  virtual int GetBottomControlsHeight();
  virtual int GetBottomControlsMinHeight();
  virtual bool ShouldAnimateBrowserControlsHeightChanges();
  virtual bool DoBrowserControlsShrinkRendererSize(WebContents* web_contents);
  virtual int GetVirtualKeyboardHeight(WebContents* web_contents);
  // Returns true if the top controls should only expand at the top of the page,
  // so they'll only be visible if the page is scrolled to the top.
  virtual bool OnlyExpandTopControlsAtPageTop();

  // Propagates to the browser that gesture scrolling has changed state. This is
  // used by the browser to assist in controlling the behavior of sliding the
  // top controls as a result of page gesture scrolling while in tablet mode.
  virtual void SetTopControlsGestureScrollInProgress(bool in_progress) {}

  // Requests to print an out-of-process subframe for the specified WebContents.
  // |rect| is the rectangular area where its content resides in its parent
  // frame. |document_cookie| is a unique id for a printed document associated
  // with a print job. |subframe_host| is the RenderFrameHost of the subframe
  // to be printed.
  virtual void PrintCrossProcessSubframe(WebContents* web_contents,
                                         const gfx::Rect& rect,
                                         int document_cookie,
                                         RenderFrameHost* subframe_host) const {
  }

  // Requests to capture a paint preview of a subframe for the specified
  // WebContents. |rect| is the rectangular area where its content resides in
  // its parent frame. |guid| is a globally unique identitier for an entire
  // paint preview. |render_frame_host| is the RenderFrameHost of the subframe
  // to be captured.
  virtual void CapturePaintPreviewOfSubframe(
      WebContents* web_contents,
      const gfx::Rect& rect,
      const base::UnguessableToken& guid,
      RenderFrameHost* render_frame_host) {}

  // Notifies the Picture-in-Picture controller that there is a new player
  // entering Picture-in-Picture.
  // Returns the result of the enter request.
  virtual PictureInPictureResult EnterPictureInPicture(
      WebContents* web_contents);

  // Updates the Picture-in-Picture controller with a signal that
  // Picture-in-Picture mode has ended.
  virtual void ExitPictureInPicture() {}

#if BUILDFLAG(IS_ANDROID)
  // Updates information to determine whether a user gesture should carryover to
  // future navigations. This is needed so navigations within a certain
  // timeframe of a request initiated by a gesture will be treated as if they
  // were initiated by a gesture too, otherwise the navigation may be blocked.
  virtual void UpdateUserGestureCarryoverInfo(WebContents* web_contents) {}
#endif

  // Returns true if lazy loading of images and frames should be enabled.
  virtual bool ShouldAllowLazyLoad();

  // Return true if the back forward cache is supported. This is not an
  // indication that the cache will be used.
  virtual bool IsBackForwardCacheSupported();

  // Returns content::PreloadingEligibility::kEligible if Prerender2 (see
  // content/browser/preloading/prerender/README.md for details) is supported.
  // If it is not supported, returns the reason.
  virtual content::PreloadingEligibility IsPrerender2Supported(
      WebContents& web_contents);

  // Requests the delegate to replace |predecessor_contents| with
  // |portal_contents| in the container that holds |predecessor_contents|. If
  // the delegate successfully replaces |predecessor_contents|, the return
  // parameter passes ownership of |predecessor_contents|. Otherwise,
  // |portal_contents| is returned.
  virtual std::unique_ptr<WebContents> ActivatePortalWebContents(
      WebContents* predecessor_contents,
      std::unique_ptr<WebContents> portal_contents);

  // If |old_contents| is being inspected by a DevTools window, it updates the
  // window to inspect |new_contents| instead and calls |callback| after it
  // finishes asynchronously. If no window is present, or no update is
  // necessary, |callback| is run synchronously (immediately on the same stack).
  virtual void UpdateInspectedWebContentsIfNecessary(
      WebContents* old_contents,
      WebContents* new_contents,
      base::OnceCallback<void()> callback);

  // Returns true if the widget's frame content needs to be stored before
  // eviction and displayed until a new frame is generated. If false, a white
  // solid color is displayed instead.
  virtual bool ShouldShowStaleContentOnEviction(WebContents* source);

  // Returns the user-visible WebContents that is responsible for the activity
  // in the provided WebContents. For example, this delegate may be aware that
  // the contents is embedded in some other contents, or hosts background
  // activity on behalf of a user-visible tab which should be used to display
  // dialogs and similar affordances to the user.
  //
  // This may be distinct from the outer web contents (for example, the
  // responsible contents may logically "own" a contents but not currently embed
  // it for rendering).
  //
  // For most delegates (where the WebContents is a tab, window or other
  // directly user-visible feature), simply returning the contents is
  // appropriate.
  virtual WebContents* GetResponsibleWebContents(WebContents* web_contents);

  // Invoked when media playback is interrupted or completed.
  virtual void MediaWatchTimeChanged(const MediaPlayerWatchTime& watch_time) {}

  // Returns a  InstalledWebappGeolocationContext if this web content is running
  // in a installed webapp and geolocation should be deleagted from the
  // installed webapp; otherwise returns nullptr.
  virtual device::mojom::GeolocationContext*
  GetInstalledWebappGeolocationContext();

  // Returns a weak ptr to the web contents delegate.
  virtual base::WeakPtr<WebContentsDelegate> GetDelegateWeakPtr();

  // Whether the WebContents is privileged.
  // It's used to prevent drag and drop between privileged and non-privileged
  // WebContents.
  virtual bool IsPrivileged();

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
