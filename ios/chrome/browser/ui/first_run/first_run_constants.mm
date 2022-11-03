// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace first_run {

NSString* const kFirstRunWelcomeScreenAccessibilityIdentifier =
    @"firstRunWelcomeScreenAccessibilityIdentifier";

NSString* const kFirstRunSignInScreenAccessibilityIdentifier =
    @"firstRunSignInScreenAccessibilityIdentifier";

NSString* const kFirstRunLegacySignInScreenAccessibilityIdentifier =
    @"firstRunLegacySignInScreenAccessibilityIdentifier";

NSString* const kFirstRunSyncScreenAccessibilityIdentifier =
    @"firstRunSyncScreenAccessibilityIdentifier";

NSString* const kFirstRunDefaultBrowserScreenAccessibilityIdentifier =
    @"firstRunDefaultBrowserScreenAccessibilityIdentifier";

// URL for the terms of service text.
NSString* const kTermsOfServiceURL = @"internal://terms-of-service";

// URL for the metric reporting text.
NSString* const kMetricReportingURL = @"internal://metric-reporting";

NSString* const kLaunchScreenAccessibilityIdentifier =
    @"launchScreenAccessibilityIdentifier";

NSString* const kEnterpriseLoadingScreenAccessibilityIdentifier =
    @"enterpriseLoadingScreenAccessibilityIdentifier";

}  // first_run
