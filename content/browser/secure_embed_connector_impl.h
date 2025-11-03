// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SECURE_EMBED_CONNECTOR_IMPL_H_
#define CONTENT_BROWSER_SECURE_EMBED_CONNECTOR_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/input/touch_action.h"
#include "content/browser/renderer_host/cross_process_frame_connector_base.h"
#include "content/public/browser/secure_embed_connector.h"
#include "content/public/browser/visibility.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom.h"
#include "third_party/blink/public/mojom/frame/viewport_intersection_state.mojom.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "third_party/blink/public/mojom/input/pointer_lock_result.mojom-shared.h"
#include "ui/display/screen_infos.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

class RenderFrameHostImpl;
class WebContentsImpl;

class SecureEmbedConnectorImpl : public SecureEmbedConnector,
                                 public CrossProcessFrameConnectorBase {
 public:
  // `embedded_web_contents` will have ownership of this.
  SecureEmbedConnectorImpl(WebContentsImpl* embedder_web_contents,
                           WebContentsImpl* embedded_web_contents);
  ~SecureEmbedConnectorImpl() override;

  WebContents* GetEmbedderWebContents() override;

  // Convenience wrapper for GetDelegate()->FocusInEmbedder that null-checks
  // the delegate.
  void FocusInEmbedder(FocusOperation focus_op);

  // SecureEmbedConnector:
  void SetDelegate(SecureEmbedConnector::Delegate* delegate) override;
  SecureEmbedConnector::Delegate* GetDelegate() override;

  // CrossProcessFrameConnectorBase:
  // TODO(secure-embed): Some of the methods that we override here don't need to
  // be on CrossProcessFrameConnectorBase class at all. Go through them and
  // remove anything that isn't directly used by the view from the base class.
  void OnSynchronizeVisualProperties(
      const blink::FrameVisualProperties& visual_properties) override;
  const viz::FrameSinkId& GetFrameSinkId() const override;
  void ForwardKeyboardEvent(
      const blink::WebKeyboardEvent& keyboard_event) override;
  void SetFocus(bool focused, blink::mojom::FocusType focus_type) override;

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
      bool propagate = true) override;
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
  content::Visibility EmbedderVisibility() override;

  // input::ChildFrameInputHelper::Delegate:
  input::RenderWidgetHostViewInput* GetParentViewInput() override;
  input::RenderWidgetHostViewInput* GetRootViewInput() override;

  void OnRenderViewReady();
  void OnRenderFrameHostChanged(RenderFrameHost* old_host,
                                RenderFrameHost* new_host);

 private:
  // Forward decl for internal observer that tracks WebContents events and
  // forwards them to this class.
  class Observer;

  // Resets the rect and the viz::LocalSurfaceId of the connector to ensure the
  // unguessable surface ID is not reused after a cross-process navigation.
  void ResetRectInParentView();

  void UpdateViewportIntersectionInternal(
      const blink::mojom::ViewportIntersectionState& intersection_state,
      bool include_visual_properties);

  // Updates the view_ member to track the current RenderWidgetHostView
  // associated with the guest WebContents.
  void UpdateViewForCurrentRenderFrameHost();

  // Get the RenderFrameHost for the embedded WebContents. This is similar to
  // the CrossProcessFrameConnectorBase::current_child_frame_host(), but adapted
  // for GuestFrameImpl to get the RenderFrameHost from the guest WebContents.
  RenderFrameHostImpl* current_child_frame_host() const;

  std::unique_ptr<Observer> observer_;
  raw_ptr<SecureEmbedConnector::Delegate> delegate_ = nullptr;

  base::WeakPtr<WebContents> embedder_web_contents_;
  raw_ptr<WebContentsImpl> guest_web_contents_ = nullptr;  // Owns us.
  raw_ptr<RenderWidgetHostViewChildFrame> view_ = nullptr;

  // This is here rather than in the implementation class so that
  // `GetIntersectionState()` can return a reference.
  blink::mojom::ViewportIntersectionState intersection_state_;

  display::ScreenInfos screen_infos_;

  bool has_size_ = false;
  gfx::Size local_frame_size_in_dip_;
  gfx::Size local_frame_size_in_pixels_;
  gfx::Rect rect_in_parent_view_in_dip_;

  // The last pre-transform frame size received from the parent renderer.
  // |last_received_local_frame_size_| may be in DIP if use zoom for DSF is
  // off.
  gfx::Size last_received_local_frame_size_;

  // The last zoom level received from parent renderer, which is used to check
  // if a new surface is created in case of zoom level change.
  double last_received_zoom_level_ = 0.0;

  // Represents CSS zoom applied to the embedding element in the parent.
  double last_received_css_zoom_factor_ = 1.0;

  // Visibility state of the corresponding secureembed element in parent process
  // which is set through CSS or scrolling.
  blink::mojom::FrameVisibility visibility_ =
      blink::mojom::FrameVisibility::kRenderedInViewport;

  // The last received FrameSinkId from the guest WebContents's view.
  viz::FrameSinkId frame_sink_id_;

  // The last received LocalSurfaceId from the SecureEmbed.
  viz::LocalSurfaceId local_surface_id_;

  // TODO(secure-embed): Not implemented fully yet.
  uint32_t capture_sequence_number_ = 0u;

  cc::TouchAction inherited_effective_touch_action_ = cc::TouchAction::kAuto;

  bool is_inert_ = false;
  bool is_throttled_ = false;
  bool subtree_throttled_ = false;
  bool display_locked_ = false;
};

}  // namespace content

#endif  // #define CONTENT_BROWSER_SECURE_EMBED_CONNECTOR_IMPL_H_
