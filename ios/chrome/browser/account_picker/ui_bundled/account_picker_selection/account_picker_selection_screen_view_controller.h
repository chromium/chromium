// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SELECTION_ACCOUNT_PICKER_SELECTION_SCREEN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SELECTION_ACCOUNT_PICKER_SELECTION_SCREEN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_screen/account_picker_screen_view_controller.h"

@protocol AccountPickerSelectionScreenTableViewControllerActionDelegate;
@protocol AccountPickerSelectionScreenTableViewControllerModelDelegate;
@protocol AccountPickerSelectionScreenConsumer;
@protocol AccountPickerLayoutDelegate;

// View controller for AccountPickerSelectionScreenCoordinator.
@interface AccountPickerSelectionScreenViewController
    : UIViewController <AccountPickerScreenViewController>

@property(nonatomic, weak)
    id<AccountPickerSelectionScreenTableViewControllerActionDelegate>
        actionDelegate;
@property(nonatomic, weak) id<AccountPickerLayoutDelegate> layoutDelegate;
@property(nonatomic, weak)
    id<AccountPickerSelectionScreenTableViewControllerModelDelegate>
        modelDelegate;
@property(nonatomic, strong, readonly) id<AccountPickerSelectionScreenConsumer>
    consumer;

@end

#endif  // IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SELECTION_ACCOUNT_PICKER_SELECTION_SCREEN_VIEW_CONTROLLER_H_
