// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_PASSWORD_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_PASSWORD_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/autofill/manual_fill/password_consumer.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

namespace manual_fill {

extern NSString* const PasswordSearchBarAccessibilityIdentifier;
extern NSString* const PasswordTableViewAccessibilityIdentifier;
extern NSString* const PasswordDoneButtonAccessibilityIdentifier;

}  // namespace manual_fill

// This class presents a list of usernames and passwords in a table view.
@interface PasswordViewController
    : ChromeTableViewController<ManualFillPasswordConsumer>

- (instancetype)initWithSearchController:(UISearchController*)searchController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithTableViewStyle:(UITableViewStyle)style
                           appBarStyle:
                               (ChromeTableViewControllerStyle)appBarStyle
    NS_UNAVAILABLE;

// If set to YES, the controller will add negative content insets inverse to the
// ones added by UITableViewController to accommodate for the keyboard.
@property(nonatomic, assign) BOOL contentInsetsAlwaysEqualToSafeArea;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_PASSWORD_VIEW_CONTROLLER_H_
