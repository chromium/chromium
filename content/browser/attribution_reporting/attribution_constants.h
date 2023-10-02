// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_CONSTANTS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_CONSTANTS_H_

#include "base/time/time.h"

namespace content {

constexpr char kAttributionReportingRegisterSourceHeader[] =
    "Attribution-Reporting-Register-Source";

constexpr char kAttributionReportingRegisterOsSourceHeader[] =
    "Attribution-Reporting-Register-OS-Source";

// TODO(crbug.com/1479944): Relocate these constants to
// //components/attribution_reporting/event_report_windows.cc.
constexpr base::TimeDelta kDefaultNavigationReportWindow1 = base::Days(2);
constexpr base::TimeDelta kDefaultNavigationReportWindow2 = base::Days(7);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_CONSTANTS_H_
