// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_MODEL_CREDENTIAL_IMPORTER_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_MODEL_CREDENTIAL_IMPORTER_H_

#import <Foundation/Foundation.h>

#import <vector>

#import "components/webauthn/ios/passkey_types.h"
#import "ios/chrome/browser/credential_exchange/model/credential_import_manager_swift.h"

namespace webauthn {
class PasskeyModel;
}  // namespace webauthn

namespace password_manager {
struct ImportResults;
class SavedPasswordsPresenter;
}  // namespace password_manager

@class PasskeyImportItem;
@class PasswordImportItem;

// Delegate for CredentialImporter.
@protocol CredentialImporterDelegate <NSObject>

// Displays the initial import screen with counts of received credentials.
- (void)showImportScreenWithPasswordCount:(NSInteger)passwordCount
                             passkeyCount:(NSInteger)passkeyCount
                      exporterDisplayName:(NSString*)exporterDisplayName;

// Displays the conflict resolution screen with conflicting `passwords` and
// `passkeys`.
- (void)showConflictResolutionScreenWithPasswords:
            (NSArray<PasswordImportItem*>*)passwords
                                         passkeys:(NSArray<PasskeyImportItem*>*)
                                                      passkeys;

// Updates the status of the password import in the UI.
- (void)onPasswordsImported:(const password_manager::ImportResults&)results;

// Updates the status of the passkey import in the UI.
- (void)onPasskeysImported:(int)passkeysImported
                   invalid:(NSArray<PasskeyImportItem*>*)invalid;

// Updates the status of the UI after importing all credential types finished.
- (void)onImportFinished;

// Notifies the delegate that an error occurred during import.
- (void)onImportError;

@end

// Handles importing credentials using the Credential Exchange Format
// (https://fidoalliance.org/specifications-credential-exchange-specifications).
@interface CredentialImporter : NSObject <CredentialImportManagerDelegate>

- (instancetype)initWithDelegate:(id<CredentialImporterDelegate>)delegate
         savedPasswordsPresenter:
             (password_manager::SavedPasswordsPresenter*)savedPasswordsPresenter
                    passkeyModel:(webauthn::PasskeyModel*)passkeyModel
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Called when the app is launched to perform credential import. `UUID` is a
// token provided by the OS on app launch, required to receive the credential
// data to be imported.
- (void)prepareImport:(NSUUID*)UUID;

// Called when the user confirms the import. `trustedVaultKeys` are needed to
// encrypt passkeys if there are any to be imported. Triggers initial processing
// of the data for all supported credential types. Processing each credential
// type is handled in a separate async task. Results are analyzed once all tasks
// complete.
- (void)startImportingCredentialsWithTrustedVaultKeys:
    (webauthn::SharedKeyList)trustedVaultKeys;

// Triggers storing data for all supported credential types in the user's
// account. This should be called after conflicts with existing credential data
// stored in the user's account were resolved for all credential types or
// immediately after identifying no conflicts. `selectedPasswordIds` contains
// ids of conflicting passwords that should be imported (if any).
// `selectedPasskeyIds` contains ids of conflicting passkeys that should be
// imported (if any).
- (void)finishImportWithSelectedPasswordIds:
            (const std::vector<int>&)selectedPasswordIds
                         selectedPasskeyIds:
                             (const std::vector<int>&)selectedPasskeyIds;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_MODEL_CREDENTIAL_IMPORTER_H_
