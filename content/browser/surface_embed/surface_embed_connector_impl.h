// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SURFACE_EMBED_SURFACE_EMBED_CONNECTOR_IMPL_H_
#define CONTENT_BROWSER_SURFACE_EMBED_SURFACE_EMBED_CONNECTOR_IMPL_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "content/browser/renderer_host/frame_connector.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/surface_embed_connector.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom.h"
#include "third_party/blink/public/mojom/frame/viewport_intersection_state.mojom.h"
#include "ui/base/cursor/cursor.h"
#include "ui/compositor/compositor.h"
#include "ui/display/screen_infos.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace input {

class RenderWidgetHostInputEventRouter;

}  // namespace input

namespace content {

class FrameTree;
class RenderFrameHost;
class RenderFrameHostImpl;
class RenderViewHostDelegateView;
class RenderWidgetHostViewChildFrame;
class TextInputManager;
class WebContentsImpl;
class WebContentsView;

class CONTENT_EXPORT SurfaceEmbedConnectorImpl
    : public SurfaceEmbedConnector,
      public FrameConnector {
 public:
  ~SurfaceEmbedConnectorImpl() override;

  static bool ContainsOrIsFocusedWebContents(WebContentsImpl* web_contents);

  WebContentsView* GetParentWebContentsView() const;
  RenderViewHostDelegateView* GetParentRenderViewHostDelegateView() const;

  // Returns the InputEventRouter appropriate for the child web contents to
  // register with. Note that this is the parent web contents's
  // InputEventRouter, and this will return nullptr if the parent web contents
  // is null.
  input::RenderWidgetHostInputEventRouter* GetInputEventRouter();

  // Returns the parent web contents's TextInputManager, or nullptr if the
  // parent web contents is null.
  TextInputManager* GetTextInputManager();

  // SurfaceEmbedConnector:
  SurfaceEmbedConnector::Delegate* GetDelegate() override;
  void OnSynchronizeVisualProperties(
      const blink::FrameVisualProperties& visual_properties) override;
  const viz::FrameSinkId& GetFrameSinkId() const override;
  double GetCssZoomFactorForTesting() override;
  const gfx::Size& GetLocalFrameSizeInPixelsForTesting() override;

  // FrameConnector:
  void SetKeepSurfaceAlive(bool keep_alive) override;
  bool IsKeepingAlive() const override;
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
  void OnVisibilityChanged(blink::mojom::FrameVisibility visibility) override;
  bool IsVisible() override;
  void DelegateWasShown() override;
  Visibility EmbedderVisibility() override;

  // input::ChildFrameInputHelper::Delegate:
  input::RenderWidgetHostViewInput* GetParentViewInput() override;
  input::RenderWidgetHostViewInput* GetRootViewInput() override;

  void OnRenderFrameCreated();

  // Updates the `view_` member to track the current RenderWidgetHostView
  // associated with the child WebContents.
  void UpdateViewForCurrentRenderFrameHost();

  // Returns nullptr if the focus is outside of this connector's child
  // WebContents.
  FrameTree* GetFocusFrameTreeIfContainsFocus();
  void SetFocusedFrameTree(FrameTree* frame_tree_to_focus);
  void ClearFocusOnInnerWebContents();

 private:
  class WCObserver;
  class ParentWCObserver;

  friend class SurfaceEmbedConnector;
  friend class SurfaceEmbedConnectorImplBrowserTest;
  friend class SurfaceEmbedConnectorWebContentsBrowserTest;

  // `child_web_contents` will have ownership of this. `delegate` is required to
  // outlive this. Assumes that `child_web_contents` is non-null.
  SurfaceEmbedConnectorImpl(WebContents* child_web_contents,
                            WebContents* parent_web_contents,
                            RenderFrameHost* embedder_rfh,
                            SurfaceEmbedConnector::Delegate* delegate);

  static WebContentsImpl* GetParentWebContents(WebContentsImpl* web_contents);
  static WebContentsImpl* GetRootWebContents(WebContentsImpl* web_contents);

  WebContentsImpl* parent_web_contents() const;
  WebContentsImpl* child_web_contents() const {
    return child_web_contents_.get();
  }

  // Resets the rect and the viz::LocalSurfaceId of the connector to ensure the
  // unguessable surface ID is not reused after a navigation.
  void ResetRectInParentView();

  RenderFrameHostImpl* current_child_frame_host() const;

  void ParentVisibilityChanged(Visibility visibility);
  void UpdateChildVisibility();

  // Observes the child WebContents to send notifications to the connector.
  std::unique_ptr<WCObserver> wc_observer_;
  // Observes the parent WebContents to propagate visibility changes.
  std::unique_ptr<ParentWCObserver> parent_wc_observer_;

  raw_ptr<SurfaceEmbedConnector::Delegate> delegate_ = nullptr;

  raw_ptr<WebContentsImpl> child_web_contents_;  // Owns this object.
  // WeakPtr to the parent WebContents. Automatically clears to nullptr when the
  // observed parent is destroyed, safely notifying all consumers.
  base::WeakPtr<WebContents> parent_web_contents_;
  raw_ptr<RenderWidgetHostViewChildFrame> view_ = nullptr;

  // The last received FrameSinkId from the guest WebContents's view.
  viz::FrameSinkId frame_sink_id_;

  // The last received LocalSurfaceId from the SurfaceEmbed.
  viz::LocalSurfaceId local_surface_id_;

  // Visibility state of the corresponding surface embed element in parent
  // process which is set through CSS or scrolling.
  blink::mojom::FrameVisibility visibility_ =
      blink::mojom::FrameVisibility::kRenderedInViewport;

  uint32_t capture_sequence_number_ = 0u;

  display::ScreenInfos screen_infos_;
  blink::mojom::ViewportIntersectionState intersection_state_;
  gfx::Rect rect_in_parent_view_in_dip_;
  gfx::Size local_frame_size_in_dip_;
  gfx::Size local_frame_size_in_pixels_;

  gfx::Size last_received_local_frame_size_;
  double last_received_css_zoom_factor_ = 1;
  double last_received_zoom_level_ = 0.0;
  bool has_size_ = false;

  // TODO(crbug.com/493315755): Update the properties as appropriate. (Currently
  // they are not updated after initialization.)
  bool is_inert_ = false;
  cc::TouchAction inherited_effective_touch_action_ = cc::TouchAction::kAuto;
  bool is_throttled_ = false;
  bool subtree_throttled_ = false;
  bool display_locked_ = false;

  void MaybeRefreshKeepSurfaceAlive();

  ui::Compositor::ScopedKeepSurfaceAliveCallback keep_surface_alive_;
  bool should_keep_alive_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SURFACE_EMBED_SURFACE_EMBED_CONNECTOR_IMPL_H_
