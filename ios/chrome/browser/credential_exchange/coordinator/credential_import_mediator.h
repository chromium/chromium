// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_IMPORT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_IMPORT_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import <memory>
#import <string>

#import "ios/chrome/browser/data_import/ui/data_import_credential_conflict_mutator.h"

namespace password_manager {
class SavedPasswordsPresenter;
}  // namespace password_manager

namespace webauthn {
class PasskeyModel;
}  // namespace webauthn

@protocol CredentialImportConsumer;
enum class CredentialImportStage;
class FaviconLoader;
@class PasskeyImportItem;
@class PasswordImportItem;

// Delegate for CredentialImportMediator.
@protocol CredentialImportMediatorDelegate <NSObject>

// Notifies the delegate to display the import screen.
- (void)showImportScreen;

// Notifies the delegate to display a conflict resolution screen.
- (void)showConflictResolutionScreenWithPasswords:
            (NSArray<PasswordImportItem*>*)passwords
                                         passkeys:(NSArray<PasskeyImportItem*>*)
                                                      passkeys;

@end

// Mediator for the credential exchange import flow.
@interface CredentialImportMediator
    : NSObject <DataImportCredentialConflictMutator>

// Consumer of this mediator.
@property(nonatomic, weak) id<CredentialImportConsumer> consumer;

// Whether passkeys are present on the import credential list.
@property(nonatomic, assign) BOOL importingPasskeys;

// Current stage of import.
@property(nonatomic, assign) CredentialImportStage importStage;

// Passwords that were not imported due to errors.
@property(nonatomic, copy) NSArray<PasswordImportItem*>* invalidPasswords;

// `UUID` is a token received from the OS during app launch, required to be
// passed back to the OS to receive the credential data.
- (instancetype)initWithUUID:(NSUUID*)UUID
                    delegate:(id<CredentialImportMediatorDelegate>)delegate
                   userEmail:(std::string)userEmail
     savedPasswordsPresenter:
         (std::unique_ptr<password_manager::SavedPasswordsPresenter>)
             savedPasswordsPresenter
                passkeyModel:(webauthn::PasskeyModel*)passkeyModel
               faviconLoader:(FaviconLoader*)faviconLoader
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Notifies the model to starts importing credentials to the user's account.
// `trustedVaultKeys` are needed to encrypt passkeys if there are any to be
// imported.
- (void)startImportingCredentialsWithTrustedVaultKeys:
    (NSArray<NSData*>*)trustedVaultKeys;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_IMPORT_MEDIATOR_H_
