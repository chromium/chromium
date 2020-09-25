// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_CHILD_FRAME_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_CHILD_FRAME_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "cc/input/touch_action.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/content_export.h"
#include "content/public/browser/touch_selection_controller_client_manager.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom-forward.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "third_party/blink/public/platform/viewport_intersection_state.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class FrameConnectorDelegate;
class RenderWidgetHost;
class RenderWidgetHostViewChildFrameTest;
class TouchSelectionControllerClientChildFrame;

// RenderWidgetHostViewChildFrame implements the view for a RenderWidgetHost
// associated with content being rendered in a separate process from
// content that is embedding it. This is not a platform-specific class; rather,
// the embedding renderer process implements the platform containing the
// widget, and the top-level frame's RenderWidgetHostView will ultimately
// manage all native widget interaction.
//
// See comments in render_widget_host_view.h about this class and its members.
class CONTENT_EXPORT RenderWidgetHostViewChildFrame
    : public RenderWidgetHostViewBase,
      public TouchSelectionControllerClientManager::Observer,
      public RenderFrameMetadataProvider::Observer,
      public viz::HostFrameSinkClient {
 public:
  static RenderWidgetHostViewChildFrame* Create(
      RenderWidgetHost* widget,
      const blink::ScreenInfo& screen_info);

  void SetFrameConnectorDelegate(FrameConnectorDelegate* frame_connector);

  // TouchSelectionControllerClientManager::Observer implementation.
  void OnManagerWillDestroy(
      TouchSelectionControllerClientManager* manager) override;

  // RenderWidgetHostView implementation.
  void InitAsChild(gfx::NativeView parent_view) override;
  void SetSize(const gfx::Size& size) override;
  void SetBounds(const gfx::Rect& rect) override;
  void Focus() override;
  bool HasFocus() override;
  bool IsSurfaceAvailableForCopy() override;
  void CopyFromSurface(
      const gfx::Rect& src_rect,
      const gfx::Size& output_size,
      base::OnceCallback<void(const SkBitmap&)> callback) override;
  void EnsureSurfaceSynchronizedForWebTest() override;
  void Show() override;
  void Hide() override;
  bool IsShowing() override;
  void WasUnOccluded() override;
  void WasOccluded() override;
  gfx::Rect GetViewBounds() override;
  gfx::Size GetVisibleViewportSize() override;
  void SetInsets(const gfx::Insets& insets) override;
  gfx::NativeView GetNativeView() override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  bool IsMouseLocked() override;
  void TakeFallbackContentFrom(RenderWidgetHostView* view) override;

  // RenderWidgetHostViewBase implementation.
  RenderWidgetHostViewBase* GetRootView() override;
  uint32_t GetCaptureSequenceNumber() const override;
  gfx::Size GetCompositorViewportPixelSize() override;
  void InitAsPopup(RenderWidgetHostView* parent_host_view,
                   const gfx::Rect& bounds) override;
  void InitAsFullscreen(RenderWidgetHostView* reference_host_view) override;
  void UpdateCursor(const WebCursor& cursor) override;
  void SetIsLoading(bool is_loading) override;
  void RenderProcessGone() override;
  void Destroy() override;
  void SetTooltipText(const base::string16& tooltip_text) override;
  void GestureEventAck(const blink::WebGestureEvent& event,
                       blink::mojom::InputEventResultState ack_result) override;
  // Since the URL of content rendered by this class is not displayed in
  // the URL bar, this method does not need an implementation.
  void ResetFallbackToFirstNavigationSurface() override {}

  void TransformPointToRootSurface(gfx::PointF* point) override;
  gfx::Rect GetBoundsInRootWindow() override;
  void DidStopFlinging() override;
  blink::mojom::PointerLockResult LockMouse(
      bool request_unadjusted_movement) override;
  blink::mojom::PointerLockResult ChangeMouseLock(
      bool request_unadjusted_movement) override;
  void UnlockMouse() override;
  const viz::FrameSinkId& GetFrameSinkId() const override;
  const viz::LocalSurfaceId& GetLocalSurfaceId() const override;
  void NotifyHitTestRegionUpdated(const viz::AggregatedHitTestRegion&) override;
  bool ScreenRectIsUnstableFor(const blink::WebInputEvent& event) override;
  void PreProcessTouchEvent(const blink::WebTouchEvent& event) override;
  viz::FrameSinkId GetRootFrameSinkId() override;
  viz::SurfaceId GetCurrentSurfaceId() const override;
  bool HasSize() const override;
  gfx::PointF TransformPointToRootCoordSpaceF(
      const gfx::PointF& point) override;
  bool TransformPointToCoordSpaceForView(
      const gfx::PointF& point,
      RenderWidgetHostViewBase* target_view,
      gfx::PointF* transformed_point) override;
  void DidNavigate() override;
  gfx::PointF TransformRootPointToViewCoordSpace(
      const gfx::PointF& point) override;
  TouchSelectionControllerClientManager*
  GetTouchSelectionControllerClientManager() override;
  void UpdateIntrinsicSizingInfo(
      blink::mojom::IntrinsicSizingInfoPtr sizing_info) override;
  std::unique_ptr<SyntheticGestureTarget> CreateSyntheticGestureTarget()
      override;
  bool IsRenderWidgetHostViewChildFrame() override;
  void WillSendScreenRects() override;

#if defined(OS_MAC)
  // RenderWidgetHostView implementation.
  void SetActive(bool active) override;
  void ShowDefinitionForSelection() override;
  void SpeakSelection() override;
  void SetWindowFrameInScreen(const gfx::Rect& rect) override;
#endif  // defined(OS_MAC)

  blink::mojom::InputEventResultState FilterInputEvent(
      const blink::WebInputEvent& input_event) override;
  BrowserAccessibilityManager* CreateBrowserAccessibilityManager(
      BrowserAccessibilityDelegate* delegate,
      bool for_root_frame) override;
  void GetScreenInfo(blink::ScreenInfo* screen_info) override;
  void EnableAutoResize(const gfx::Size& min_size,
                        const gfx::Size& max_size) override;
  void DisableAutoResize(const gfx::Size& new_size) override;
  viz::ScopedSurfaceIdAllocator DidUpdateVisualProperties(
      const cc::RenderFrameMetadata& metadata) override;

  // RenderFrameMetadataProvider::Observer implementation.
  void OnRenderFrameMetadataChangedBeforeActivation(
      const cc::RenderFrameMetadata& metadata) override {}
  void OnRenderFrameMetadataChangedAfterActivation() override;
  void OnRenderFrameSubmission() override {}
  void OnLocalSurfaceIdChanged(
      const cc::RenderFrameMetadata& metadata) override {}

  // viz::HostFrameSinkClient implementation.
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void OnFrameTokenChanged(uint32_t frame_token) override;

  FrameConnectorDelegate* FrameConnectorForTesting() const {
    return frame_connector_;
  }

  // Returns the view into which this view is directly embedded. This can
  // return nullptr when this view's associated child frame is not connected
  // to the frame tree.
  virtual RenderWidgetHostViewBase* GetParentView();

  void RegisterFrameSinkId();
  void UnregisterFrameSinkId();

  void UpdateViewportIntersection(
      const blink::ViewportIntersectionState& intersection_state);

  // TODO(sunxd): Rename SetIsInert to UpdateIsInert.
  void SetIsInert();
  void UpdateInheritedEffectiveTouchAction();

  void UpdateRenderThrottlingStatus();

  ui::TextInputType GetTextInputType() const;

  RenderWidgetHostViewBase* GetRootRenderWidgetHostView() const;

 protected:
  friend class RenderWidgetHostView;
  friend class RenderWidgetHostViewChildFrameTest;
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewChildFrameTest,
                           ForwardsBeginFrameAcks);

  explicit RenderWidgetHostViewChildFrame(RenderWidgetHost* widget,
                                          const blink::ScreenInfo& screen_info);
  void Init();

  // Sets |parent_frame_sink_id_| and registers frame sink hierarchy. If the
  // parent was already set then it also unregisters hierarchy.
  void SetParentFrameSinkId(const viz::FrameSinkId& parent_frame_sink_id);

  // Clears current compositor surface, if one is in use.
  void ClearCompositorSurfaceIfNecessary();

  void ProcessFrameSwappedCallbacks();

  // RenderWidgetHostViewBase:
  void UpdateBackgroundColor() override;

  void StopFlingingIfNecessary(
      const blink::WebGestureEvent& event,
      blink::mojom::InputEventResultState ack_result) override;

  // The ID for FrameSink associated with this view.
  viz::FrameSinkId frame_sink_id_;

  // Surface-related state.
  viz::SurfaceInfo last_activated_surface_info_;
  gfx::Rect last_screen_rect_;

  // frame_connector_ provides a platform abstraction. Messages
  // sent through it are routed to the embedding renderer process.
  FrameConnectorDelegate* frame_connector_;

  base::WeakPtr<RenderWidgetHostViewChildFrame> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 protected:
  ~RenderWidgetHostViewChildFrame() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           HiddenOOPIFWillNotGenerateCompositorFrames);
  FRIEND_TEST_ALL_PREFIXES(
      SitePerProcessBrowserTest,
      HiddenOOPIFWillNotGenerateCompositorFramesAfterNavigation);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           SubframeVisibleAfterRenderViewBecomesSwappedOut);

  virtual void FirstSurfaceActivation(const viz::SurfaceInfo& surface_info);

  void DetachFromTouchSelectionClientManagerIfNecessary();

  // Returns false if the view cannot be shown. This is the case where the frame
  // associated with this view or a cross process ancestor frame has been hidden
  // using CSS.
  bool CanBecomeVisible();

  void OnDidUpdateVisualPropertiesComplete(
      const cc::RenderFrameMetadata& metadata);

  void ProcessTouchpadZoomEventAckInRoot(
      const blink::WebGestureEvent& event,
      blink::mojom::InputEventResultState ack_result);
  void ForwardTouchpadZoomEventIfNecessary(
      const blink::WebGestureEvent& event,
      blink::mojom::InputEventResultState ack_result) override;

  std::vector<base::OnceClosure> frame_swapped_callbacks_;

  // The surface client ID of the parent RenderWidgetHostView.  0 if none.
  viz::FrameSinkId parent_frame_sink_id_;

  gfx::RectF last_stable_screen_rect_;
  base::TimeTicks screen_rect_stable_since_;

  gfx::Insets insets_;

  std::unique_ptr<TouchSelectionControllerClientChildFrame>
      selection_controller_client_;

  // True if there is currently a scroll sequence being bubbled to our parent.
  bool is_scroll_sequence_bubbling_ = false;

  // The ScreenInfo information from the parent at the time this class is
  // created, to be used before this view is connected to its FrameDelegate.
  // This is kept up to date anytime GetScreenInfo() is called and we have
  // a FrameDelegate.
  blink::ScreenInfo screen_info_;

  base::WeakPtrFactory<RenderWidgetHostViewChildFrame> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewChildFrame);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_CHILD_FRAME_H_
