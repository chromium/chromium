// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_ARCHIVABLE_CREDENTIAL_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_ARCHIVABLE_CREDENTIAL_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/common/credential_provider/credential.h"

// Credential that can be archived. `serviceIdentifier` must be unique between
// credentials, as it is used for equality.
//
// Credentials are immutable and don't hold state, and because of this the
// source of truth should always be the store.
@interface ArchivableCredential : NSObject <Credential, NSSecureCoding>

// Initializer used to create a credential based on another credential, but with
// a new favicon.
- (instancetype)initWithFavicon:(NSString*)favicon
                     credential:(id<Credential>)credential;

// Initializer used for password credentials.
- (instancetype)initWithFavicon:(NSString*)favicon
                           gaia:(NSString*)gaia
                       password:(NSString*)password
                           rank:(int64_t)rank
               recordIdentifier:(NSString*)recordIdentifier
              serviceIdentifier:(NSString*)serviceIdentifier
                    serviceName:(NSString*)serviceName
                       username:(NSString*)username
                           note:(NSString*)note NS_DESIGNATED_INITIALIZER;

// Initializer used for passkey credentials.
- (instancetype)initWithFavicon:(NSString*)favicon
                           gaia:(NSString*)gaia
               recordIdentifier:(NSString*)recordIdentifier
                         syncId:(NSData*)syncId
                       username:(NSString*)username
                userDisplayName:(NSString*)userDisplayName
                         userId:(NSData*)userId
                   credentialId:(NSData*)credentialId
                           rpId:(NSString*)rpId
                     privateKey:(NSData*)privateKey
                      encrypted:(NSData*)encrypted
                   creationTime:(int64_t)creationTime
                   lastUsedTime:(int64_t)lastUsedTime NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_ARCHIVABLE_CREDENTIAL_H_
