// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_FRAME_CONNECTOR_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_FRAME_CONNECTOR_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "cc/input/touch_action.h"
#include "components/viz/common/surfaces/local_surface_id_allocation.h"
#include "components/viz/host/hit_test/hit_test_query.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/common/content_export.h"
#include "content/public/common/input_event_ack_state.h"
#include "content/public/common/screen_info.h"
#include "third_party/blink/public/platform/viewport_intersection_state.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {
class WebGestureEvent;
struct WebIntrinsicSizingInfo;
}

namespace cc {
class RenderFrameMetadata;
}

namespace viz {
class SurfaceId;
class SurfaceInfo;
}  // namespace viz

namespace content {
class RenderWidgetHostViewBase;
class RenderWidgetHostViewChildFrame;
class WebCursor;
struct FrameVisualProperties;

//
// FrameConnectorDelegate
//
// An interface to be implemented by an object supplying platform semantics
// for a child frame.
//
// A RenderWidgetHostViewChildFrame, specified by a call to |SetView|, uses
// this interface to communicate renderer-originating messages such as mouse
// cursor changes or input event ACKs to its platform.
// CrossProcessFrameConnector implements this interface and coordinates with
// higher-level RenderWidgetHostViews to ensure that the underlying platform
// (e.g. Mac, Aura, Android) correctly reflects state from frames in multiple
// processes.
//
// RenderWidgetHostViewChildFrame also uses this interface to query relevant
// platform information, such as the size of the rect that the frame will draw
// into, and whether the view currently has keyboard focus.
class CONTENT_EXPORT FrameConnectorDelegate {
 public:
  virtual void SetView(RenderWidgetHostViewChildFrame* view);

  // Returns the parent RenderWidgetHostView or nullptr if it doesn't have one.
  virtual RenderWidgetHostViewBase* GetParentRenderWidgetHostView();

  // Returns the view for the top-level frame under the same WebContents.
  virtual RenderWidgetHostViewBase* GetRootRenderWidgetHostView();

  // Notify the frame connector that the renderer process has terminated.
  virtual void RenderProcessGone() {}

  // Provide the SurfaceInfo to the embedder, which becomes a reference to the
  // current view's Surface that is included in higher-level compositor
  // frames.
  virtual void FirstSurfaceActivation(const viz::SurfaceInfo& surface_info) {}

  // Sends the given intrinsic sizing information from a sub-frame to
  // its corresponding remote frame in the parent frame's renderer.
  virtual void SendIntrinsicSizingInfoToParent(
      const blink::WebIntrinsicSizingInfo&) {}

  // Sends new resize parameters to the sub-frame's renderer.
  void SynchronizeVisualProperties(
      const viz::FrameSinkId& frame_sink_id,
      const FrameVisualProperties& visual_properties);

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
  const gfx::Rect& screen_space_rect_in_dip() const {
    return screen_space_rect_in_dip_;
  }

  // Return the rect in pixels that the RenderWidgetHostViewChildFrame's content
  // will render into.
  const gfx::Rect& screen_space_rect_in_pixels() const {
    return screen_space_rect_in_pixels_;
  }

  // Return the latest capture sequence number of this delegate.
  uint32_t capture_sequence_number() const { return capture_sequence_number_; }

  // Request that the platform change the mouse cursor when the mouse is
  // positioned over this view's content.
  virtual void UpdateCursor(const WebCursor& cursor) {}

  // Given a point in the current view's coordinate space, return the same
  // point transformed into the coordinate space of the top-level view's
  // coordinate space.
  virtual gfx::PointF TransformPointToRootCoordSpace(
      const gfx::PointF& point,
      const viz::SurfaceId& surface_id);

  // Transform a point into the coordinate space of the root
  // RenderWidgetHostView, for the current view's coordinate space.
  // Returns false if |target_view| and |view_| do not have the same root
  // RenderWidgetHostView.
  virtual bool TransformPointToCoordSpaceForView(
      const gfx::PointF& point,
      RenderWidgetHostViewBase* target_view,
      const viz::SurfaceId& local_surface_id,
      gfx::PointF* transformed_point);

  // Pass acked touchpad pinch or double tap gesture events to the root view
  // for processing.
  virtual void ForwardAckedTouchpadZoomEvent(
      const blink::WebGestureEvent& event,
      InputEventAckState ack_result) {}

  // A gesture scroll sequence that is not consumed by a child must be bubbled
  // to ancestors who may consume it.
  // Returns false if the scroll event could not be bubbled. The caller must
  // not attempt to bubble the rest of the scroll sequence in this case.
  // Otherwise, returns true.
  virtual bool BubbleScrollEvent(const blink::WebGestureEvent& event)
      WARN_UNUSED_RESULT;

