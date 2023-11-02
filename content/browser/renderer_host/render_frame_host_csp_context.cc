// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_frame_host_csp_context.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace content {

RenderFrameHostCSPContext::RenderFrameHostCSPContext(
    RenderFrameHostImpl* render_frame_host)
    : render_frame_host_(render_frame_host) {}

void RenderFrameHostCSPContext::ReportContentSecurityPolicyViolation(
    network::mojom::CSPViolationPtr violation_params) {
  if (!render_frame_host_)
    return;
  render_frame_host_->GetAssociatedLocalFrame()
      ->ReportContentSecurityPolicyViolation(std::move(violation_params));
}

void RenderFrameHostCSPContext::SanitizeDataForUseInCspViolation(
    network::mojom::CSPDirectiveName directive,
    GURL* blocked_url,
    network::mojom::SourceLocation* source_location) const {
  DCHECK(blocked_url);
  DCHECK(source_location);
  GURL source_location_url(source_location->url);

  // The main goal of this is to avoid leaking information between potentially
  // separate renderers, in the event of one of them being compromised.
  // See https://crbug.com/633306.
  //
  // We need to sanitize the `blocked_url` only for frame-src and
  // fenced-frame-src. All other directive checks pass as `blocked_url` the
  // initial URL (before redirects), which the renderer already knows. check in
  // the browser is reporting to the wrong frame.
  bool sanitize_blocked_url =
      directive == network::mojom::CSPDirectiveName::FrameSrc ||
      directive == network::mojom::CSPDirectiveName::FencedFrameSrc;
  bool sanitize_source_location = true;

  // There is no need to sanitize data when it is same-origin with the current
  // url of the renderer.
  if (render_frame_host_) {
    if (render_frame_host_->GetLastCommittedOrigin().IsSameOriginWith(
            *blocked_url)) {
      sanitize_blocked_url = false;
    }
    if (render_frame_host_->GetLastCommittedOrigin().IsSameOriginWith(
            source_location_url)) {
      sanitize_source_location = false;
    }
  }

  if (sanitize_blocked_url)
    *blocked_url = blocked_url->DeprecatedGetOriginAsURL();
  if (sanitize_source_location) {
    source_location->url =
        source_location_url.DeprecatedGetOriginAsURL().spec();
    source_location->line = 0u;
    source_location->column = 0u;
  }
}

}  // namespace content
