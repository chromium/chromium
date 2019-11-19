// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_CROSS_PROCESS_FRAME_CONNECTOR_H_
#define CONTENT_BROWSER_FRAME_HOST_CROSS_PROCESS_FRAME_CONNECTOR_H_

#include <stdint.h>

#include "cc/input/touch_action.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/browser/renderer_host/frame_connector_delegate.h"
#include "content/common/content_export.h"
#include "content/common/frame_visual_properties.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom.h"

namespace IPC {
class Message;
}

namespace content {
class RenderFrameProxyHost;

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
class CONTENT_EXPORT CrossProcessFrameConnector
    : public FrameConnectorDelegate {
 public:
  // |frame_proxy_in_parent_renderer| corresponds to A2 in the example above.
  explicit CrossProcessFrameConnector(
      RenderFrameProxyHost* frame_proxy_in_parent_renderer);
  ~CrossProcessFrameConnector() override;

  bool OnMessageReceived(const IPC::Message& msg);

  // |view| corresponds to B2's RenderWidgetHostViewChildFrame in the example
  // above.
  RenderWidgetHostViewChildFrame* get_view_for_testing() { return view_; }

  // FrameConnectorDelegate implementation.
  void SetView(RenderWidgetHostViewChildFrame* view) override;
  RenderWidgetHostViewBase* GetParentRenderWidgetHostView() override;
  RenderWidgetHostViewBase* GetRootRenderWidgetHostView() override;
  void RenderProcessGone() override;
  void SendIntrinsicSizingInfoToParent(
      const blink::WebIntrinsicSizingInfo&) override;

  void UpdateCursor(const WebCursor& cursor) override;
  gfx::PointF TransformPointToRootCoordSpace(
      const gfx::PointF& point,
      const viz::SurfaceId& surface_id) override;
  bool TransformPointToCoordSpaceForView(
      const gfx::PointF& point,
      RenderWidgetHostViewBase* target_view,
      const viz::SurfaceId& local_surface_id,
      gfx::PointF* transformed_point) override;
  void ForwardAckedTouchpadZoomEvent(const blink::WebGestureEvent& event,
                                     InputEventAckState ack_result) override;
  bool BubbleScrollEvent(const blink::WebGestureEvent& event) override;
  bool HasFocus() override;
  void FocusRootView() override;
  bool LockMouse(bool request_unadjusted_movement) override;
  void UnlockMouse() override;
  void EnableAutoResize(const gfx::Size& min_size,
                        const gfx::Size& max_size) override;
  void DisableAutoResize() override;
  bool IsInert() const override;
  cc::TouchAction InheritedEffectiveTouchAction() const override;
  bool IsHidden() const override;
  bool IsThrottled() const override;
  bool IsSubtreeThrottled() const override;
  void DidUpdateVisualProperties(
      const cc::RenderFrameMetadata& metadata) override;

  // Set the visibility of immediate child views, i.e. views whose parent view
  // is |view_|.
  void SetVisibilityForChildViews(bool visible) const override;

  void SetScreenSpaceRect(const gfx::Rect& screen_space_rect) override;

  // Handlers for messages received from the parent frame called
  // from RenderFrameProxyHost to be sent to |view_|.
  void OnSetInheritedEffectiveTouchAction(cc::TouchAction);
  void OnVisibilityChanged(blink::mojom::FrameVisibility visibility);

  // Exposed for tests.
  RenderWidgetHostViewBase* GetRootRenderWidgetHostViewForTesting() {
    return GetRootRenderWidgetHostView();
  }

  // These enums back crashed frame histograms - see MaybeLogCrash() and
  // MaybeLogShownCrash() below.  Please do not modify or remove existing enum
  // values.  When adding new values, please also update enums.xml. See
  // enums.xml for descriptions of enum values.
  enum class CrashVisibility {
    kCrashedWhileVisible = 0,
    kShownAfterCrashing = 1,
    kNeverVisibleAfterCrash = 2,
    kMaxValue = kNeverVisibleAfterCrash
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

  blink::mojom::FrameVisibility visibility() const { return visibility_; }

 private:
  friend class MockCrossProcessFrameConnector;

  // Resets the rect and the viz::LocalSurfaceId of the connector to ensure the
  // unguessable surface ID is not reused after a cross-process navigation.
  void ResetScreenSpaceRect();

  // Logs the Stability.ChildFrameCrash.Visibility metric after checking that a
  // crash has indeed happened and checking that the crash has not already been
  // logged in UMA.  Returns true if this metric was actually logged.
  bool MaybeLogCrash(CrashVisibility visibility);

  // Check if a crashed child frame has become visible, and if so, log the
  // Stability.ChildFrameCrash.Visibility.ShownAfterCrashing* metrics.
  void MaybeLogShownCrash(ShownAfterCrashingReason reason);

  // Handlers for messages received from the parent frame.
  void OnSynchronizeVisualProperties(
      const viz::FrameSinkId& frame_sink_id,
      const FrameVisualProperties& visual_properties);
  void OnUpdateViewportIntersection(
      const blink::ViewportIntersectionState& viewport_intersection);
  void OnSetIsInert(bool);
  void OnUpdateRenderThrottlingStatus(bool is_throttled,
                                      bool subtree_throttled);

  // The RenderFrameProxyHost that routes messages to the parent frame's
  // renderer process.
  RenderFrameProxyHost* frame_proxy_in_parent_renderer_;

  bool is_inert_ = false;
  cc::TouchAction inherited_effective_touch_action_ =
      cc::TouchAction::kTouchActionAuto;

  bool is_throttled_ = false;
  bool subtree_throttled_ = false;

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

  DISALLOW_COPY_AND_ASSIGN(CrossProcessFrameConnector);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_CROSS_PROCESS_FRAME_CONNECTOR_H_
