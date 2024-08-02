// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_CONSTANTS_H_
#define DEVICE_FIDO_ENCLAVE_CONSTANTS_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "crypto/signature_verifier.h"

namespace device::enclave {

// This file contains various constants used to communicate with the enclave.

struct EnclaveIdentity;

// GetEnclaveIdentity returns the default URL & public-key of the enclave. In
// tests, its return value can be set using `ScopedEnclaveOverride`.
COMPONENT_EXPORT(DEVICE_FIDO)
EnclaveIdentity GetEnclaveIdentity();

// Creating a `ScopedEnclaveOverride` allows the URL and public key of the
// enclave to be overridden for testing. These objects can be nested.
class COMPONENT_EXPORT(DEVICE_FIDO) ScopedEnclaveOverride {
 public:
  explicit ScopedEnclaveOverride(EnclaveIdentity identity);
  ~ScopedEnclaveOverride();

 private:
  const raw_ptr<const EnclaveIdentity> prev_;
  const std::unique_ptr<EnclaveIdentity> enclave_identity_;
};

// Maximum number of consecutive failed PIN attempts for a UV passkey request
// before getting locked out. This is enforced by the service, so it needs to
// match MAX_PIN_ATTEMPTS in
// third_party/cloud_authenticator/processor/src/passkeys.rs.
inline constexpr int kMaxFailedPINAttempts = 5;

// The length of a recovery key store counter ID.
inline constexpr size_t kCounterIDLen = 8;
// The length of a recovery key store "vault handle" value.
inline constexpr size_t kVaultHandleLen = 17;

// The maximum number of times that GPM enclave bootstrapping can be declined
// before it becomes deprioritized as an authenticator option.
inline constexpr int kMaxGPMBootstrapPrompts = 2;

// The list of algorithms that are acceptable as device identity keys.
inline constexpr crypto::SignatureVerifier::SignatureAlgorithm
    kSigningAlgorithms[] = {
        // This is in preference order and the enclave must support all the
        // algorithms listed here.
        crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
        crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256,
};

// Keys in the top-level request message.
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kCommandEncodedRequestsKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kCommandDeviceIdKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kCommandSigKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kCommandAuthLevelKey[];

// Generic keys for all request types.
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kRequestCommandKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kRequestWrappedSecretKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kRequestSecretKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kRequestCounterIDKey[];
COMPONENT_EXPORT(DEVICE_FIDO)
extern const char kRequestVaultHandleWithoutTypeKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kRequestWrappedPINDataKey[];

// Keys in the top-level of each response.
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kResponseSuccessKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kResponseErrorKey[];

// Command names
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kRegisterCommandName[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kForgetCommandName[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kWrapKeyCommandName[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kGenKeyPairCommandName[];
COMPONENT_EXPORT(DEVICE_FIDO)
extern const char kRecoveryKeyStoreWrapCommandName[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kPasskeysWrapPinCommandName[];
COMPONENT_EXPORT(DEVICE_FIDO)
extern const char kRecoveryKeyStoreWrapAsMemberCommandName[];
COMPONENT_EXPORT(DEVICE_FIDO)
extern const char kRecoveryKeyStoreRewrapCommandName[];

// Register request keys
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kRegisterPubKeysKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kRegisterDeviceIdKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kRegisterUVKeyPending[];

// Device key types
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kHardwareKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kSoftwareKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kUserVerificationKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kSoftwareUserVerificationKey[];

// Wrapping request keys
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kWrappingPurpose[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kWrappingKeyToWrap[];

// Wrap PIN request keys
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kPinHash[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kGeneration[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kClaimKey[];

// Wrapping response keys
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kWrappingResponsePublicKey[];
COMPONENT_EXPORT(DEVICE_FIDO)
extern const char kWrappingResponseWrappedPrivateKey[];

// Key purpose strings.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const char kKeyPurposeSecurityDomainMemberKey[];
COMPONENT_EXPORT(DEVICE_FIDO)
extern const char kKeyPurposeSecurityDomainSecret[];

// Recovery key store commands.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const char kRecoveryKeyStorePinHash[];
COMPONENT_EXPORT(DEVICE_FIDO)
extern const char kRecoveryKeyStoreCertXml[];
COMPONENT_EXPORT(DEVICE_FIDO)
extern const char kRecoveryKeyStoreSigXml[];

// Constants for the recovery key store service, which is used in conjunction
// with the enclave.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const char kRecoveryKeyStoreURL[];
COMPONENT_EXPORT(DEVICE_FIDO)
extern const char kRecoveryKeyStoreCertFileURL[];
COMPONENT_EXPORT(DEVICE_FIDO)
extern const char kRecoveryKeyStoreSigFileURL[];

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_CONSTANTS_H_
