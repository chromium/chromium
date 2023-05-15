// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_ICLOUD_KEYCHAIN_INTERNALS_H_
#define DEVICE_FIDO_MAC_ICLOUD_KEYCHAIN_INTERNALS_H_

NS_ASSUME_NONNULL_BEGIN

// The following definitions of ASC* interfaces are from
// AuthenticationServicesCore, which is a private framework. The full
// definitions can be found in
// Source/WebKit/Platform/spi/Cocoa/AuthenticationServicesCoreSPI.h from
// WebKit, but only the needed parts are specified here.
//
// These interfaces are needed to implement several behaviours that browsers
// require. Most importantly, specifying the full clientDataHash rather than
// the challenge.

@interface ASCPublicKeyCredentialDescriptor : NSObject <NSSecureCoding>
- (instancetype)initWithCredentialID:(NSData*)credentialID
                          transports:
                              (nullable NSArray<NSString*>*)allowedTransports;
@end

@protocol ASCPublicKeyCredentialCreationOptions
@property(nonatomic, copy) NSData* clientDataHash;
@property(nonatomic, nullable, copy) NSData* challenge;
@property(nonatomic, nullable, copy) NSString* userVerificationPreference;
@property(nonatomic, copy) NSArray<NSNumber*>* supportedAlgorithmIdentifiers;
@property(nonatomic) BOOL shouldRequireResidentKey;
@property(nonatomic, copy)
    NSArray<ASCPublicKeyCredentialDescriptor*>* excludedCredentials;
@end

@protocol ASCPublicKeyCredentialAssertionOptions <NSCopying>
@property(nonatomic, copy) NSData* clientDataHash;
@end

@protocol ASCCredentialRequestContext
@property(nonatomic, nullable, copy) id<ASCPublicKeyCredentialAssertionOptions>
    platformKeyCredentialAssertionOptions;
@property(nonatomic, nullable, copy) id<ASCPublicKeyCredentialCreationOptions>
    platformKeyCredentialCreationOptions;
@end

@interface ASAuthorizationController (Secrets)
- (id<ASCCredentialRequestContext>)
    _requestContextWithRequests:(NSArray<ASAuthorizationRequest*>*)requests
                          error:(NSError**)outError;
@end

NS_ASSUME_NONNULL_END

#endif  // DEVICE_FIDO_MAC_ICLOUD_KEYCHAIN_INTERNALS_H_
