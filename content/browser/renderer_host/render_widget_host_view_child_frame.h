// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_CHILD_FRAME_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_CHILD_FRAME_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/input/touch_action.h"
#include "components/input/event_with_latency_info.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/content_export.h"
#include "content/public/browser/touch_selection_controller_client_manager.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "third_party/blink/public/common/widget/visual_properties.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/viewport_intersection_state.mojom-forward.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "third_party/blink/public/mojom/widget/record_content_to_visible_time_request.mojom-forward.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(IS_MAC)
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"
#endif  // BUILDFLAG(IS_MAC)

namespace content {
class CrossProcessFrameConnector;
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
  // TODO(crbug.com/40170974): Pass multi-screen info from the parent.
  static RenderWidgetHostViewChildFrame* Create(
      RenderWidgetHost* widget,
      const display::ScreenInfos& parent_screen_infos);

  RenderWidgetHostViewChildFrame(const RenderWidgetHostViewChildFrame&) =
      delete;
  RenderWidgetHostViewChildFrame& operator=(
      const RenderWidgetHostViewChildFrame&) = delete;

  void SetFrameConnector(CrossProcessFrameConnector* frame_connector);

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
  void Hide() override;
  bool IsShowing() override;
  void WasUnOccluded() override;
  void WasOccluded() override;
  gfx::Rect GetViewBounds() override;
  gfx::Size GetVisibleViewportSize() override;
  void SetInsets(const gfx::Insets& insets) override;
  gfx::NativeView GetNativeView() override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  bool IsPointerLocked() override;
  void TakeFallbackContentFrom(RenderWidgetHostView* view) override;

  // RenderWidgetHostViewBase implementation.
  RenderWidgetHostViewBase* GetRootView() override;
  uint32_t GetCaptureSequenceNumber() const override;
  gfx::Size GetCompositorViewportPixelSize() override;
  void InitAsPopup(RenderWidgetHostView* parent_host_view,
                   const gfx::Rect& bounds,
                   const gfx::Rect& anchor_rect) override;
  void UpdateCursor(const ui::Cursor& cursor) override;
  void UpdateScreenInfo() override;
  void SendInitialPropertiesIfNeeded() override;
  void SetIsLoading(bool is_loading) override;
  void RenderProcessGone() override;
  void ShowWithVisibility(PageVisibilityState page_visibility) override;
  void Destroy() override;
  void UpdateTooltipUnderCursor(const std::u16string& tooltip_text) override;
  void UpdateTooltipFromKeyboard(const std::u16string& tooltip_text,
                                 const gfx::Rect& bounds) override;
  void ClearKeyboardTriggeredTooltip() override;
  void GestureEventAck(const blink::WebGestureEvent& event,
                       blink::mojom::InputEventResultSource ack_source,
                       blink::mojom::InputEventResultState ack_result) override;
  // Since the URL of content rendered by this class is not displayed in
  // the URL bar, this method does not need an implementation.
  void ResetFallbackToFirstNavigationSurface() override {}

