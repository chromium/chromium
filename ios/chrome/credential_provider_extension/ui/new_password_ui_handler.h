// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_UI_HANDLER_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_UI_HANDLER_H_

// Protocol to allow the NewPasswordMediator to interact with the UI
@protocol NewPasswordUIHandler

// Asks the UI to alert the user that the credential they are trying to create
// already exists.
- (void)alertUserCredentialExists;

// Asks the UI to alert the user that the saving process failed.
- (void)alertSavePasswordFailed;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_UI_HANDLER_H_
