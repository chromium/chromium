/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_WIDGET_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_WIDGET_CLIENT_H_

#include <memory>

#include "cc/input/event_listener_properties.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/layers/layer.h"
#include "cc/paint/paint_worklet_layer_painter.h"
#include "cc/trees/layer_tree_host.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_drag_operation.h"
#include "third_party/blink/public/platform/web_gesture_event.h"
#include "third_party/blink/public/platform/web_intrinsic_sizing_info.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_screen_info.h"
#include "third_party/blink/public/platform/web_touch_action.h"
#include "third_party/blink/public/web/web_meaningful_layout.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "third_party/blink/public/web/web_text_direction.h"

class SkBitmap;

namespace cc {
struct ElementId;
class LayerTreeMutator;
class ScopedDeferMainFrameUpdate;
class PaintImage;
}

namespace gfx {
class Point;
class Vector2d;
}

namespace blink {
class WebDragData;
class WebGestureEvent;
class WebString;
class WebWidget;
struct WebCursorInfo;
struct WebFloatPoint;
struct WebFloatRect;
struct WebFloatSize;
class WebLocalFrame;

class WebWidgetClient {
 public:
  virtual ~WebWidgetClient() = default;

  // Sets an object which the compositor uses to ask blink for mutations on the
  // compositor thread, in order to modify compositor state directly, avoiding
  // the need to generate and commit main frames, and avoiding the potentially-
  // janky main thread. This is used to allow AnimationWorklet to operate in
  // sync with composited animations running ahead of the main frame state.
  virtual void SetLayerTreeMutator(std::unique_ptr<cc::LayerTreeMutator>) {}

  // Similar to the |SetLayerTreeMutator|, but used by PaintWorklet.
  virtual void SetPaintWorkletLayerPainterClient(
      std::unique_ptr<cc::PaintWorkletLayerPainter>) {}

  // Sets the root layer of the tree in the compositor. It may be null to remove
  // the root layer in which case nothing would be shown by the compositor.
  virtual void SetRootLayer(scoped_refptr<cc::Layer>) {}

  // Called to request a BeginMainFrame from the compositor. For tests with
  // single thread and no scheduler, the impl should schedule a task to run
  // a synchronous composite.
  virtual void ScheduleAnimation() {}

  // Show or hide compositor debug visualizations.
  virtual void SetShowFPSCounter(bool) {}
  virtual void SetShowPaintRects(bool) {}
  virtual void SetShowLayoutShiftRegions(bool) {}
  virtual void SetShowDebugBorders(bool) {}
  virtual void SetShowScrollBottleneckRects(bool) {}
  virtual void SetShowHitTestBorders(bool) {}

  // Sets the background color to be filled in as gutter behind/around the
  // painted content. Non-composited WebViews need not implement this, as they
  // paint into another widget which has a background color of its own.
  virtual void SetBackgroundColor(SkColor color) {}

  // A notification callback for when the intrinsic sizing of the
  // widget changed. This is only called for SVG within a remote frame.
  virtual void IntrinsicSizingInfoChanged(const WebIntrinsicSizingInfo&) {}

  // Called immediately following the first compositor-driven (frame-generating)
  // layout that happened after an interesting document lifecyle change (see
  // WebMeaningfulLayout for details.)
  virtual void DidMeaningfulLayout(WebMeaningfulLayout) {}

  // Called when the cursor for the widget changes.
  virtual void DidChangeCursor(const WebCursorInfo&) {}

  virtual void AutoscrollStart(const WebFloatPoint&) {}
  virtual void AutoscrollFling(const WebFloatSize& velocity) {}
  virtual void AutoscrollEnd() {}

  // Called to show the widget according to the given policy.
  virtual void Show(WebNavigationPolicy) {}

  // Returns information about the screen where this view's widgets are being
  // displayed.
  virtual WebScreenInfo GetScreenInfo() { return {}; }

  // Called to get/set the position of the widget's window in screen
  // coordinates. Note, the window includes any decorations such as borders,
  // scrollbars, URL bar, tab strip, etc. if they exist.
  virtual WebRect WindowRect() { return WebRect(); }
  virtual void SetWindowRect(const WebRect&) {}

  // Called to get the view rect in screen coordinates. This is the actual
  // content view area, i.e. doesn't include any window decorations.
  virtual WebRect ViewRect() { return WebRect(); }

  // Called when a tooltip should be shown at the current cursor position.
  virtual void SetToolTipText(const WebString&, WebTextDirection hint) {}

  // Requests to lock the mouse cursor for the |requester_frame| in the
  // widget. If true is returned, the success result will be asynchronously
  // returned via a single call to WebWidget::didAcquirePointerLock() or
  // WebWidget::didNotAcquirePointerLock().
  // If false, the request has been denied synchronously.
  virtual bool RequestPointerLock(WebLocalFrame* requester_frame,
                                  bool request_unadjusted_movement) {
    return false;
  }

