// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/pin.h"

#include <string>
#include <utility>

#include "base/i18n/char_iterator.h"
#include "base/strings/string_util.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "device/fido/fido_constants.h"
#include "device/fido/pin_internal.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdh.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/hmac.h"
#include "third_party/boringssl/src/include/openssl/obj.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace device {
namespace pin {

// HasAtLeastFourCodepoints returns true if |pin| is UTF-8 encoded and contains
// four or more code points. This reflects the "4 Unicode characters"
// requirement in CTAP2.
static bool HasAtLeastFourCodepoints(const std::string& pin) {
  base::i18n::UTF8CharIterator it(&pin);
  return it.Advance() && it.Advance() && it.Advance() && it.Advance();
}

// MakePinAuth returns `LEFT(HMAC-SHA-256(secret, data), 16)`.
std::vector<uint8_t> MakePinAuth(base::span<const uint8_t> secret,
                                 base::span<const uint8_t> data) {
  std::vector<uint8_t> pin_auth;
  pin_auth.resize(SHA256_DIGEST_LENGTH);
  unsigned hmac_bytes;
  CHECK(HMAC(EVP_sha256(), secret.data(), secret.size(), data.data(),
             data.size(), pin_auth.data(), &hmac_bytes));
  DCHECK_EQ(pin_auth.size(), static_cast<size_t>(hmac_bytes));
  pin_auth.resize(16);
  return pin_auth;
}

bool IsValid(const std::string& pin) {
  return pin.size() >= kMinBytes && pin.size() <= kMaxBytes &&
         pin.back() != 0 && base::IsStringUTF8(pin) &&
         HasAtLeastFourCodepoints(pin);
}

// EncodePINCommand returns a CTAP2 PIN command for the operation |subcommand|.
// Additional elements of the top-level CBOR map can be added with the optional
// |add_additional| callback.
static std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
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
base::Optional<RetriesResponse> RetriesResponse::ParsePinRetries(
    const base::Optional<cbor::Value>& cbor) {
  return RetriesResponse::Parse(std::move(cbor),
                                static_cast<int>(ResponseKey::kRetries));
}

// static
base::Optional<RetriesResponse> RetriesResponse::ParseUvRetries(
    const base::Optional<cbor::Value>& cbor) {
  return RetriesResponse::Parse(std::move(cbor),
                                static_cast<int>(ResponseKey::kUvRetries));
}

// static
base::Optional<RetriesResponse> RetriesResponse::Parse(
    const base::Optional<cbor::Value>& cbor,
    const int retries_key) {
  if (!cbor || !cbor->is_map()) {
    return base::nullopt;
  }
  const auto& response_map = cbor->GetMap();

  auto it = response_map.find(cbor::Value(retries_key));
  if (it == response_map.end() || !it->second.is_unsigned()) {
    return base::nullopt;
  }

  const int64_t retries = it->second.GetUnsigned();
  if (retries > INT_MAX) {
    return base::nullopt;
  }

  RetriesResponse ret;
  ret.retries = static_cast<int>(retries);
  return ret;
}

KeyAgreementResponse::KeyAgreementResponse() = default;

// PointFromKeyAgreementResponse returns an |EC_POINT| that represents the same
// P-256 point as |response|. It returns |nullopt| if |response| encodes an
// invalid point.
base::Optional<bssl::UniquePtr<EC_POINT>> PointFromKeyAgreementResponse(
    const EC_GROUP* group,
    const KeyAgreementResponse& response) {
  bssl::UniquePtr<EC_POINT> ret(EC_POINT_new(group));

  bssl::UniquePtr<BIGNUM> x_bn(BN_new()), y_bn(BN_new());
  BN_bin2bn(response.x, sizeof(response.x), x_bn.get());
  BN_bin2bn(response.y, sizeof(response.y), y_bn.get());
  const bool on_curve =
      EC_POINT_set_affine_coordinates_GFp(group, ret.get(), x_bn.get(),
                                          y_bn.get(), nullptr /* ctx */) == 1;

  if (!on_curve) {
    return base::nullopt;
  }

  return ret;
}

// static
base::Optional<KeyAgreementResponse> KeyAgreementResponse::Parse(
    const base::Optional<cbor::Value>& cbor) {
  if (!cbor || !cbor->is_map()) {
    return base::nullopt;
  }
  const auto& response_map = cbor->GetMap();

  // The ephemeral key is encoded as a COSE structure.
  auto it = response_map.find(
      cbor::Value(static_cast<int>(ResponseKey::kKeyAgreement)));
  if (it == response_map.end() || !it->second.is_map()) {
    return base::nullopt;
  }
  const auto& cose_key = it->second.GetMap();

  return ParseFromCOSE(cose_key);
}

// static
base::Optional<KeyAgreementResponse> KeyAgreementResponse::ParseFromCOSE(
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
      return base::nullopt;
    }
  }

  // See https://tools.ietf.org/html/rfc8152#section-13.1.1
  const auto& x_it = cose_key.find(cbor::Value(-2));
  const auto& y_it = cose_key.find(cbor::Value(-3));
  if (x_it == cose_key.end() || y_it == cose_key.end() ||
      !x_it->second.is_bytestring() || !y_it->second.is_bytestring()) {
    return base::nullopt;
  }

  const auto& x = x_it->second.GetBytestring();
  const auto& y = y_it->second.GetBytestring();
  KeyAgreementResponse ret;
  if (x.size() != sizeof(ret.x) || y.size() != sizeof(ret.y)) {
    return base::nullopt;
  }
  memcpy(ret.x, x.data(), sizeof(ret.x));
  memcpy(ret.y, y.data(), sizeof(ret.y));

  bssl::UniquePtr<EC_GROUP> group(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));

  // Check that the point is on the curve.
  auto point = PointFromKeyAgreementResponse(group.get(), ret);
  if (!point) {
    return base::nullopt;
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
  DCHECK(IsValid(pin));
  memset(pin_, 0, sizeof(pin_));
  memcpy(pin_, pin.data(), pin.size());
}

