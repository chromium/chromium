// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CONNECTION_ALLOWLIST_UTIL_H_
#define CONTENT_PUBLIC_BROWSER_CONNECTION_ALLOWLIST_UTIL_H_

#include "content/common/content_export.h"

class GURL;

namespace content {

class RenderFrameHost;

// Returns true if the network request is allowed by the frame's connection
// allowlist.
CONTENT_EXPORT bool FrameConnectionAllowlistAllowsRequestAndReportIfNeeded(
    RenderFrameHost* render_frame_host,
    const GURL& url,
    bool is_redirect);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CONNECTION_ALLOWLIST_UTIL_H_