  // Cause the pointer lock to be released. This may be called at any time,
  // including when a lock is pending but not yet acquired.
  // WebWidget::didLosePointerLock() is called when unlock is complete.
  virtual void RequestPointerUnlock() {}

  // Returns true iff the pointer is locked to this widget.
  virtual bool IsPointerLocked() { return false; }

  // Called when a gesture event is handled.
  virtual void DidHandleGestureEvent(const WebGestureEvent& event,
                                     bool event_cancelled) {}

  // Called when overscrolled on main thread. All parameters are in
  // viewport-space.
  virtual void DidOverscroll(const WebFloatSize& overscroll_delta,
                             const WebFloatSize& accumulated_overscroll,
                             const WebFloatPoint& position_in_viewport,
                             const WebFloatSize& velocity_in_viewport) {}

  // Requests that a gesture of |injected_type| be reissued at a later point in
  // time. |injected_type| is required to be one of
  // GestureScroll{Begin,Update,End}. The dispatched gesture will scroll the
  // ScrollableArea identified by |scrollable_area_element_id| by the given
  // delta + granularity.
  virtual void InjectGestureScrollEvent(
      WebGestureDevice device,
      const WebFloatSize& delta,
      ui::input_types::ScrollGranularity granularity,
      cc::ElementId scrollable_area_element_id,
      WebInputEvent::Type injected_type) {}

  // Set the browser's behavior when overscroll happens, e.g. whether to glow
  // or navigate.
  virtual void SetOverscrollBehavior(const cc::OverscrollBehavior&) {}

  // Called to update if pointerrawupdate events should be sent.
  virtual void SetHasPointerRawUpdateEventHandlers(bool) {}

  // Called to update if touch events should be sent.
  virtual void SetHasTouchEventHandlers(bool) {}

  // Called to update if scroll events should be sent.
  virtual void SetHaveScrollEventHandlers(bool) {}

  // Called to update whether low latency input mode is enabled or not.
  virtual void SetNeedsLowLatencyInput(bool) {}

  // Requests unbuffered (ie. low latency) input until a pointerup
  // event occurs.
  virtual void RequestUnbufferedInputEvents() {}

  // Requests unbuffered (ie. low latency) input due to debugger being
  // attached. Debugger needs to paint when stopped in the event handler.
  virtual void SetNeedsUnbufferedInputForDebugger(bool) {}

  // Called during WebWidget::HandleInputEvent for a TouchStart event to inform
  // the embedder of the touch actions that are permitted for this touch.
  virtual void SetTouchAction(WebTouchAction touch_action) {}

  // Request the browser to show virtual keyboard for current input type.
  virtual void ShowVirtualKeyboardOnElementFocus() {}

  // Converts the |rect| from Blink's Viewport coordinates to the
  // coordinates in the native window used to display the content, in
  // DIP.  They're identical in tradional world, but will differ when
  // use-zoom-for-dsf feature is eanbled, and Viewport coordinates
  // becomes DSF times larger than window coordinates.
  // TODO(oshima): Update the comment when the migration is completed.
  virtual void ConvertViewportToWindow(WebRect* rect) {}

  // Converts the |rect| from Blink's Viewport coordinates to the
  // coordinates in the native window used to display the content, in
  // DIP.  They're identical in tradional world, but will differ when
  // use-zoom-for-dsf feature is eanbled, and Viewport coordinates
  // becomes DSF times larger than window coordinates.
  // TODO(oshima): Update the comment when the migration is completed.
  virtual void ConvertViewportToWindow(WebFloatRect* rect) {}

  // Converts the |rect| from the coordinates in native window in
  // DIP to Blink's Viewport coordinates. They're identical in
  // tradional world, but will differ when use-zoom-for-dsf feature
  // is eanbled.  TODO(oshima): Update the comment when the
  // migration is completed.
  virtual void ConvertWindowToViewport(WebFloatRect* rect) {}

  // Called when a drag-and-drop operation should begin.
  virtual void StartDragging(network::mojom::ReferrerPolicy,
                             const WebDragData&,
                             WebDragOperationsMask,
                             const SkBitmap& drag_image,
                             const gfx::Point& drag_image_offset) {}

  // Double tap zooms a rect in the main-frame renderer.
  virtual void AnimateDoubleTapZoomInMainFrame(const blink::WebPoint& point,
                                               const blink::WebRect& bounds) {}

  // Find in page zooms a rect in the main-frame renderer.
  virtual void ZoomToFindInPageRectInMainFrame(const blink::WebRect& rect) {}

  // Used to update the active selection bounds. Pass a default-constructed
  // LayerSelection to clear it.
  virtual void RegisterSelection(const cc::LayerSelection&) {}

