// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OS_LEVEL_MANAGER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OS_LEVEL_MANAGER_H_

class GURL;

namespace url {
class Origin;
}  // namespace url

namespace content {

// Interface between the browser's Attribution Reporting implementation and the
// operating system's.
class AttributionOsLevelManager {
 public:
  virtual ~AttributionOsLevelManager() = default;

  virtual void RegisterAttributionSource(const GURL& registration_url,
                                         const url::Origin& top_level_origin,
                                         bool is_debug_key_allowed) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OS_LEVEL_MANAGER_H_