// SHA256KDF implements CTAP2's KDF, which just runs SHA-256 on the x-coordinate
// of the result. The function signature is such that it fits into OpenSSL's
// ECDH API.
static void* SHA256KDF(const void* in,
                       size_t in_len,
                       void* out,
                       size_t* out_len) {
  DCHECK_GE(*out_len, static_cast<size_t>(SHA256_DIGEST_LENGTH));
  SHA256(reinterpret_cast<const uint8_t*>(in), in_len,
         reinterpret_cast<uint8_t*>(out));
  *out_len = SHA256_DIGEST_LENGTH;
  return out;
}

// CalculateSharedKey writes the CTAP2 shared key between |key| and |peers_key|
// to |out_shared_key|.
void CalculateSharedKey(const EC_KEY* key,
                        const EC_POINT* peers_key,
                        uint8_t out_shared_key[SHA256_DIGEST_LENGTH]) {
  CHECK_EQ(static_cast<int>(SHA256_DIGEST_LENGTH),
           ECDH_compute_key(out_shared_key, SHA256_DIGEST_LENGTH, peers_key,
                            key, SHA256KDF));
}

// EncodeCOSEPublicKey converts an X9.62 public key into a COSE structure.
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

// GenerateSharedKey generates and returns an ephemeral key, and writes the
// shared key between that ephemeral key and the authenticator's ephemeral key
// (from |peers_key|) to |out_shared_key|.
static std::array<uint8_t, kP256X962Length> GenerateSharedKey(
    const KeyAgreementResponse& peers_key,
    uint8_t out_shared_key[SHA256_DIGEST_LENGTH]) {
  bssl::UniquePtr<EC_KEY> key(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  CHECK(EC_KEY_generate_key(key.get()));
  auto peers_point =
      PointFromKeyAgreementResponse(EC_KEY_get0_group(key.get()), peers_key);
  CalculateSharedKey(key.get(), peers_point->get(), out_shared_key);
  std::array<uint8_t, kP256X962Length> x962;
  CHECK_EQ(x962.size(),
           EC_POINT_point2oct(EC_KEY_get0_group(key.get()),
                              EC_KEY_get0_public_key(key.get()),
                              POINT_CONVERSION_UNCOMPRESSED, x962.data(),
                              x962.size(), nullptr /* BN_CTX */));

  return x962;
}

// Encrypt encrypts |plaintext| using |key|, writing the ciphertext to
// |out_ciphertext|. |plaintext| must be a whole number of AES blocks.
void Encrypt(const uint8_t key[SHA256_DIGEST_LENGTH],
             base::span<const uint8_t> plaintext,
             uint8_t* out_ciphertext) {
  DCHECK_EQ(0u, plaintext.size() % AES_BLOCK_SIZE);

  EVP_CIPHER_CTX aes_ctx;
  EVP_CIPHER_CTX_init(&aes_ctx);
  const uint8_t kZeroIV[AES_BLOCK_SIZE] = {0};
  CHECK(EVP_EncryptInit_ex(&aes_ctx, EVP_aes_256_cbc(), nullptr, key, kZeroIV));
  CHECK(EVP_CIPHER_CTX_set_padding(&aes_ctx, 0 /* no padding */));
  CHECK(
      EVP_Cipher(&aes_ctx, out_ciphertext, plaintext.data(), plaintext.size()));
  EVP_CIPHER_CTX_cleanup(&aes_ctx);
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

  DCHECK(IsValid(new_pin));
  memset(new_pin_, 0, sizeof(new_pin_));
  memcpy(new_pin_, new_pin.data(), new_pin.size());
}

// static
base::Optional<EmptyResponse> EmptyResponse::Parse(
    const base::Optional<cbor::Value>& cbor) {
  // Yubikeys can return just the status byte, and no CBOR bytes, for the empty
  // response, which will end up here with |cbor| being |nullopt|. This seems
  // wrong, but is handled. (The response should, instead, encode an empty CBOR
  // map.)
  if (cbor && (!cbor->is_map() || !cbor->GetMap().empty())) {
    return base::nullopt;
  }

  EmptyResponse ret;
  return ret;
}

TokenResponse::TokenResponse(PINUVAuthProtocol protocol)
    : protocol_(protocol) {}
TokenResponse::~TokenResponse() = default;
TokenResponse::TokenResponse(const TokenResponse&) = default;
TokenResponse& TokenResponse::operator=(const TokenResponse&) = default;

// Decrypt AES-256 CBC decrypts some number of whole blocks from |ciphertext|
// into |plaintext|, using |key|.
void Decrypt(const uint8_t key[SHA256_DIGEST_LENGTH],
             base::span<const uint8_t> ciphertext,
             uint8_t* out_plaintext) {
  DCHECK_EQ(0u, ciphertext.size() % AES_BLOCK_SIZE);

  EVP_CIPHER_CTX aes_ctx;
  EVP_CIPHER_CTX_init(&aes_ctx);
  const uint8_t kZeroIV[AES_BLOCK_SIZE] = {0};
  CHECK(EVP_DecryptInit_ex(&aes_ctx, EVP_aes_256_cbc(), nullptr, key, kZeroIV));
  CHECK(EVP_CIPHER_CTX_set_padding(&aes_ctx, 0 /* no padding */));

  CHECK(EVP_Cipher(&aes_ctx, out_plaintext, ciphertext.data(),
                   ciphertext.size()));
  EVP_CIPHER_CTX_cleanup(&aes_ctx);
}

base::Optional<TokenResponse> TokenResponse::Parse(
    PINUVAuthProtocol protocol,
    base::span<const uint8_t> shared_key,
    const base::Optional<cbor::Value>& cbor) {
  if (!cbor || !cbor->is_map()) {
    return base::nullopt;
  }
  const auto& response_map = cbor->GetMap();

  auto it =
      response_map.find(cbor::Value(static_cast<int>(ResponseKey::kPINToken)));
  if (it == response_map.end() || !it->second.is_bytestring()) {
    return base::nullopt;
  }
  const auto& encrypted_token = it->second.GetBytestring();
  if (encrypted_token.size() % AES_BLOCK_SIZE != 0) {
    return base::nullopt;
  }

  TokenResponse ret(protocol);
  ret.token_.resize(encrypted_token.size());
  Decrypt(shared_key.data(), encrypted_token, ret.token_.data());
  return ret;
}

std::pair<PINUVAuthProtocol, std::vector<uint8_t>> TokenResponse::PinAuth(
    base::span<const uint8_t> client_data_hash) const {
  return {protocol_, MakePinAuth(token_, client_data_hash)};
}

// static
std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const PinRetriesRequest& request) {
  return EncodePINCommand(request.protocol, Subcommand::kGetRetries);
}

