// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_UI_HANDLER_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_UI_HANDLER_H_

#import <Foundation/Foundation.h>

@protocol Credential;

// Handler for presenting UI components for the credential list.
@protocol CredentialListUIHandler <NSObject>

// Asks the presenter to display the empty credentials view.
- (void)showEmptyCredentials;

// Calls the presenter when user has selected given `credential`.
- (void)userSelectedCredential:(id<Credential>)credential;

// Asks the presenter to display the details for given `credential`.
- (void)showDetailsForCredential:(id<Credential>)credential;

// Called when user wants to create a new credential.
- (void)showCreateNewPasswordUI;

// List of passkey credentials allowed by the relying party. If the array is
// empty, then the relying party accepts any passkey credential.
- (NSArray<NSData*>*)allowedCredentials;

// Whether the current request is a passkey related request.
- (BOOL)isRequestingPasskey;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_UI_HANDLER_H_
