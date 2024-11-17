// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_INCOGNITO_INCOGNITO_LOCK_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_INCOGNITO_INCOGNITO_LOCK_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_consumer.h"
#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@protocol IncognitoLockViewControllerPresentationDelegate;
@protocol IncognitoLockMutator;
@protocol ReauthenticationProtocol;

// View controller to manage the Incognito Lock setting.
@interface IncognitoLockViewController
    : SettingsRootTableViewController <IncognitoLockConsumer,
                                       SettingsControllerProtocol>

// Presentation delegate.
@property(nonatomic, weak) id<IncognitoLockViewControllerPresentationDelegate>
    presentationDelegate;

// Mutator to apply all user changes on the view.
@property(nonatomic, weak) id<IncognitoLockMutator> mutator;

// Designated initializer. All the parameters should not be null.
// `reauthModule`: provides access to currently enabled iOS authentication
// capabilities.
- (instancetype)initWithReauthModule:(id<ReauthenticationProtocol>)reauthModule
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_INCOGNITO_INCOGNITO_LOCK_VIEW_CONTROLLER_H_
