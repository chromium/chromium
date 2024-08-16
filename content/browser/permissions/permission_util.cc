// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_util.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::PermissionDescriptorPtr;

namespace content {

#if BUILDFLAG(IS_ANDROID)
namespace {
constexpr const char* kIsFileURLHistogram =
    "Permissions.GetLastCommittedOriginAsURL.IsFileURL";
}
#endif

// Due to dependency issues, this method is duplicated from
// components/permissions/permission_util.cc.
GURL PermissionUtil::GetLastCommittedOriginAsURL(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
#if BUILDFLAG(IS_ANDROID)
  // If `allow_universal_access_from_file_urls` flag is enabled, a file:/// can
  // change its url via history.pushState/replaceState to any other url,
  // including about:blank. To avoid user confusion we should always use a
  // visible url, in other words `GetLastCommittedURL`.
  if (web_contents->GetOrCreateWebPreferences()
          .allow_universal_access_from_file_urls &&
      render_frame_host->GetLastCommittedOrigin().GetURL().SchemeIsFile()) {
    base::UmaHistogramBoolean(kIsFileURLHistogram, true);
    return render_frame_host->GetLastCommittedURL().DeprecatedGetOriginAsURL();
  } else {
    base::UmaHistogramBoolean(kIsFileURLHistogram, false);
  }
#endif

  if (render_frame_host->GetLastCommittedOrigin().GetURL().is_empty()) {
    if (!web_contents->GetVisibleURL().is_empty()) {
      return web_contents->GetVisibleURL();
    }
  }
  return render_frame_host->GetLastCommittedOrigin().GetURL();
}

bool PermissionUtil::IsDomainOverride(
    const PermissionDescriptorPtr& descriptor) {
  return descriptor->extension &&
         descriptor->extension->is_top_level_storage_access();
}

const url::Origin& PermissionUtil::ExtractDomainOverride(
    const PermissionDescriptorPtr& descriptor) {
  const blink::mojom::TopLevelStorageAccessPermissionDescriptorPtr&
      override_descriptor =
          descriptor->extension->get_top_level_storage_access();
  return override_descriptor->requestedOrigin;
}

bool PermissionUtil::ValidateDomainOverride(
    const std::vector<blink::PermissionType>& types,
    RenderFrameHost* rfh,
    const blink::mojom::PermissionDescriptorPtr& descriptor) {
  if (types.size() > 1) {
    // Requests with domain overrides must be requested individually.
    return false;
  }
  if (!rfh || !rfh->IsInPrimaryMainFrame()) {
    // Requests with domain overrides must be requested from a top-level
    // browsing context.
    return false;
  }

  const url::Origin& overridden_origin =
      PermissionUtil::ExtractDomainOverride(descriptor);

  if (rfh->GetLastCommittedOrigin().IsSameOriginWith(overridden_origin)) {
    // In case `overridden_origin` equals
    // `render_frame_host->GetLastCommittedOrigin()` then you should use
    // `GetPermissionStatusForCurrentDocument`.
    return false;
  }

  return true;
}

}  // namespace content
