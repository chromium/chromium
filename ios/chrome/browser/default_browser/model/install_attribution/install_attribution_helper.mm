// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/install_attribution/install_attribution_helper.h"

#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"
#import "ios/chrome/browser/default_browser/model/install_attribution/gmo_sko_acceptance_data.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/app_group/app_group_constants.h"

namespace install_attribution {

namespace {

// The different time windows used for considering an install to be
// attributable to the acceptance of an external promo.
const base::TimeDelta kShortAttributionWindow = base::Days(1);
const base::TimeDelta kLongAttributionWindow = base::Days(15);

}  // namespace

void LogInstallAttribution() {
  if (!IsInstallAttributionLoggingEnabled()) {
    return;
  }

  NSUserDefaults* sharedDefaults = app_group::GetCommonGroupUserDefaults();
  if (!sharedDefaults) {
    return;
  }

  NSData* archivedData =
      [sharedDefaults dataForKey:app_group::kGMOSKOInstallAttribution];
  if (archivedData) {
    NSError* unarchiveError = nil;
    GMOSKOAcceptanceData* acceptanceData =
        [NSKeyedUnarchiver unarchivedObjectOfClass:[GMOSKOAcceptanceData class]
                                          fromData:archivedData
                                             error:&unarchiveError];

    if (acceptanceData != nil && acceptanceData.placementID != nil &&
        acceptanceData.timestamp != nil) {
      base::Time acceptanceTime =
          base::Time::FromNSDate(acceptanceData.timestamp);
      base::TimeDelta elapsedSinceAcceptance =
          base::Time::Now() - acceptanceTime;

      if (elapsedSinceAcceptance < kLongAttributionWindow) {
        base::UmaHistogramEnumeration(
            "IOS.GMOSKOInstallAttribution",
            elapsedSinceAcceptance < kShortAttributionWindow
                ? InstallAttributionType::Within24Hours
                : InstallAttributionType::Within15Days);
      }
    }
  }

  // Clear the acceptance data to prevent recording the install attribution
  // multiple times.
  [sharedDefaults removeObjectForKey:app_group::kGMOSKOInstallAttribution];
}

}  // namespace install_attribution
