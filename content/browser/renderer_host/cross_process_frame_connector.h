// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_CROSS_PROCESS_FRAME_CONNECTOR_H_
#define CONTENT_BROWSER_RENDERER_HOST_CROSS_PROCESS_FRAME_CONNECTOR_H_

#include <stdint.h>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "cc/input/touch_action.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom.h"
#include "third_party/blink/public/mojom/frame/viewport_intersection_state.mojom.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "third_party/blink/public/mojom/input/pointer_lock_result.mojom-shared.h"
#include "ui/display/screen_infos.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {
struct FrameVisualProperties;
class WebGestureEvent;
}  // namespace blink

namespace cc {
class RenderFrameMetadata;
}

namespace input {
class RenderWidgetHostViewInput;
}  // namespace input

namespace ui {
class Cursor;
}

namespace viz {
class SurfaceId;
class SurfaceInfo;
}  // namespace viz

namespace content {
class RenderFrameHostImpl;
class RenderFrameProxyHost;
class RenderWidgetHostViewBase;
class RenderWidgetHostViewChildFrame;

// CrossProcessFrameConnector provides the platform view abstraction for
// RenderWidgetHostViewChildFrame allowing RWHVChildFrame to remain ignorant
// of RenderFrameHost.
//
// The RenderWidgetHostView of an out-of-process child frame needs to
// communicate with the RenderFrameProxyHost representing this frame in the
// process of the parent frame. For example, assume you have this page:
//
//   -----------------
//   | frame 1       |
//   |  -----------  |
//   |  | frame 2 |  |
//   |  -----------  |
//   -----------------
//
// If frames 1 and 2 are in process A and B, there are 4 hosts:
//   A1 - RFH for frame 1 in process A
//   B1 - RFPH for frame 1 in process B
//   A2 - RFPH for frame 2 in process A
//   B2 - RFH for frame 2 in process B
//
// B2, having a parent frame in a different process, will have a
// RenderWidgetHostViewChildFrame. This RenderWidgetHostViewChildFrame needs
// to communicate with A2 because the embedding frame represents the platform
// that the child frame is rendering into -- it needs information necessary for
// compositing child frame textures, and also can pass platform messages such as
// view resizing. CrossProcessFrameConnector bridges between B2's
// RenderWidgetHostViewChildFrame and A2 to allow for this communication.
// (Note: B1 is only mentioned for completeness. It is not needed in this
// example.)
//
// CrossProcessFrameConnector objects are owned by the RenderFrameProxyHost
// in the child frame's RenderFrameHostManager corresponding to the parent's
// SiteInstance, A2 in the picture above. When a child frame navigates in a new
// process, SetView() is called to update to the new view.
//
class CONTENT_EXPORT CrossProcessFrameConnector {
 public:
  // |frame_proxy_in_parent_renderer| corresponds to A2 in the example above.
  explicit CrossProcessFrameConnector(
      RenderFrameProxyHost* frame_proxy_in_parent_renderer);

  CrossProcessFrameConnector(const CrossProcessFrameConnector&) = delete;
  CrossProcessFrameConnector& operator=(const CrossProcessFrameConnector&) =
      delete;

  virtual ~CrossProcessFrameConnector();

  // |view| corresponds to B2's RenderWidgetHostViewChildFrame in the example
  // above.
  RenderWidgetHostViewChildFrame* get_view_for_testing() { return view_; }

  void SetView(RenderWidgetHostViewChildFrame* view, bool allow_paint_holding);

  // Returns the parent RenderWidgetHostView or nullptr if it doesn't have one.
  virtual RenderWidgetHostViewBase* GetParentRenderWidgetHostView();

  // Returns the view for the top-level frame under the same WebContents.
  virtual RenderWidgetHostViewBase* GetRootRenderWidgetHostView();

  // Notify the frame connector that the renderer process has terminated.
  void RenderProcessGone();

  // Provide the SurfaceInfo to the embedder, which becomes a reference to the
  // current view's Surface that is included in higher-level compositor
  // frames. This is virtual to be overridden in tests.
  virtual void FirstSurfaceActivation(const viz::SurfaceInfo& surface_info) {}

  // Sends the given intrinsic sizing information from a sub-frame to
  // its corresponding remote frame in the parent frame's renderer.
  void SendIntrinsicSizingInfoToParent(blink::mojom::IntrinsicSizingInfoPtr);

