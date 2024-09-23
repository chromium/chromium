// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/fido/pin.h"

#include <numeric>
#include <string>
#include <utility>

#include "base/i18n/char_iterator.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "device/fido/fido_constants.h"
#include "device/fido/pin_internal.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace device {
namespace pin {

namespace {

uint8_t PermissionsToByte(base::span<const pin::Permissions> permissions) {
  return std::accumulate(permissions.begin(), permissions.end(), 0,
                         [](uint8_t byte, pin::Permissions flag) {
                           return byte |= static_cast<uint8_t>(flag);
                         });
}

}  // namespace

// HasAtLeastFourCodepoints returns true if |pin| is UTF-8 encoded and contains
// four or more code points. This reflects the "4 Unicode characters"
// requirement in CTAP2.
static bool HasAtLeastFourCodepoints(const std::string& pin) {
  base::i18n::UTF8CharIterator it(pin);
  return it.Advance() && it.Advance() && it.Advance() && it.Advance();
}

PINEntryError ValidatePIN(const std::string& pin,
                          uint32_t min_pin_length,
                          std::optional<std::string> current_pin) {
  if (pin.size() < min_pin_length) {
    return PINEntryError::kTooShort;
  }
  if (pin.size() > kMaxBytes || pin.back() == 0 || !base::IsStringUTF8(pin)) {
    return PINEntryError::kInvalidCharacters;
  }
  if (!HasAtLeastFourCodepoints(pin)) {
    return PINEntryError::kTooShort;
  }
  if (pin == current_pin) {
    return pin::PINEntryError::kSameAsCurrentPIN;
  }
  return PINEntryError::kNoError;
}

PINEntryError ValidatePIN(const std::u16string& pin16,
                          uint32_t min_pin_length,
                          std::optional<std::string> current_pin) {
  std::string pin;
  if (!base::UTF16ToUTF8(pin16.c_str(), pin16.size(), &pin)) {
    return pin::PINEntryError::kInvalidCharacters;
  }
  return ValidatePIN(std::move(pin), min_pin_length, std::move(current_pin));
}

// EncodePINCommand returns a CTAP2 PIN command for the operation |subcommand|.
// Additional elements of the top-level CBOR map can be added with the optional
// |add_additional| callback.
static std::pair<CtapRequestCommand, std::optional<cbor::Value>>
EncodePINCommand(
    PINUVAuthProtocol protocol_version,
    Subcommand subcommand,
    std::function<void(cbor::Value::MapValue*)> add_additional = nullptr) {
  cbor::Value::MapValue map;
  map.emplace(static_cast<int>(RequestKey::kProtocol),
              static_cast<uint8_t>(protocol_version));
  map.emplace(static_cast<int>(RequestKey::kSubcommand),
              static_cast<int>(subcommand));

  if (add_additional) {
    add_additional(&map);
  }

  return std::make_pair(CtapRequestCommand::kAuthenticatorClientPin,
                        cbor::Value(std::move(map)));
}

RetriesResponse::RetriesResponse() = default;

// static
std::optional<RetriesResponse> RetriesResponse::ParsePinRetries(
    const std::optional<cbor::Value>& cbor) {
  return RetriesResponse::Parse(std::move(cbor),
                                static_cast<int>(ResponseKey::kRetries));
}

// static
std::optional<RetriesResponse> RetriesResponse::ParseUvRetries(
    const std::optional<cbor::Value>& cbor) {
  return RetriesResponse::Parse(std::move(cbor),
                                static_cast<int>(ResponseKey::kUvRetries));
}

// static
std::optional<RetriesResponse> RetriesResponse::Parse(
    const std::optional<cbor::Value>& cbor,
    const int retries_key) {
  if (!cbor || !cbor->is_map()) {
    return std::nullopt;
  }
  const auto& response_map = cbor->GetMap();

  auto it = response_map.find(cbor::Value(retries_key));
  if (it == response_map.end() || !it->second.is_unsigned()) {
    return std::nullopt;
  }

  const int64_t retries = it->second.GetUnsigned();
  if (retries > INT_MAX) {
    return std::nullopt;
  }

  RetriesResponse ret;
  ret.retries = static_cast<int>(retries);
  return ret;
}

KeyAgreementResponse::KeyAgreementResponse() = default;

// static
std::optional<KeyAgreementResponse> KeyAgreementResponse::Parse(
    const std::optional<cbor::Value>& cbor) {
  if (!cbor || !cbor->is_map()) {
    return std::nullopt;
  }
  const auto& response_map = cbor->GetMap();

  // The ephemeral key is encoded as a COSE structure.
  auto it = response_map.find(
      cbor::Value(static_cast<int>(ResponseKey::kKeyAgreement)));
  if (it == response_map.end() || !it->second.is_map()) {
    return std::nullopt;
  }
  const auto& cose_key = it->second.GetMap();

  return ParseFromCOSE(cose_key);
}

// static
std::optional<KeyAgreementResponse> KeyAgreementResponse::ParseFromCOSE(
    const cbor::Value::MapValue& cose_key) {
  // The COSE key must be a P-256 point. See
  // https://tools.ietf.org/html/rfc8152#section-7.1
  for (const auto& pair : std::vector<std::pair<int, int>>({
           {1 /* key type */, 2 /* elliptic curve, uncompressed */},
           {3 /* algorithm */, -25 /* ECDH, ephemeral–static, HKDF-SHA-256 */},
           {-1 /* curve */, 1 /* P-256 */},
       })) {
    auto it = cose_key.find(cbor::Value(pair.first));
    if (it == cose_key.end() || !it->second.is_integer() ||
        it->second.GetInteger() != pair.second) {
      return std::nullopt;
    }
  }

  // See https://tools.ietf.org/html/rfc8152#section-13.1.1
  const auto& x_it = cose_key.find(cbor::Value(-2));
  const auto& y_it = cose_key.find(cbor::Value(-3));
  if (x_it == cose_key.end() || y_it == cose_key.end() ||
      !x_it->second.is_bytestring() || !y_it->second.is_bytestring()) {
    return std::nullopt;
  }

  const auto& x = x_it->second.GetBytestring();
  const auto& y = y_it->second.GetBytestring();
  KeyAgreementResponse ret;
  if (x.size() != sizeof(ret.x) || y.size() != sizeof(ret.y)) {
    return std::nullopt;
  }
  memcpy(ret.x, x.data(), sizeof(ret.x));
  memcpy(ret.y, y.data(), sizeof(ret.y));

  bssl::UniquePtr<EC_GROUP> group(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));

