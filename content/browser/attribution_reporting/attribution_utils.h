// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_UTILS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_UTILS_H_

namespace base {
class Time;
}  // namespace base

namespace content {

class StorableSource;

// Calculates the report time for a conversion associated with a given
// impression.
base::Time ComputeReportTime(const StorableSource& impression,
                             base::Time trigger_time);
}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_UTILS_H_
