// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains structures to implement the CTAP2 PIN protocol, version
// one. See
// https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#authenticatorClientPIN

#ifndef DEVICE_FIDO_PIN_H_
#define DEVICE_FIDO_PIN_H_

#include <stdint.h>

#include <array>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/optional.h"
#include "components/cbor/values.h"
#include "device/fido/fido_constants.h"

namespace device {
namespace pin {

// Permission list flags. See
// https://drafts.fidoalliance.org/fido-2/stable-links-to-latest/fido-client-to-authenticator-protocol.html#permissions
enum class Permissions : uint8_t {
  kMakeCredential = 0x01,
  kGetAssertion = 0x02,
  kCredentialManagement = 0x04,
  kBioEnrollment = 0x08,
  kLargeBlobWrite = 0x10,
};

// kProtocolVersion is the version of the PIN protocol that this code
// implements.
constexpr int kProtocolVersion = 1;

// Some commands that validate PinUvAuthTokens include this padding to ensure a
// PinUvAuthParam cannot be reused across different commands.
constexpr std::array<uint8_t, 32> kPinUvAuthTokenSafetyPadding = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

// IsValid returns true if |pin|, which must be UTF-8, is a syntactically valid
// PIN.
COMPONENT_EXPORT(DEVICE_FIDO) bool IsValid(const std::string& pin);

// kMinBytes is the minimum number of *bytes* of PIN data that a CTAP2 device
// will accept. Since the PIN is UTF-8 encoded, this could be a single code
// point. However, the platform is supposed to additionally enforce a 4
// *character* minimum
constexpr size_t kMinBytes = 4;
// kMaxBytes is the maximum number of bytes of PIN data that a CTAP2 device will
// accept.
constexpr size_t kMaxBytes = 63;

// EncodeCOSEPublicKey converts an X9.62 public key to a COSE structure.
cbor::Value::MapValue EncodeCOSEPublicKey(
    base::span<const uint8_t, kP256X962Length> x962);

// PinRetriesRequest asks an authenticator for the number of remaining PIN
// attempts before the device is locked.
struct PinRetriesRequest {};

// UVRetriesRequest asks an authenticator for the number of internal user
// verification attempts before the feature is locked.
struct UvRetriesRequest {};

// RetriesResponse reflects an authenticator's response to a |PinRetriesRequest|
// or a |UvRetriesRequest|.
struct RetriesResponse {
  static base::Optional<RetriesResponse> ParsePinRetries(
      const base::Optional<cbor::Value>& cbor);

  static base::Optional<RetriesResponse> ParseUvRetries(
      const base::Optional<cbor::Value>& cbor);

  // retries is the number of PIN attempts remaining before the authenticator
  // locks.
  int retries;

 private:
  static base::Optional<RetriesResponse> Parse(
      const base::Optional<cbor::Value>& cbor,
      const int retries_key);

  RetriesResponse();
};

// KeyAgreementRequest asks an authenticator for an ephemeral ECDH key for
// encrypting PIN material in future requests.
struct KeyAgreementRequest {};

// KeyAgreementResponse reflects an authenticator's response to a
// |KeyAgreementRequest| and is also used as representation of the
// authenticator's ephemeral key.
struct KeyAgreementResponse {
  static base::Optional<KeyAgreementResponse> Parse(
      const base::Optional<cbor::Value>& cbor);
  static base::Optional<KeyAgreementResponse> ParseFromCOSE(
      const cbor::Value::MapValue& cose_key);

  // X962 returns the public key from the response in X9.62 form.
  std::array<uint8_t, kP256X962Length> X962() const;

  // x and y contain the big-endian coordinates of a P-256 point. It is ensured
  // that this is a valid point on the curve.
  uint8_t x[32], y[32];

 private:
  KeyAgreementResponse();
};

// SetRequest sets an initial PIN on an authenticator. (This is distinct from
// changing a PIN.)
class SetRequest {
 public:
  // IsValid(pin) must be true.
  SetRequest(const std::string& pin, const KeyAgreementResponse& peer_key);

  friend std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
  AsCTAPRequestValuePair(const SetRequest&);

 private:
  const KeyAgreementResponse peer_key_;
  uint8_t pin_[kMaxBytes + 1];
};

struct EmptyResponse {
  static base::Optional<EmptyResponse> Parse(
      const base::Optional<cbor::Value>& cbor);
};

// ChangeRequest changes the PIN on an authenticator that already has a PIN set.
// (This is distinct from setting an initial PIN.)
class ChangeRequest {
 public:
  // IsValid(new_pin) must be true.
  ChangeRequest(const std::string& old_pin,
                const std::string& new_pin,
                const KeyAgreementResponse& peer_key);

  friend std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
  AsCTAPRequestValuePair(const ChangeRequest&);