  // Check that the point is on the curve.
  auto point = PointFromKeyAgreementResponse(group.get(), ret);
  if (!point) {
    return std::nullopt;
  }

  return ret;
}

std::array<uint8_t, kP256X962Length> KeyAgreementResponse::X962() const {
  std::array<uint8_t, kP256X962Length> ret;
  static_assert(ret.size() == 1 + sizeof(this->x) + sizeof(this->y),
                "Bad length for return type");
  ret[0] = POINT_CONVERSION_UNCOMPRESSED;
  memcpy(&ret[1], this->x, sizeof(this->x));
  memcpy(&ret[1 + sizeof(this->x)], this->y, sizeof(this->y));
  return ret;
}

SetRequest::SetRequest(PINUVAuthProtocol protocol,
                       const std::string& pin,
                       const KeyAgreementResponse& peer_key)
    : protocol_(protocol), peer_key_(peer_key) {
  DCHECK_EQ(ValidatePIN(pin), PINEntryError::kNoError);
  memset(pin_, 0, sizeof(pin_));
  memcpy(pin_, pin.data(), pin.size());
}

cbor::Value::MapValue EncodeCOSEPublicKey(
    base::span<const uint8_t, kP256X962Length> x962) {
  cbor::Value::MapValue cose_key;
  cose_key.emplace(1 /* key type */, 2 /* uncompressed elliptic curve */);
  cose_key.emplace(3 /* algorithm */,
                   -25 /* ECDH, ephemeral–static, HKDF-SHA-256 */);
  cose_key.emplace(-1 /* curve */, 1 /* P-256 */);
  cose_key.emplace(-2 /* x */, x962.subspan(1, 32));
  cose_key.emplace(-3 /* y */, x962.subspan(33, 32));

  return cose_key;
}

ChangeRequest::ChangeRequest(PINUVAuthProtocol protocol,
                             const std::string& old_pin,
                             const std::string& new_pin,
                             const KeyAgreementResponse& peer_key)
    : protocol_(protocol), peer_key_(peer_key) {
  uint8_t digest[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const uint8_t*>(old_pin.data()), old_pin.size(),
         digest);
  memcpy(old_pin_hash_, digest, sizeof(old_pin_hash_));

  DCHECK_EQ(ValidatePIN(new_pin), PINEntryError::kNoError);
  memset(new_pin_, 0, sizeof(new_pin_));
  memcpy(new_pin_, new_pin.data(), new_pin.size());
}

