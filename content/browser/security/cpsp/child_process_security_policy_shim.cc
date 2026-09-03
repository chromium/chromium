// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/security/cpsp/child_process_security_policy_shim.h"

#include "content/browser/isolated_origin_util.h"
#include "content/browser/site_info.h"

namespace content::rust::child_process_security_policy {

void PushOriginToVector(const url::Origin& origin,
                        std::vector<url::Origin>& vec) noexcept {
  vec.push_back(origin);
}

std::unique_ptr<GURL> GetSiteForOrigin(const url::Origin& origin) {
  return std::make_unique<GURL>(SiteInfo::GetSiteForOrigin(origin));
}

std::unique_ptr<GURL> RemoveTrailingDotFromUrlIfNecessary(const GURL& url) {
  std::optional<GURL> trimmed_url =
      IsolatedOriginUtil::RemoveTrailingDotFromUrlIfNecessary(url);
  return trimmed_url ? std::make_unique<GURL>(*trimmed_url) : nullptr;
}

std::unique_ptr<url::Origin> CreateOriginWithDefaultPortIfNecessary(
    const url::Origin& origin) {
  return std::make_unique<url::Origin>(
      IsolatedOriginUtil::CreateOriginWithDefaultPortIfNecessary(origin));
}

}  // namespace content::rust::child_process_security_policy
