// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_FEATURES_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_FEATURES_H_

#import "base/feature_list.h"

namespace enterprise_reporting {

// Kill-switch for stripping the "/var/mobile/.../Library/Application Support/"
// prefix from profile paths in reports.
//
// TODO(crbug.com/385175028): Clean up after January 2026.
BASE_DECLARE_FEATURE(kSanitizeProfilePaths);

// Enables Cloud Profile Reporting on iOS.
BASE_DECLARE_FEATURE(kCloudProfileReporting);

// Reports all known profiles, not just loaded profiles, in the browser report.
BASE_DECLARE_FEATURE(kBrowserReportIncludeAllProfiles);

}  // namespace enterprise_reporting

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_FEATURES_H_
