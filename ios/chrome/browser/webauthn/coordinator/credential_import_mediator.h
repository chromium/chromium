// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_COORDINATOR_CREDENTIAL_IMPORT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_COORDINATOR_CREDENTIAL_IMPORT_MEDIATOR_H_

#import <Foundation/Foundation.h>

// Mediator for the credential exchange import flow.
@interface CredentialImportMediator : NSObject

// `UUID` is a token received from the OS during app launch, required to be
// passed back to the OS to receive the credential data.
- (instancetype)initWithUUID:(NSUUID*)UUID NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_COORDINATOR_CREDENTIAL_IMPORT_MEDIATOR_H_
