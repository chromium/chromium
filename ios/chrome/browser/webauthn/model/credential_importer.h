// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_MODEL_CREDENTIAL_IMPORTER_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_MODEL_CREDENTIAL_IMPORTER_H_

#import <Foundation/Foundation.h>

// Handles importing credentials using the Credential Exchange Format
// (https://fidoalliance.org/specifications-credential-exchange-specifications).
@interface CredentialImporter : NSObject

- (instancetype)init NS_DESIGNATED_INITIALIZER;

// Called when the app is launched to perform credential import. `UUID` is a
// token provided by the OS on app launch, required to receive the credential
// data to be imported.
- (void)startImport:(NSUUID*)UUID;

@end

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_MODEL_CREDENTIAL_IMPORTER_H_
