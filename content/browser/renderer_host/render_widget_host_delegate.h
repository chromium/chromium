// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_DELEGATE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "build/build_config.h"
#include "components/viz/common/vertical_scroll_direction.h"
#include "content/common/content_export.h"
#include "content/public/common/drop_data.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/gfx/native_widget_types.h"

namespace blink {
class WebMouseEvent;
class WebMouseWheelEvent;
class WebGestureEvent;
}

namespace gfx {
class Point;
class Size;
}

namespace rappor {
class Sample;
}

namespace content {

class BrowserAccessibilityManager;
class FrameTree;
class RenderFrameHostImpl;
class RenderWidgetHostImpl;
class RenderWidgetHostInputEventRouter;
class RenderViewHostDelegateView;
class TextInputManager;
class WebContents;
enum class KeyboardEventProcessingResult;
struct NativeWebKeyboardEvent;

//
// RenderWidgetHostDelegate
//
//  An interface implemented by an object interested in knowing about the state
//  of the RenderWidgetHost.
class CONTENT_EXPORT RenderWidgetHostDelegate {
 public:
  // Functions for controlling the browser top controls slide behavior with page
  // gesture scrolling.
  virtual void SetTopControlsShownRatio(
      RenderWidgetHostImpl* render_widget_host,
      float ratio) {}
  virtual void SetTopControlsGestureScrollInProgress(bool in_progress) {}

  // The RenderWidgetHost has just been created.
  virtual void RenderWidgetCreated(RenderWidgetHostImpl* render_widget_host) {}

  // The RenderWidgetHost is going to be deleted.
  virtual void RenderWidgetDeleted(RenderWidgetHostImpl* render_widget_host) {}

  // The RenderWidgetHost got the focus.
  virtual void RenderWidgetGotFocus(RenderWidgetHostImpl* render_widget_host) {}

  // If a main frame navigation is in progress, this will return the zoom level
  // for the pending page. Otherwise, this returns the zoom level for the
  // current page. Note that subframe navigations do not affect the zoom level,
  // which is tracked at the level of the page.
  virtual double GetPendingPageZoomLevel();

  // The RenderWidgetHost lost the focus.
  virtual void RenderWidgetLostFocus(
      RenderWidgetHostImpl* render_widget_host) {}

  // The RenderWidget was resized.
  virtual void RenderWidgetWasResized(RenderWidgetHostImpl* render_widget_host,
                                      bool width_changed) {}

  // The contents auto-resized and the container should match it.
  virtual void ResizeDueToAutoResize(RenderWidgetHostImpl* render_widget_host,
                                     const gfx::Size& new_size) {}

  // Callback to give the browser a chance to handle the specified keyboard
  // event before sending it to the renderer. See enum for details on return
  // value.
  virtual KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const NativeWebKeyboardEvent& event);

  // Callback to give the browser a chance to handle the specified mouse
  // event before sending it to the renderer.
  // Returns true if the |event| was handled.
  // TODO(carlosil, nasko): remove once committed interstitial pages are
  // fully implemented.
  virtual bool PreHandleMouseEvent(const blink::WebMouseEvent& event);

  // Callback to inform the browser that the renderer did not process the
  // specified events. This gives an opportunity to the browser to process the
  // back/forward mouse buttons.
  virtual bool HandleMouseEvent(const blink::WebMouseEvent& event);

  // Callback to inform the browser that the renderer did not process the
  // specified events. This gives an opportunity to the browser to process the
  // event (used for keyboard shortcuts).
  virtual bool HandleKeyboardEvent(const NativeWebKeyboardEvent& event);

  // Callback to inform the browser that the renderer did not process the
  // specified mouse wheel event.  Returns true if the browser has handled
  // the event itself.
  virtual bool HandleWheelEvent(const blink::WebMouseWheelEvent& event);

  // Notification that an input event from the user was dispatched to the
  // widget.
  virtual void DidReceiveInputEvent(RenderWidgetHostImpl* render_widget_host,
                                    const blink::WebInputEvent& event) {}

  // Asks whether the page is in a state of ignoring input events.
  virtual bool ShouldIgnoreInputEvents();

  // Callback to give the browser a chance to handle the specified gesture
  // event before sending it to the renderer.
  // Returns true if the |event| was handled.
  virtual bool PreHandleGestureEvent(const blink::WebGestureEvent& event);

  // Notifies that screen rects were sent to renderer process.
  virtual void DidSendScreenRects(RenderWidgetHostImpl* rwh) {}

  // Get the root BrowserAccessibilityManager for this frame tree.
  virtual BrowserAccessibilityManager* GetRootBrowserAccessibilityManager();

