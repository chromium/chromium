// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_util.h"

#include "base/check.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// Returns the last committed URL for `render_frame_host`. If the frame's URL is
// about:blank, returns GetLastCommittedOrigin.
// Due to dependency issues, this method is duplicated from
// components/permissions/permission_util.cc.
GURL PermissionUtil::GetLastCommittedOriginAsURL(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  // If `allow_universal_access_from_file_urls` flag is enabled, a file can
  // introduce discrepancy between GetLastCommittedURL and
  // GetLastCommittedOrigin. In that case GetLastCommittedURL should be used
  // for requesting and verifying permissions.
  if (web_contents->GetOrCreateWebPreferences()
          .allow_universal_access_from_file_urls &&
      render_frame_host->GetLastCommittedOrigin().GetURL().SchemeIsFile()) {
    return render_frame_host->GetLastCommittedURL().DeprecatedGetOriginAsURL();
  }

  return render_frame_host->GetLastCommittedOrigin().GetURL();
}

}  // namespace content
