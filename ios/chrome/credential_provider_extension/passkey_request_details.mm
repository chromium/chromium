// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/passkey_request_details.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "components/webauthn/core/browser/passkey_model_utils.h"
#import "ios/chrome/common/credential_provider/credential.h"
#import "ios/chrome/credential_provider_extension/passkey_util.h"
#import "ios/chrome/credential_provider_extension/passkey_util_swift.h"
#import "ios/chrome/credential_provider_extension/ui/feature_flags.h"

@interface PasskeyRequestDetails ()

// Hash of client data for credential provider to sign as part of the operation.
@property(strong, nonatomic, readwrite) NSData* clientDataHash;

// A preference for whether the authenticator should attempt to verify that it
// is being used by its owner.
@property(nonatomic, readwrite) BOOL userVerificationRequired;

// The relying party identifier for this request.
@property(copy, nonatomic, readwrite) NSString* relyingPartyIdentifier;

// A list of allowed credential IDs for this request. An empty list means all
// credentials are allowed.
@property(strong, nonatomic, readwrite) NSArray<NSData*>* allowedCredentials;

// A list of excluded credentials for this request. An empty list means no
// credentials are excluded.
@property(strong, nonatomic, readwrite) NSArray<NSData*>* excludedCredentials;

// Whether at least one signing algorithm is supported by the relying party.
// Unused by assertion requests.
@property(nonatomic, readwrite) BOOL algorithmIsSupported;

// The user name of the passkey credential.
@property(copy, nonatomic, readwrite) NSString* userName;

// The user handle of the passkey credential.
@property(strong, nonatomic, readwrite) NSData* userHandle;

@end

@implementation PasskeyRequestDetails {
  PRFData* _prf;
  // Caches whether the registration request supports the large blob extension.
  BOOL _largeBlobCheckSupported;
}

// Checks if Large Blob support is requested from the registration input.
// This is determined by the presence of the 'largeBlob' property on the input
// object which indicates that support is either required or preferred.
+ (BOOL)isLargeBlobSupportRequestedFromRegistrationInput:
    (ASPasskeyRegistrationCredentialExtensionInput*)registrationInput
    API_AVAILABLE(ios(18.0)) {
  if (!IsPasskeyLargeBlobEnabled()) {
    return NO;
  }
  // The presence of the Large Blob input means support is either required or
  // preferred.
  return registrationInput.largeBlob ? YES : NO;
}

- (instancetype)initWithParameters:(ASPasskeyCredentialRequestParameters*)
                                       passkeyCredentialRequestParameters
    isBiometricAuthenticationEnabled:(BOOL)isBiometricAuthenticationEnabled {
  CHECK(passkeyCredentialRequestParameters);

  self = [super init];
  if (self) {
    self.clientDataHash = passkeyCredentialRequestParameters.clientDataHash;
    self.userVerificationRequired = ShouldPerformUserVerificationForPreference(
        passkeyCredentialRequestParameters.userVerificationPreference,
        isBiometricAuthenticationEnabled, /*is_conditional_create=*/NO);
    self.relyingPartyIdentifier =
        passkeyCredentialRequestParameters.relyingPartyIdentifier;
    self.allowedCredentials =
        passkeyCredentialRequestParameters.allowedCredentials;
    self.excludedCredentials = nil;
    self.algorithmIsSupported = NO;
    self.userName = nil;
    self.userHandle = nil;

    if (@available(iOS 18.0, *)) {
      if (IsPasskeyPRFEnabled()) {
        _prf = [PRFData fromParameters:passkeyCredentialRequestParameters];
      }
    }
  }
  return self;
}

- (instancetype)initWithRequest:(id<ASCredentialRequest>)credentialRequest
    isBiometricAuthenticationEnabled:(BOOL)isBiometricAuthenticationEnabled
                 isConditionalCreate:(BOOL)isConditionalCreate {
  CHECK(credentialRequest);

  self = [super init];
  if (self) {
    ASPasskeyCredentialRequest* passkeyCredentialRequest =
        base::apple::ObjCCastStrict<ASPasskeyCredentialRequest>(
            credentialRequest);

    self.clientDataHash = passkeyCredentialRequest.clientDataHash;
    self.userVerificationRequired = ShouldPerformUserVerificationForPreference(
        passkeyCredentialRequest.userVerificationPreference,
        isBiometricAuthenticationEnabled, isConditionalCreate);

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
    self.excludedCredentials = nil;

    if (@available(iOS 18.0, *)) {
      if (passkeyCredentialRequest.excludedCredentials.count) {
        NSMutableArray<NSData*>* excludedCredentials = [NSMutableArray array];
        for (ASAuthorizationPlatformPublicKeyCredentialDescriptor* credential in
                 passkeyCredentialRequest.excludedCredentials) {
          [excludedCredentials addObject:credential.credentialID];
        }
        self.excludedCredentials = [excludedCredentials copy];
      }

      if (IsPasskeyPRFEnabled()) {
        _prf = [PRFData fromRequest:passkeyCredentialRequest];
      }

      // Registration side large blob extension.
      _largeBlobCheckSupported = [PasskeyRequestDetails
          isLargeBlobSupportRequestedFromRegistrationInput:
              passkeyCredentialRequest.registrationExtensionInput];
    }
  }
  return self;
}

