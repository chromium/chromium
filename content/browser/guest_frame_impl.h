// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GUEST_FRAME_IMPL_H_
#define CONTENT_BROWSER_GUEST_FRAME_IMPL_H_

#include "content/browser/renderer_host/cross_process_frame_connector_base.h"
#include "content/public/browser/guest_frame.h"

namespace content {

class GuestFrameImpl : public GuestFrame,
                       public CrossProcessFrameConnectorBase {
 public:
  GuestFrameImpl(WebContents* guest_web_contents);
  ~GuestFrameImpl() override;

  // GuestFrame:
  void SetLocalSurfaceId(const viz::LocalSurfaceId& local_surface_id) override;
  const viz::FrameSinkId& GetFrameSinkId() const override;

  // CrossProcessFrameConnectorBase:
  void SetView(RenderWidgetHostViewChildFrame* view,
               bool allow_paint_holding) override;
  RenderWidgetHostViewBase* GetParentRenderWidgetHostView() override;
  RenderWidgetHostViewBase* GetRootRenderWidgetHostView() override;
  void RenderProcessGone() override;
  void FirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void SendIntrinsicSizingInfoToParent(
      blink::mojom::IntrinsicSizingInfoPtr) override;
  void SynchronizeVisualProperties(
      const blink::FrameVisualProperties& visual_properties,
      bool propagate) override;
  void UpdateCursor(const ui::Cursor& cursor) override;
  RootViewFocusState HasFocus() override;
  void FocusRootView() override;
  blink::mojom::PointerLockResult LockPointer(
      bool request_unadjusted_movement) override;
  blink::mojom::PointerLockResult ChangePointerLock(
      bool request_unadjusted_movement) override;
  void UnlockPointer() override;
  bool HasSize() const override;
  const display::ScreenInfos& GetScreenInfos() const override;
  const viz::LocalSurfaceId& GetLocalSurfaceId() const override;
  const blink::mojom::ViewportIntersectionState& GetIntersectionState()
      const override;
  uint32_t GetCaptureSequenceNumber() const override;
  const gfx::Rect& GetRectInParentViewInDip() const override;
  const gfx::Size& GetLocalFrameSizeInDip() const override;
  const gfx::Size& GetLocalFrameSizeInPixels() const override;
  double GetCssZoomFactor() const override;
  void EnableAutoResize(const gfx::Size& min_size,
                        const gfx::Size& max_size) override;
  void DisableAutoResize() override;
  bool IsInert() const override;
  cc::TouchAction InheritedEffectiveTouchAction() const override;
  bool IsHidden() const override;
  bool IsThrottled() const override;
  bool IsSubtreeThrottled() const override;
  bool IsDisplayLocked() const override;
  void DidUpdateVisualProperties(
      const cc::RenderFrameMetadata& metadata) override;
  void SetVisibilityForChildViews(bool visible) const override;
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
  content::Visibility EmbedderVisibility() override;

  // input::ChildFrameInputHelper::Delegate:
  input::RenderWidgetHostViewInput* GetParentViewInput() override;
  input::RenderWidgetHostViewInput* GetRootViewInput() override;

 private:
  viz::FrameSinkId frame_sink_id_;
  viz::LocalSurfaceId local_surface_id_;
  mutable display::ScreenInfos screen_infos_;
  raw_ptr<WebContents> guest_web_contents_ = nullptr;
  raw_ptr<RenderWidgetHostViewChildFrame> view_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GUEST_FRAME_IMPL_H_