 private:
  const KeyAgreementResponse peer_key_;
  uint8_t old_pin_hash_[16];
  uint8_t new_pin_[kMaxBytes + 1];
};

// ResetRequest resets an authenticator, which should invalidate all
// credentials and clear any configured PIN. This is not strictly a
// PIN-related command, but is generally used to reset a PIN and so is
// included here.
struct ResetRequest {};

using ResetResponse = EmptyResponse;

// TokenRequest requests a pin-token from an authenticator. These tokens can be
// used to show user-verification in other operations, e.g. when getting an
// assertion.
class TokenRequest {
 public:
  TokenRequest(const TokenRequest&) = delete;

  // shared_key returns the shared ECDH key that was used to encrypt the PIN.
  // This is needed to decrypt the response.
  const std::array<uint8_t, 32>& shared_key() const;

 protected:
  TokenRequest(TokenRequest&&);
  explicit TokenRequest(const KeyAgreementResponse& peer_key);
  ~TokenRequest();
  std::array<uint8_t, 32> shared_key_;
  std::array<uint8_t, kP256X962Length> public_key_;
};

class PinTokenRequest : public TokenRequest {
 public:
  PinTokenRequest(const std::string& pin, const KeyAgreementResponse& peer_key);
  PinTokenRequest(PinTokenRequest&&);
  PinTokenRequest(const PinTokenRequest&) = delete;
  virtual ~PinTokenRequest();

  friend std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
  AsCTAPRequestValuePair(const PinTokenRequest&);

 protected:
  uint8_t pin_hash_[16];
};

class PinTokenWithPermissionsRequest : public PinTokenRequest {
 public:
  PinTokenWithPermissionsRequest(const std::string& pin,
                                 const KeyAgreementResponse& peer_key,
                                 const uint8_t permissions,
                                 const base::Optional<std::string> rp_id);
  PinTokenWithPermissionsRequest(PinTokenWithPermissionsRequest&&);
  PinTokenWithPermissionsRequest(const PinTokenWithPermissionsRequest&) =
      delete;
  ~PinTokenWithPermissionsRequest() override;

  friend std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
  AsCTAPRequestValuePair(const PinTokenWithPermissionsRequest&);

 private:
  uint8_t permissions_;
  base::Optional<std::string> rp_id_;
};

class UvTokenRequest : public TokenRequest {
 public:
  UvTokenRequest(const KeyAgreementResponse& peer_key,
                 base::Optional<std::string> rp_id);
  UvTokenRequest(UvTokenRequest&&);
  UvTokenRequest(const UvTokenRequest&) = delete;
  virtual ~UvTokenRequest();

  friend std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
  AsCTAPRequestValuePair(const UvTokenRequest&);

 private:
  base::Optional<std::string> rp_id_;
};

class HMACSecretRequest {
 public:
  HMACSecretRequest(const KeyAgreementResponse& peer_key,
                    base::span<const uint8_t, 32> salt1,
                    const base::Optional<std::array<uint8_t, 32>>& salt2);
  HMACSecretRequest(const HMACSecretRequest&);
  ~HMACSecretRequest();
  HMACSecretRequest& operator=(const HMACSecretRequest&);

  base::Optional<std::vector<uint8_t>> Decrypt(
      base::span<const uint8_t> ciphertext);

 private:
  std::array<uint8_t, 32> shared_key_ = {};

 public:
  const std::array<uint8_t, kP256X962Length> public_key_x962;
  const std::vector<uint8_t> encrypted_salts;
  const std::vector<uint8_t> salts_auth;
};

// TokenResponse represents the response to a pin-token request. In order to
// decrypt a response, the shared key from the request is needed. Once a pin-
// token has been decrypted, it can be used to calculate the pinAuth parameters
// needed to show user-verification in future operations.
class COMPONENT_EXPORT(DEVICE_FIDO) TokenResponse {
 public:
  ~TokenResponse();
  TokenResponse(const TokenResponse&);

  static base::Optional<TokenResponse> Parse(
      std::array<uint8_t, 32> shared_key,
      const base::Optional<cbor::Value>& cbor);

  // PinAuth returns a pinAuth parameter for a request that will use the given
  // client-data hash.
  std::vector<uint8_t> PinAuth(
      base::span<const uint8_t> client_data_hash) const;

  const std::vector<uint8_t>& token() const { return token_; }

 private:
  TokenResponse();

  std::vector<uint8_t> token_;
};

std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const PinRetriesRequest&);

std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const UvRetriesRequest&);

std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const KeyAgreementRequest&);

std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const SetRequest&);

std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const ChangeRequest&);

std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const ResetRequest&);

std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const TokenRequest&);

}  // namespace pin

}  // namespace device

#endif  // DEVICE_FIDO_PIN_H_
