// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SYNC_SYNC_ENCRYPTION_PASSPHRASE_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SYNC_SYNC_ENCRYPTION_PASSPHRASE_TABLE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

class Browser;

namespace sync_encryption_passphrase {
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierPassphrase = kSectionIdentifierEnumZero,
};
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeMessage = kItemTypeEnumZero,
  ItemTypeEnterPassphrase,
  ItemTypeConfirmPassphrase,
  ItemTypeError,
  ItemTypeFooter,
};
}  // namespace sync_encryption_passphrase

// Controller to allow user to specify encryption passphrase for Sync.
@interface SyncEncryptionPassphraseTableViewController
    : SettingsRootTableViewController <SyncObserverModelBridge>

@property(weak, nonatomic, readonly) UITextField* passphrase;
@property(nonatomic, copy) NSString* headerMessage;
@property(nonatomic, copy) NSString* footerMessage;
@property(nonatomic, copy) NSString* processingMessage;
@property(nonatomic, copy) NSString* syncErrorMessage;
@property(nonatomic, assign) BOOL presentModally;

// `profile` must not be nil.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

@interface SyncEncryptionPassphraseTableViewController (
    Subclassing) <SettingsControllerProtocol, UITextFieldDelegate>

// Whether this controller is for encryption or decryption. Returns `YES`, if
// the used for the user to enter an existing passphrase that is not yet
// available on the device. Returns `NO` if the user is setting a new
// passphrase.
- (BOOL)forDecryption;

// User has pressed the Sign In button.
- (void)signInPressed;

// Clears all fields after displaying an error.
- (void)clearFieldsOnError:(NSString*)errorMessage;

// Whether the text field(s) is(are) filled.
- (BOOL)areAllFieldsFilled;

// Registers listening to the events of `textField`.
- (void)registerTextField:(UITextField*)textField;

// Unregisters listening to the events of `textField`.
- (void)unregisterTextField:(UITextField*)textField;

// Called after a touch event entering a `UITextField`.
- (void)textFieldDidBeginEditing:(id)sender;

// Called after a touch event changing a `UITextField`.
- (void)textFieldDidChange:(id)sender;

// Called after a touch event leaving a `UITextField` by clicking "return" key.
- (void)textFieldDidEndEditing:(id)sender;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SYNC_SYNC_ENCRYPTION_PASSPHRASE_TABLE_VIEW_CONTROLLER_H_
