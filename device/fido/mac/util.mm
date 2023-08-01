// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/util.h"

#include <array>
#include <set>
#include <string>

#import <Foundation/Foundation.h>

#include "base/functional/bind.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "build/branding_buildflags.h"
#include "components/cbor/writer.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/mac/keychain.h"
#include "device/fido/p256_public_key.h"
#include "device/fido/public_key.h"

namespace device::fido::mac {

using base::ScopedCFTypeRef;
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
          static_cast<uint32_t>(base::Time::Now().ToDoubleT());
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
absl::optional<AttestedCredentialData> MakeAttestedCredentialData(
    std::vector<uint8_t> credential_id,
    std::unique_ptr<PublicKey> public_key) {
  if (credential_id.empty() || credential_id.size() > 255) {
    LOG(ERROR) << "invalid credential id: "
               << base::HexEncode(credential_id.data(), credential_id.size());
    return absl::nullopt;
  }
  if (!public_key) {
    LOG(ERROR) << "no public key";
    return absl::nullopt;
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
    absl::optional<AttestedCredentialData> attested_credential_data,
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

absl::optional<std::vector<uint8_t>> GenerateSignature(
    const AuthenticatorData& authenticator_data,
    base::span<const uint8_t, kClientDataHashLength> client_data_hash,
    SecKeyRef private_key) {
  const std::vector<uint8_t> serialized_authenticator_data =
      authenticator_data.SerializeToByteArray();
  size_t capacity =
      serialized_authenticator_data.size() + client_data_hash.size();
  ScopedCFTypeRef<CFMutableDataRef> sig_input(
      CFDataCreateMutable(kCFAllocatorDefault, capacity));
  CFDataAppendBytes(sig_input, serialized_authenticator_data.data(),
                    serialized_authenticator_data.size());
  CFDataAppendBytes(sig_input, client_data_hash.data(),
                    client_data_hash.size());
  ScopedCFTypeRef<CFErrorRef> err;
  ScopedCFTypeRef<CFDataRef> sig_data(
      Keychain::GetInstance().KeyCreateSignature(
          private_key, kSecKeyAlgorithmECDSASignatureMessageX962SHA256,
          sig_input, err.InitializeInto()));
  if (!sig_data) {
    LOG(ERROR) << "SecKeyCreateSignature failed: " << err;
    return absl::nullopt;
  }
  return std::vector<uint8_t>(
      CFDataGetBytePtr(sig_data),
      CFDataGetBytePtr(sig_data) + CFDataGetLength(sig_data));
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
    LOG(ERROR) << "SecCopyExternalRepresentation failed: " << err;
    return nullptr;
  }
  base::span<const uint8_t> key_data =
      base::make_span(CFDataGetBytePtr(data_ref),
                      base::checked_cast<size_t>(CFDataGetLength(data_ref)));
  auto key = P256PublicKey::ParseX962Uncompressed(
      static_cast<int32_t>(CoseAlgorithmIdentifier::kEs256), key_data);
  if (!key) {
    LOG(ERROR) << "Unexpected public key format: "
               << base::HexEncode(key_data.data(), key_data.size());
    return nullptr;
  }
  return key;
}

CodeSigningState ProcessIsSigned() {
  base::ScopedCFTypeRef<SecTaskRef> task(SecTaskCreateFromSelf(nullptr));
  if (!task) {
    return CodeSigningState::kNotSigned;
  }

  base::ScopedCFTypeRef<CFStringRef> sign_id(
      SecTaskCopySigningIdentifier(task.get(), /*error=*/nullptr));
  return static_cast<bool>(sign_id) ? CodeSigningState::kSigned
                                    : CodeSigningState::kNotSigned;
}

bool DeviceHasBiometricsAvailable() {
  LAContext* context = [[LAContext alloc] init];
  NSError* nserr;
  return
      [context canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics
                           error:&nserr];
}

}  // namespace device::fido::mac
