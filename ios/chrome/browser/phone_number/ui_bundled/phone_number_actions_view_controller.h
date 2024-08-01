// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PHONE_NUMBER_UI_BUNDLED_PHONE_NUMBER_ACTIONS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PHONE_NUMBER_UI_BUNDLED_PHONE_NUMBER_ACTIONS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@protocol AddContactsCommands;

// This class presents a list of actions related to a phone number (call, send
// message, add to contacts and  facetime).
@interface PhoneNumberActionsViewController
    : SettingsRootTableViewController <SettingsControllerProtocol>

// The add contact handler to display the add contacts view controller.
@property(nonatomic, weak) id<AddContactsCommands> addContactsHandler;

// Creates a instance with a given phoneNumber passed by the
// CountryCodeViewController.
- (instancetype)initWithPhoneNumber:(NSString*)phoneNumber
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PHONE_NUMBER_UI_BUNDLED_PHONE_NUMBER_ACTIONS_VIEW_CONTROLLER_H_
