// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CREDENTIAL_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CREDENTIAL_H_

#import <Foundation/Foundation.h>

// Contains the data for a Credential that can be used with iOS AutoFill.
@protocol Credential

// Associated favicon name.
@property(nonatomic, readonly) NSString* favicon;

// Identifier used to look up the secret in the Keychain.
@property(nonatomic, readonly) NSString* keychainIdentifier;

// Importance ranking of this credential.
@property(nonatomic, readonly) int64_t rank;

// Identifier to use with ASCredentialIdentityStore.
@property(nonatomic, readonly) NSString* recordIdentifier;

// Service identifier of this credential. Should match
// ASCredentialServiceIdentifier.
@property(nonatomic, readonly) NSString* serviceIdentifier;

// Human readable name of the associated service.
@property(nonatomic, readonly) NSString* serviceName;

// Username of the service.
@property(nonatomic, readonly) NSString* user;

// ID to validate the credential before providing it to the system.
@property(nonatomic, readonly) NSString* validationIdentifier;

@end

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CREDENTIAL_H_
