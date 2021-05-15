// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_MOCK_APPCACHE_POLICY_H_
#define CONTENT_BROWSER_APPCACHE_MOCK_APPCACHE_POLICY_H_

#include "base/compiler_specific.h"
#include "content/browser/appcache/appcache_policy.h"
#include "url/gurl.h"

namespace content {

class MockAppCachePolicy : public AppCachePolicy {
 public:
  MockAppCachePolicy();
  virtual ~MockAppCachePolicy();

  bool CanLoadAppCache(
      const GURL& manifest_url,
      const GURL& site_for_cookies,
      const absl::optional<url::Origin>& top_frame_origin) override;
  bool CanCreateAppCache(
      const GURL& manifest_url,
      const GURL& site_for_cookies,
      const absl::optional<url::Origin>& top_frame_origin) override;
  bool IsOriginTrialRequiredForAppCache() override;

  bool can_load_return_value_;
  bool can_create_return_value_;
  GURL requested_manifest_url_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_MOCK_APPCACHE_POLICY_H_
