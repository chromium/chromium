// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OS_LEVEL_MANAGER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OS_LEVEL_MANAGER_H_

#include <set>
#include <string>

#include "base/functional/callback_forward.h"
#include "content/public/browser/browsing_data_filter_builder.h"

namespace base {
class Time;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

namespace content {

struct OsRegistration;

// Interface between the browser's Attribution Reporting implementation and the
// operating system's.
class AttributionOsLevelManager {
 public:
  virtual ~AttributionOsLevelManager() = default;

  virtual void Register(const OsRegistration&,
                        bool is_debug_key_allowed,
                        base::OnceCallback<void(bool success)>) = 0;

  // Clears storage data with the OS.
  // Note that `done` is not run if `AttributionOsLevelManager` is destroyed
  // first.
  virtual void ClearData(base::Time delete_begin,
                         base::Time delete_end,
                         const std::set<url::Origin>& origins,
                         const std::set<std::string>& domains,
                         BrowsingDataFilterBuilder::Mode mode,
                         bool delete_rate_limit_data,
                         base::OnceClosure done) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OS_LEVEL_MANAGER_H_
