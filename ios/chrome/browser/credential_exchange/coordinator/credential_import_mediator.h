// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_IMPORT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_IMPORT_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import <memory>
#import <string>

#import "components/webauthn/ios/passkey_types.h"
#import "ios/chrome/browser/data_import/ui/data_import_credential_conflict_mutator.h"

namespace password_manager {
class SavedPasswordsPresenter;
}  // namespace password_manager

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

namespace webauthn {
class PasskeyModel;
}  // namespace webauthn

@protocol CredentialImportConsumer;
enum class CredentialImportStage;
class FaviconLoader;
@class PasskeyImportItem;
@class PasswordImportItem;
class PrefService;

// Delegate for CredentialImportMediator.
@protocol CredentialImportMediatorDelegate <NSObject>

// Notifies the delegate to display the import screen.
- (void)showImportScreen;

// Notifies the delegate to display the "nothing imported" alert.
- (void)showNothingImportedScreen;

// Notifies the delegate to display the "nothing imported" alert due to
// all credential types import being blocked by enterprise policies.
- (void)showNothingImportedEnterpriseScreen;

// Notifies the delegate to display a conflict resolution screen.
- (void)showConflictResolutionScreenWithPasswords:
            (NSArray<PasswordImportItem*>*)passwords
                                         passkeys:(NSArray<PasskeyImportItem*>*)
                                                      passkeys;

// Notifies the delegate to display a generic error alert.
- (void)showGenericError;

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

// Passkeys that were not imported due to errors.
@property(nonatomic, copy) NSArray<PasskeyImportItem*>* invalidPasskeys;

// `UUID` is a token received from the OS during app launch, required to be
// passed back to the OS to receive the credential data.
- (instancetype)initWithUUID:(NSUUID*)UUID
                    delegate:(id<CredentialImportMediatorDelegate>)delegate
             identityManager:(signin::IdentityManager*)identityManager
     savedPasswordsPresenter:
         (std::unique_ptr<password_manager::SavedPasswordsPresenter>)
             savedPasswordsPresenter
                passkeyModel:(webauthn::PasskeyModel*)passkeyModel
               faviconLoader:(FaviconLoader*)faviconLoader
                 syncService:(syncer::SyncService*)syncService
                 prefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Notifies the model to starts importing credentials to the user's account.
// `trustedVaultKeys` are needed to encrypt passkeys if there are any to be
// imported.
- (void)startImportingCredentialsWithTrustedVaultKeys:
    (webauthn::SharedKeyList)trustedVaultKeys;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_IMPORT_MEDIATOR_H_
