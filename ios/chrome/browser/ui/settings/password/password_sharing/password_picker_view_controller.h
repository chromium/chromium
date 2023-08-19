// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_PICKER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_PICKER_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller.h"

// Screen that presents a list of password credential groups for passwords that
// have more than 1 affiliated group.
@interface PasswordPickerViewController : TableViewBottomSheetViewController

- (instancetype)init;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_PICKER_VIEW_CONTROLLER_H_
