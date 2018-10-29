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
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/content_export.h"
#include "content/public/browser/touch_selection_controller_client_manager.h"
#include "content/public/common/input_event_ack_state.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"
#include "third_party/blink/public/platform/web_intrinsic_sizing_info.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace viz {
class CompositorFrameSinkSupport;
}

namespace content {
class FrameConnectorDelegate;
class RenderWidgetHost;
class RenderWidgetHostViewChildFrameTest;
class RenderWidgetHostViewGuestSurfaceTest;
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
      public viz::mojom::CompositorFrameSinkClient,
      public viz::HostFrameSinkClient {
 public:
  static RenderWidgetHostViewChildFrame* Create(RenderWidgetHost* widget);
  ~RenderWidgetHostViewChildFrame() override;

  void SetFrameConnectorDelegate(FrameConnectorDelegate* frame_connector);

#if defined(USE_AURA)
  // When the viz::FrameSinkId for this child frame is created and registered
  // remotely, it can be set here.
  void SetFrameSinkId(const viz::FrameSinkId& frame_sink_id);
#endif  // defined(USE_AURA)

  // This functions registers single-use callbacks that want to be notified when
  // the next frame is swapped. The callback is triggered by
  // SubmitCompositorFrame, which is the appropriate time to request pixel
  // readback for the frame that is about to be drawn. Once called, the callback
  // pointer is released.
  // TODO(crbug.com/787941): This should be removed because it doesn't work when
  // VIZ display compositing is enabled. The public CopyFromSurface() API does
  // not make guarantees that it will succeed before the first frame is
  // composited.
  void RegisterFrameSwappedCallback(base::OnceClosure callback);

  // TouchSelectionControllerClientManager::Observer implementation.
  void OnManagerWillDestroy(
      TouchSelectionControllerClientManager* manager) override;

  // RenderWidgetHostView implementation.
  void InitAsChild(gfx::NativeView parent_view) override;
  void SetSize(const gfx::Size& size) override;
  void SetBounds(const gfx::Rect& rect) override;
  void Focus() override;
  bool HasFocus() const override;
  bool IsSurfaceAvailableForCopy() const override;
  void CopyFromSurface(
      const gfx::Rect& src_rect,
      const gfx::Size& output_size,
      base::OnceCallback<void(const SkBitmap&)> callback) override;
  void EnsureSurfaceSynchronizedForLayoutTest() override;
  uint32_t GetCaptureSequenceNumber() const override;
  void Show() override;
  void Hide() override;
  bool IsShowing() override;
  gfx::Rect GetViewBounds() const override;
  gfx::Size GetVisibleViewportSize() const override;
  void SetInsets(const gfx::Insets& insets) override;
  gfx::NativeView GetNativeView() const override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  gfx::Size GetCompositorViewportPixelSize() const override;
  bool IsMouseLocked() override;
  void SetNeedsBeginFrames(bool needs_begin_frames) override;
  void SetWantsAnimateOnlyBeginFrames() override;
  void TakeFallbackContentFrom(RenderWidgetHostView* view) override;

  // RenderWidgetHostViewBase implementation.
  RenderWidgetHostViewBase* GetRootView() override;
  void InitAsPopup(RenderWidgetHostView* parent_host_view,
                   const gfx::Rect& bounds) override;
  void InitAsFullscreen(RenderWidgetHostView* reference_host_view) override;
  void UpdateCursor(const WebCursor& cursor) override;
  void SetIsLoading(bool is_loading) override;
  void RenderProcessGone(base::TerminationStatus status,
                         int error_code) override;
  void Destroy() override;
  void SetTooltipText(const base::string16& tooltip_text) override;
  void GestureEventAck(const blink::WebGestureEvent& event,
                       InputEventAckState ack_result) override;
  void DidCreateNewRendererCompositorFrameSink(
      viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink)
      override;
  void SubmitCompositorFrame(
      const viz::LocalSurfaceId& local_surface_id,
      viz::CompositorFrame frame,
      base::Optional<viz::HitTestRegionList> hit_test_region_list) override;
  void OnDidNotProduceFrame(const viz::BeginFrameAck& ack) override;
  // Since the URL of content rendered by this class is not displayed in
  // the URL bar, this method does not need an implementation.
  void ClearCompositorFrame() override {}
  void ResetFallbackToFirstNavigationSurface() override{};

  void TransformPointToRootSurface(gfx::PointF* point) override;
  gfx::Rect GetBoundsInRootWindow() override;
  void DidStopFlinging() override;
  bool LockMouse() override;
  void UnlockMouse() override;
  const viz::FrameSinkId& GetFrameSinkId() const override;
  const viz::LocalSurfaceId& GetLocalSurfaceId() const override;
  base::TimeTicks GetLocalSurfaceIdAllocationTime() const override;
  void PreProcessTouchEvent(const blink::WebTouchEvent& event) override;
  viz::FrameSinkId GetRootFrameSinkId() override;
  viz::SurfaceId GetCurrentSurfaceId() const override;
  bool HasSize() const override;
  gfx::PointF TransformPointToRootCoordSpaceF(
      const gfx::PointF& point) override;
  bool TransformPointToLocalCoordSpaceLegacy(
      const gfx::PointF& point,
      const viz::SurfaceId& original_surface,
      gfx::PointF* transformed_point) override;
  bool TransformPointToCoordSpaceForView(
      const gfx::PointF& point,
      RenderWidgetHostViewBase* target_view,
      gfx::PointF* transformed_point,
      viz::EventSource source = viz::EventSource::ANY) override;
  void DidNavigate() override;
  gfx::PointF TransformRootPointToViewCoordSpace(
      const gfx::PointF& point) override;
  TouchSelectionControllerClientManager*
  GetTouchSelectionControllerClientManager() override;
  void OnRenderFrameMetadataChangedAfterActivation() override;
  void UpdateIntrinsicSizingInfo(
      const blink::WebIntrinsicSizingInfo& sizing_info) override;

  bool IsRenderWidgetHostViewChildFrame() override;

  void WillSendScreenRects() override;

#if defined(OS_MACOSX)
  // RenderWidgetHostView implementation.
  void SetActive(bool active) override;
  void ShowDefinitionForSelection() override;
  void SpeakSelection() override;
#endif  // defined(OS_MACOSX)

  InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& input_event) override;
  InputEventAckState FilterChildGestureEvent(
      const blink::WebGestureEvent& gesture_event) override;
  BrowserAccessibilityManager* CreateBrowserAccessibilityManager(
      BrowserAccessibilityDelegate* delegate,
      bool for_root_frame) override;
  void GetScreenInfo(ScreenInfo* screen_info) const override;
  void EnableAutoResize(const gfx::Size& min_size,
                        const gfx::Size& max_size) override;
  void DisableAutoResize(const gfx::Size& new_size) override;
  viz::ScopedSurfaceIdAllocator DidUpdateVisualProperties(
      const cc::RenderFrameMetadata& metadata) override;

