// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_SAFARI_DOWNLOAD_COORDINATOR_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_SAFARI_DOWNLOAD_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Key of the UMA Download.IOSDownloadCalendarFileUI histogram.
extern const char kUmaDownloadCalendarFileUI[];
// Key of the UMA Download.IOSDownloadMobileConfigFileUI histogram.
extern const char kUmaDownloadMobileConfigFileUI[];

// Values of the UMA Download.IOSDownloadMobileConfigFileUI and
// Download.IOSDownloadCalendarFileUI histograms. These values are persisted to
// logs. Entries should not be renumbered and numeric values should never be
// reused.
enum class SafariDownloadFileUI {
  // The Warning alert was presented.
  kWarningAlertIsPresented = 0,
  // The user chose to abort the download process.
  kWarningAlertIsDismissed = 1,
  // The user chose to continue the download process.
  kSFSafariViewIsPresented = 2,
  kMaxValue = kSFSafariViewIsPresented
};

// Presents SFSafariViewController in order to download files that Chrome can't
// natively open.
@interface SafariDownloadCoordinator : ChromeCoordinator

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_SAFARI_DOWNLOAD_COORDINATOR_H_
