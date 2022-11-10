// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_CONTROLLER_EGTEST_H_
#define IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_CONTROLLER_EGTEST_H_

#import "ios/chrome/test/earl_grey/chrome_test_case.h"

// Name of the downloaded PNG.
extern NSString* kPNGFilename;

// Tests Open in Feature.
// TODO(crbug.com/1338585): Remove the ZZZ prefix once the bug is fixed, where
// the parent class needs to be called last.
@interface ZZZOpenInManagerTestCase : ChromeTestCase {
  // The variant of the feature to use. This is consumed in
  // -appConfigurationForTestCase, as part of -setUp. Subclasses should set this
  // before calling the parent class -setUp.
  std::string _variant;
}

// Open activity menu, depending on variant. Default behavior opens the menu by
// tapping the "Open In" button.
// TODO(crbug.com/1357553): Remove when Open In download experiment is
// finished.
- (void)openActivityMenu;

// Offline assert depending on variant. Default behavior is waiting for the
// error dialog.
- (void)offlineAssertBehavior;

// YES if should skip test on iPad, depending on variant. Default value is YES.
// TODO(crbug.com/1357553): Remove when Open In download experiment is
// finished.
- (BOOL)shouldSkipIpad;

// YES if should run all tests, depending on variant. Default value is YES.
// TODO(crbug.com/1357553): Remove when Open In download experiment is
// finished and iOS 14 is not supported anymore.
- (BOOL)shouldRunTests;

// Asserts the activity service is visible.
- (void)assertActivityServiceVisible;

// Closes the activity menu.
- (void)closeActivityMenu;

// Asserts the activity service is dismissed.
- (void)assertActivityMenuDismissed;

@end

#endif  // IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_CONTROLLER_EGTEST_H_
