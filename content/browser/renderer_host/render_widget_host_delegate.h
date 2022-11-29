// Copyright 2012 The Chromium Authors
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
#include "third_party/abseil-cpp/absl/types/optional.h"
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
class Rect;
class Size;
}

namespace content {

class BrowserAccessibilityManager;
class RenderFrameProxyHost;
class RenderWidgetHostImpl;
class RenderWidgetHostInputEventRouter;
class RenderViewHostDelegateView;
class TextInputManager;
class VisibleTimeRequestTrigger;
enum class KeyboardEventProcessingResult;
struct NativeWebKeyboardEvent;

//
// RenderWidgetHostDelegate
//
//  An interface implemented by an object interested in knowing about the state
//  of the RenderWidgetHost.
//
// Layering note: Generally, WebContentsImpl should be the only implementation
// of this interface. In particular, WebContentsImpl::FromRenderWidgetHostImpl()
// assumes this. This delegate interface is useful for renderer_host/ to make
// requests to WebContentsImpl, as renderer_host/ is not permitted to know the
// WebContents type (see //renderer_host/DEPS).
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

  // If a main frame navigation is in progress, this will return the zoom level
  // for the pending page. Otherwise, this returns the zoom level for the
  // current page. Note that subframe navigations do not affect the zoom level,
  // which is tracked at the level of the page.
  virtual double GetPendingPageZoomLevel();

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

  // Get the root BrowserAccessibilityManager for this frame tree.
  virtual BrowserAccessibilityManager* GetRootBrowserAccessibilityManager();

  // Get the root BrowserAccessibilityManager for this frame tree,
  // or create it if it doesn't exist.
  virtual BrowserAccessibilityManager*
      GetOrCreateRootBrowserAccessibilityManager();

  // Send OS Cut/Copy/Paste actions to the focused frame.
  virtual void ExecuteEditCommand(
      const std::string& command,
      const absl::optional<std::u16string>& value) = 0;
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

  // Returns the Window Control Overlay rectangle. Only applies to an
  // outermost main frame's widget. Other widgets always returns an empty rect.
  virtual gfx::Rect GetWindowsControlsOverlayRect() const;

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

  // Called when the visibility of the RenderFrameProxyHost changes.
  // This method should only handle visibility for inner WebContents and
  // will eventually notify all the RenderWidgetHostViews belonging to that
  // WebContents. If this is not an inner WebContents or the inner WebContents
  // FrameTree root does not match `render_frame_proxy_host` FrameTreeNode it
  // should return false.
  virtual bool OnRenderFrameProxyVisibilityChanged(
      RenderFrameProxyHost* render_frame_proxy_host,
      blink::mojom::FrameVisibility visibility);

  // Update the renderer's cache of the screen rect of the view and window.
  virtual void SendScreenRects() {}

  // Update the renderer's active focus state. This will replicate it for
  // all descendants (including inner frame trees) of the primary page's
  // frame tree.
  virtual void SendActiveState(bool active) {}

  // Returns the TextInputManager tracking text input state.
  virtual TextInputManager* GetTextInputManager();

  // Returns the associated RenderViewHostDelegateView*, if possible.
  virtual RenderViewHostDelegateView* GetDelegateView();

  // Returns true if the provided RenderWidgetHostImpl matches the current
  // RenderWidgetHost on the primary main frame, and false otherwise.
  virtual bool IsWidgetForPrimaryMainFrame(RenderWidgetHostImpl*);

  // Returns the object that tracks the start of content to visible events for
  // the WebContents. May return nullptr if there is no RenderWidgetHostView.
  virtual VisibleTimeRequestTrigger* GetVisibleTimeRequestTrigger();

  // Inner WebContents Helpers -------------------------------------------------
  //
  // These functions are helpers in managing a hierarchy of WebContents
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

  // Get the UKM source ID for current content. This is used for providing
  // data about the content to the URL-keyed metrics service.
  // Note: Prefer using RenderFrameHost::GetPageUkmSourceId wherever
  // possible.
  virtual ukm::SourceId GetCurrentPageUkmSourceId();

  // Returns true if there is context menu shown on page.
  virtual bool IsShowingContextMenuOnPage() const;

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

  // Show the newly created widget with the specified bounds.
  // The widget is identified by the route_id passed to CreateNewWidget.
  virtual void ShowCreatedWidget(int process_id,
                                 int widget_route_id,
                                 const gfx::Rect& initial_rect_in_dips,
                                 const gfx::Rect& initial_anchor_rect_in_dips) {
  }

  // Returns the amount that this view has been resized by a showing virtual
  // keyboard or 0 if the virtual keyboard is hidden or in a mode that doesn't
  // resize the view.
  virtual int GetVirtualKeyboardResizeHeight();

 protected:
  virtual ~RenderWidgetHostDelegate() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_DELEGATE_H_
