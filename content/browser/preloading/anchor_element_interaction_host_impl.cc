// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/anchor_element_interaction_host_impl.h"
#include "content/browser/preloading/preloading_decider.h"

namespace content {

AnchorElementInteractionHostImpl::AnchorElementInteractionHostImpl(
    RenderFrameHost& frame_host,
    mojo::PendingReceiver<blink::mojom::AnchorElementInteractionHost> receiver)
    : DocumentService(frame_host, std::move(receiver)) {}

// static
void AnchorElementInteractionHostImpl::Create(
    RenderFrameHost* frame_host,
    mojo::PendingReceiver<blink::mojom::AnchorElementInteractionHost>
        receiver) {
  CHECK(frame_host);

  new AnchorElementInteractionHostImpl(*frame_host, std::move(receiver));
}

void AnchorElementInteractionHostImpl::OnPointerDown(const GURL& url) {
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&render_frame_host());
  preloading_decider->OnPointerDown(url);
}

void AnchorElementInteractionHostImpl::OnPointerHover(
    const GURL& url,
    blink::mojom::AnchorElementPointerDataPtr mouse_data) {
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&render_frame_host());
  preloading_decider->OnPointerHover(url, std::move(mouse_data));
}

}  // namespace content
