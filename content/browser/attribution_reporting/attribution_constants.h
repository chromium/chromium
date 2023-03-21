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

constexpr base::TimeDelta kDefaultAttributionSourceExpiry = base::Days(30);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_CONSTANTS_H_