// static
std::optional<EmptyResponse> EmptyResponse::Parse(
    const std::optional<cbor::Value>& cbor) {
  // Yubikeys can return just the status byte, and no CBOR bytes, for the empty
  // response, which will end up here with |cbor| being |nullopt|. This seems
  // wrong, but is handled. (The response should, instead, encode an empty CBOR
  // map.)
  if (cbor && (!cbor->is_map() || !cbor->GetMap().empty())) {
    return std::nullopt;
  }

  EmptyResponse ret;
  return ret;
}

TokenResponse::TokenResponse(PINUVAuthProtocol protocol)
    : protocol_(protocol) {}
TokenResponse::~TokenResponse() = default;
TokenResponse::TokenResponse(const TokenResponse&) = default;
TokenResponse& TokenResponse::operator=(const TokenResponse&) = default;

std::optional<TokenResponse> TokenResponse::Parse(
    PINUVAuthProtocol protocol,
    base::span<const uint8_t> shared_key,
    const std::optional<cbor::Value>& cbor) {
  if (!cbor || !cbor->is_map()) {
    return std::nullopt;
  }
  const auto& response_map = cbor->GetMap();

  auto it =
      response_map.find(cbor::Value(static_cast<int>(ResponseKey::kPINToken)));
  if (it == response_map.end() || !it->second.is_bytestring()) {
    return std::nullopt;
  }
  const auto& encrypted_token = it->second.GetBytestring();
  if (encrypted_token.size() % AES_BLOCK_SIZE != 0) {
    return std::nullopt;
  }

  std::vector<uint8_t> token =
      ProtocolVersion(protocol).Decrypt(shared_key, encrypted_token);

  // The token must have the correct size for the given protocol.
  switch (protocol) {
    case PINUVAuthProtocol::kV1:
      // In CTAP2.1, V1 tokens are fixed at 16 or 32 bytes. But in CTAP2.0 they
      // may be any multiple of 16 bytes. We don't know the CTAP version, so
      // only enforce the latter.
      if (token.empty() || token.size() % AES_BLOCK_SIZE != 0) {
        return std::nullopt;
      }
      break;
    case PINUVAuthProtocol::kV2:
      if (token.size() != 32u) {
        return std::nullopt;
      }
      break;
  }

  TokenResponse ret(protocol);
  ret.token_ = std::move(token);
  return ret;
}

std::pair<PINUVAuthProtocol, std::vector<uint8_t>> TokenResponse::PinAuth(
    base::span<const uint8_t> client_data_hash) const {
  return {protocol_,
          ProtocolVersion(protocol_).Authenticate(token_, client_data_hash)};
}

// static
std::pair<CtapRequestCommand, std::optional<cbor::Value>>
AsCTAPRequestValuePair(const PinRetriesRequest& request) {
  return EncodePINCommand(request.protocol, Subcommand::kGetRetries);
}

// static
std::pair<CtapRequestCommand, std::optional<cbor::Value>>
AsCTAPRequestValuePair(const UvRetriesRequest& request) {
  return EncodePINCommand(request.protocol, Subcommand::kGetUvRetries);
}

// static
std::pair<CtapRequestCommand, std::optional<cbor::Value>>
AsCTAPRequestValuePair(const KeyAgreementRequest& request) {
  return EncodePINCommand(request.protocol, Subcommand::kGetKeyAgreement);
}

// static
std::pair<CtapRequestCommand, std::optional<cbor::Value>>
AsCTAPRequestValuePair(const SetRequest& request) {
  // See
  // https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#settingNewPin
  std::vector<uint8_t> shared_key;
  const Protocol& pin_protocol = ProtocolVersion(request.protocol_);
  auto cose_key = EncodeCOSEPublicKey(
      pin_protocol.Encapsulate(request.peer_key_, &shared_key));

  static_assert((sizeof(request.pin_) % AES_BLOCK_SIZE) == 0,
                "pin_ is not a multiple of the AES block size");
  std::vector<uint8_t> encrypted_pin =
      pin_protocol.Encrypt(shared_key, request.pin_);

  std::vector<uint8_t> pin_auth =
      pin_protocol.Authenticate(shared_key, encrypted_pin);

  return EncodePINCommand(
      request.protocol_, Subcommand::kSetPIN,
      [&cose_key, &encrypted_pin, &pin_auth](cbor::Value::MapValue* map) {
        map->emplace(static_cast<int>(RequestKey::kKeyAgreement),
                     std::move(cose_key));
        map->emplace(static_cast<int>(RequestKey::kNewPINEnc),
                     std::move(encrypted_pin));
        map->emplace(static_cast<int>(RequestKey::kPINAuth),
                     std::move(pin_auth));
      });
}

