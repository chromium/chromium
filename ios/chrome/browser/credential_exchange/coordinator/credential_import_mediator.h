// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_IMPORT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_IMPORT_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import <string>

@protocol CredentialImportConsumer;

// Delegate for CredentialImportMediator.
@protocol CredentialImportMediatorDelegate <NSObject>

// Notifies the delegate to display the import screen.
- (void)showImportScreen;

@end

// Mediator for the credential exchange import flow.
@interface CredentialImportMediator : NSObject

// `UUID` is a token received from the OS during app launch, required to be
// passed back to the OS to receive the credential data.
- (instancetype)initWithUUID:(NSUUID*)UUID
                    delegate:(id<CredentialImportMediatorDelegate>)delegate
                   userEmail:(std::string)userEmail NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Consumer of this mediator.
@property(nonatomic, weak) id<CredentialImportConsumer> consumer;

// Notifies the model to starts importing credentials to the user's account.
// `securityDomainSecrets` is needed to encrypt passkeys if there are any to be
// imported.
- (void)startImportingCredentialsWithSecurityDomainSecrets:
    (NSArray<NSData*>*)securityDomainSecrets;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_IMPORT_MEDIATOR_H_
