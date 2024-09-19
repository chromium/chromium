// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/content_security_notifier.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"

namespace content {

ContentSecurityNotifier::ContentSecurityNotifier(
    GlobalRenderFrameHostId render_frame_host_id)
    : render_frame_host_id_(render_frame_host_id) {}

void ContentSecurityNotifier::NotifyContentWithCertificateErrorsRan() {
  auto* render_frame_host = RenderFrameHostImpl::FromID(render_frame_host_id_);
  if (render_frame_host) {
    render_frame_host->OnDidRunContentWithCertificateErrors();
  }
}

void ContentSecurityNotifier::NotifyContentWithCertificateErrorsDisplayed() {
  auto* render_frame_host = RenderFrameHostImpl::FromID(render_frame_host_id_);
  if (render_frame_host) {
    render_frame_host->OnDidDisplayContentWithCertificateErrors();
  }
}

void ContentSecurityNotifier::NotifyInsecureContentRan(
    const GURL& origin,
    const GURL& insecure_url) {
  auto* render_frame_host = RenderFrameHostImpl::FromID(render_frame_host_id_);
  if (render_frame_host) {
    render_frame_host->OnDidRunInsecureContent(origin, insecure_url);
  }
}

}  // namespace content
