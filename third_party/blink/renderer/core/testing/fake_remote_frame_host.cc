// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/fake_remote_frame_host.h"

#include "third_party/blink/public/mojom/frame/viewport_intersection_state.mojom-blink.h"

namespace blink {

mojo::PendingAssociatedRemote<mojom::blink::RemoteFrameHost>
FakeRemoteFrameHost::BindNewAssociatedRemote() {
  return receiver_.BindNewEndpointAndPassDedicatedRemote();
}

void FakeRemoteFrameHost::SetInheritedEffectiveTouchAction(
    cc::TouchAction touch_action) {}

void FakeRemoteFrameHost::UpdateRenderThrottlingStatus(bool is_throttled,
                                                       bool subtree_throttled,
                                                       bool display_locked) {}

void FakeRemoteFrameHost::VisibilityChanged(
    mojom::blink::FrameVisibility visibility) {}

void FakeRemoteFrameHost::DidFocusFrame() {}

void FakeRemoteFrameHost::CheckCompleted() {}

void FakeRemoteFrameHost::CapturePaintPreviewOfCrossProcessSubframe(
    const gfx::Rect& clip_rect,
    const base::UnguessableToken& guid) {}

void FakeRemoteFrameHost::SetIsInert(bool inert) {}

void FakeRemoteFrameHost::DidChangeOpener(
    const std::optional<LocalFrameToken>& opener_frame_token) {}

void FakeRemoteFrameHost::AdvanceFocus(
    blink::mojom::FocusType focus_type,
    const LocalFrameToken& source_frame_token) {}

void FakeRemoteFrameHost::RouteMessageEvent(
    const std::optional<LocalFrameToken>& source_frame_token,
    const scoped_refptr<const SecurityOrigin>& source_origin,
    const String& target_origin,
    BlinkTransferableMessage message) {}

void FakeRemoteFrameHost::PrintCrossProcessSubframe(const gfx::Rect& rect,
                                                    int document_cookie) {}

void FakeRemoteFrameHost::Detach() {}

void FakeRemoteFrameHost::UpdateViewportIntersection(
    blink::mojom::blink::ViewportIntersectionStatePtr intersection_state,
    const std::optional<FrameVisualProperties>& visual_properties) {}

void FakeRemoteFrameHost::SynchronizeVisualProperties(
    const blink::FrameVisualProperties& properties) {}

void FakeRemoteFrameHost::OpenURL(mojom::blink::OpenURLParamsPtr params) {}

}  // namespace blink
