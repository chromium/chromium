// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_FIREBASE_UTILS_H_
#define IOS_CHROME_APP_FIREBASE_UTILS_H_

// Name of histogram to log whether Firebase SDK is initialized.
extern const char kFirebaseConfiguredHistogramName[];

// Number of days for Ad Conversion Attribution Window. In order to consider
// a first_open event to be associated with a marketing event (e.g. ad_click),
// the first_open event must have happened within this conversion attribution
// window.
extern const int kConversionAttributionWindowInDays;

// Firebase SDK may not be initialized, or initialized during First Run or not
// during First Run.
enum class FirebaseConfiguredState {
  // Firebase is not initialized because application is not built with the
  // SDK.
  kDisabled = 0,
  // Firebase is initialized at the app's first session (First Run).
  kEnabledFirstRun,
  // Firebase is initialized at the app's second or subsequent session.
  kEnabledNotFirstRun,
  // Firebase Analytics is for installation reporting only. Once a user has
  // passed the conversion attribution window, there is nothing to report.
  kDisabledConversionWindow,
  // Users who installed Chrome prior to incorporating Firebase into Chrome
  // should not initialize Firebase because these users could not have been
  // the result of any promption campaigns that use Firebase Analytics.
  kDisabledLegacyInstallation,
  // Count of enum values. Must be equal to the last value above.
  kMaxValue = kDisabledLegacyInstallation,
};

// Initializes Firebase SDK if configured and necessary.
void InitializeFirebase(bool is_first_run);

#endif  // IOS_CHROME_APP_FIREBASE_UTILS_H_