  // Determines whether the root RenderWidgetHostView (and thus the current
  // page) has focus.
  virtual bool HasFocus();

  // Cause the root RenderWidgetHostView to become focused.
  virtual void FocusRootView() {}

  // Locks the mouse, if |request_unadjusted_movement_| is true, try setting the
  // unadjusted movement mode. Returns true if mouse is locked.
  virtual bool LockMouse(bool request_unadjusted_movement);

  // Unlocks the mouse if the mouse is locked.
  virtual void UnlockMouse() {}

  // Returns the state of the frame's intersection with the top-level viewport.
  const blink::ViewportIntersectionState& intersection_state() const {
    return intersection_state_;
  }

  // Returns the viz::LocalSurfaceIdAllocation propagated from the parent to be
  // used by this child frame.
  const viz::LocalSurfaceIdAllocation& local_surface_id_allocation() const {
    return local_surface_id_allocation_;
  }

  // Returns the ScreenInfo propagated from the parent to be used by this
  // child frame.
  const ScreenInfo& screen_info() const { return screen_info_; }

  void SetScreenInfoForTesting(const ScreenInfo& screen_info) {
    screen_info_ = screen_info;
  }

  // Informs the parent the child will enter auto-resize mode, automatically
  // resizing itself to the provided |min_size| and |max_size| constraints.
  virtual void EnableAutoResize(const gfx::Size& min_size,
                                const gfx::Size& max_size);

  // Turns off auto-resize mode.
  virtual void DisableAutoResize();

  // Determines whether the current view's content is inert, either because
  // an HTMLDialogElement is being modally displayed in a higher-level frame,
  // or because the inert attribute has been specified.
  virtual bool IsInert() const;

  // Returns the inherited effective touch action property that should be
  // applied to any nested child RWHVCFs inside the caller RWHVCF.
  virtual cc::TouchAction InheritedEffectiveTouchAction() const;

  // Determines whether the RenderWidgetHostViewChildFrame is hidden due to
  // a higher-level embedder being hidden. This is distinct from the
  // RenderWidgetHostImpl being hidden, which is a property set when
  // RenderWidgetHostView::Hide() is called on the current view.
  virtual bool IsHidden() const;

  // Determines whether the child frame should be render throttled, which
  // happens when the entire rect is offscreen.
  virtual bool IsThrottled() const;
  virtual bool IsSubtreeThrottled() const;

  // Called by RenderWidgetHostViewChildFrame to update the visibility of any
  // nested child RWHVCFs inside it.
  virtual void SetVisibilityForChildViews(bool visible) const {}

  // Called to resize the child renderer's CompositorFrame.
  // |local_frame_size| is in pixels if zoom-for-dsf is enabled, and in DIP
  // if not.
  virtual void SetLocalFrameSize(const gfx::Size& local_frame_size);

  // Called to resize the child renderer. |screen_space_rect| is in pixels if
  // zoom-for-dsf is enabled, and in DIP if not.
  virtual void SetScreenSpaceRect(const gfx::Rect& screen_space_rect);

  // Called by RenderWidgetHostViewChildFrame when the child frame has updated
  // its visual properties and its viz::LocalSurfaceId has changed.
  virtual void DidUpdateVisualProperties(
      const cc::RenderFrameMetadata& metadata) {}

  bool has_size() const { return has_size_; }

 protected:
  explicit FrameConnectorDelegate(bool use_zoom_for_device_scale_factor);

  virtual ~FrameConnectorDelegate() {}

  // The RenderWidgetHostView for the frame. Initially NULL.
  RenderWidgetHostViewChildFrame* view_ = nullptr;

  // This is here rather than in the implementation class so that
  // intersection_state() can return a reference.
  blink::ViewportIntersectionState intersection_state_;

  ScreenInfo screen_info_;
  gfx::Size local_frame_size_in_dip_;
  gfx::Size local_frame_size_in_pixels_;
  gfx::Rect screen_space_rect_in_dip_;
  gfx::Rect screen_space_rect_in_pixels_;

  viz::LocalSurfaceIdAllocation local_surface_id_allocation_;

  bool has_size_ = false;
  const bool use_zoom_for_device_scale_factor_;

  uint32_t capture_sequence_number_ = 0u;

  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewChildFrameZoomForDSFTest,
                           CompositorViewportPixelSize);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_FRAME_CONNECTOR_DELEGATE_H_
