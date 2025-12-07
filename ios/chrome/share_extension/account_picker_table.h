// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_SHARE_EXTENSION_ACCOUNT_PICKER_TABLE_H_
#define IOS_CHROME_SHARE_EXTENSION_ACCOUNT_PICKER_TABLE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/share_extension/account_info.h"

@protocol AccountPickerDelegate;

// A view controller when the account are displayed and can be selected.
@interface AccountPickerTable : UIViewController

@property(nonatomic, strong) UISheetPresentationControllerDetent* customDetent;

// The delegate for interactions in `AccountPickerTable`.
@property(nonatomic, weak) id<AccountPickerDelegate> delegate;

- (instancetype)initWithAccounts:(NSArray<AccountInfo*>*)accounts
                 selectedAccount:(AccountInfo*)selectedAccount
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
@end

#endif  // IOS_CHROME_SHARE_EXTENSION_ACCOUNT_PICKER_TABLE_H_
