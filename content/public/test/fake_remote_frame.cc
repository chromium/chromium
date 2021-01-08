// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/fake_remote_frame.h"

#include "third_party/blink/public/mojom/timing/resource_timing.mojom.h"

namespace content {

FakeRemoteFrame::FakeRemoteFrame() = default;

FakeRemoteFrame::~FakeRemoteFrame() = default;

void FakeRemoteFrame::Init(blink::AssociatedInterfaceProvider* provider) {
  provider->OverrideBinderForTesting(
      blink::mojom::RemoteFrame::Name_,
      base::BindRepeating(&FakeRemoteFrame::BindFrameHostReceiver,
                          base::Unretained(this)));
}

void FakeRemoteFrame::WillEnterFullscreen(blink::mojom::FullscreenOptionsPtr) {}

void FakeRemoteFrame::AddReplicatedContentSecurityPolicies(
    std::vector<network::mojom::ContentSecurityPolicyHeaderPtr> headers) {}

void FakeRemoteFrame::ResetReplicatedContentSecurityPolicy() {}

void FakeRemoteFrame::EnforceInsecureNavigationsSet(
    const std::vector<uint32_t>& set) {}

void FakeRemoteFrame::SetFrameOwnerProperties(
    blink::mojom::FrameOwnerPropertiesPtr properties) {}

void FakeRemoteFrame::EnforceInsecureRequestPolicy(
    blink::mojom::InsecureRequestPolicy policy) {}

void FakeRemoteFrame::SetReplicatedOrigin(
    const url::Origin& origin,
    bool is_potentially_trustworthy_unique_origin) {}

void FakeRemoteFrame::SetReplicatedAdFrameType(
    blink::mojom::AdFrameType ad_frame_type) {}

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
    const gfx::Rect& rect,
    blink::mojom::ScrollIntoViewParamsPtr params) {}

void FakeRemoteFrame::DidStartLoading() {}

void FakeRemoteFrame::DidStopLoading() {}

void FakeRemoteFrame::IntrinsicSizingInfoOfChildChanged(
    blink::mojom::IntrinsicSizingInfoPtr sizing_info) {}

void FakeRemoteFrame::UpdateOpener(
    const base::Optional<base::UnguessableToken>& opener_frame_token) {}

void FakeRemoteFrame::FakeRemoteFrame::BindFrameHostReceiver(
    mojo::ScopedInterfaceEndpointHandle handle) {
  receiver_.Bind(mojo::PendingAssociatedReceiver<blink::mojom::RemoteFrame>(
      std::move(handle)));
}

void FakeRemoteFrame::DetachAndDispose() {}

}  // namespace content