  void TransformPointToRootSurface(gfx::PointF* point) override;
  gfx::Rect GetBoundsInRootWindow() override;
  void DidStopFlinging() override;
  blink::mojom::PointerLockResult LockPointer(
      bool request_unadjusted_movement) override;
  blink::mojom::PointerLockResult ChangePointerLock(
      bool request_unadjusted_movement) override;
  void UnlockPointer() override;
  const viz::FrameSinkId& GetFrameSinkId() const override;
  const viz::LocalSurfaceId& GetLocalSurfaceId() const override;
  void NotifyHitTestRegionUpdated(const viz::AggregatedHitTestRegion&) override;
  bool ScreenRectIsUnstableFor(const blink::WebInputEvent& event) override;
  bool ScreenRectIsUnstableForIOv2For(
      const blink::WebInputEvent& event) override;
  void PreProcessTouchEvent(const blink::WebTouchEvent& event) override;
  viz::FrameSinkId GetRootFrameSinkId() override;
  viz::SurfaceId GetCurrentSurfaceId() const override;
  bool HasSize() const override;
  double GetCSSZoomFactor() const override;
  gfx::PointF TransformPointToRootCoordSpaceF(
      const gfx::PointF& point) override;
  bool TransformPointToCoordSpaceForView(
      const gfx::PointF& point,
      input::RenderWidgetHostViewInput* target_view,
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
  void InvalidateLocalSurfaceIdAndAllocationGroup() override;

#if BUILDFLAG(IS_MAC)
  // RenderWidgetHostView implementation.
  void SetActive(bool active) override;
  void ShowDefinitionForSelection() override;
  void SpeakSelection() override;
  void SetWindowFrameInScreen(const gfx::Rect& rect) override;
  void ShowSharePicker(
      const std::string& title,
      const std::string& text,
      const std::string& url,
      const std::vector<std::string>& file_paths,
      blink::mojom::ShareService::ShareCallback callback) override;
  uint64_t GetNSViewId() const override;
#endif  // BUILDFLAG(IS_MAC)

  blink::mojom::InputEventResultState FilterInputEvent(
      const blink::WebInputEvent& input_event) override;
  void EnableAutoResize(const gfx::Size& min_size,
                        const gfx::Size& max_size) override;
  void DisableAutoResize(const gfx::Size& new_size) override;
  viz::ScopedSurfaceIdAllocator DidUpdateVisualProperties(
      const cc::RenderFrameMetadata& metadata) override;

  // RenderFrameMetadataProvider::Observer implementation.
  void OnRenderFrameMetadataChangedBeforeActivation(
      const cc::RenderFrameMetadata& metadata) override {}
  void OnRenderFrameMetadataChangedAfterActivation(
      base::TimeTicks activation_time) override;
  void OnRenderFrameSubmission() override {}
  void OnLocalSurfaceIdChanged(
      const cc::RenderFrameMetadata& metadata) override {}

  // viz::HostFrameSinkClient implementation.
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void OnFrameTokenChanged(uint32_t frame_token,
                           base::TimeTicks activation_time) override;

  CrossProcessFrameConnector* FrameConnectorForTesting() const {
    return frame_connector_;
  }

  RenderWidgetHostViewBase* GetParentViewInput() override;

  void RegisterFrameSinkId();
  void UnregisterFrameSinkId();

  void UpdateViewportIntersection(
      const blink::mojom::ViewportIntersectionState& intersection_state,
      const std::optional<blink::VisualProperties>& visual_properties);

  // TODO(sunxd): Rename SetIsInert to UpdateIsInert.
  void SetIsInert();
  void UpdateInheritedEffectiveTouchAction();

  void UpdateRenderThrottlingStatus();

  ui::TextInputType GetTextInputType() const;

  // Retrieves the UTF-16 code unit range containing accessible text in the
  // view. Returns false if the information cannot be retrieved right now.
  bool GetTextRange(gfx::Range* range) const;

  RenderWidgetHostViewBase* GetRootRenderWidgetHostView() const;

 protected:
  friend class RenderWidgetHostView;
  friend class RenderWidgetHostViewChildFrameTest;
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewChildFrameTest,
                           ForwardsBeginFrameAcks);

  RenderWidgetHostViewChildFrame(
      RenderWidgetHost* widget,
      const display::ScreenInfos& parent_screen_infos);
  void Init();

  // Sets |parent_frame_sink_id_| and registers frame sink hierarchy. If the
  // parent was already set then it also unregisters hierarchy.
  void SetParentFrameSinkId(const viz::FrameSinkId& parent_frame_sink_id);

  // Clears current compositor surface, if one is in use.
  void ClearCompositorSurfaceIfNecessary();

  void ProcessFrameSwappedCallbacks();

  // RenderWidgetHostViewBase:
  void UpdateFrameSinkIdRegistration() override;
  void UpdateBackgroundColor() override;
  std::optional<DisplayFeature> GetDisplayFeature() override;
  void SetDisplayFeatureForTesting(
      const DisplayFeature* display_feature) override;
  void NotifyHostAndDelegateOnWasShown(
      blink::mojom::RecordContentToVisibleTimeRequestPtr) final;
  void RequestSuccessfulPresentationTimeFromHostOrDelegate(
      blink::mojom::RecordContentToVisibleTimeRequestPtr) final;
  void CancelSuccessfulPresentationTimeRequestForHostAndDelegate() final;

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
  raw_ptr<CrossProcessFrameConnector> frame_connector_;

  base::WeakPtr<RenderWidgetHostViewChildFrame> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  ui::Compositor* GetCompositor() override;

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
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostInputEventRouterTest,
                           FilteredGestureDoesntInterruptBubbling);

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
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result);
  void ForwardTouchpadZoomEventIfNecessary(
      const blink::WebGestureEvent& event,
      blink::mojom::InputEventResultState ack_result) override;

  // Performs gesture ack handling needed for swipe-to-move-cursor gestures.
  void HandleSwipeToMoveCursorGestureAck(const blink::WebGestureEvent& event);

  std::vector<base::OnceClosure> frame_swapped_callbacks_;

  // The surface client ID of the parent RenderWidgetHostView.  0 if none.
  viz::FrameSinkId parent_frame_sink_id_;

  gfx::RectF last_stable_screen_rect_;
  gfx::RectF last_stable_screen_rect_for_iov2_;
  base::TimeTicks screen_rect_stable_since_;
  base::TimeTicks screen_rect_stable_since_for_iov2_;

  gfx::Insets insets_;

  std::unique_ptr<TouchSelectionControllerClientChildFrame>
      selection_controller_client_;

  // True if there is currently a scroll sequence being bubbled to our parent.
  bool is_scroll_sequence_bubbling_ = false;

  // Whether a swipe-to-move-cursor gesture is activated.
  bool swipe_to_move_cursor_activated_ = false;

  // If a new RWHVCF is created for a cross-origin navigation, the parent
  // will typically not notice and will not transmit a full complement of
  // properties.
  bool initial_properties_sent_ = false;

  base::WeakPtrFactory<RenderWidgetHostViewChildFrame> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_CHILD_FRAME_H_
