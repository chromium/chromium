// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_util.h"

#include "components/permissions/features.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "url/gurl.h"

namespace content {

// Returns the last committed URL for `web_contents`. If the frame's URL is
// about:blank, returns GetLastCommittedOrigin.
// Due to dependency issues, this method is duplicated in
// content/browser/permissions/permission_util.cc.
// TODO(crbug.com/698985): Resolve GetLastCommitted[URL|Origin]() usage.
GURL PermissionUtil::GetLastCommittedOriginAsURL(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  return GetLastCommittedOriginAsURL(web_contents->GetMainFrame());
}

GURL PermissionUtil::GetLastCommittedOriginAsURL(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);

  if (base::FeatureList::IsEnabled(
          permissions::features::kRevisedOriginHandling)) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(render_frame_host);
    // If `allow_universal_access_from_file_urls` flag is enabled, a file can
    // introduce discrepancy between GetLastCommittedURL and
    // GetLastCommittedOrigin. In that case GetLastCommittedURL should be used
    // for requesting and verifying permissions.
    // Disabling `kRevisedOriginHandling` feature introduces no side effects,
    // because in both cases we rely on GetLastCommittedURL().GetOrigin().
    if (web_contents->GetOrCreateWebPreferences()
            .allow_universal_access_from_file_urls &&
        render_frame_host->GetLastCommittedOrigin().GetURL().SchemeIsFile()) {
      return render_frame_host->GetLastCommittedURL().GetOrigin();
    }

    return render_frame_host->GetLastCommittedOrigin().GetURL();
  }

  return render_frame_host->GetLastCommittedURL().GetOrigin();
}

}  // namespace content