// static
std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const UvRetriesRequest& request) {
  return EncodePINCommand(request.protocol, Subcommand::kGetUvRetries);
}

// static
std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const KeyAgreementRequest& request) {
  return EncodePINCommand(request.protocol, Subcommand::kGetKeyAgreement);
}

// static
std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const SetRequest& request) {
  // See
  // https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#settingNewPin
  uint8_t shared_key[SHA256_DIGEST_LENGTH];
  auto cose_key =
      EncodeCOSEPublicKey(GenerateSharedKey(request.peer_key_, shared_key));

  static_assert((sizeof(request.pin_) % AES_BLOCK_SIZE) == 0,
                "pin_ is not a multiple of the AES block size");
  uint8_t encrypted_pin[sizeof(request.pin_)];
  Encrypt(shared_key, request.pin_, encrypted_pin);

  std::vector<uint8_t> pin_auth =
      MakePinAuth(base::make_span(shared_key, sizeof(shared_key)),
                  base::make_span(encrypted_pin, sizeof(encrypted_pin)));

  return EncodePINCommand(
      request.protocol_, Subcommand::kSetPIN,
      [&cose_key, &encrypted_pin, &pin_auth](cbor::Value::MapValue* map) {
        map->emplace(static_cast<int>(RequestKey::kKeyAgreement),
                     std::move(cose_key));
        map->emplace(
            static_cast<int>(RequestKey::kNewPINEnc),
            base::span<const uint8_t>(encrypted_pin, sizeof(encrypted_pin)));
        map->emplace(static_cast<int>(RequestKey::kPINAuth),
                     std::move(pin_auth));
      });
}