  // Record and apply new visual properties for the subframe. If 'propagate' is
  // true, the new properties will be sent to the subframe's renderer process.
  void SynchronizeVisualProperties(
      const blink::FrameVisualProperties& visual_properties,
      bool propagate = true);

  double css_zoom_factor() const { return last_received_css_zoom_factor_; }

  // Return the size of the CompositorFrame to use in the child renderer.
  const gfx::Size& local_frame_size_in_pixels() const {
    return local_frame_size_in_pixels_;
  }

  // Return the size of the CompositorFrame to use in the child renderer in DIP.
  // This is used to set the layout size of the child renderer.
  const gfx::Size& local_frame_size_in_dip() const {
    return local_frame_size_in_dip_;
  }

  // Return the rect in DIP that the RenderWidgetHostViewChildFrame's content
  // will render into.
  const gfx::Rect& rect_in_parent_view_in_dip() const {
    return rect_in_parent_view_in_dip_;
  }

  // Return the latest capture sequence number for this subframe.
  uint32_t capture_sequence_number() const { return capture_sequence_number_; }

  // Request that the platform change the mouse cursor when the mouse is
  // positioned over this view's content.
  void UpdateCursor(const ui::Cursor& cursor);

  // Given a point in the current view's coordinate space, return the same
  // point transformed into the coordinate space of the top-level view's
  // coordinate space.
  gfx::PointF TransformPointToRootCoordSpace(const gfx::PointF& point,
                                             const viz::SurfaceId& surface_id);

  // Transform a point into the coordinate space of the root
  // RenderWidgetHostView, for the current view's coordinate space.
  // Returns false if |target_view| and |view_| do not have the same root
  // RenderWidgetHostView. RenderWidgetHostViewInput is the abstract class that
  // defines the interface for handling user input and is one to one with
  // RenderWidgetHostViewBase in the browser.
  bool TransformPointToCoordSpaceForView(
      const gfx::PointF& point,
      input::RenderWidgetHostViewInput* target_view,
      const viz::SurfaceId& local_surface_id,
      gfx::PointF* transformed_point);

  // Pass acked touchpad pinch or double tap gesture events to the root view
  // for processing.
  void ForwardAckedTouchpadZoomEvent(
      const blink::WebGestureEvent& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result);

  // A gesture scroll sequence that is not consumed by a child must be bubbled
  // to ancestors who may consume it.
  // Returns false if the scroll event could not be bubbled. The caller must
  // not attempt to bubble the rest of the scroll sequence in this case.
  // Otherwise, returns true.
  // Made virtual for test override.
  [[nodiscard]] virtual bool BubbleScrollEvent(
      const blink::WebGestureEvent& event);

  // These values are written to logs. Do not renumber or delete existing items;
  // add new entries to the end of the list.
  enum class RootViewFocusState {
    // RootView is NULL.
    kNullView = 0,
    // Root View is already focused.
    kFocused = 1,
    // Root View is not focused at TouchStart. Calls
    // RenderWidgetHostViewChildFrame::Focus() to focus it.
    kNotFocused = 2,
    kMaxValue = kNotFocused
  };

  // Determines whether the root RenderWidgetHostView (and thus the current
  // page) has focus. We need a tri-state enum as a return variable to
  // differentiate between the cases where root view is NULL and when it's
  // actually focused/unfocused. No behaviour change expected in focus handling.
  RootViewFocusState HasFocus();

  // Cause the root RenderWidgetHostView to become focused.
  void FocusRootView();

  // Locks the mouse pointer, if |request_unadjusted_movement_| is true, try
  // setting the unadjusted movement mode. Returns true if mouse pointer is
  // locked.
  blink::mojom::PointerLockResult LockPointer(bool request_unadjusted_movement);

  // Change the current pointer lock to match the unadjusted movement option
  // given.
  blink::mojom::PointerLockResult ChangePointerLock(
      bool request_unadjusted_movement);

  // Unlocks the mouse pointer if it is locked.
  void UnlockPointer();

  // Returns the state of the frame's intersection with the top-level viewport.
  const blink::mojom::ViewportIntersectionState& intersection_state() const {
    return intersection_state_;
  }

  // Returns the viz::LocalSurfaceId propagated from the parent to be
  // used by this child frame.
  const viz::LocalSurfaceId& local_surface_id() const {
    return local_surface_id_;
  }

  // Returns the ScreenInfos propagated from the parent to be used by this
  // child frame.
  const display::ScreenInfos& screen_infos() const { return screen_infos_; }

