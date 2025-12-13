// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/features.h"

namespace enterprise_reporting {

// Kill-switch for stripping the "/var/mobile/.../Library/Application Support/"
// prefix from profile paths in reports.
BASE_FEATURE(kSanitizeProfilePaths, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Cloud Profile Reporting on iOS.
BASE_FEATURE(kCloudProfileReporting, base::FEATURE_DISABLED_BY_DEFAULT);

// Reports all known profiles, not just loaded profiles, in the browser report.
BASE_FEATURE(kBrowserReportIncludeAllProfiles,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace enterprise_reporting
