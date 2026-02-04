// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_FRAME_CONNECTOR_H_
#define CONTENT_BROWSER_RENDERER_HOST_FRAME_CONNECTOR_H_

#include <stdint.h>

#include "cc/input/touch_action.h"
#include "components/input/child_frame_input_helper.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "content/public/browser/visibility.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/viewport_intersection_state.mojom-forward.h"
#include "third_party/blink/public/mojom/input/pointer_lock_result.mojom-shared.h"
#include "ui/display/screen_infos.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
class RenderFrameMetadata;
}

namespace blink {
struct FrameVisualProperties;
}  // namespace blink

namespace ui {
class Cursor;
}

namespace viz {
class SurfaceInfo;
}  // namespace viz

namespace content {

class RenderWidgetHostViewBase;
class RenderWidgetHostViewChildFrame;

// FrameConnector provides the platform view abstraction for
// RenderWidgetHostViewChildFrame, allowing RWHVChildFrame to remain ignorant
// of the specific embedding mechanism.
//
// A child frame's RenderWidgetHostView needs to communicate with its embedder
// to receive information necessary for compositing (screen info, viewport
// intersection, surface synchronization) and platform messages (view resizing,
// visibility changes, input coordination).
//
// When a child frame navigates to a new process, SetView() is called to
// update the connector to point to the new RenderWidgetHostViewChildFrame.
//
// This base class defines the interface for that communication. Concrete
// implementations handle the specifics of how the child frame is embedded:
//
// - CrossProcessFrameConnector: Used for out-of-process iframes where the
//   child frame is embedded via a RenderFrameProxyHost in the parent's
//   process. Owned by the RenderFrameProxyHost corresponding to the parent's
//   SiteInstance.
//
// - SurfaceEmbedConnectorImpl: Used for surface embed scenarios
//   where the child frame is embedded without a RenderFrameProxyHost
//   relationship.
//
class FrameConnector : public input::ChildFrameInputHelper::Delegate {
 public:
  ~FrameConnector() override = default;

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

  virtual void SetView(RenderWidgetHostViewChildFrame* view,
                       bool allow_paint_holding) = 0;

  // Returns the parent RenderWidgetHostView or nullptr if it doesn't have one.
  virtual RenderWidgetHostViewBase* GetParentRenderWidgetHostView() = 0;

  // Returns the view for the top-level frame under the same WebContents.
  virtual RenderWidgetHostViewBase* GetRootRenderWidgetHostView() = 0;

  // Notify the frame connector that the renderer process has terminated.
  virtual void RenderProcessGone() = 0;

  // Provide the SurfaceInfo to the embedder, which becomes a reference to the
  // current view's Surface that is included in higher-level compositor
  // frames. This is virtual to be overridden in tests.
  virtual void FirstSurfaceActivation(const viz::SurfaceInfo& surface_info) = 0;

  // Sends the given intrinsic sizing information from a sub-frame to
  // its corresponding remote frame in the parent frame's renderer.
  virtual void SendIntrinsicSizingInfoToParent(
      blink::mojom::IntrinsicSizingInfoPtr) = 0;

  // Record and apply new visual properties for the subframe. If 'propagate' is
  // true, the new properties will be sent to the subframe's renderer process.
  virtual void SynchronizeVisualProperties(
      const blink::FrameVisualProperties& visual_properties,
      bool propagate) = 0;

  // Request that the platform change the mouse cursor when the mouse is
  // positioned over this view's content.
  virtual void UpdateCursor(const ui::Cursor& cursor) = 0;

  // Determines whether the root RenderWidgetHostView (and thus the current
  // page) has focus. We need a tri-state enum as a return variable to
  // differentiate between the cases where root view is NULL and when it's
  // actually focused/unfocused. No behaviour change expected in focus handling.
  virtual RootViewFocusState HasFocus() = 0;

  // Cause the root RenderWidgetHostView to become focused.
  virtual void FocusRootView() = 0;

  // Locks the mouse pointer, if |request_unadjusted_movement_| is true, try
  // setting the unadjusted movement mode. Returns true if mouse pointer is
  // locked.
  virtual blink::mojom::PointerLockResult LockPointer(
      bool request_unadjusted_movement) = 0;

  // Change the current pointer lock to match the unadjusted movement option
  // given.
  virtual blink::mojom::PointerLockResult ChangePointerLock(
      bool request_unadjusted_movement) = 0;

  // Unlocks the mouse pointer if it is locked.
  virtual void UnlockPointer() = 0;