  // Informs the parent the child will enter auto-resize mode, automatically
  // resizing itself to the provided |min_size| and |max_size| constraints.
  void EnableAutoResize(const gfx::Size& min_size, const gfx::Size& max_size);

  // Turns off auto-resize mode.
  void DisableAutoResize();

  // Determines whether the current view's content is inert, either because
  // an HTMLDialogElement is being modally displayed in a higher-level frame,
  // or because the inert attribute has been specified.
  bool IsInert() const;

  // Returns the inherited effective touch action property that should be
  // applied to any nested child RWHVCFs inside the caller RWHVCF.
  cc::TouchAction InheritedEffectiveTouchAction() const;

  // Determines whether the RenderWidgetHostViewChildFrame is hidden due to
  // a higher-level embedder being hidden. This is distinct from the
  // RenderWidgetHostImpl being hidden, which is a property set when
  // RenderWidgetHostView::Hide() is called on the current view.
  bool IsHidden() const;

  // IsThrottled() indicates that the frame is outside of it's parent frame's
  // visible viewport, and should be render throttled.
  bool IsThrottled() const;
  // IsSubtreeThrottled() indicates that IsThrottled() is true for one of this
  // frame's ancestors, which means this frame must also be throttled.
  bool IsSubtreeThrottled() const;
  // IsDisplayLocked() indicates that a DOM ancestor of this frame's owning
  // <iframe> element in the parent frame is currently display locked; or that
  // IsDisplayLocked() is true for one of this frame's ancestors; which means
  // this frame should be render throttled.
  bool IsDisplayLocked() const;

  // Called by RenderWidgetHostViewChildFrame when the child frame has updated
  // its visual properties and its viz::LocalSurfaceId has changed.
  void DidUpdateVisualProperties(const cc::RenderFrameMetadata& metadata);

  bool has_size() const { return has_size_; }

  void DidAckGestureEvent(const blink::WebGestureEvent& event,
                          blink::mojom::InputEventResultState ack_result);

  // Called by RenderWidgetHostViewChildFrame to update the visibility of any
  // nested child RWHVCFs inside it.
  void SetVisibilityForChildViews(bool visible) const;

  // Called to resize the child renderer's CompositorFrame.
  // |local_frame_size| is in pixels if zoom-for-dsf is enabled, and in DIP
  // if not.
  void SetLocalFrameSize(const gfx::Size& local_frame_size);

  // Called to resize the child renderer. |rect_in_parent_view| is in physical
  // pixels.
  void SetRectInParentView(const gfx::Rect& rect_in_parent_view);

  void SetIsInert(bool inert);

  // Handlers for messages received from the parent frame called
  // from RenderFrameProxyHost to be sent to |view_|.
  void OnSetInheritedEffectiveTouchAction(cc::TouchAction);
  void OnVisibilityChanged(blink::mojom::FrameVisibility visibility);

  // Exposed for tests.
  RenderWidgetHostViewBase* GetRootRenderWidgetHostViewForTesting() {
    return GetRootRenderWidgetHostView();
  }

  void UpdateRenderThrottlingStatus(bool is_throttled,
                                    bool subtree_throttled,
                                    bool display_locked);
  void UpdateViewportIntersection(
      const blink::mojom::ViewportIntersectionState& intersection_state,
      const std::optional<blink::FrameVisualProperties>& visual_properties);

  // These enums back crashed frame histograms - see MaybeLogCrash() and
  // MaybeLogShownCrash() below.  Please do not modify or remove existing enum
  // values.  When adding new values, please also update enums.xml. See
  // enums.xml for descriptions of enum values.
  enum class CrashVisibility {
    kCrashedWhileVisible = 0,
    kShownAfterCrashing = 1,
    kNeverVisibleAfterCrash = 2,
    kShownWhileAncestorIsLoading = 3,
    kMaxValue = kShownWhileAncestorIsLoading
  };

  enum class ShownAfterCrashingReason {
    kTabWasShown = 0,
    kViewportIntersection = 1,
    kVisibility = 2,
    kViewportIntersectionAfterTabWasShown = 3,
    kVisibilityAfterTabWasShown = 4,
    kMaxValue = kVisibilityAfterTabWasShown
  };

  // Returns whether the child widget is actually visible to the user.  This is
  // different from the IsHidden override, and takes into account viewport
  // intersection as well as the visibility of the RenderFrameHostDelegate.
  bool IsVisible();

