// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_CROSS_PROCESS_FRAME_CONNECTOR_H_
#define CONTENT_BROWSER_RENDERER_HOST_CROSS_PROCESS_FRAME_CONNECTOR_H_

#include <stdint.h>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "cc/input/touch_action.h"
#include "components/input/child_frame_input_helper.h"
#include "content/browser/renderer_host/frame_connector.h"
#include "content/common/content_export.h"
#include "content/public/browser/visibility.h"
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
class CONTENT_EXPORT CrossProcessFrameConnector : public FrameConnector {
 public:
  // |frame_proxy_in_parent_renderer| corresponds to A2 in the example above.
  explicit CrossProcessFrameConnector(
      RenderFrameProxyHost* frame_proxy_in_parent_renderer);

  CrossProcessFrameConnector(const CrossProcessFrameConnector&) = delete;
  CrossProcessFrameConnector& operator=(const CrossProcessFrameConnector&) =
      delete;

  ~CrossProcessFrameConnector() override;

  // |view| corresponds to B2's RenderWidgetHostViewChildFrame in the example
  // above.
  RenderWidgetHostViewChildFrame* get_view_for_testing() { return view_; }

  // FrameConnector implementation.
  void SetView(RenderWidgetHostViewChildFrame* view,
               bool allow_paint_holding) override;

  RenderWidgetHostViewBase* GetParentRenderWidgetHostView() override;

  RenderWidgetHostViewBase* GetRootRenderWidgetHostView() override;

  void RenderProcessGone() override;

  void FirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override {}

  void SendIntrinsicSizingInfoToParent(
      blink::mojom::IntrinsicSizingInfoPtr) override;

  void SynchronizeVisualProperties(
      const blink::FrameVisualProperties& visual_properties,
      bool propagate = true) override;

  void UpdateCursor(const ui::Cursor& cursor) override;

  RootViewFocusState HasFocus() override;

  void FocusRootView() override;

  blink::mojom::PointerLockResult LockPointer(
      bool request_unadjusted_movement) override;

  blink::mojom::PointerLockResult ChangePointerLock(
      bool request_unadjusted_movement) override;

  void UnlockPointer() override;

  bool HasSize() override;
  const display::ScreenInfos& GetScreenInfos() override;
  const viz::LocalSurfaceId& GetLocalSurfaceId() override;
  const blink::mojom::ViewportIntersectionState& GetIntersectionState()
      override;
  uint32_t GetCaptureSequenceNumber() override;
  const gfx::Rect& GetRectInParentViewInDip() override;
  const gfx::Size& GetLocalFrameSizeInDip() override;
  const gfx::Size& GetLocalFrameSizeInPixels() override;
  double GetCssZoomFactor() override;

  void EnableAutoResize(const gfx::Size& min_size,
                        const gfx::Size& max_size) override;

  void DisableAutoResize() override;

  bool IsInert() override;

  cc::TouchAction InheritedEffectiveTouchAction() override;

  bool IsHidden() override;

  bool IsThrottled() override;
  bool IsSubtreeThrottled() override;
  bool IsDisplayLocked() override;

  void DidUpdateVisualProperties(
      const cc::RenderFrameMetadata& metadata) override;

  void SetVisibilityForChildViews(bool visible) override;

  void SetLocalFrameSize(const gfx::Size& local_frame_size) override;

  void SetRectInParentView(const gfx::Rect& rect_in_parent_view) override;

  void SetIsInert(bool inert) override;

  void OnSetInheritedEffectiveTouchAction(cc::TouchAction) override;
  void OnVisibilityChanged(blink::mojom::FrameVisibility visibility) override;

  void UpdateRenderThrottlingStatus(bool is_throttled,
                                    bool subtree_throttled,
                                    bool display_locked) override;
  void UpdateViewportIntersection(
      const blink::mojom::ViewportIntersectionState& intersection_state,
      const std::optional<blink::FrameVisualProperties>& visual_properties)
      override;

  bool IsVisible() override;

  void DelegateWasShown() override;

  void OnSynchronizeVisualProperties(
      const blink::FrameVisualProperties& visual_properties) override;

  Visibility EmbedderVisibility() override;

  // ChildFrameInputHelper::Delegate implementation.
  input::RenderWidgetHostViewInput* GetParentViewInput() override;
  input::RenderWidgetHostViewInput* GetRootViewInput() override;

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