  virtual bool HasSize() = 0;
  // Returns the ScreenInfos propagated from the parent to be used by this
  // child frame.
  virtual const display::ScreenInfos& GetScreenInfos() = 0;
  // Returns the viz::LocalSurfaceId propagated from the parent to be
  // used by this child frame.
  virtual const viz::LocalSurfaceId& GetLocalSurfaceId() = 0;
  // Returns the state of the frame's intersection with the top-level viewport.
  virtual const blink::mojom::ViewportIntersectionState&
  GetIntersectionState() = 0;
  // Return the latest capture sequence number for this subframe.
  virtual uint32_t GetCaptureSequenceNumber() = 0;
  // Return the rect in DIP that the RenderWidgetHostViewChildFrame's content
  // will render into.
  virtual const gfx::Rect& GetRectInParentViewInDip() = 0;
  // Return the size of the CompositorFrame to use in the child renderer in DIP.
  // This is used to set the layout size of the child renderer.
  virtual const gfx::Size& GetLocalFrameSizeInDip() = 0;
  // Return the size of the CompositorFrame to use in the child renderer.
  virtual const gfx::Size& GetLocalFrameSizeInPixels() = 0;
  virtual double GetCssZoomFactor() = 0;

  // Informs the parent the child will enter auto-resize mode, automatically
  // resizing itself to the provided |min_size| and |max_size| constraints.
  virtual void EnableAutoResize(const gfx::Size& min_size,
                                const gfx::Size& max_size) = 0;

  // Turns off auto-resize mode.
  virtual void DisableAutoResize() = 0;

  // Determines whether the current view's content is inert, either because
  // an HTMLDialogElement is being modally displayed in a higher-level frame,
  // or because the inert attribute has been specified.
  virtual bool IsInert() = 0;

  // Returns the inherited effective touch action property that should be
  // applied to any nested child RWHVCFs inside the caller RWHVCF.
  virtual cc::TouchAction InheritedEffectiveTouchAction() = 0;

  // Determines whether the RenderWidgetHostViewChildFrame is hidden due to
  // a higher-level embedder being hidden. This is distinct from the
  // RenderWidgetHostImpl being hidden, which is a property set when
  // RenderWidgetHostView::Hide() is called on the current view.
  virtual bool IsHidden() = 0;

  // IsThrottled() indicates that the frame is outside of it's parent frame's
  // visible viewport, and should be render throttled.
  virtual bool IsThrottled() = 0;
  // IsSubtreeThrottled() indicates that IsThrottled() is true for one of this
  // frame's ancestors, which means this frame must also be throttled.
  virtual bool IsSubtreeThrottled() = 0;
  // IsDisplayLocked() indicates that a DOM ancestor of this frame's owning
  // <iframe> element in the parent frame is currently display locked; or that
  // IsDisplayLocked() is true for one of this frame's ancestors; which means
  // this frame should be render throttled.
  virtual bool IsDisplayLocked() = 0;

  // Called by RenderWidgetHostViewChildFrame when the child frame has updated
  // its visual properties and its viz::LocalSurfaceId has changed.
  virtual void DidUpdateVisualProperties(
      const cc::RenderFrameMetadata& metadata) = 0;

  // Called by RenderWidgetHostViewChildFrame to update the visibility of any
  // nested child RWHVCFs inside it.
  virtual void SetVisibilityForChildViews(bool visible) = 0;

  // Called to resize the child renderer's CompositorFrame.
  // |local_frame_size| is in pixels if zoom-for-dsf is enabled, and in DIP
  // if not.
  virtual void SetLocalFrameSize(const gfx::Size& local_frame_size) = 0;

  // Called to resize the child renderer. |rect_in_parent_view| is in physical
  // pixels.
  virtual void SetRectInParentView(const gfx::Rect& rect_in_parent_view) = 0;

  virtual void SetIsInert(bool inert) = 0;

  // Handlers for messages received from the parent frame called
  // from RenderFrameProxyHost to be sent to |view_|.
  virtual void OnSetInheritedEffectiveTouchAction(cc::TouchAction) = 0;
  virtual void OnVisibilityChanged(
      blink::mojom::FrameVisibility visibility) = 0;

  virtual void UpdateRenderThrottlingStatus(bool is_throttled,
                                            bool subtree_throttled,
                                            bool display_locked) = 0;
  virtual void UpdateViewportIntersection(
      const blink::mojom::ViewportIntersectionState& intersection_state,
      const std::optional<blink::FrameVisualProperties>& visual_properties) = 0;

  // Returns whether the child widget is actually visible to the user.  This is
  // different from the IsHidden override, and takes into account viewport
  // intersection as well as the visibility of the embedder.
  virtual bool IsVisible() = 0;

  // This function is called by the embedder to signal that it
  // became visible. This is called after any navigations resulting from
  // visibility changes have been queued (e.g. if needs-reload was set).
  virtual void DelegateWasShown() = 0;

  // Handlers for messages received from the parent frame.
  virtual void OnSynchronizeVisualProperties(
      const blink::FrameVisualProperties& visual_properties) = 0;

  // Returns the embedder's visibility.
  virtual Visibility EmbedderVisibility() = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_FRAME_CONNECTOR_H_