  // Get the root BrowserAccessibilityManager for this frame tree,
  // or create it if it doesn't exist.
  virtual BrowserAccessibilityManager*
      GetOrCreateRootBrowserAccessibilityManager();

  // Send OS Cut/Copy/Paste actions to the focused frame.
  virtual void ExecuteEditCommand(
      const std::string& command,
      const base::Optional<base::string16>& value) = 0;
  virtual void Undo() = 0;
  virtual void Redo() = 0;
  virtual void Cut() = 0;
  virtual void Copy() = 0;
  virtual void Paste() = 0;
  virtual void PasteAndMatchStyle() = 0;
  virtual void SelectAll() = 0;

  // Requests the renderer to move the selection extent to a new position.
  virtual void MoveRangeSelectionExtent(const gfx::Point& extent) {}

  // Requests the renderer to select the region between two points in the
  // currently focused frame.
  virtual void SelectRange(const gfx::Point& base, const gfx::Point& extent) {}

  // Request the renderer to Move the caret to the new position.
  virtual void MoveCaret(const gfx::Point& extent) {}

  virtual RenderWidgetHostInputEventRouter* GetInputEventRouter();

  // Send page-level focus state to all SiteInstances involved in rendering the
  // current FrameTree, not including the main frame's SiteInstance.
  virtual void ReplicatePageFocus(bool is_focused) {}

  // Get the focused RenderWidgetHost associated with |receiving_widget|. A
  // RenderWidgetHostView, upon receiving a keyboard event, will pass its
  // RenderWidgetHost to this function to determine who should ultimately
  // consume the event.  This facilitates keyboard event routing with
  // out-of-process iframes, where multiple RenderWidgetHosts may be involved
  // in rendering a page, yet keyboard events all arrive at the main frame's
  // RenderWidgetHostView.  When a main frame's RenderWidgetHost is passed in,
  // the function returns the focused frame that should consume keyboard
  // events. In all other cases, the function returns back |receiving_widget|.
  virtual RenderWidgetHostImpl* GetFocusedRenderWidgetHost(
      RenderWidgetHostImpl* receiving_widget);

  // Notification that the renderer has become unresponsive. The
  // delegate can use this notification to show a warning to the user.
  // See also WebContentsDelegate::RendererUnresponsive.
  virtual void RendererUnresponsive(
      RenderWidgetHostImpl* render_widget_host,
      base::RepeatingClosure hang_monitor_restarter) {}

  // Notification that a previously unresponsive renderer has become
  // responsive again. The delegate can use this notification to end the
  // warning shown to the user.
  virtual void RendererResponsive(RenderWidgetHostImpl* render_widget_host) {}

  // Notification that a cross-process subframe on this page has crashed, and a
  // sad frame is shown if the subframe was visible.  |frame_visibility|
  // specifies whether the subframe is visible, scrolled out of view, or hidden
  // (e.g., with "display: none").
  virtual void SubframeCrashed(blink::mojom::FrameVisibility visibility) {}

  // Requests to lock the mouse. Once the request is approved or rejected,
  // GotResponseToLockMouseRequest() will be called on the requesting render
  // widget host. |privileged| means that the request is always granted, used
  // for Pepper Flash.
  virtual void RequestToLockMouse(RenderWidgetHostImpl* render_widget_host,
                                  bool user_gesture,
                                  bool last_unlocked_by_target,
                                  bool privileged) {}

  virtual void UnlockMouse(RenderWidgetHostImpl* render_widget_host) {}

  // Returns whether the associated tab is in fullscreen mode.
  virtual bool IsFullscreen();

  // Returns true if the widget's frame content needs to be stored before
  // eviction and displayed until a new frame is generated. If false, a white
  // solid color is displayed instead.
  virtual bool ShouldShowStaleContentOnEviction();

  // Returns the display mode for all widgets in the frame tree. Only applies
  // to frame-based widgets. Other widgets are always kBrowser.
  virtual blink::mojom::DisplayMode GetDisplayMode() const;

  // Notification that the widget has lost capture.
  virtual void LostCapture(RenderWidgetHostImpl* render_widget_host) {}

  // Notification that the widget has lost the mouse lock.
  virtual void LostMouseLock(RenderWidgetHostImpl* render_widget_host) {}

  // Returns true if |render_widget_host| holds the mouse lock.
  virtual bool HasMouseLock(RenderWidgetHostImpl* render_widget_host);

  // Returns the widget that holds the mouse lock or nullptr if the mouse isn't
  // locked.
  virtual RenderWidgetHostImpl* GetMouseLockWidget();

  // Requests to lock the keyboard. Once the request is approved or rejected,
  // GotResponseToKeyboardLockRequest() will be called on the requesting render
  // widget host.
  virtual bool RequestKeyboardLock(RenderWidgetHostImpl* render_widget_host,
                                   bool esc_key_locked);

