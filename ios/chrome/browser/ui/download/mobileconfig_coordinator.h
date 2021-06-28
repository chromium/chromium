// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DOWNLOAD_MOBILECONFIG_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_DOWNLOAD_MOBILECONFIG_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

// Key of the UMA Download.IOSDownloadMobileConfigFileUI histogram.
extern const char kUmaDownloadMobileConfigFileUI[];

// Values of the UMA Download.IOSDownloadMobileConfigFileUI histogram. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class DownloadMobileConfigFileUI {
  // The Warning alert was presented.
  KWarningAlertIsPresented = 0,
  // The user chose to abort the download process.
  KWarningAlertIsDismissed = 1,
  // The user chose to continue the download process.
  kSFSafariViewIsPresented = 2,
  kMaxValue = kSFSafariViewIsPresented
};

// Presents SFSafariViewController in order to download .mobileconfig file.
@interface MobileConfigCoordinator : ChromeCoordinator

@end

#endif  // IOS_CHROME_BROWSER_UI_DOWNLOAD_MOBILECONFIG_COORDINATOR_H_
