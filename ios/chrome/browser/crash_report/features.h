// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_FEATURES_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_FEATURES_H_

#include "base/feature_list.h"

extern const base::Feature kCrashpadIOS;

extern const base::Feature kMetrickitCrashReport;

extern const base::Feature kMetrickitNonCrashReport;

extern const base::Feature kSyntheticCrashReportsForUte;

// Returns true if kSyntheticCrashReportsForUte and kLogBreadcrumbs features are
// both enabled. There is not much value in uploading Synthetic Crash Reports
// without Breadcrumbs.
bool EnableSyntheticCrashReportsForUte();

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_FEATURES_H_