  // viz::mojom::CompositorFrameSinkClient implementation.
  void DidReceiveCompositorFrameAck(
      const std::vector<viz::ReturnedResource>& resources) override;
  void DidPresentCompositorFrame(
      uint32_t presentation_token,
      const gfx::PresentationFeedback& feedback) override;
  void OnBeginFrame(const viz::BeginFrameArgs& args) override;
  void ReclaimResources(
      const std::vector<viz::ReturnedResource>& resources) override;
  void OnBeginFramePausedChanged(bool paused) override;

  // viz::HostFrameSinkClient implementation.
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void OnFrameTokenChanged(uint32_t frame_token) override;

  FrameConnectorDelegate* FrameConnectorForTesting() const {
    return frame_connector_;
  }

  // Returns the current surface scale factor.
  float current_surface_scale_factor() {
    return last_activated_surface_info_.device_scale_factor();
  }

  // Returns the view into which this view is directly embedded. This can
  // return nullptr when this view's associated child frame is not connected
  // to the frame tree.
  virtual RenderWidgetHostViewBase* GetParentView();

  void RegisterFrameSinkId();
  void UnregisterFrameSinkId();

  void UpdateViewportIntersection(const gfx::Rect& viewport_intersection,
                                  const gfx::Rect& compositor_visible_rect,
                                  bool occluded_or_obscured);

