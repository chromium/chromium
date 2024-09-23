// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/pressure_service_for_frame.h"

#include "content/browser/compute_pressure/web_contents_pressure_manager_proxy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace content {

PressureServiceForFrame::PressureServiceForFrame(
    RenderFrameHost* render_frame_host)
    : DocumentUserData<PressureServiceForFrame>(render_frame_host) {
  CHECK(render_frame_host);
}

PressureServiceForFrame::~PressureServiceForFrame() = default;

bool PressureServiceForFrame::CanCallAddClient() const {
  return render_frame_host().IsActive() &&
         !render_frame_host().IsNestedWithinFencedFrame();
}

bool PressureServiceForFrame::ShouldDeliverUpdate() const {
  return HasImplicitFocus(&render_frame_host());
}

std::optional<base::UnguessableToken> PressureServiceForFrame::GetTokenFor(
    device::mojom::PressureSource source) const {
  const auto* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  if (const auto* pressure_manager_proxy =
          WebContentsPressureManagerProxy::FromWebContents(web_contents)) {
    return pressure_manager_proxy->GetTokenFor(source);
  }
  return std::nullopt;
}

DOCUMENT_USER_DATA_KEY_IMPL(PressureServiceForFrame);

}  // namespace content
