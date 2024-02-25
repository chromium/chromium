// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_MANAGER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_MANAGER_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/browser/ui/settings/password/passwords_consumer.h"
#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

class ChromeAccountManagerService;
@protocol PasswordsSettingsCommands;
@protocol PasswordManagerViewControllerDelegate;
@protocol PasswordManagerViewControllerPresentationDelegate;
class PrefService;

namespace password_manager {
struct CredentialUIEntry;
}  // namespace password_manager

@interface PasswordManagerViewController
    : SettingsRootTableViewController <PasswordsConsumer,
                                       SettingsControllerProtocol>

// The designated initializer.
- (instancetype)initWithChromeAccountManagerService:
                    (ChromeAccountManagerService*)accountManagerService
                                        prefService:(PrefService*)prefService
                             shouldOpenInSearchMode:(BOOL)shouldOpenInSearchMode
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Stores the most recently updated credential.
- (void)setMostRecentlyUpdatedPasswordDetails:
    (const password_manager::CredentialUIEntry&)credential;

@property(nonatomic, weak) id<PasswordsSettingsCommands> handler;

// Delegate.
@property(nonatomic, weak) id<PasswordManagerViewControllerDelegate> delegate;

@property(nonatomic, weak) id<PasswordManagerViewControllerPresentationDelegate>
    presentationDelegate;

// Reauthentication module.
@property(nonatomic, strong) id<ReauthenticationProtocol>
    reauthenticationModule;

// Data source for favicon images.
@property(nonatomic, weak) id<TableViewFaviconDataSource> imageDataSource;

// Method to delete items at index paths used for testing.
- (void)deleteItemAtIndexPathsForTesting:(NSArray<NSIndexPath*>*)indexPaths;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_MANAGER_VIEW_CONTROLLER_H_
