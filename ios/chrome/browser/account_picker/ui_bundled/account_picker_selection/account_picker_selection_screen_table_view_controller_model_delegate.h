// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SELECTION_ACCOUNT_PICKER_SELECTION_SCREEN_TABLE_VIEW_CONTROLLER_MODEL_DELEGATE_H_
#define IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SELECTION_ACCOUNT_PICKER_SELECTION_SCREEN_TABLE_VIEW_CONTROLLER_MODEL_DELEGATE_H_

#import <Foundation/Foundation.h>

@class AccountPickerSelectionScreenViewController;

// Protocol to get the model.
@protocol
    AccountPickerSelectionScreenTableViewControllerModelDelegate <NSObject>

// Returns all the configurators to generate model items.
@property(nonatomic, strong, readonly) NSArray* sortedIdentityItemConfigurators;

@end

#endif  // IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SELECTION_ACCOUNT_PICKER_SELECTION_SCREEN_TABLE_VIEW_CONTROLLER_MODEL_DELEGATE_H_
