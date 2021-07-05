// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_util.h"

#include "components/permissions/features.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace content {

// Returns the last committed URL for `web_contents`. If the frame's URL is
// about:blank, returns GetLastCommittedOrigin.
// Due to dependency issues, this method is duplicated in
// content/shell/browser/shell_permission_manager.cc and
// content/browser/permissions/permission_util.cc.
// TODO(crbug.com/698985): Resolve GetLastCommitted[URL|Origin]() usage.
GURL PermissionUtil::GetLastCommittedOriginAsURL(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (base::FeatureList::IsEnabled(
          permissions::features::kRevisedOriginHandling)) {
    if (web_contents->GetLastCommittedURL().IsAboutBlank()) {
      return web_contents->GetMainFrame()->GetLastCommittedOrigin().GetURL();
    }
  }

  return web_contents->GetLastCommittedURL().GetOrigin();
}

}  // namespace content
