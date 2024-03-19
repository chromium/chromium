// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_CONSTANTS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_CONSTANTS_H_

namespace content {

inline constexpr char kAttributionReportingRegisterSourceHeader[] =
    "Attribution-Reporting-Register-Source";

inline constexpr char kAttributionReportingRegisterTriggerHeader[] =
    "Attribution-Reporting-Register-Trigger";

inline constexpr char kAttributionReportingRegisterOsSourceHeader[] =
    "Attribution-Reporting-Register-OS-Source";

inline constexpr char kAttributionReportingRegisterOsTriggerHeader[] =
    "Attribution-Reporting-Register-OS-Trigger";

inline constexpr char kAttributionReportingInfoHeader[] =
    "Attribution-Reporting-Info";

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_CONSTANTS_H_
