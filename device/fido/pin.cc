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
static std::vector<uint8_t> MakePinAuth(base::span<const uint8_t> secret,
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
    Subcommand subcommand,
    std::function<void(cbor::Value::MapValue*)> add_additional = nullptr) {
  cbor::Value::MapValue map;
  map.emplace(static_cast<int>(RequestKey::kProtocol), kProtocolVersion);
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
base::Optional<RetriesResponse> RetriesResponse::Parse(
    const base::Optional<cbor::Value>& cbor) {
  if (!cbor || !cbor->is_map()) {
    return base::nullopt;
  }
  const auto& response_map = cbor->GetMap();

  auto it =
      response_map.find(cbor::Value(static_cast<int>(ResponseKey::kRetries)));
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

SetRequest::SetRequest(const std::string& pin,
                       const KeyAgreementResponse& peer_key)
    : peer_key_(peer_key) {
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

// EncodeCOSEPublicKey returns the public part of |key| as a COSE structure.
cbor::Value::MapValue EncodeCOSEPublicKey(const EC_KEY* key) {
  // X9.62 is the standard for serialising elliptic-curve points.
  uint8_t x962[1 /* type byte */ + 32 /* x */ + 32 /* y */];
  CHECK_EQ(
      sizeof(x962),
      EC_POINT_point2oct(EC_KEY_get0_group(key), EC_KEY_get0_public_key(key),
                         POINT_CONVERSION_UNCOMPRESSED, x962, sizeof(x962),
                         nullptr /* BN_CTX */));

  cbor::Value::MapValue cose_key;
  cose_key.emplace(1 /* key type */, 2 /* uncompressed elliptic curve */);
  cose_key.emplace(3 /* algorithm */,
                   -25 /* ECDH, ephemeral–static, HKDF-SHA-256 */);
  cose_key.emplace(-1 /* curve */, 1 /* P-256 */);
  cose_key.emplace(-2 /* x */, base::span<const uint8_t>(x962 + 1, 32));
  cose_key.emplace(-3 /* y */, base::span<const uint8_t>(x962 + 33, 32));

  return cose_key;
}

// GenerateSharedKey generates and returns an ephemeral key, and writes the
// shared key between that ephemeral key and the authenticator's ephemeral key
// (from |peers_key|) to |out_shared_key|.
static cbor::Value::MapValue GenerateSharedKey(
    const KeyAgreementResponse& peers_key,
    uint8_t out_shared_key[SHA256_DIGEST_LENGTH]) {
  bssl::UniquePtr<EC_KEY> key(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  CHECK(EC_KEY_generate_key(key.get()));
  auto peers_point =
      PointFromKeyAgreementResponse(EC_KEY_get0_group(key.get()), peers_key);
  CalculateSharedKey(key.get(), peers_point->get(), out_shared_key);
  return EncodeCOSEPublicKey(key.get());
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

ChangeRequest::ChangeRequest(const std::string& old_pin,
                             const std::string& new_pin,
                             const KeyAgreementResponse& peer_key)
    : peer_key_(peer_key) {
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

TokenResponse::TokenResponse() = default;
TokenResponse::~TokenResponse() = default;
TokenResponse::TokenResponse(const TokenResponse&) = default;

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
    std::array<uint8_t, 32> shared_key,
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

  TokenResponse ret;
  ret.token_.resize(encrypted_token.size());
  Decrypt(shared_key.data(), encrypted_token, ret.token_.data());

  return ret;
}

std::vector<uint8_t> TokenResponse::PinAuth(
    base::span<const uint8_t> client_data_hash) const {
  return MakePinAuth(token_, client_data_hash);
}

// static
std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const RetriesRequest&) {
  return EncodePINCommand(Subcommand::kGetRetries);
}

// static
std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const KeyAgreementRequest&) {
  return EncodePINCommand(Subcommand::kGetKeyAgreement);
}

// static
std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const SetRequest& request) {
  // See
  // https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#settingNewPin
  uint8_t shared_key[SHA256_DIGEST_LENGTH];
  auto cose_key = GenerateSharedKey(request.peer_key_, shared_key);

  static_assert((sizeof(request.pin_) % AES_BLOCK_SIZE) == 0,
                "pin_ is not a multiple of the AES block size");
  uint8_t encrypted_pin[sizeof(request.pin_)];
  Encrypt(shared_key, request.pin_, encrypted_pin);

  std::vector<uint8_t> pin_auth =
      MakePinAuth(base::make_span(shared_key, sizeof(shared_key)),
                  base::make_span(encrypted_pin, sizeof(encrypted_pin)));

  return EncodePINCommand(
      Subcommand::kSetPIN,
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
  auto cose_key = GenerateSharedKey(request.peer_key_, shared_key);

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
      Subcommand::kChangePIN, [&cose_key, &encrypted_pin, &old_pin_hash_enc,
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

TokenRequest::TokenRequest(const std::string& pin,
                           const KeyAgreementResponse& peer_key)
    : cose_key_(GenerateSharedKey(peer_key, shared_key_.data())) {
  DCHECK_EQ(static_cast<size_t>(SHA256_DIGEST_LENGTH), shared_key_.size());
  uint8_t digest[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const uint8_t*>(pin.data()), pin.size(), digest);
  memcpy(pin_hash_, digest, sizeof(pin_hash_));
}

TokenRequest::~TokenRequest() = default;

TokenRequest::TokenRequest(TokenRequest&& other) = default;

const std::array<uint8_t, 32>& TokenRequest::shared_key() const {
  return shared_key_;
}

// static
std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const TokenRequest& request) {
  static_assert((sizeof(request.pin_hash_) % AES_BLOCK_SIZE) == 0,
                "pin_hash_ is not a multiple of the AES block size");
  uint8_t encrypted_pin[sizeof(request.pin_hash_)];
  Encrypt(request.shared_key_.data(), request.pin_hash_, encrypted_pin);

  return EncodePINCommand(
      Subcommand::kGetPINToken,
      [&request, &encrypted_pin](cbor::Value::MapValue* map) {
        map->emplace(static_cast<int>(RequestKey::kKeyAgreement),
                     std::move(request.cose_key_));
        map->emplace(
            static_cast<int>(RequestKey::kPINHashEnc),
            base::span<const uint8_t>(encrypted_pin, sizeof(encrypted_pin)));
      });
}

}  // namespace pin

}  // namespace device
