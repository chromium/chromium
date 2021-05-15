// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_POLICY_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_POLICY_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace url {
class Origin;
}

namespace content {

class AppCachePolicy {
 public:
  AppCachePolicy() = default;

  // Called prior to loading a main resource from the appache.
  // Returns true if allowed. This is expected to return immediately
  // without any user prompt.
  virtual bool CanLoadAppCache(
      const GURL& manifest_url,

      const GURL& site_for_cookies,
      const absl::optional<url::Origin>& top_frame_origin) = 0;

  // Called prior to creating a new appcache. Returns true if allowed.
  virtual bool CanCreateAppCache(
      const GURL& manifest_url,

      const GURL& site_for_cookies,
      const absl::optional<url::Origin>& top_frame_origin) = 0;

  // Returns true if origin trial tokens are required in order to fetch or
  // update manifests, as well as load any resources from such a manifest.
  virtual bool IsOriginTrialRequiredForAppCache() = 0;

 protected:
  ~AppCachePolicy() = default;
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_POLICY_H_
