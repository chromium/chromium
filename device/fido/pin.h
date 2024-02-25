// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains structures to implement the CTAP2 PIN protocol, version
// one. See
// https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#authenticatorClientPIN

#ifndef DEVICE_FIDO_PIN_H_
#define DEVICE_FIDO_PIN_H_

#include <stdint.h>

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "components/cbor/values.h"
#include "device/fido/fido_constants.h"

namespace device {
namespace pin {

// The reason we are prompting for a new PIN.
enum class PINEntryReason {
  // Indicates a new PIN is being set.
  kSet,

  // The existing PIN must be changed before using this authenticator.
  kChange,

  // The existing PIN is being collected to prove user verification.
  kChallenge
};

// The errors that may prompt asking for a PIN.
enum class PINEntryError {
  // No error has occurred.
  kNoError,

  // Internal UV is locked, so we are falling back to PIN.
  kInternalUvLocked,

  // The PIN the user entered does not match the authenticator PIN.
  kWrongPIN,

  // The new PIN the user entered is too short.
  kTooShort,

  // The new PIN the user entered contains invalid characters.
  kInvalidCharacters,

  // The new PIN the user entered is the same as the currently set PIN.
  kSameAsCurrentPIN,
};

// Permission list flags. See
// https://drafts.fidoalliance.org/fido-2/stable-links-to-latest/fido-client-to-authenticator-protocol.html#permissions
enum class Permissions : uint8_t {
  kMakeCredential = 0x01,
  kGetAssertion = 0x02,
  kCredentialManagement = 0x04,
  kBioEnrollment = 0x08,
  kLargeBlobWrite = 0x10,
};

// Some commands that validate PinUvAuthTokens include this padding to ensure a
// PinUvAuthParam cannot be reused across different commands.
constexpr std::array<uint8_t, 32> kPinUvAuthTokenSafetyPadding = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

// Validates |pin|, returning |kNoError| if valid or an appropriate error code
// otherwise.
COMPONENT_EXPORT(DEVICE_FIDO)
PINEntryError ValidatePIN(
    const std::string& pin,
    uint32_t min_pin_length = kMinPinLength,
    std::optional<std::string> current_pin = std::nullopt);

// Like |ValidatePIN| above but takes a wide string.
COMPONENT_EXPORT(DEVICE_FIDO)
PINEntryError ValidatePIN(
    const std::u16string& pin16,
    uint32_t min_pin_length = kMinPinLength,
    std::optional<std::string> current_pin = std::nullopt);

// kMinBytes is the minimum number of *bytes* of PIN data that a CTAP2 device
// will accept. Since the PIN is UTF-8 encoded, this could be a single code
// point. However, the platform is supposed to additionally enforce a 4
// *character* minimum
constexpr size_t kMinBytes = 4;
// kMaxBytes is the maximum number of bytes of PIN data that a CTAP2 device will
// accept.
constexpr size_t kMaxBytes = 63;

// EncodeCOSEPublicKey converts an X9.62 public key to a COSE structure.
COMPONENT_EXPORT(DEVICE_FIDO)
cbor::Value::MapValue EncodeCOSEPublicKey(
    base::span<const uint8_t, kP256X962Length> x962);

// PinRetriesRequest asks an authenticator for the number of remaining PIN
// attempts before the device is locked.
struct PinRetriesRequest {
  PINUVAuthProtocol protocol;
};

// UVRetriesRequest asks an authenticator for the number of internal user
// verification attempts before the feature is locked.
struct UvRetriesRequest {
  PINUVAuthProtocol protocol;
};

// RetriesResponse reflects an authenticator's response to a |PinRetriesRequest|
// or a |UvRetriesRequest|.
struct RetriesResponse {
  static std::optional<RetriesResponse> ParsePinRetries(
      const std::optional<cbor::Value>& cbor);

  static std::optional<RetriesResponse> ParseUvRetries(
      const std::optional<cbor::Value>& cbor);

  // retries is the number of PIN attempts remaining before the authenticator
  // locks.
  int retries;

 private:
  static std::optional<RetriesResponse> Parse(
      const std::optional<cbor::Value>& cbor,
      const int retries_key);

  RetriesResponse();
};

// KeyAgreementRequest asks an authenticator for an ephemeral ECDH key for
// encrypting PIN material in future requests.
struct KeyAgreementRequest {
  PINUVAuthProtocol protocol;
};

// KeyAgreementResponse reflects an authenticator's response to a
// |KeyAgreementRequest| and is also used as representation of the
// authenticator's ephemeral key.
struct COMPONENT_EXPORT(DEVICE_FIDO) KeyAgreementResponse {
  static std::optional<KeyAgreementResponse> Parse(
      const std::optional<cbor::Value>& cbor);
  static std::optional<KeyAgreementResponse> ParseFromCOSE(
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
  SetRequest(PINUVAuthProtocol protocol,
             const std::string& pin,
             const KeyAgreementResponse& peer_key);

  friend std::pair<CtapRequestCommand, std::optional<cbor::Value>>
  AsCTAPRequestValuePair(const SetRequest&);

 private:
  const PINUVAuthProtocol protocol_;
  const KeyAgreementResponse peer_key_;
  uint8_t pin_[kMaxBytes + 1];
};

struct EmptyResponse {
  static std::optional<EmptyResponse> Parse(
      const std::optional<cbor::Value>& cbor);
};

// ChangeRequest changes the PIN on an authenticator that already has a PIN set.
// (This is distinct from setting an initial PIN.)
class ChangeRequest {
 public:
  // IsValid(new_pin) must be true.
  ChangeRequest(PINUVAuthProtocol protocol,
                const std::string& old_pin,
                const std::string& new_pin,
                const KeyAgreementResponse& peer_key);