// static
std::pair<CtapRequestCommand, std::optional<cbor::Value>>
AsCTAPRequestValuePair(const ChangeRequest& request) {
  // See
  // https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#changingExistingPin
  std::vector<uint8_t> shared_key;
  const Protocol& pin_protocol = ProtocolVersion(request.protocol_);
  auto cose_key = EncodeCOSEPublicKey(
      pin_protocol.Encapsulate(request.peer_key_, &shared_key));

  static_assert((sizeof(request.new_pin_) % AES_BLOCK_SIZE) == 0,
                "new_pin_ is not a multiple of the AES block size");
  std::vector<uint8_t> encrypted_pin =
      pin_protocol.Encrypt(shared_key, request.new_pin_);

  static_assert((sizeof(request.old_pin_hash_) % AES_BLOCK_SIZE) == 0,
                "old_pin_hash_ is not a multiple of the AES block size");
  std::vector<uint8_t> old_pin_hash_enc =
      pin_protocol.Encrypt(shared_key, request.old_pin_hash_);

  std::vector<uint8_t> ciphertexts_concat(encrypted_pin.size() +
                                          old_pin_hash_enc.size());
  memcpy(ciphertexts_concat.data(), encrypted_pin.data(), encrypted_pin.size());
  memcpy(ciphertexts_concat.data() + encrypted_pin.size(),
         old_pin_hash_enc.data(), old_pin_hash_enc.size());

  std::vector<uint8_t> pin_auth =
      pin_protocol.Authenticate(shared_key, ciphertexts_concat);

  return EncodePINCommand(
      request.protocol_, Subcommand::kChangePIN,
      [&cose_key, &encrypted_pin, &old_pin_hash_enc,
       &pin_auth](cbor::Value::MapValue* map) {
        map->emplace(static_cast<int>(RequestKey::kKeyAgreement),
                     std::move(cose_key));
        map->emplace(static_cast<int>(RequestKey::kPINHashEnc),
                     std::move(old_pin_hash_enc));
        map->emplace(static_cast<int>(RequestKey::kNewPINEnc),
                     std::move(encrypted_pin));
        map->emplace(static_cast<int>(RequestKey::kPINAuth),
                     std::move(pin_auth));
      });
}

// static
std::pair<CtapRequestCommand, std::optional<cbor::Value>>
AsCTAPRequestValuePair(const ResetRequest&) {
  return std::make_pair(CtapRequestCommand::kAuthenticatorReset, std::nullopt);
}

TokenRequest::TokenRequest(PINUVAuthProtocol protocol,
                           const KeyAgreementResponse& peer_key)
    : protocol_(protocol),
      public_key_(
          ProtocolVersion(protocol_).Encapsulate(peer_key, &shared_key_)) {}

TokenRequest::~TokenRequest() = default;

TokenRequest::TokenRequest(TokenRequest&& other) = default;

const std::vector<uint8_t>& TokenRequest::shared_key() const {
  return shared_key_;
}

PinTokenRequest::PinTokenRequest(PINUVAuthProtocol protocol,
                                 const std::string& pin,
                                 const KeyAgreementResponse& peer_key)
    : TokenRequest(protocol, peer_key) {
  uint8_t digest[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const uint8_t*>(pin.data()), pin.size(), digest);
  memcpy(pin_hash_, digest, sizeof(pin_hash_));
}

PinTokenRequest::~PinTokenRequest() = default;

PinTokenRequest::PinTokenRequest(PinTokenRequest&& other) = default;

// static
std::pair<CtapRequestCommand, std::optional<cbor::Value>>
AsCTAPRequestValuePair(const PinTokenRequest& request) {
  static_assert((sizeof(request.pin_hash_) % AES_BLOCK_SIZE) == 0,
                "pin_hash_ is not a multiple of the AES block size");
  std::vector<uint8_t> encrypted_pin =
      ProtocolVersion(request.protocol_)
          .Encrypt(request.shared_key_, request.pin_hash_);

  return EncodePINCommand(
      request.protocol_, Subcommand::kGetPINToken,
      [&request, &encrypted_pin](cbor::Value::MapValue* map) {
        map->emplace(static_cast<int>(RequestKey::kKeyAgreement),
                     EncodeCOSEPublicKey(request.public_key_));
        map->emplace(static_cast<int>(RequestKey::kPINHashEnc),
                     std::move(encrypted_pin));
      });
}

PinTokenWithPermissionsRequest::PinTokenWithPermissionsRequest(
    PINUVAuthProtocol protocol,
    const std::string& pin,
    const KeyAgreementResponse& peer_key,
    base::span<const pin::Permissions> permissions,
    const std::optional<std::string> rp_id)
    : PinTokenRequest(protocol, pin, peer_key),
      permissions_(PermissionsToByte(permissions)),
      rp_id_(rp_id) {}

