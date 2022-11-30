// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/download_request_utils.h"

#include <memory>

#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

namespace content {

// static
std::unique_ptr<download::DownloadUrlParameters>
DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
    WebContents* web_contents,
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  RenderFrameHost* render_frame_host = web_contents->GetPrimaryMainFrame();
  return std::make_unique<download::DownloadUrlParameters>(
      url, render_frame_host->GetProcess()->GetID(),
      render_frame_host->GetRoutingID(), traffic_annotation);
}

// static
bool DownloadRequestUtils::IsURLSafe(int render_process_id, const GURL& url) {
  // Check if the renderer is permitted to request the requested URL.
  if (!ChildProcessSecurityPolicy::GetInstance()->CanRequestURL(
          render_process_id, url)) {
    DVLOG(1) << "Denied unauthorized download request for "
             << url.possibly_invalid_spec();
    return false;
  }
  return true;
}

}  // namespace content
