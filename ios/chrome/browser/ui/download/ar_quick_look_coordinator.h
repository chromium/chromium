// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DOWNLOAD_AR_QUICK_LOOK_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_DOWNLOAD_AR_QUICK_LOOK_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

// The UMA IOSPresentQLPreviewController histogram name.
extern const char kIOSPresentQLPreviewControllerHistogram[];

// Enum for the Download.IOSPresentQLPreviewControllerResult UMA histogram
// Note: This enum should be appended to only.
enum class PresentQLPreviewController {
  // The dialog was sucessesfully presented.
  kSuccessful = 0,
  // The dialog cannot be presented, because the given USDZ file in invalid.
  kInvalidFile = 1,
  // The dialog cannot be presented, because another ARPreviewController is
  // already presented.
  kAnotherQLPreviewControllerIsPresented = 2,
  // The dialog cannot be presented, because another view controller is already
  // presented. Does not include items already counted in the more specific
  // bucket (kAnotherQLPreviewControllerIsPresented).
  kAnotherViewControllerIsPresented = 3,
  kMaxValue = kAnotherViewControllerIsPresented,
};

// Presents QLPreviewController in order to display USDZ format 3D models.
@interface ARQuickLookCoordinator : ChromeCoordinator

@end

#endif  // IOS_CHROME_BROWSER_UI_DOWNLOAD_AR_QUICK_LOOK_COORDINATOR_H_