// static
std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const ChangeRequest& request) {
  // See
  // https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#changingExistingPin
  uint8_t shared_key[SHA256_DIGEST_LENGTH];
  auto cose_key =
      EncodeCOSEPublicKey(GenerateSharedKey(request.peer_key_, shared_key));

  static_assert((sizeof(request.new_pin_) % AES_BLOCK_SIZE) == 0,
                "new_pin_ is not a multiple of the AES block size");
  uint8_t encrypted_pin[sizeof(request.new_pin_)];
  Encrypt(shared_key, request.new_pin_, encrypted_pin);

  static_assert((sizeof(request.old_pin_hash_) % AES_BLOCK_SIZE) == 0,
                "old_pin_hash_ is not a multiple of the AES block size");
  uint8_t old_pin_hash_enc[sizeof(request.old_pin_hash_)];
  Encrypt(shared_key, request.old_pin_hash_, old_pin_hash_enc);

  uint8_t ciphertexts_concat[sizeof(encrypted_pin) + sizeof(old_pin_hash_enc)];
  memcpy(ciphertexts_concat, encrypted_pin, sizeof(encrypted_pin));
  memcpy(ciphertexts_concat + sizeof(encrypted_pin), old_pin_hash_enc,
         sizeof(old_pin_hash_enc));
  std::vector<uint8_t> pin_auth = MakePinAuth(
      base::make_span(shared_key, sizeof(shared_key)),
      base::make_span(ciphertexts_concat, sizeof(ciphertexts_concat)));

  return EncodePINCommand(
      request.protocol_, Subcommand::kChangePIN,
      [&cose_key, &encrypted_pin, &old_pin_hash_enc,
       &pin_auth](cbor::Value::MapValue* map) {
        map->emplace(static_cast<int>(RequestKey::kKeyAgreement),
                     std::move(cose_key));
        map->emplace(static_cast<int>(RequestKey::kPINHashEnc),
                     base::span<const uint8_t>(old_pin_hash_enc,
                                               sizeof(old_pin_hash_enc)));
        map->emplace(
            static_cast<int>(RequestKey::kNewPINEnc),
            base::span<const uint8_t>(encrypted_pin, sizeof(encrypted_pin)));
        map->emplace(static_cast<int>(RequestKey::kPINAuth),
                     std::move(pin_auth));
      });
}

// static
std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const ResetRequest&) {
  return std::make_pair(CtapRequestCommand::kAuthenticatorReset, base::nullopt);
}

TokenRequest::TokenRequest(PINUVAuthProtocol protocol,
                           const KeyAgreementResponse& peer_key)
    : protocol_(protocol),
      public_key_(GenerateSharedKey(peer_key, shared_key_.data())) {
  DCHECK_EQ(static_cast<size_t>(SHA256_DIGEST_LENGTH), shared_key_.size());
}

TokenRequest::~TokenRequest() = default;

TokenRequest::TokenRequest(TokenRequest&& other) = default;

