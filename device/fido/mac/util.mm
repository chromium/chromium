// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/fido/mac/util.h"

#import <Foundation/Foundation.h>
#import <LocalAuthentication/LocalAuthentication.h>

#include <array>
#include <set>
#include <string>

#include "base/apple/foundation_util.h"
#include "base/apple/osstatus_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "build/branding_buildflags.h"
#include "components/cbor/writer.h"
#include "crypto/apple_keychain_v2.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/p256_public_key.h"
#include "device/fido/public_key.h"

namespace device::fido::mac {

using base::apple::ScopedCFTypeRef;
using cbor::Value;
using cbor::Writer;

// The Touch ID authenticator AAGUID value. Despite using self-attestation,
// Chrome will return this non-zero AAGUID for all MakeCredential
// responses coming from the Touch ID platform authenticator.
constexpr std::array<uint8_t, 16> kAaguid =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {0xad, 0xce, 0x00, 0x02, 0x35, 0xbc, 0xc6, 0x0a,
     0x64, 0x8b, 0x0b, 0x25, 0xf1, 0xf0, 0x55, 0x03};
#else
    {0xb5, 0x39, 0x76, 0x66, 0x48, 0x85, 0xaa, 0x6b,
     0xce, 0xbf, 0xe5, 0x22, 0x62, 0xa4, 0x39, 0xa2};
#endif

namespace {

// Returns the signature counter to use in the authenticatorData.
std::array<uint8_t, 4> MakeSignatureCounter(
    CredentialMetadata::SignCounter counter_type) {
  // For current credentials, the counter is fixed at 0.
  switch (counter_type) {
    case CredentialMetadata::SignCounter::kTimestamp: {
      // Legacy credentials use a timestamp-based counter. RPs expect a non-zero
      // counter to be increasing with each assertion, so we can't fix the
      // counter at 0 for old credentials. Because of the conversion to a 32-bit
      // unsigned integer, the counter will overflow in the year 2108.
      uint32_t sign_counter =
          static_cast<uint32_t>(base::Time::Now().InSecondsFSinceUnixEpoch());
      return std::array<uint8_t, 4>{
          static_cast<uint8_t>((sign_counter >> 24) & 0xff),
          static_cast<uint8_t>((sign_counter >> 16) & 0xff),
          static_cast<uint8_t>((sign_counter >> 8) & 0xff),
          static_cast<uint8_t>(sign_counter & 0xff),
      };
    }
    case CredentialMetadata::SignCounter::kZero:
      return {0, 0, 0, 0};
  }
}

}  // namespace

COMPONENT_EXPORT(DEVICE_FIDO)
std::optional<AttestedCredentialData> MakeAttestedCredentialData(
    std::vector<uint8_t> credential_id,
    std::unique_ptr<PublicKey> public_key) {
  if (credential_id.empty() || credential_id.size() > 255) {
    LOG(ERROR) << "invalid credential id: " << base::HexEncode(credential_id);
    return std::nullopt;
  }
  if (!public_key) {
    LOG(ERROR) << "no public key";
    return std::nullopt;
  }
  std::array<uint8_t, 2> encoded_credential_id_length = {
      0, static_cast<uint8_t>(credential_id.size())};
  return AttestedCredentialData(kAaguid, encoded_credential_id_length,
                                std::move(credential_id),
                                std::move(public_key));
}

AuthenticatorData MakeAuthenticatorData(
    CredentialMetadata::SignCounter counter_type,
    const std::string& rp_id,
    std::optional<AttestedCredentialData> attested_credential_data,
    bool has_uv) {
  uint8_t flags =
      static_cast<uint8_t>(AuthenticatorData::Flag::kTestOfUserPresence);
  if (has_uv) {
    flags |=
        static_cast<uint8_t>(AuthenticatorData::Flag::kTestOfUserVerification);
  }
  if (attested_credential_data) {
    flags |= static_cast<uint8_t>(AuthenticatorData::Flag::kAttestation);
  }
  return AuthenticatorData(fido_parsing_utils::CreateSHA256Hash(rp_id), flags,
                           MakeSignatureCounter(counter_type),
                           std::move(attested_credential_data));
}

std::optional<std::vector<uint8_t>> GenerateSignature(
    const AuthenticatorData& authenticator_data,
    base::span<const uint8_t, kClientDataHashLength> client_data_hash,
    SecKeyRef private_key) {
  const std::vector<uint8_t> serialized_authenticator_data =
      authenticator_data.SerializeToByteArray();
  size_t capacity =
      serialized_authenticator_data.size() + client_data_hash.size();
  ScopedCFTypeRef<CFMutableDataRef> sig_input(
      CFDataCreateMutable(kCFAllocatorDefault, capacity));
  CFDataAppendBytes(sig_input.get(), serialized_authenticator_data.data(),
                    serialized_authenticator_data.size());
  CFDataAppendBytes(sig_input.get(), client_data_hash.data(),
                    client_data_hash.size());
  ScopedCFTypeRef<CFErrorRef> err;
  ScopedCFTypeRef<CFDataRef> sig_data(
      crypto::AppleKeychainV2::GetInstance().KeyCreateSignature(
          private_key, kSecKeyAlgorithmECDSASignatureMessageX962SHA256,
          sig_input.get(), err.InitializeInto()));
  if (!sig_data) {
    LOG(ERROR) << "SecKeyCreateSignature failed: " << err.get();
    return std::nullopt;
  }
  auto sig_data_span = base::apple::CFDataToSpan(sig_data.get());
  return std::vector<uint8_t>(sig_data_span.begin(), sig_data_span.end());
}

// SecKeyRefToECPublicKey converts a SecKeyRef for a public key into an
// equivalent |PublicKey| instance. It returns |nullptr| if the key cannot
// be converted.
std::unique_ptr<PublicKey> SecKeyRefToECPublicKey(SecKeyRef public_key_ref) {
  CHECK(public_key_ref);
  ScopedCFTypeRef<CFErrorRef> err;
  ScopedCFTypeRef<CFDataRef> data_ref(
      SecKeyCopyExternalRepresentation(public_key_ref, err.InitializeInto()));
  if (!data_ref) {
    LOG(ERROR) << "SecCopyExternalRepresentation failed: " << err.get();
    return nullptr;
  }
  auto key_data = base::apple::CFDataToSpan(data_ref.get());
  auto key = P256PublicKey::ParseX962Uncompressed(
      static_cast<int32_t>(CoseAlgorithmIdentifier::kEs256), key_data);
  if (!key) {
    LOG(ERROR) << "Unexpected public key format: " << base::HexEncode(key_data);
    return nullptr;
  }
  return key;
}

std::optional<CodeSigningState>& GetProcessIsSignedOverride() {
  static std::optional<CodeSigningState> flag;
  return flag;
}

ScopedProcessIsSignedOverride::ScopedProcessIsSignedOverride(
    CodeSigningState process_is_signed) {
  std::optional<CodeSigningState>& flag = GetProcessIsSignedOverride();
  // Overrides don't nest.
  CHECK(!flag.has_value());
  flag = process_is_signed;
}

ScopedProcessIsSignedOverride::~ScopedProcessIsSignedOverride() {
  std::optional<CodeSigningState>& flag = GetProcessIsSignedOverride();
  CHECK(flag.has_value());
  flag.reset();
}

CodeSigningState ProcessIsSigned() {
  std::optional<CodeSigningState>& flag = GetProcessIsSignedOverride();
  if (flag.has_value()) {
    return *flag;
  }
  base::apple::ScopedCFTypeRef<SecTaskRef> task(SecTaskCreateFromSelf(nullptr));
  if (!task) {
    return CodeSigningState::kNotSigned;
  }

  base::apple::ScopedCFTypeRef<CFStringRef> sign_id(
      SecTaskCopySigningIdentifier(task.get(), /*error=*/nullptr));
  return static_cast<bool>(sign_id) ? CodeSigningState::kSigned
                                    : CodeSigningState::kNotSigned;
}

bool ProfileAuthenticatorWillDoUserVerification(
    device::UserVerificationRequirement requirement,
    bool platform_has_biometrics) {
  return requirement == device::UserVerificationRequirement::kRequired ||
         platform_has_biometrics;
}

std::optional<bool>& GetBiometricOverride() {
  static std::optional<bool> flag;
  return flag;
}

ScopedBiometricsOverride::ScopedBiometricsOverride(bool has_biometrics) {
  std::optional<bool>& flag = GetBiometricOverride();
  // Overrides don't nest.
  CHECK(!flag.has_value());
  flag = has_biometrics;
}

ScopedBiometricsOverride::~ScopedBiometricsOverride() {
  std::optional<bool>& flag = GetBiometricOverride();
  CHECK(flag.has_value());
  flag.reset();
}

bool DeviceHasBiometricsAvailable() {
  std::optional<bool>& flag = GetBiometricOverride();
  if (flag.has_value()) {
    return *flag;
  }

  return crypto::AppleKeychainV2::GetInstance().LAContextCanEvaluatePolicy(
      LAPolicyDeviceOwnerAuthenticationWithBiometrics, /*error=*/nil);
}

}  // namespace device::fido::mac
