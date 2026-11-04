// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_USER_BROWSER_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_USER_BROWSER_CONTEXT_H_

#include "content/common/content_export.h"

namespace content {

class BrowserContext;
class RenderFrameHost;

// Returns the BrowserContext of the user that `render_frame_host` is shown
// for. Its policies, permissions and off-the-record state apply to user data
// in the frame. The embedder decides it in
// ContentBrowserClient::GetUserBrowserContext(), and this function lets code
// outside content, e.g. components, use the same decision.
CONTENT_EXPORT BrowserContext* GetUserBrowserContext(
    RenderFrameHost& render_frame_host);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_USER_BROWSER_CONTEXT_H_
