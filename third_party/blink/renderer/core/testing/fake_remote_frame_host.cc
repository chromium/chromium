// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/fake_remote_frame_host.h"

namespace blink {

void FakeRemoteFrameHost::Init(blink::AssociatedInterfaceProvider* provider) {
  provider->OverrideBinderForTesting(
      mojom::blink::RemoteFrameHost::Name_,
      base::BindRepeating(&FakeRemoteFrameHost::BindFrameHostReceiver,
                          base::Unretained(this)));
}

void FakeRemoteFrameHost::SetInheritedEffectiveTouchAction(
    cc::TouchAction touch_action) {}

void FakeRemoteFrameHost::UpdateRenderThrottlingStatus(bool is_throttled,
                                                       bool subtree_throttled) {
}

void FakeRemoteFrameHost::VisibilityChanged(
    mojom::blink::FrameVisibility visibility) {}

void FakeRemoteFrameHost::DidFocusFrame() {}

void FakeRemoteFrameHost::CheckCompleted() {}

void FakeRemoteFrameHost::CapturePaintPreviewOfCrossProcessSubframe(
    const gfx::Rect& clip_rect,
    const base::UnguessableToken& guid) {}

void FakeRemoteFrameHost::SetIsInert(bool inert) {}

void FakeRemoteFrameHost::DidChangeOpener(
    const base::Optional<base::UnguessableToken>& opener_frame_token) {}

void FakeRemoteFrameHost::AdvanceFocus(
    blink::mojom::FocusType focus_type,
    const base::UnguessableToken& source_frame_token) {}

void FakeRemoteFrameHost::RouteMessageEvent(
    const base::Optional<base::UnguessableToken>& source_frame_token,
    const String& source_origin,
    const String& target_origin,
    BlinkTransferableMessage message) {}

void FakeRemoteFrameHost::PrintCrossProcessSubframe(const gfx::Rect& rect,
                                                    int document_cookie) {}

void FakeRemoteFrameHost::Detach() {}

void FakeRemoteFrameHost::UpdateViewportIntersection(
    blink::mojom::blink::ViewportIntersectionStatePtr intersection_state) {}

void FakeRemoteFrameHost::SynchronizeVisualProperties(
    const blink::FrameVisualProperties& properties) {}

void FakeRemoteFrameHost::BindFrameHostReceiver(
    mojo::ScopedInterfaceEndpointHandle handle) {
  receiver_.Bind(mojo::PendingAssociatedReceiver<mojom::blink::RemoteFrameHost>(
      std::move(handle)));
}

}  // namespace blink