const std::array<uint8_t, 32>& TokenRequest::shared_key() const {
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
std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const PinTokenRequest& request) {
  static_assert((sizeof(request.pin_hash_) % AES_BLOCK_SIZE) == 0,
                "pin_hash_ is not a multiple of the AES block size");
  uint8_t encrypted_pin[sizeof(request.pin_hash_)];
  Encrypt(request.shared_key_.data(), request.pin_hash_, encrypted_pin);

  return EncodePINCommand(
      request.protocol_, Subcommand::kGetPINToken,
      [&request, &encrypted_pin](cbor::Value::MapValue* map) {
        map->emplace(static_cast<int>(RequestKey::kKeyAgreement),
                     EncodeCOSEPublicKey(request.public_key_));
        map->emplace(
            static_cast<int>(RequestKey::kPINHashEnc),
            base::span<const uint8_t>(encrypted_pin, sizeof(encrypted_pin)));
      });
}

PinTokenWithPermissionsRequest::PinTokenWithPermissionsRequest(
    PINUVAuthProtocol protocol,
    const std::string& pin,
    const KeyAgreementResponse& peer_key,
    const uint8_t permissions,
    const base::Optional<std::string> rp_id)
    : PinTokenRequest(protocol, pin, peer_key),
      permissions_(permissions),
      rp_id_(rp_id) {}

// static
std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const PinTokenWithPermissionsRequest& request) {
  uint8_t encrypted_pin[sizeof(request.pin_hash_)];
  Encrypt(request.shared_key_.data(), request.pin_hash_, encrypted_pin);

  return EncodePINCommand(
      request.protocol_, Subcommand::kGetPinUvAuthTokenUsingPinWithPermissions,
      [&request, &encrypted_pin](cbor::Value::MapValue* map) {
        map->emplace(static_cast<int>(RequestKey::kKeyAgreement),
                     EncodeCOSEPublicKey(request.public_key_));
        map->emplace(
            static_cast<int>(RequestKey::kPINHashEnc),
            base::span<const uint8_t>(encrypted_pin, sizeof(encrypted_pin)));
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
                               base::Optional<std::string> rp_id)
    : TokenRequest(protocol, peer_key), rp_id_(rp_id) {}

UvTokenRequest::~UvTokenRequest() = default;

UvTokenRequest::UvTokenRequest(UvTokenRequest&& other) = default;

// static
std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const UvTokenRequest& request) {
  return EncodePINCommand(
      request.protocol_, Subcommand::kGetUvToken,
      [&request](cbor::Value::MapValue* map) {
        map->emplace(static_cast<int>(RequestKey::kKeyAgreement),
                     EncodeCOSEPublicKey(request.public_key_));
        map->emplace(static_cast<int>(RequestKey::kPermissions),
                     static_cast<uint8_t>(Permissions::kMakeCredential) |
                         static_cast<uint8_t>(Permissions::kGetAssertion));
        if (request.rp_id_) {
          map->emplace(static_cast<int>(RequestKey::kPermissionsRPID),
                       *request.rp_id_);
        }
      });
}

static std::vector<uint8_t> EncryptToVector(
    base::span<const uint8_t, SHA256_DIGEST_LENGTH> key,
    base::span<const uint8_t> plaintext) {
  std::vector<uint8_t> ret;
  ret.resize(plaintext.size());
  Encrypt(key.data(), plaintext, ret.data());
  return ret;
}

static std::vector<uint8_t> ConcatSalts(
    base::span<const uint8_t, 32> salt1,
    const base::Optional<std::array<uint8_t, 32>>& salt2) {
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
    const base::Optional<std::array<uint8_t, 32>>& salt2)
    : protocol_(protocol),
      public_key_x962(GenerateSharedKey(peer_key, shared_key_.data())),
      encrypted_salts(EncryptToVector(shared_key_, ConcatSalts(salt1, salt2))),
      salts_auth(MakePinAuth(shared_key_, encrypted_salts)) {}

HMACSecretRequest::~HMACSecretRequest() = default;

HMACSecretRequest::HMACSecretRequest(const HMACSecretRequest& other) = default;

base::Optional<std::vector<uint8_t>> HMACSecretRequest::Decrypt(
    base::span<const uint8_t> ciphertext) {
  if (ciphertext.size() != this->encrypted_salts.size()) {
    return base::nullopt;
  }

  std::vector<uint8_t> ret;
  ret.resize(ciphertext.size());
  pin::Decrypt(shared_key_.data(), ciphertext, ret.data());
  return ret;
}

}  // namespace pin

}  // namespace device
