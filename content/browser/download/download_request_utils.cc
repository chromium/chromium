// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/download_request_utils.h"

#include "content/public/browser/browser_context.h"
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
  RenderFrameHost* render_frame_host = web_contents->GetMainFrame();
  return std::unique_ptr<download::DownloadUrlParameters>(
      new download::DownloadUrlParameters(
          url, render_frame_host->GetProcess()->GetID(),
          render_frame_host->GetRenderViewHost()->GetRoutingID(),
          render_frame_host->GetRoutingID(), traffic_annotation));
}

}  // namespace content
