// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/safety_check/safety_check_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kSafeBrowsingSafetyCheckStringURL =
    @"chrome://settings/syncSetup";

NSString* const kTimestampOfLastIssueFoundKey =
    @"TimestampOfLastSafetyCheckIssueFound";

const char kSafetyCheckMetricsUpdates[] = "Settings.SafetyCheck.UpdatesResult";

const char kSafetyCheckMetricsPasswords[] =
    "Settings.SafetyCheck.PasswordsResult2";

const char kSafetyCheckMetricsSafeBrowsing[] =
    "Settings.SafetyCheck.SafeBrowsingResult";

const char kSafetyCheckInteractions[] = "Settings.SafetyCheck.Interactions";