  // TODO(sunxd): Rename SetIsInert to UpdateIsInert.
  void SetIsInert();
  void UpdateInheritedEffectiveTouchAction();

  void UpdateRenderThrottlingStatus();

  bool has_frame() { return has_frame_; }

  ui::TextInputType GetTextInputType() const;
  bool GetSelectionRange(gfx::Range* range) const;

  RenderWidgetHostViewBase* GetRootRenderWidgetHostView() const;

 protected:
  friend class RenderWidgetHostView;
  friend class RenderWidgetHostViewChildFrameTest;
  friend class RenderWidgetHostViewGuestSurfaceTest;
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewChildFrameTest,
                           ForwardsBeginFrameAcks);

  explicit RenderWidgetHostViewChildFrame(RenderWidgetHost* widget);
  void Init();

  // Sets |parent_frame_sink_id_| and registers frame sink hierarchy. If the
  // parent was already set then it also unregisters hierarchy.
  void SetParentFrameSinkId(const viz::FrameSinkId& parent_frame_sink_id);

  void SendSurfaceInfoToEmbedder();

  // Clears current compositor surface, if one is in use.
  void ClearCompositorSurfaceIfNecessary();

  void ProcessFrameSwappedCallbacks();

  // RenderWidgetHostViewBase:
  void UpdateBackgroundColor() override;

  // The ID for FrameSink associated with this view.
  viz::FrameSinkId frame_sink_id_;

  // Surface-related state.
  std::unique_ptr<viz::CompositorFrameSinkSupport> support_;
  viz::SurfaceInfo last_activated_surface_info_;
  gfx::Rect last_screen_rect_;

  // frame_connector_ provides a platform abstraction. Messages
  // sent through it are routed to the embedding renderer process.
  FrameConnectorDelegate* frame_connector_;

  base::WeakPtr<RenderWidgetHostViewChildFrame> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           HiddenOOPIFWillNotGenerateCompositorFrames);
  FRIEND_TEST_ALL_PREFIXES(
      SitePerProcessBrowserTest,
      HiddenOOPIFWillNotGenerateCompositorFramesAfterNavigation);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           SubframeVisibleAfterRenderViewBecomesSwappedOut);

  virtual void FirstSurfaceActivation(const viz::SurfaceInfo& surface_info);

  void CreateCompositorFrameSinkSupport();
  void ResetCompositorFrameSinkSupport();
  void DetachFromTouchSelectionClientManagerIfNecessary();

  // Returns false if the view cannot be shown. This is the case where the frame
  // associated with this view or a cross process ancestor frame has been hidden
  // using CSS.
  bool CanBecomeVisible();

  void OnDidUpdateVisualPropertiesComplete(
      const cc::RenderFrameMetadata& metadata);

  void ProcessTouchpadZoomEventAckInRoot(const blink::WebGestureEvent& event,
                                         InputEventAckState ack_result);
  void ForwardTouchpadZoomEventIfNecessary(
      const blink::WebGestureEvent& event,
      InputEventAckState ack_result) override;

  std::vector<base::OnceClosure> frame_swapped_callbacks_;

  // The surface client ID of the parent RenderWidgetHostView.  0 if none.
  viz::FrameSinkId parent_frame_sink_id_;

  const bool enable_viz_;
  bool has_frame_ = false;
  viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink_ =
      nullptr;

  gfx::Insets insets_;

  std::unique_ptr<TouchSelectionControllerClientChildFrame>
      selection_controller_client_;

  // True if there is currently a scroll sequence being bubbled to our parent.
  bool is_scroll_sequence_bubbling_ = false;

  base::WeakPtrFactory<RenderWidgetHostViewChildFrame> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewChildFrame);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_CHILD_FRAME_H_