  // This function is called by the RenderFrameHostDelegate to signal that it
  // became visible.
  void DelegateWasShown();

  // Handlers for messages received from the parent frame.
  void OnSynchronizeVisualProperties(
      const blink::FrameVisualProperties& visual_properties);

  blink::mojom::FrameVisibility visibility() const { return visibility_; }

  void set_child_frame_crash_shown_closure_for_testing(
      base::OnceClosure closure) {
    child_frame_crash_shown_closure_for_testing_ = std::move(closure);
  }

 protected:
  friend class MockCrossProcessFrameConnector;
  friend class SitePerProcessBrowserTestBase;

  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewChildFrameZoomForDSFTest,
                           CompositorViewportPixelSize);

  // Resets the rect and the viz::LocalSurfaceId of the connector to ensure the
  // unguessable surface ID is not reused after a cross-process navigation.
  void ResetRectInParentView();

  // Logs the Stability.ChildFrameCrash.Visibility metric after checking that a
  // crash has indeed happened and checking that the crash has not already been
  // logged in UMA.  Returns true if this metric was actually logged.
  bool MaybeLogCrash(CrashVisibility visibility);

  // Check if a crashed child frame has become visible, and if so, log the
  // Stability.ChildFrameCrash.Visibility.ShownAfterCrashing* metrics.
  void MaybeLogShownCrash(ShownAfterCrashingReason reason);

  void UpdateViewportIntersectionInternal(
      const blink::mojom::ViewportIntersectionState& intersection_state,
      bool include_visual_properties);

  // The RenderWidgetHostView for the frame. Initially nullptr.
  raw_ptr<RenderWidgetHostViewChildFrame> view_ = nullptr;

  // This is here rather than in the implementation class so that
  // intersection_state() can return a reference.
  blink::mojom::ViewportIntersectionState intersection_state_;

  display::ScreenInfos screen_infos_;
  gfx::Size local_frame_size_in_dip_;
  gfx::Size local_frame_size_in_pixels_;
  gfx::Rect rect_in_parent_view_in_dip_;

  viz::LocalSurfaceId local_surface_id_;

  bool has_size_ = false;

  uint32_t capture_sequence_number_ = 0u;

  // Gets the current RenderFrameHost for the
  // |frame_proxy_in_parent_renderer_|'s (i.e., the child frame's)
  // FrameTreeNode. This corresponds to B2 in the class-level comment
  // above for CrossProcessFrameConnector.
  RenderFrameHostImpl* current_child_frame_host() const;

  // The RenderFrameProxyHost that routes messages to the parent frame's
  // renderer process.
  // Can be nullptr in tests.
  raw_ptr<RenderFrameProxyHost> frame_proxy_in_parent_renderer_;

  bool is_inert_ = false;
  cc::TouchAction inherited_effective_touch_action_ = cc::TouchAction::kAuto;

  bool is_throttled_ = false;
  bool subtree_throttled_ = false;
  bool display_locked_ = false;

  // Visibility state of the corresponding frame owner element in parent process
  // which is set through CSS or scrolling.
  blink::mojom::FrameVisibility visibility_ =
      blink::mojom::FrameVisibility::kRenderedInViewport;

  // Used to make sure we only log UMA once per renderer crash.
  bool is_crash_already_logged_ = false;

  // Used to make sure that MaybeLogCrash only logs the UMA in case of an actual
  // crash (in case it is called from the destructor of
  // CrossProcessFrameConnector or when WebContentsImpl::WasShown is called).
  bool has_crashed_ = false;

  // Remembers whether or not the RenderFrameHostDelegate (i.e., tab) was
  // shown after a crash. This is only used when recording renderer crashes.
  bool delegate_was_shown_after_crash_ = false;

  // The last pre-transform frame size received from the parent renderer.
  // |last_received_local_frame_size_| may be in DIP if use zoom for DSF is
  // off.
  gfx::Size last_received_local_frame_size_;

  // The last zoom level received from parent renderer, which is used to check
  // if a new surface is created in case of zoom level change.
  double last_received_zoom_level_ = 0.0;

  // Represents CSS zoom applied to the embedding element in the parent.
  double last_received_css_zoom_factor_ = 1.0;

  // Closure that will be run whenever a sad frame is shown and its visibility
  // metrics have been logged. Used for testing only.
  base::OnceClosure child_frame_crash_shown_closure_for_testing_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_CROSS_PROCESS_FRAME_CONNECTOR_H_