  friend std::pair<CtapRequestCommand, std::optional<cbor::Value>>
  AsCTAPRequestValuePair(const ChangeRequest&);

 private:
  const PINUVAuthProtocol protocol_;
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
  const std::vector<uint8_t>& shared_key() const;

 protected:
  TokenRequest(TokenRequest&&);
  TokenRequest(PINUVAuthProtocol protocol,
               const KeyAgreementResponse& peer_key);
  ~TokenRequest();

  const PINUVAuthProtocol protocol_;
  std::vector<uint8_t> shared_key_;
  std::array<uint8_t, kP256X962Length> public_key_;
};

class PinTokenRequest : public TokenRequest {
 public:
  PinTokenRequest(PINUVAuthProtocol protocol,
                  const std::string& pin,
                  const KeyAgreementResponse& peer_key);
  PinTokenRequest(PinTokenRequest&&);
  PinTokenRequest(const PinTokenRequest&) = delete;
  virtual ~PinTokenRequest();

  friend std::pair<CtapRequestCommand, std::optional<cbor::Value>>
  AsCTAPRequestValuePair(const PinTokenRequest&);

 protected:
  uint8_t pin_hash_[16];
};

class PinTokenWithPermissionsRequest : public PinTokenRequest {
 public:
  PinTokenWithPermissionsRequest(PINUVAuthProtocol protocol,
                                 const std::string& pin,
                                 const KeyAgreementResponse& peer_key,
                                 base::span<const pin::Permissions> permissions,
                                 const std::optional<std::string> rp_id);
  PinTokenWithPermissionsRequest(PinTokenWithPermissionsRequest&&);
  PinTokenWithPermissionsRequest(const PinTokenWithPermissionsRequest&) =
      delete;
  ~PinTokenWithPermissionsRequest() override;

  friend std::pair<CtapRequestCommand, std::optional<cbor::Value>>
  AsCTAPRequestValuePair(const PinTokenWithPermissionsRequest&);

 private:
  uint8_t permissions_;
  std::optional<std::string> rp_id_;
};

class UvTokenRequest : public TokenRequest {
 public:
  UvTokenRequest(PINUVAuthProtocol protocol,
                 const KeyAgreementResponse& peer_key,
                 std::optional<std::string> rp_id,
                 base::span<const pin::Permissions> permissions);
  UvTokenRequest(UvTokenRequest&&);
  UvTokenRequest(const UvTokenRequest&) = delete;
  virtual ~UvTokenRequest();

  friend std::pair<CtapRequestCommand, std::optional<cbor::Value>>
  AsCTAPRequestValuePair(const UvTokenRequest&);

 private:
  std::optional<std::string> rp_id_;
  uint8_t permissions_;
};

class HMACSecretRequest {
 public:
  HMACSecretRequest(PINUVAuthProtocol protocol,
                    const KeyAgreementResponse& peer_key,
                    base::span<const uint8_t, 32> salt1,
                    const std::optional<std::array<uint8_t, 32>>& salt2);
  HMACSecretRequest(const HMACSecretRequest&);
  ~HMACSecretRequest();
  HMACSecretRequest& operator=(const HMACSecretRequest&);

  std::optional<std::vector<uint8_t>> Decrypt(
      base::span<const uint8_t> ciphertext);

 private:
  const PINUVAuthProtocol protocol_;
  const bool have_two_salts_;
  std::vector<uint8_t> shared_key_;

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
  TokenResponse& operator=(const TokenResponse&);

  static std::optional<TokenResponse> Parse(
      PINUVAuthProtocol protocol,
      base::span<const uint8_t> shared_key,
      const std::optional<cbor::Value>& cbor);

  std::pair<PINUVAuthProtocol, std::vector<uint8_t>> PinAuth(
      base::span<const uint8_t> client_data_hash) const;

  PINUVAuthProtocol protocol() const { return protocol_; }
  const std::vector<uint8_t>& token_for_testing() const { return token_; }

 private:
  explicit TokenResponse(PINUVAuthProtocol protocol);

  PINUVAuthProtocol protocol_;
  std::vector<uint8_t> token_;
};

std::pair<CtapRequestCommand, std::optional<cbor::Value>>
AsCTAPRequestValuePair(const PinRetriesRequest&);

std::pair<CtapRequestCommand, std::optional<cbor::Value>>
AsCTAPRequestValuePair(const UvRetriesRequest&);

std::pair<CtapRequestCommand, std::optional<cbor::Value>>
AsCTAPRequestValuePair(const KeyAgreementRequest&);

std::pair<CtapRequestCommand, std::optional<cbor::Value>>
AsCTAPRequestValuePair(const SetRequest&);

std::pair<CtapRequestCommand, std::optional<cbor::Value>>
AsCTAPRequestValuePair(const ChangeRequest&);

std::pair<CtapRequestCommand, std::optional<cbor::Value>>
AsCTAPRequestValuePair(const ResetRequest&);

std::pair<CtapRequestCommand, std::optional<cbor::Value>>
AsCTAPRequestValuePair(const TokenRequest&);

}  // namespace pin

}  // namespace device

#endif  // DEVICE_FIDO_PIN_H_
