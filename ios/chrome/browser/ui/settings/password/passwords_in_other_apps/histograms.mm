// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/histograms.h"

#import <string>
#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/ui/settings/utils/password_auto_fill_status_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Histogram names. Make sure to add an entry in histograms.xml when creating
// a new one that will get used.
char const kPasswordsInOtherAppsOpenHistogram[] =
    "IOS.PasswordsInOtherApps.Open";
char const kPasswordsInOtherAppsGoToIOSSettingHistogram[] =
    "IOS.PasswordsInOtherApps.GoToIOSSetting";
char const kPasswordsInOtherAppsAutoFillStatusChangeHistogram[] =
    "IOS.PasswordsInOtherApps.AutoFillStatusChange";
char const kPasswordsInOtherAppsDismissHistogram[] =
    "IOS.PasswordsInOtherApps.Dismiss";

// Enum specifying whether the user is current enrolled in password auto fill
// from Chrome. Must stay in sync with `PasswordAutoFillEnrollmentStatus` from
// enums.xml.
enum class PasswordAutoFillEnrollmentStatus {
  Unknown = 0,
  NotEnrolled = 1,
  Enrolled = 2,
  kMaxValue = Enrolled,
};

}  // namespace

void RecordEventOnUMA(PasswordsInOtherAppsActionType action) {
  PasswordAutoFillStatusManager* manager =
      [PasswordAutoFillStatusManager sharedManager];
  PasswordAutoFillEnrollmentStatus status;
  if (manager.ready) {
    status = manager.autoFillEnabled
                 ? PasswordAutoFillEnrollmentStatus::Enrolled
                 : PasswordAutoFillEnrollmentStatus::NotEnrolled;
  } else {
    status = PasswordAutoFillEnrollmentStatus::Unknown;
  }
  switch (action) {
    case PasswordsInOtherAppsActionOpen:
      base::UmaHistogramEnumeration(kPasswordsInOtherAppsOpenHistogram, status);
      break;
    case PasswordsInOtherAppsActionGoToIOSSetting:
      base::UmaHistogramEnumeration(
          kPasswordsInOtherAppsGoToIOSSettingHistogram, status);
      break;
    case PasswordsInOtherAppsActionAutoFillStatusChange:
      base::UmaHistogramEnumeration(
          kPasswordsInOtherAppsAutoFillStatusChangeHistogram, status);
      break;
    case PasswordsInOtherAppsActionDismiss:
      base::UmaHistogramEnumeration(kPasswordsInOtherAppsDismissHistogram,
                                    status);
      break;
  }
}
