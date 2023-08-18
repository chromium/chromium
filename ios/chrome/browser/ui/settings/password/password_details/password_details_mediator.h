// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#import "base/memory/scoped_refptr.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller_delegate.h"

namespace password_manager {
struct CredentialUIEntry;
}  // namespace password_manager

namespace syncer {
class SyncService;
}  // namespace syncer

class PrefService;
class IOSChromePasswordCheckManager;
@protocol PasswordDetailsConsumer;
@protocol PasswordDetailsMediatorDelegate;

// This mediator fetches and organises the credentials for its consumer.
@interface PasswordDetailsMediator
    : NSObject <PasswordDetailsTableViewControllerDelegate>

// Vector of CredentialUIEntry is converted to an array of PasswordDetails and
// passed to a consumer with the display name (title) for the Password Details
// view.
- (instancetype)
       initWithPasswords:
           (const std::vector<password_manager::CredentialUIEntry>&)credentials
             displayName:(NSString*)displayName
    passwordCheckManager:(scoped_refptr<IOSChromePasswordCheckManager>)manager
             prefService:(PrefService*)prefService
             syncService:(syncer::SyncService*)syncService
                 context:(DetailsContext)context
                delegate:(id<PasswordDetailsMediatorDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Consumer of this mediator.
@property(nonatomic, weak) id<PasswordDetailsConsumer> consumer;

// Disconnects the mediator from all observers.
- (void)disconnect;

// Remove credential from credentials cache.
- (void)removeCredential:(PasswordDetails*)password;

// Moves credential and its duplicates to account store.
- (void)moveCredentialToAccountStore:(PasswordDetails*)password;

// Called to handle moving a credential to account store in case of a duplicate
// conflict. Deletes the outdated password, and moves the local credential if it
// is the recent one.
- (void)moveCredentialToAccountStoreWithConflict:(PasswordDetails*)password;

// Called when the user chooses to move a password to account store.
// Returns YES if the account stores the same username for the website with a
// different password, NO otherwise.
- (BOOL)hasPasswordConflictInAccount:(PasswordDetails*)password;

// Dismisses the compromised credential warning.
- (void)didConfirmWarningDismissalForPassword:(PasswordDetails*)password;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_MEDIATOR_H_
