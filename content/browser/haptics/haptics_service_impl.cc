// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/haptics/haptics_service_impl.h"

#include <cmath>

#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/render_frame_host.h"

namespace content {

// static
void HapticsServiceImpl::Create(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::HapticsService> receiver) {
  CHECK(render_frame_host);
  new HapticsServiceImpl(*render_frame_host, std::move(receiver));
}

HapticsServiceImpl::HapticsServiceImpl(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::HapticsService> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

void HapticsServiceImpl::PlayHaptics(blink::mojom::HapticEffect effect,
                                     double intensity) {
  if (!std::isfinite(intensity) || intensity < 0.0 || intensity > 1.0) {
    bad_message::ReceivedBadMessage(
        render_frame_host().GetProcess(),
        bad_message::HSI_PLAY_HAPTICS_INVALID_INTENSITY);
    return;
  }

  auto& rfh = static_cast<RenderFrameHostImpl&>(render_frame_host());

  if (rfh.IsNestedWithinFencedFrame()) {
    bad_message::ReceivedBadMessage(
        render_frame_host().GetProcess(),
        bad_message::HSI_PLAY_HAPTICS_IN_FENCED_FRAME);
    return;
  }

  // Drop calls queued by a document that is no longer active.
  if (!rfh.IsActive()) {
    return;
  }

  // TODO(crbug.com/531787872): Re-check the "haptics" permissions policy here
  // once the feature is defined.

  if (!rfh.HasStickyUserActivation()) {
    return;
  }

  // TODO(crbug.com/531787872): Forward to the platform backend. On Windows
  // this is an in-process call into HapticsManagerImplWin. Until then this is a
  // no-op.
}

}  // namespace content