// static
std::pair<CtapRequestCommand, std::optional<cbor::Value>>
AsCTAPRequestValuePair(const PinTokenWithPermissionsRequest& request) {
  std::vector<uint8_t> encrypted_pin =
      ProtocolVersion(request.protocol_)
          .Encrypt(request.shared_key_, request.pin_hash_);

  return EncodePINCommand(
      request.protocol_, Subcommand::kGetPinUvAuthTokenUsingPinWithPermissions,
      [&request, &encrypted_pin](cbor::Value::MapValue* map) {
        map->emplace(static_cast<int>(RequestKey::kKeyAgreement),
                     EncodeCOSEPublicKey(request.public_key_));
        map->emplace(static_cast<int>(RequestKey::kPINHashEnc),
                     std::move(encrypted_pin));
        map->emplace(static_cast<int>(RequestKey::kPermissions),
                     std::move(request.permissions_));
        if (request.rp_id_) {
          map->emplace(static_cast<int>(RequestKey::kPermissionsRPID),
                       *request.rp_id_);
        }
      });
}

PinTokenWithPermissionsRequest::~PinTokenWithPermissionsRequest() = default;

PinTokenWithPermissionsRequest::PinTokenWithPermissionsRequest(
    PinTokenWithPermissionsRequest&& other) = default;

UvTokenRequest::UvTokenRequest(PINUVAuthProtocol protocol,
                               const KeyAgreementResponse& peer_key,
                               std::optional<std::string> rp_id,
                               base::span<const pin::Permissions> permissions)
    : TokenRequest(protocol, peer_key),
      rp_id_(rp_id),
      permissions_(PermissionsToByte(permissions)) {}

UvTokenRequest::~UvTokenRequest() = default;

UvTokenRequest::UvTokenRequest(UvTokenRequest&& other) = default;

// static
std::pair<CtapRequestCommand, std::optional<cbor::Value>>
AsCTAPRequestValuePair(const UvTokenRequest& request) {
  return EncodePINCommand(
      request.protocol_, Subcommand::kGetUvToken,
      [&request](cbor::Value::MapValue* map) {
        map->emplace(static_cast<int>(RequestKey::kKeyAgreement),
                     EncodeCOSEPublicKey(request.public_key_));
        map->emplace(static_cast<int>(RequestKey::kPermissions),
                     request.permissions_);
        if (request.rp_id_) {
          map->emplace(static_cast<int>(RequestKey::kPermissionsRPID),
                       *request.rp_id_);
        }
      });
}

static std::vector<uint8_t> ConcatSalts(
    base::span<const uint8_t, 32> salt1,
    const std::optional<std::array<uint8_t, 32>>& salt2) {
  const size_t salts_size =
      salt1.size() + (salt2.has_value() ? salt2->size() : 0);
  std::vector<uint8_t> salts(salts_size);

  memcpy(salts.data(), salt1.data(), salt1.size());
  if (salt2.has_value()) {
    memcpy(salts.data() + salt1.size(), salt2->data(), salt2->size());
  }

  return salts;
}

HMACSecretRequest::HMACSecretRequest(
    PINUVAuthProtocol protocol,
    const KeyAgreementResponse& peer_key,
    base::span<const uint8_t, 32> salt1,
    const std::optional<std::array<uint8_t, 32>>& salt2)
    : protocol_(protocol),
      have_two_salts_(salt2.has_value()),
      public_key_x962(
          ProtocolVersion(protocol_).Encapsulate(peer_key, &shared_key_)),
      encrypted_salts(
          ProtocolVersion(protocol_).Encrypt(shared_key_,
                                             ConcatSalts(salt1, salt2))),
      salts_auth(ProtocolVersion(protocol_).Authenticate(shared_key_,
                                                         encrypted_salts)) {}

HMACSecretRequest::~HMACSecretRequest() = default;

HMACSecretRequest::HMACSecretRequest(const HMACSecretRequest& other) = default;

std::optional<std::vector<uint8_t>> HMACSecretRequest::Decrypt(
    base::span<const uint8_t> ciphertext) {
  const std::optional<std::vector<uint8_t>> plaintext =
      pin::ProtocolVersion(protocol_).Decrypt(shared_key_, ciphertext);

  const unsigned num_salts = have_two_salts_ ? 2 : 1;
  if (plaintext && plaintext->size() != 32 * num_salts) {
    return std::nullopt;
  }

  return plaintext;
}

}  // namespace pin
}  // namespace device
