// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_MODEL_CREDENTIAL_EXCHANGE_PASSWORD_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_MODEL_CREDENTIAL_EXCHANGE_PASSWORD_H_

#import <Foundation/Foundation.h>

// Intermediate data model for a password credential, used to translate
// between password manager's C++ representations of it and the Swift struct
// used by the OS Credential Exchange library.
// (https://fidoalliance.org/specs/cx/cxf-v1.0-ps-20250814.html#dict-basic-auth)
@interface CredentialExchangePassword : NSObject

// Url of the website where the credential can be used.
@property(nonatomic, strong) NSURL* URL;

// Username associated with the credential.
@property(nonatomic, copy) NSString* username;

// Password associated with the credential.
@property(nonatomic, copy) NSString* password;

// Note associated with the credential.
@property(nonatomic, copy) NSString* note;

- (instancetype)initWithURL:(NSURL*)URL
                   username:(NSString*)username
                   password:(NSString*)password
                       note:(NSString*)note NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_MODEL_CREDENTIAL_EXCHANGE_PASSWORD_H_
