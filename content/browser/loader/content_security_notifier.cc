// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/content_security_notifier.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

ContentSecurityNotifier::ContentSecurityNotifier(
    GlobalFrameRoutingId render_frame_host_id)
    : render_frame_host_id_(render_frame_host_id) {}

void ContentSecurityNotifier::NotifyContentWithCertificateErrorsRan() {
  auto* render_frame_host = RenderFrameHostImpl::FromID(render_frame_host_id_);
  if (!render_frame_host)
    return;
  auto* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(render_frame_host));
  if (!web_contents)
    return;
  web_contents->OnDidRunContentWithCertificateErrors(render_frame_host);
}

void ContentSecurityNotifier::NotifyContentWithCertificateErrorsDisplayed() {
  auto* render_frame_host = RenderFrameHostImpl::FromID(render_frame_host_id_);
  if (!render_frame_host)
    return;
  auto* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(render_frame_host));
  if (!web_contents)
    return;
  web_contents->OnDidDisplayContentWithCertificateErrors();
}

void ContentSecurityNotifier::NotifyInsecureContentRan(
    const GURL& origin,
    const GURL& insecure_url) {
  auto* render_frame_host = RenderFrameHostImpl::FromID(render_frame_host_id_);
  if (!render_frame_host)
    return;
  auto* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(render_frame_host));
  if (!web_contents)
    return;
  web_contents->OnDidRunInsecureContent(render_frame_host, origin,
                                        insecure_url);
}

}  // namespace content
