// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_SHARING_STATUS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_SHARING_STATUS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

// Presents the following animation:
// * Recipient and sender images appear on the middle.
// * Both images slide out (sender to the left, recipient to the right).
// * Shield with lock appears on the middle.
// * Progress bar is loading between sender and the recipient.
// * Shield and progress bar disappear, profile images slide to the middle
// If the user clicks "cancel" button at any point, the animation stops and
// cancelled status is presented. Otherwise, success status is displayed when
// the animation finishes.
@interface SharingStatusViewController : ChromeTableViewController
@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_SHARING_STATUS_VIEW_CONTROLLER_H_
