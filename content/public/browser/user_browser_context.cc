// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/user_browser_context.h"

#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"

namespace content {

BrowserContext* GetUserBrowserContext(RenderFrameHost& render_frame_host) {
  return GetUserBrowserContext(
      *WebContents::FromRenderFrameHost(&render_frame_host));
}

BrowserContext* GetUserBrowserContext(WebContents& web_contents) {
  return GetContentClient()->browser()->GetUserBrowserContext(web_contents);
}

}  // namespace content
