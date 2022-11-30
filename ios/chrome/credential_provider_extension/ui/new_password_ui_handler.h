// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_UI_HANDLER_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_UI_HANDLER_H_

@class ArchivableCredential;

// Protocol to allow the NewPasswordMediator to interact with the UI
@protocol NewPasswordUIHandler

// Sets the password in the respective field to whatever value is passed.
- (void)setPassword:(NSString*)password;

// Asks the UI to alert the user that the credential they are trying to create
// already exists.
- (void)alertUserCredentialExists;

// Asks the UI to alert the user that the saving process failed.
- (void)alertSavePasswordFailed;

// Informs the UI that a credential was successfully saved.
- (void)credentialSaved:(ArchivableCredential*)credential;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_UI_HANDLER_H_
