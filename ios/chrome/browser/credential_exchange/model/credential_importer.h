// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_MODEL_CREDENTIAL_IMPORTER_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_MODEL_CREDENTIAL_IMPORTER_H_

#import <Foundation/Foundation.h>

// Delegate for CredentialImporter.
@protocol CredentialImporterDelegate <NSObject>

// Displays the initial import screen with counts of received credentials.
- (void)showImportScreenWithPasswordCount:(NSInteger)passwordCount
                             passkeyCount:(NSInteger)passkeyCount;

@end

// Handles importing credentials using the Credential Exchange Format
// (https://fidoalliance.org/specifications-credential-exchange-specifications).
@interface CredentialImporter : NSObject

- (instancetype)initWithDelegate:(id<CredentialImporterDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Called when the app is launched to perform credential import. `UUID` is a
// token provided by the OS on app launch, required to receive the credential
// data to be imported.
- (void)prepareImport:(NSUUID*)UUID;

// Called when the user confirms the import. `securityDomainSecrets` is needed
// to encrypt passkeys if there are any to be imported.
// TODO(crbug.com/449701042): Document this method better.
- (void)startImportingCredentialsWithSecurityDomainSecrets:
    (NSArray<NSData*>*)securityDomainSecrets;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_MODEL_CREDENTIAL_IMPORTER_H_
