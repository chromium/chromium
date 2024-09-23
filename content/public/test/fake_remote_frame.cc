// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/fake_remote_frame.h"

#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom.h"

namespace content {

FakeRemoteFrame::FakeRemoteFrame() = default;

FakeRemoteFrame::~FakeRemoteFrame() = default;

void FakeRemoteFrame::Init(
    mojo::PendingAssociatedReceiver<blink::mojom::RemoteFrame> receiver) {
  receiver_.Bind(std::move(receiver));
}

void FakeRemoteFrame::WillEnterFullscreen(blink::mojom::FullscreenOptionsPtr) {}

void FakeRemoteFrame::EnforceInsecureNavigationsSet(
    const std::vector<uint32_t>& set) {}

void FakeRemoteFrame::SetFrameOwnerProperties(
    blink::mojom::FrameOwnerPropertiesPtr properties) {}

void FakeRemoteFrame::EnforceInsecureRequestPolicy(
    blink::mojom::InsecureRequestPolicy policy) {}

void FakeRemoteFrame::SetReplicatedOrigin(
    const url::Origin& origin,
    bool is_potentially_trustworthy_unique_origin) {}

void FakeRemoteFrame::SetReplicatedIsAdFrame(bool is_ad_frame) {}

void FakeRemoteFrame::SetReplicatedName(const std::string& name,
                                        const std::string& unique_name) {}

void FakeRemoteFrame::DispatchLoadEventForFrameOwner() {}

void FakeRemoteFrame::Collapse(bool collapsed) {}

void FakeRemoteFrame::Focus() {}

void FakeRemoteFrame::SetHadStickyUserActivationBeforeNavigation(bool value) {}

void FakeRemoteFrame::SetNeedsOcclusionTracking(bool needs_tracking) {}

void FakeRemoteFrame::BubbleLogicalScroll(
    blink::mojom::ScrollDirection direction,
    ui::ScrollGranularity granularity) {}

void FakeRemoteFrame::UpdateUserActivationState(
    blink::mojom::UserActivationUpdateType update_type,
    blink::mojom::UserActivationNotificationType notification_type) {}

void FakeRemoteFrame::SetEmbeddingToken(
    const base::UnguessableToken& embedding_token) {}

void FakeRemoteFrame::SetPageFocus(bool is_focused) {}

void FakeRemoteFrame::RenderFallbackContent() {}

void FakeRemoteFrame::AddResourceTimingFromChild(
    blink::mojom::ResourceTimingInfoPtr timing) {}

void FakeRemoteFrame::ScrollRectToVisible(
    const gfx::RectF& rect,
    blink::mojom::ScrollIntoViewParamsPtr params) {}

void FakeRemoteFrame::DidStartLoading() {}

void FakeRemoteFrame::DidStopLoading() {}

void FakeRemoteFrame::IntrinsicSizingInfoOfChildChanged(
    blink::mojom::IntrinsicSizingInfoPtr sizing_info) {}

void FakeRemoteFrame::UpdateOpener(
    const std::optional<blink::FrameToken>& opener_frame_token) {}

void FakeRemoteFrame::DetachAndDispose() {}

void FakeRemoteFrame::EnableAutoResize(const gfx::Size& min_size,
                                       const gfx::Size& max_size) {}

void FakeRemoteFrame::DisableAutoResize() {}

void FakeRemoteFrame::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {}

void FakeRemoteFrame::SetFrameSinkId(const viz::FrameSinkId& frame_sink_id,
                                     bool allow_paint_holding) {}

void FakeRemoteFrame::ChildProcessGone() {}

void FakeRemoteFrame::CreateRemoteChild(
    const blink::RemoteFrameToken& token,
    const std::optional<blink::FrameToken>& opener_frame_token,
    blink::mojom::TreeScopeType tree_scope_type,
    blink::mojom::FrameReplicationStatePtr replication_state,
    blink::mojom::FrameOwnerPropertiesPtr owner_properties,
    bool is_loading,
    const base::UnguessableToken& devtools_frame_token,
    blink::mojom::RemoteFrameInterfacesFromBrowserPtr remote_frame_interfaces) {
}

void FakeRemoteFrame::CreateRemoteChildren(
    std::vector<blink::mojom::CreateRemoteChildParamsPtr> params) {}

void FakeRemoteFrame::ForwardFencedFrameEventToEmbedder(
    const std::string& event_type) {}
}  // namespace content
