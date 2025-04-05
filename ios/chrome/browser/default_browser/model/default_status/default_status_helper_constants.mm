// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/default_status/default_status_helper_constants.h"

// The cohorts are monthly, aligned with the Gregorian calendar. The logic in
// this file should work for any cohort count between 1 and 12. Anything higher
// than that means the cohorts are no longer aligned to calendar months, so
// logic needs to be revisited.
//
// Changing this value requires updating the
// IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort{Number} histogram to make sure
// there is a variant for each cohort.
//
// LINT.IfChange(kCohortCount)
const int kCohortCount = 4;
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/histograms.xml:CohortNumber)
