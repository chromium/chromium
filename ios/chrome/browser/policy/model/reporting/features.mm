// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/features.h"

namespace enterprise_reporting {

// Enables Cloud Profile Reporting on iOS.
BASE_FEATURE(kCloudProfileReporting,
             "CloudProfileReporting",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace enterprise_reporting
