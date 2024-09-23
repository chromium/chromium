// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_UTIL_H_
#define DEVICE_FIDO_MAC_UTIL_H_

#import <Security/Security.h>
#include <os/availability.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fido_constants.h"
#include "device/fido/mac/credential_metadata.h"
#include "device/fido/p256_public_key.h"

namespace device::fido::mac {

// MakeAttestedCredentialData returns an AttestedCredentialData instance for
// the Touch ID authenticator credential ID and public key or |std::nullopt|
// on failure.
COMPONENT_EXPORT(DEVICE_FIDO)
std::optional<AttestedCredentialData> MakeAttestedCredentialData(
    std::vector<uint8_t> credential_id,
    std::unique_ptr<PublicKey> public_key);

// MakeAuthenticatorData returns an AuthenticatorData instance for the Touch ID
// authenticator with the given Relying Party ID and AttestedCredentialData,
// which may be |std::nullopt| in GetAssertion operations.
COMPONENT_EXPORT(DEVICE_FIDO)
AuthenticatorData MakeAuthenticatorData(
    CredentialMetadata::SignCounter counter_type,
    const std::string& rp_id,
    std::optional<AttestedCredentialData> attested_credential_data,
    bool has_uv);

// GenerateSignature signs the concatenation of the serialization of the given
// authenticator data and the given client data hash, as required for
// (self-)attestation and assertion. Returns |std::nullopt| if the operation
// fails.
std::optional<std::vector<uint8_t>> GenerateSignature(
    const AuthenticatorData& authenticator_data,
    base::span<const uint8_t, kClientDataHashLength> client_data_hash,
    SecKeyRef private_key);

// SecKeyRefToECPublicKey converts a SecKeyRef for a public key into an
// equivalent |PublicKey| instance. It returns |nullptr| if the key cannot
// be converted.
std::unique_ptr<PublicKey> SecKeyRefToECPublicKey(SecKeyRef public_key_ref);

enum class CodeSigningState {
  kSigned,
  kNotSigned,
};

// ProcessIsSigned returns whether the current process has been code
// signed.
CodeSigningState ProcessIsSigned();

// Overrides the result of `ProcessIsSigned` for testing.
class COMPONENT_EXPORT(DEVICE_FIDO) ScopedProcessIsSignedOverride {
 public:
  explicit ScopedProcessIsSignedOverride(CodeSigningState process_is_signed);
  ScopedProcessIsSignedOverride(const ScopedProcessIsSignedOverride&) = delete;
  ScopedProcessIsSignedOverride& operator=(
      const ScopedProcessIsSignedOverride&) = delete;
  ~ScopedProcessIsSignedOverride();
};

// Returns whether the macOS profile authenticator will do user verification.
COMPONENT_EXPORT(DEVICE_FIDO)
bool ProfileAuthenticatorWillDoUserVerification(
    device::UserVerificationRequirement requirement,
    bool platform_has_biometrics);

// Returns whether biometrics are available for use. On macOS, this translates
// to whether the device supports Touch ID, and whether the sensor is ready to
// be used (i.e. not soft-locked from consecutive bad attempts; laptop lid not
// closed).
COMPONENT_EXPORT(DEVICE_FIDO)
bool DeviceHasBiometricsAvailable();

// Overrides the result of `DeviceHasBiometricsAvailable` for testing.
class COMPONENT_EXPORT(DEVICE_FIDO) ScopedBiometricsOverride {
 public:
  explicit ScopedBiometricsOverride(bool has_biometrics);
  ScopedBiometricsOverride(const ScopedBiometricsOverride&) = delete;
  ScopedBiometricsOverride& operator=(const ScopedBiometricsOverride&) = delete;
  ~ScopedBiometricsOverride();
};

}  // namespace device::fido::mac

#endif  // DEVICE_FIDO_MAC_UTIL_H_
