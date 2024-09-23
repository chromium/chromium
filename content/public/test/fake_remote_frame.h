// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_FAKE_REMOTE_FRAME_H_
#define CONTENT_PUBLIC_TEST_FAKE_REMOTE_FRAME_H_

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom.h"
#include "third_party/blink/public/mojom/frame/remote_frame.mojom.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom.h"
#include "ui/events/types/scroll_types.h"

namespace base {
class UnguessableToken;
}

namespace url {
class Origin;
}  // namespace url

namespace content {

// This class implements a RemoteFrame that can be bound and passed to
// the RenderFrameProxy. Calls typically routed to the renderer process
// will then be intercepted to this class.
class FakeRemoteFrame : public blink::mojom::RemoteFrame {
 public:
  FakeRemoteFrame();
  ~FakeRemoteFrame() override;

  void Init(
      mojo::PendingAssociatedReceiver<blink::mojom::RemoteFrame> receiver);

  // blink::mojom::RemoteFrame overrides:
  void WillEnterFullscreen(blink::mojom::FullscreenOptionsPtr) override;
  void EnforceInsecureNavigationsSet(const std::vector<uint32_t>& set) override;
  void SetFrameOwnerProperties(
      blink::mojom::FrameOwnerPropertiesPtr properties) override;
  void EnforceInsecureRequestPolicy(
      blink::mojom::InsecureRequestPolicy policy) override;
  void SetReplicatedOrigin(
      const url::Origin& origin,
      bool is_potentially_trustworthy_unique_origin) override;
  void SetReplicatedIsAdFrame(bool is_ad_frame) override;
  void SetReplicatedName(const std::string& name,
                         const std::string& unique_name) override;
  void DispatchLoadEventForFrameOwner() override;
  void Collapse(bool collapsed) final;
  void Focus() override;
  void SetHadStickyUserActivationBeforeNavigation(bool value) override;
  void SetNeedsOcclusionTracking(bool needs_tracking) override;
  void BubbleLogicalScroll(blink::mojom::ScrollDirection direction,
                           ui::ScrollGranularity granularity) override;
  void UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType update_type,
      blink::mojom::UserActivationNotificationType notification_type) override;
  void SetEmbeddingToken(
      const base::UnguessableToken& embedding_token) override;
  void SetPageFocus(bool is_focused) override;
  void RenderFallbackContent() override;
  void AddResourceTimingFromChild(
      blink::mojom::ResourceTimingInfoPtr timing) override;

  void ScrollRectToVisible(
      const gfx::RectF& rect,
      blink::mojom::ScrollIntoViewParamsPtr params) override;
  void DidStartLoading() override;
  void DidStopLoading() override;
  void IntrinsicSizingInfoOfChildChanged(
      blink::mojom::IntrinsicSizingInfoPtr sizing_info) override;
  void DidSetFramePolicyHeaders(
      network::mojom::WebSandboxFlags sandbox_flags,
      const std::vector<blink::ParsedPermissionsPolicyDeclaration>&
          parsed_permissions_policy) override {}
  void DidUpdateFramePolicy(const blink::FramePolicy& frame_policy) override {}
  void UpdateOpener(
      const std::optional<blink::FrameToken>& opener_frame_token) override;
  void DetachAndDispose() override;
  void EnableAutoResize(const gfx::Size& min_size,
                        const gfx::Size& max_size) override;
  void DisableAutoResize() override;
  void DidUpdateVisualProperties(
      const cc::RenderFrameMetadata& metadata) override;
  void SetFrameSinkId(const viz::FrameSinkId& frame_sink_id,
                      bool allow_paint_holding) override;
  void ChildProcessGone() override;
  void CreateRemoteChild(
      const blink::RemoteFrameToken& token,
      const std::optional<blink::FrameToken>& opener_frame_token,
      blink::mojom::TreeScopeType tree_scope_type,
      blink::mojom::FrameReplicationStatePtr replication_state,
      blink::mojom::FrameOwnerPropertiesPtr owner_properties,
      bool is_loading,
      const base::UnguessableToken& devtools_frame_token,
      blink::mojom::RemoteFrameInterfacesFromBrowserPtr remote_frame_interfaces)
      override;
  void CreateRemoteChildren(
      std::vector<blink::mojom::CreateRemoteChildParamsPtr> params) override;
  void ForwardFencedFrameEventToEmbedder(
      const std::string& event_type) override;

 private:
  mojo::AssociatedReceiver<blink::mojom::RemoteFrame> receiver_{this};
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FAKE_REMOTE_FRAME_H_
