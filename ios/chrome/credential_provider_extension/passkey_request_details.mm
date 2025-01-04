// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/passkey_request_details.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "components/webauthn/core/browser/passkey_model_utils.h"
#import "ios/chrome/credential_provider_extension/passkey_util.h"

@interface PasskeyRequestDetails ()

// Hash of client data for credential provider to sign as part of the operation.
@property(strong, nonatomic, readwrite) NSData* clientDataHash;

// A preference for whether the authenticator should attempt to verify that it
// is being used by its owner.
@property(nonatomic, readwrite) BOOL userVerificationRequired;

// The relying party identifier for this request.
@property(strong, nonatomic, readwrite) NSString* relyingPartyIdentifier;

// A list of allowed credential IDs for this request. An empty list means all
// credentials are allowed.
@property(strong, nonatomic, readwrite) NSArray<NSData*>* allowedCredentials;

// Whether at least one signing algorithm is supported by the relying party.
// Unused by assertion requests.
@property(nonatomic, readwrite) BOOL algorithmIsSupported;

// The user name of the passkey credential.
@property(strong, nonatomic, readwrite) NSString* userName;

// The user handle of the passkey credential.
@property(strong, nonatomic, readwrite) NSData* userHandle;

@end

@implementation PasskeyRequestDetails

- (instancetype)initWithParameters:(ASPasskeyCredentialRequestParameters*)
                                       passkeyCredentialRequestParameters
    isBiometricAuthenticationEnabled:(BOOL)isBiometricAuthenticationEnabled
    API_AVAILABLE(ios(17.0)) {
  CHECK(passkeyCredentialRequestParameters);

  self = [super init];
  if (self) {
    self.clientDataHash = passkeyCredentialRequestParameters.clientDataHash;
    self.userVerificationRequired = ShouldPerformUserVerificationForPreference(
        passkeyCredentialRequestParameters.userVerificationPreference,
        isBiometricAuthenticationEnabled);
    self.relyingPartyIdentifier =
        passkeyCredentialRequestParameters.relyingPartyIdentifier;
    self.allowedCredentials =
        passkeyCredentialRequestParameters.allowedCredentials;
    self.algorithmIsSupported = NO;
    self.userName = nil;
    self.userHandle = nil;
  }
  return self;
}

- (instancetype)initWithRequest:(id<ASCredentialRequest>)credentialRequest
    isBiometricAuthenticationEnabled:(BOOL)isBiometricAuthenticationEnabled
    API_AVAILABLE(ios(17.0)) {
  CHECK(credentialRequest);

  self = [super init];
  if (self) {
    ASPasskeyCredentialRequest* passkeyCredentialRequest =
        base::apple::ObjCCastStrict<ASPasskeyCredentialRequest>(
            credentialRequest);

    self.clientDataHash = passkeyCredentialRequest.clientDataHash;
    self.userVerificationRequired = ShouldPerformUserVerificationForPreference(
        passkeyCredentialRequest.userVerificationPreference,
        isBiometricAuthenticationEnabled);

    NSArray<NSNumber*>* supportedAlgorithms = [passkeyCredentialRequest
                                                   .supportedAlgorithms
        filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(
                                                     NSNumber* algorithm,
                                                     NSDictionary* bindings) {
          return webauthn::passkey_model_utils::IsSupportedAlgorithm(
              algorithm.intValue);
        }]];
    self.algorithmIsSupported = supportedAlgorithms.count > 0;

    ASPasskeyCredentialIdentity* identity =
        base::apple::ObjCCastStrict<ASPasskeyCredentialIdentity>(
            passkeyCredentialRequest.credentialIdentity);

    self.relyingPartyIdentifier = identity.relyingPartyIdentifier;
    self.userName = identity.userName;
    self.userHandle = identity.userHandle;
    self.allowedCredentials = nil;
  }
  return self;
}

- (ASPasskeyRegistrationCredential*)createPasskeyForGaia:(NSString*)gaia
                                   securityDomainSecrets:
                                       (NSArray<NSData*>*)securityDomainSecrets
    API_AVAILABLE(ios(17.0)) {
  return PerformPasskeyCreation(self.clientDataHash,
                                self.relyingPartyIdentifier, self.userName,
                                self.userHandle, gaia, securityDomainSecrets);
}

- (ASPasskeyAssertionCredential*)
    assertPasskeyCredential:(id<Credential>)credential
      securityDomainSecrets:(NSArray<NSData*>*)securityDomainSecrets
    API_AVAILABLE(ios(17.0)) {
  return PerformPasskeyAssertion(credential, self.clientDataHash,
                                 self.allowedCredentials,
                                 securityDomainSecrets);
}

@end
