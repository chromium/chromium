// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORTER_ANDROID_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORTER_ANDROID_H_

#include <cstdint>
#include <string>

#include "content/common/content_export.h"

namespace base {
class Time;
}  // namespace base

namespace content {

class AttributionManager;

namespace attribution_reporter_android {

// Exposed separately from the JNI functions to allow for easier testing.
CONTENT_EXPORT void ReportAppImpression(AttributionManager& attribution_manager,
                                        const std::string& source_package_name,
                                        const std::string& source_event_id,
                                        const std::string& destination,
                                        const std::string& report_to,
                                        int64_t expiry,
                                        base::Time reportTime);

}  // namespace attribution_reporter_android

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORTER_ANDROID_H_