- (ASPasskeyRegistrationCredential*)
           createPasskeyForGaia:(NSString*)gaia
          securityDomainSecrets:(NSArray<NSData*>*)securityDomainSecrets
    didCompleteUserVerification:(BOOL)didCompleteUserVerification {
  NSArray<NSData*>* prfInputs = nil;
  if (@available(iOS 18.0, *)) {
    if (_prf.inputValues) {
      prfInputs = [NSArray arrayWithObjects:_prf.inputValues.saltInput1,
                                            _prf.inputValues.saltInput2, nil];
    } else if (_prf.checkForSupport) {
      // Initialize prfInputs with a non nil empty array to check for support.
      prfInputs = [NSArray array];
    }
  }
  PasskeyCreationOutput passkeyCreationOutput = PerformPasskeyCreation(
      self.clientDataHash, self.relyingPartyIdentifier, self.userName,
      self.userHandle, gaia, securityDomainSecrets, prfInputs,
      didCompleteUserVerification);
  if (@available(iOS 18.0, *)) {
    if (passkeyCreationOutput.credential) {
      if ([passkeyCreationOutput.prf_outputs count]) {
        PRFOutputValues* prfOutputValues =
            [PRFOutputValues fromValues:passkeyCreationOutput.prf_outputs];
        [passkeyCreationOutput.credential
            setPRFFromOutputValues:prfOutputValues];
      } else if (_prf.checkForSupport) {
        [passkeyCreationOutput.credential setPRFIsSupported];
      }
      if (_largeBlobCheckSupported) {
        [passkeyCreationOutput.credential setLargeBlobIsSupported];
      }
    }
  }
  return passkeyCreationOutput.credential;
}

- (ASPasskeyAssertionCredential*)
        assertPasskeyCredential:(id<Credential>)credential
          securityDomainSecrets:(NSArray<NSData*>*)securityDomainSecrets
    didCompleteUserVerification:(BOOL)didCompleteUserVerification {
  NSArray<NSData*>* prfInputs = nil;
  PRFInputValues* inputValues = nil;
  if (@available(iOS 18.0, *)) {
    if (_prf) {
      // Check if there's per credential values available.
      inputValues = _prf.perCredentialInputValues[credential.credentialId];
      if (!inputValues) {
        // If there are no per credential values, use the generic values.
        inputValues = _prf.inputValues;
      }
      if (inputValues) {
        prfInputs = [NSArray arrayWithObjects:inputValues.saltInput1,
                                              inputValues.saltInput2, nil];
      }
    }
  }
  PasskeyAssertionOutput passkeyAssertionOutput = PerformPasskeyAssertion(
      credential, self.clientDataHash, self.allowedCredentials,
      securityDomainSecrets, prfInputs, didCompleteUserVerification);
  if (@available(iOS 18.0, *)) {
    if (passkeyAssertionOutput.credential &&
        [passkeyAssertionOutput.prf_outputs count]) {
      PRFOutputValues* prfOutputValues =
          [PRFOutputValues fromValues:passkeyAssertionOutput.prf_outputs];
      [passkeyAssertionOutput.credential
          setPRFFromOutputValues:prfOutputValues];
    }
  }
  return passkeyAssertionOutput.credential;
}

- (BOOL)hasMatchingPassword:(NSArray<id<Credential>>*)credentials {
  if (!credentials.count) {
    return NO;
  }

  NSString* rpID = self.relyingPartyIdentifier;
  NSUInteger credentialIndex =
      [credentials indexOfObjectPassingTest:^BOOL(id<Credential> credential,
                                                  NSUInteger idx, BOOL* stop) {
        NSString* domainSuffix = [NSString
            stringWithFormat:@".%@", credential.registryControlledDomain];
        BOOL matchingDomain =
            [rpID isEqualToString:credential.registryControlledDomain] ||
            [rpID hasSuffix:domainSuffix];
        return !credential.isPasskey && matchingDomain &&
               [credential.username isEqualToString:self.userName];
      }];
  return credentialIndex != NSNotFound;
}

- (BOOL)hasExcludedPasskey:(NSArray<id<Credential>>*)credentials {
  if (!credentials.count || !self.excludedCredentials.count) {
    return NO;
  }

  NSUInteger credentialIndex = [credentials indexOfObjectPassingTest:^BOOL(
                                                id<Credential> credential,
                                                NSUInteger idx, BOOL* stop) {
    return credential.isPasskey &&
           [credential.rpId isEqualToString:self.relyingPartyIdentifier] &&
           [self.excludedCredentials containsObject:credential.credentialId];
  }];
  return credentialIndex != NSNotFound;
}

#pragma mark - PasskeyRequestDetails (Testing)

- (instancetype)initWithURL:(NSString*)url
                   username:(NSString*)username
        excludedCredentials:(NSArray<NSData*>*)excludedCredentials {
  self = [super init];
  if (self) {
    self.relyingPartyIdentifier = url;
    self.userName = username;
    self.excludedCredentials = excludedCredentials;
  }
  return self;
}

@end
