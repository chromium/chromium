// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_POLICY_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_POLICY_H_

#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Time;
class TimeDelta;
}  // namespace base

namespace content {

// Returns the expiry time for an impression that is clamped to a maximum
// value of 30 days from |impression_time|.
CONTENT_EXPORT
base::Time GetExpiryTimeForImpression(
    const absl::optional<base::TimeDelta>& declared_expiry,
    base::Time impression_time,
    CommonSourceInfo::SourceType source_type);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_POLICY_H_