  // Used to call platform API for FallbackCursorMode.
  virtual void FallbackCursorModeLockCursor(bool left,
                                            bool right,
                                            bool up,
                                            bool down) {}
  virtual void FallbackCursorModeSetCursorVisibility(bool visible) {}

  // Sets the current page scale factor and minimum / maximum limits. Both
  // limits are initially 1 (no page scale allowed).
  virtual void SetPageScaleStateAndLimits(float page_scale_factor,
                                          bool is_pinch_gesture_active,
                                          float minimum,
                                          float maximum) {}

  // Starts an animation of the page scale to a target scale factor and scroll
  // offset.
  // If use_anchor is true, destination is a point on the screen that will
  // remain fixed for the duration of the animation.
  // If use_anchor is false, destination is the final top-left scroll position.
  virtual void StartPageScaleAnimation(const gfx::Vector2d& destination,
                                       bool use_anchor,
                                       float new_page_scale,
                                       base::TimeDelta duration) {}

  // For when the embedder itself change scales on the page (e.g. devtools)
  // and wants all of the content at the new scale to be crisp.
  virtual void ForceRecalculateRasterScales() {}

  // Requests an image decode and will have the |callback| run asynchronously
  // when it completes. Forces a new main frame to occur that will trigger
  // pushing the decode through the compositor.
  virtual void RequestDecode(const cc::PaintImage& image,
                             base::OnceCallback<void(bool)> callback) {}

  // SwapResult mirrors the values of cc::SwapPromise::DidNotSwapReason, and
  // should be kept consistent with it. SwapResult additionally adds a success
  // value (kDidSwap).
  // These values are written to logs. New enum values can be added, but
  // existing enums must never be renumbered, deleted or reused.
  enum SwapResult {
    kDidSwap = 0,
    kDidNotSwapSwapFails = 1,
    kDidNotSwapCommitFails = 2,
    kDidNotSwapCommitNoUpdate = 3,
    kDidNotSwapActivationFails = 4,
    kSwapResultMax,
  };
  using ReportTimeCallback =
      base::OnceCallback<void(SwapResult, base::TimeTicks)>;

  // The |callback| will be fired when the corresponding renderer frame is
  // submitted (still called "swapped") to the display compositor (either with
  // DidSwap or DidNotSwap).
  virtual void NotifySwapTime(ReportTimeCallback callback) {}

  // Set or get what event handlers exist in the document contained in the
  // WebWidget in order to inform the compositor thread if it is able to handle
  // an input event, or it needs to pass it to the main thread to be handled.
  // The class is the type of input event, and for each class there is a
  // properties defining if the compositor thread can handle the event.
  virtual void SetEventListenerProperties(cc::EventListenerClass,
                                          cc::EventListenerProperties) {}
  virtual cc::EventListenerProperties EventListenerProperties(
      cc::EventListenerClass) const {
    return cc::EventListenerProperties::kNone;
  }

  // Prevents any updates to the input for the layer tree, and the layer tree
  // itself, and the layer tree from becoming visible.
  virtual std::unique_ptr<cc::ScopedDeferMainFrameUpdate>
  DeferMainFrameUpdate() {
    return nullptr;
  }

  // Start deferring commits to the compositor, allowing document lifecycle
  // updates without committing the layer tree. Commits are deferred
  // until at most the given |timeout| has passed. If multiple calls are made
  // when deferal is active then the initial timeout applies.
  virtual void StartDeferringCommits(base::TimeDelta timeout) {}
  // Immediately stop deferring commits.
  virtual void StopDeferringCommits(cc::PaintHoldingCommitTrigger) {}

  // Enable or disable BeginMainFrameNotExpected signals from the compositor,
  // which are consumed by the blink scheduler.
  virtual void RequestBeginMainFrameNotExpected(bool request) {}

  // A stable numeric Id for the local root's compositor. For tracing/debugging
  // purposes.
  virtual int GetLayerTreeId() const { return 0; }

  // Sets the amount that the top and bottom browser controls are showing, from
  // 0 (hidden) to 1 (fully shown).
  virtual void SetBrowserControlsShownRatio(float top_ratio,
                                            float bottom_ratio) {}

  // Set browser controls height. If |shrink_viewport| is set to true, then
  // Blink shrunk the viewport clip layers by the top and bottom browser
  // controls height. Top controls will translate the web page down and do not
  // immediately scroll when hiding. The bottom controls scroll immediately and
  // never translate the content (only clip it).
  virtual void SetBrowserControlsHeight(float top_height,
                                        float bottom_height,
                                        bool shrink_viewport) {}

  virtual viz::FrameSinkId GetFrameSinkId() {
    NOTREACHED();
    return viz::FrameSinkId();
  }
};

}  // namespace blink

#endif
