// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/content_security_notifier.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"

namespace content {

ContentSecurityNotifier::ContentSecurityNotifier(WeakDocumentPtr weak_document)
    : weak_document_(std::move(weak_document)) {}

void ContentSecurityNotifier::NotifyContentWithCertificateErrorsRan() {
  auto* render_frame_host = static_cast<RenderFrameHostImpl*>(
      weak_document_.AsRenderFrameHostIfValid());
  if (render_frame_host) {
    render_frame_host->OnDidRunContentWithCertificateErrors();
  }
}

void ContentSecurityNotifier::NotifyContentWithCertificateErrorsDisplayed() {
  auto* render_frame_host = static_cast<RenderFrameHostImpl*>(
      weak_document_.AsRenderFrameHostIfValid());
  if (render_frame_host) {
    render_frame_host->OnDidDisplayContentWithCertificateErrors();
  }
}

void ContentSecurityNotifier::NotifyInsecureContentRan(
    const GURL& insecure_url,
    blink::mojom::ContentSecurityNotifier::InsecureContentOrigin origin_type) {
  auto* render_frame_host = static_cast<RenderFrameHostImpl*>(
      weak_document_.AsRenderFrameHostIfValid());
  if (render_frame_host) {
    render_frame_host->OnDidRunInsecureContent(insecure_url, origin_type);
  }
}

}  // namespace content
