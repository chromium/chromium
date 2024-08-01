// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PHONE_NUMBER_UI_BUNDLED_COUNTRY_CODE_PICKER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PHONE_NUMBER_UI_BUNDLED_COUNTRY_CODE_PICKER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

@protocol AddContactsCommands;

// This class presents a list of countries/country codes item in a table view.
@interface CountryCodePickerViewController : LegacyChromeTableViewController

// The add contact handler to pass to the `PhoneNumberActionsViewController`.
@property(nonatomic, weak) id<AddContactsCommands> addContactsHandler;

// Creates a instance with a given phoneNumber passed by the detector.
- (instancetype)initWithPhoneNumber:(NSString*)phoneNumber
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PHONE_NUMBER_UI_BUNDLED_COUNTRY_CODE_PICKER_VIEW_CONTROLLER_H_