  // Cancels a previous keyboard lock request.
  virtual void CancelKeyboardLock(RenderWidgetHostImpl* render_widget_host) {}

  // Returns the widget that holds the keyboard lock or nullptr if not locked.
  virtual RenderWidgetHostImpl* GetKeyboardLockWidget();

  // Called when the visibility of the RenderFrameProxyHost in outer
  // WebContents changes. This method is only called on an inner WebContents and
  // will eventually notify all the RenderWidgetHostViews belonging to that
  // WebContents.
  virtual void OnRenderFrameProxyVisibilityChanged(
      blink::mojom::FrameVisibility visibility) {}

  // Update the renderer's cache of the screen rect of the view and window.
  virtual void SendScreenRects() {}

  // Returns the TextInputManager tracking text input state.
  virtual TextInputManager* GetTextInputManager();

  // Returns true if this RenderWidgetHost should remain hidden. This is used by
  // the RenderWidgetHost to ask the delegate if it can be shown in the event of
  // something other than the WebContents attempting to enable visibility of
  // this RenderWidgetHost.
  // TODO(nasko): Move this to RenderViewHostDelegate.
  virtual bool IsHidden();

  // Returns the associated RenderViewHostDelegateView*, if possible.
  virtual RenderViewHostDelegateView* GetDelegateView();

  // Returns the current Flash fullscreen RenderWidgetHostImpl if any. This is
  // not intended for use with other types of fullscreen, such as HTML
  // fullscreen, and will return nullptr for those cases.
  virtual RenderWidgetHostImpl* GetFullscreenRenderWidgetHost() const;

  // Allow the delegate to handle the cursor update. Returns true if handled.
  virtual bool OnUpdateDragCursor();

  // Returns true if the provided RenderWidgetHostImpl matches the current
  // RenderWidgetHost on the main frame, and false otherwise.
  virtual bool IsWidgetForMainFrame(RenderWidgetHostImpl*);

  // Inner WebContents Helpers -------------------------------------------------
  //
  // These functions are helpers in managing a hierharchy of WebContents
  // involved in rendering inner WebContents.

  // Get the RenderWidgetHost that should receive page level focus events. This
  // will be the widget that is rendering the main frame of the currently
  // focused WebContents.
  virtual RenderWidgetHostImpl* GetRenderWidgetHostWithPageFocus();

  // In cases with multiple RenderWidgetHosts involved in rendering a page, only
  // one widget should be focused and active. This ensures that
  // |render_widget_host| is focused and that its owning WebContents is also
  // the focused WebContents.
  virtual void FocusOwningWebContents(
      RenderWidgetHostImpl* render_widget_host) {}

  // Augment a Rappor sample with eTLD+1 context. The caller is still
  // responsible for logging the sample to the RapporService. Returns false
  // if the eTLD+1 is not known for |render_widget_host|.
  virtual bool AddDomainInfoToRapporSample(rappor::Sample* sample);

  // Return this object cast to a WebContents, if it is one. If the object is
  // not a WebContents, returns nullptr.
  virtual WebContents* GetAsWebContents();

  // Get the UKM source ID for current content. This is used for providing
  // data about the content to the URL-keyed metrics service.
  // Note: Prefer using RenderFrameHost::GetPageUkmSourceId wherever
  // possible.
  virtual ukm::SourceId GetCurrentPageUkmSourceId();

  // Returns true if there is context menu shown on page.
  virtual bool IsShowingContextMenuOnPage() const;

  // Returns the focused frame across all delegates, or nullptr if none.
  virtual RenderFrameHostImpl* GetFocusedFrameFromFocusedDelegate();

  // Invoked when the vertical scroll direction of the root layer changes. Note
  // that if a scroll in a given direction occurs, the scroll is completed, and
  // then another scroll in the *same* direction occurs, we will not consider
  // the second scroll event to have caused a change in direction. Also note
  // note that this API will *never* be called with |kNull| which only exists to
  // indicate the absence of a vertical scroll direction.
  virtual void OnVerticalScrollDirectionChanged(
      viz::VerticalScrollDirection scroll_direction) {}

  // Returns true if the delegate is a portal.
  virtual bool IsPortal();

  // Notify the delegate that the screen orientation has been changed.
  virtual void DidChangeScreenOrientation() {}

  // Returns the FrameTree that this RenderWidgetHost is attached to. If the
  // RenderWidgetHost is attached to a frame, then its RenderFrameHost will be
  // in the tree. Otherwise, the RenderWidgetHost is for a popup which was
  // opened by a frame in the FrameTree.
  virtual FrameTree* GetFrameTree();

 protected:
  virtual ~RenderWidgetHostDelegate() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_DELEGATE_H_
