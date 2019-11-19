// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/credential_metadata.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"
#include "third_party/boringssl/src/include/openssl/rand.h"

namespace device {
namespace fido {
namespace mac {

using cbor::Reader;
using cbor::Value;
using cbor::Writer;

// The version tag encoded into encrypted credential metadata.
static constexpr uint8_t kVersionLegacy0 = 0x00;

// The version tag encoded into encrypted credential metadata.
static constexpr uint8_t kVersion = 0x01;

static constexpr size_t kNonceLength = 12;

namespace {

// MakeAad returns the concatenation of |version| and |rp_id|,
// which is used as the additional authenticated data (AAD) input to the AEAD.
std::string MakeAad(const uint8_t version, const std::string& rp_id) {
  return std::string(1, version) + rp_id;
}

// Cryptor provides methods for encrypting and authenticating credential
// metadata.
class Cryptor {
 public:
  explicit Cryptor(std::string secret) : secret_(std::move(secret)) {}
  Cryptor(Cryptor&&) = default;
  Cryptor& operator=(Cryptor&&) = default;
  ~Cryptor() = default;

  enum Algorithm : uint8_t {
    kAes256Gcm = 0,
    kHmacSha256 = 1,
    kAes256GcmSiv = 2,
  };

  base::Optional<std::string> Seal(Algorithm alg,
                                   base::span<const uint8_t> nonce,
                                   base::span<const uint8_t> plaintext,
                                   base::StringPiece authenticated_data) const;

  base::Optional<std::string> Unseal(
      Algorithm alg,
      base::span<const uint8_t> nonce,
      base::span<const uint8_t> ciphertext,
      base::StringPiece authenticated_data) const;

  base::Optional<std::string> HmacForStorage(base::StringPiece data) const;

 private:
  static base::Optional<crypto::Aead::AeadAlgorithm> ToAeadAlgorithm(
      Algorithm alg);

  // Derives an Algorithm-specific key from |secret_| to avoid using the same
  // key for different algorithms.
  std::string DeriveKey(Algorithm alg) const;

  Cryptor(const Cryptor&) = delete;
  Cryptor& operator=(const Cryptor&) = delete;

  // Used to derive keys for the HMAC and AEAD operations. Chrome picks
  // different secrets for each user profile. This ensures that credentials are
  // logically tied to the Chrome user profile under which they were created.
  std::string secret_;
};

base::Optional<std::string> Cryptor::Seal(
    Algorithm algorithm,
    base::span<const uint8_t> nonce,
    base::span<const uint8_t> plaintext,
    base::StringPiece authenticated_data) const {
  auto opt_aead_algorithm = ToAeadAlgorithm(algorithm);
  if (!opt_aead_algorithm)
    return base::nullopt;

  const std::string key = DeriveKey(algorithm);
  crypto::Aead aead(*opt_aead_algorithm);
  aead.Init(&key);
  std::string ciphertext;
  if (!aead.Seal(
          base::StringPiece(reinterpret_cast<const char*>(plaintext.data()),
                            plaintext.size()),
          base::StringPiece(reinterpret_cast<const char*>(nonce.data()),
                            nonce.size()),
          authenticated_data, &ciphertext)) {
    return base::nullopt;
  }
  return ciphertext;
}

base::Optional<std::string> Cryptor::Unseal(
    Algorithm algorithm,
    base::span<const uint8_t> nonce,
    base::span<const uint8_t> ciphertext,
    base::StringPiece authenticated_data) const {
  auto opt_aead_algorithm = ToAeadAlgorithm(algorithm);
  if (!opt_aead_algorithm)
    return base::nullopt;

  const std::string key = DeriveKey(algorithm);
  crypto::Aead aead(*opt_aead_algorithm);
  aead.Init(&key);
  std::string plaintext;
  if (!aead.Open(
          base::StringPiece(reinterpret_cast<const char*>(ciphertext.data()),
                            ciphertext.size()),
          base::StringPiece(reinterpret_cast<const char*>(nonce.data()),
                            nonce.size()),
          authenticated_data, &plaintext)) {
    return base::nullopt;
  }
  return plaintext;
}

base::Optional<std::string> Cryptor::HmacForStorage(
    base::StringPiece data) const {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  const std::string key = DeriveKey(Algorithm::kHmacSha256);
  std::vector<uint8_t> digest(hmac.DigestLength());
  if (!hmac.Init(key) || !hmac.Sign(data, digest.data(), hmac.DigestLength())) {
    return base::nullopt;
  }
  // The keychain fields that store RP ID and User ID seem to only accept
  // NSString (not NSData), so we HexEncode to ensure the result to be
  // UTF-8-decodable.
  return base::HexEncode(digest.data(), digest.size());
}

// static
base::Optional<crypto::Aead::AeadAlgorithm> Cryptor::ToAeadAlgorithm(
    Algorithm alg) {
  switch (alg) {
    case Algorithm::kAes256Gcm:
      return crypto::Aead::AES_256_GCM;
    case Algorithm::kAes256GcmSiv:
      return crypto::Aead::AES_256_GCM_SIV;
    case Algorithm::kHmacSha256:
      NOTREACHED() << "invalid AEAD";
      return base::nullopt;
  }
}

std::string Cryptor::DeriveKey(Algorithm alg) const {
  static constexpr size_t kKeyLength = 32u;
  std::string key;
  const uint8_t info = static_cast<uint8_t>(alg);
  const bool hkdf_init =
      ::HKDF(reinterpret_cast<uint8_t*>(base::WriteInto(&key, kKeyLength + 1)),
             kKeyLength, EVP_sha256(),
             reinterpret_cast<const uint8_t*>(secret_.data()), secret_.size(),
             nullptr /* salt */, 0, &info, 1);
  DCHECK(hkdf_init);
  return key;
}

}  // namespace

// static
CredentialMetadata CredentialMetadata::FromPublicKeyCredentialUserEntity(
    const PublicKeyCredentialUserEntity& user,
    bool is_resident) {
  return CredentialMetadata(user.id, user.name.value_or(""),
                            user.display_name.value_or(""), is_resident);
}

PublicKeyCredentialUserEntity
CredentialMetadata::ToPublicKeyCredentialUserEntity() {
  PublicKeyCredentialUserEntity user_entity(user_id);
  if (!user_name.empty()) {
    user_entity.name = user_name;
  }
  if (!user_display_name.empty()) {
    user_entity.display_name = user_display_name;
  }
  return user_entity;
}

CredentialMetadata::CredentialMetadata(std::vector<uint8_t> user_id_,
                                       std::string user_name_,
                                       std::string user_display_name_,
                                       bool is_resident_)
    : user_id(std::move(user_id_)),
      user_name(std::move(user_name_)),
      user_display_name(std::move(user_display_name_)),
      is_resident(is_resident_) {}
CredentialMetadata::CredentialMetadata(const CredentialMetadata&) = default;
CredentialMetadata::CredentialMetadata(CredentialMetadata&&) = default;
CredentialMetadata& CredentialMetadata::operator=(CredentialMetadata&&) =
    default;
CredentialMetadata::~CredentialMetadata() = default;

std::string GenerateCredentialMetadataSecret() {
  static constexpr size_t kSecretSize = 32u;
  std::string secret;
  RAND_bytes(
      reinterpret_cast<uint8_t*>(base::WriteInto(&secret, kSecretSize + 1)),
      kSecretSize);
  return secret;
}

static std::string MaybeTruncateWithTrailingEllipsis(const std::string& in) {
  constexpr size_t kMaxLength = 70u;
  if (in.size() <= kMaxLength) {
    return in;
  }
  std::string out;
  // CTAP authenticators are not supposed to truncate before 64 bytes, but
  // there is no truncate-with-min-size method, so truncate to a 67 byte max
  // instead. Adding the 3-byte ellipsis gets us to a maximum of 70 bytes.
  base::TruncateUTF8ToByteSize(in, kMaxLength - 3, &out);
  out += "…";  // HORIZONTAL ELLIPSIS (E2 80 A6).
  return out;
}

base::Optional<std::vector<uint8_t>> SealCredentialId(
    const std::string& secret,
    const std::string& rp_id,
    const CredentialMetadata& metadata) {
  // The first 13 bytes are the version and nonce.
  std::vector<uint8_t> result(1 + kNonceLength);
  result[0] = kVersion;
  // Pick a random nonce. N.B. the nonce is similar to an IV. It needs to be
  // distinct (but not necessarily random). Nonce reuse breaks confidentiality
  // (in particular, it leaks the XOR of the plaintexts encrypted under the
  // same nonce and key).
  base::span<uint8_t> nonce(result.data() + 1, kNonceLength);
  RAND_bytes(nonce.data(), nonce.size());  // RAND_bytes always returns 1.

  // The remaining bytes are the CBOR-encoded CredentialMetadata, encrypted with
  // AES-256-GCM and authenticated with the version and RP ID.
  Value::ArrayValue cbor_user;
  cbor_user.emplace_back(Value(metadata.user_id));
  cbor_user.emplace_back(
      Value(MaybeTruncateWithTrailingEllipsis(metadata.user_name),
            Value::Type::BYTE_STRING));
  cbor_user.emplace_back(
      Value(MaybeTruncateWithTrailingEllipsis(metadata.user_display_name),
            Value::Type::BYTE_STRING));
  cbor_user.emplace_back(Value(metadata.is_resident));
  base::Optional<std::vector<uint8_t>> pt =
      Writer::Write(Value(std::move(cbor_user)));
  if (!pt) {
    return base::nullopt;
  }
  base::Optional<std::string> ciphertext = Cryptor(secret).Seal(
      Cryptor::Algorithm::kAes256Gcm, nonce, *pt, MakeAad(kVersion, rp_id));
  if (!ciphertext) {
    return base::nullopt;
  }
  base::span<const char> cts(reinterpret_cast<const char*>(ciphertext->data()),
                             ciphertext->size());
  result.insert(result.end(), cts.begin(), cts.end());
  return result;
}

// UnsealLegacyCredentialId attempts to decrypt a credential ID that has been
// encrypted under the scheme for version 0x00, which is:
//    | version  |    nonce   | AEAD(pt=CBOR(user_entity), |
//    | (1 byte) | (12 bytes) |      nonce=nonce,          |
//    |          |            |      ad=(version, rpID))   |
// Note the absence of the rk bit, which is always false.
static base::Optional<CredentialMetadata> UnsealLegacyCredentialId(
    const std::string& secret,
    const std::string& rp_id,
    base::span<const uint8_t> credential_id) {
  // Recover the nonce and check for the correct version byte. Then try to
  // decrypt the remaining bytes.
  if (credential_id.size() <= 1 + kNonceLength ||
      credential_id[0] != kVersionLegacy0) {
    return base::nullopt;
  }

  base::Optional<std::string> plaintext = Cryptor(secret).Unseal(
      Cryptor::Algorithm::kAes256Gcm, credential_id.subspan(1, kNonceLength),
      credential_id.subspan(1 + kNonceLength), MakeAad(kVersionLegacy0, rp_id));
  if (!plaintext) {
    return base::nullopt;
  }

  // The recovered plaintext should decode into the CredentialMetadata struct.
  base::Optional<Value> maybe_array = Reader::Read(base::make_span(
      reinterpret_cast<const uint8_t*>(plaintext->data()), plaintext->size()));
  if (!maybe_array || !maybe_array->is_array()) {
    return base::nullopt;
  }
  const Value::ArrayValue& array = maybe_array->GetArray();
  if (array.size() != 3 || !array[0].is_bytestring() ||
      !array[1].is_bytestring() || !array[2].is_bytestring()) {
    return base::nullopt;
  }
  return CredentialMetadata(array[0].GetBytestring(),
                            array[1].GetBytestringAsString().as_string(),
                            array[2].GetBytestringAsString().as_string(),
                            /*is_resident=*/false);
}

base::Optional<CredentialMetadata> UnsealCredentialId(
    const std::string& secret,
    const std::string& rp_id,
    base::span<const uint8_t> credential_id) {
  if (!credential_id.empty() && credential_id[0] == kVersionLegacy0) {
    return UnsealLegacyCredentialId(secret, rp_id, credential_id);
  }

  if (credential_id.size() <= 1 + kNonceLength ||
      credential_id[0] != kVersion) {
    return base::nullopt;
  }

  base::Optional<std::string> plaintext = Cryptor(secret).Unseal(
      Cryptor::Algorithm::kAes256Gcm, credential_id.subspan(1, kNonceLength),
      credential_id.subspan(1 + kNonceLength), MakeAad(kVersion, rp_id));
  if (!plaintext) {
    return base::nullopt;
  }

  // The recovered plaintext should decode into the CredentialMetadata struct.
  base::Optional<Value> maybe_array = Reader::Read(base::make_span(
      reinterpret_cast<const uint8_t*>(plaintext->data()), plaintext->size()));
  if (!maybe_array || !maybe_array->is_array()) {
    return base::nullopt;
  }
  const Value::ArrayValue& array = maybe_array->GetArray();
  if (array.size() != 4 || !array[0].is_bytestring() ||
      !array[1].is_bytestring() || !array[2].is_bytestring() ||
      !array[3].is_bool()) {
    return base::nullopt;
  }
  return CredentialMetadata(
      array[0].GetBytestring(), array[1].GetBytestringAsString().as_string(),
      array[2].GetBytestringAsString().as_string(), array[3].GetBool());
}

base::Optional<std::string> EncodeRpIdAndUserId(
    const std::string& secret,
    const std::string& rp_id,
    base::span<const uint8_t> user_id) {
  // Encoding RP ID along with the user ID hides whether the same user ID was
  // reused on different RPs.
  const auto* user_id_data = reinterpret_cast<const char*>(user_id.data());
  return Cryptor(secret).HmacForStorage(
      rp_id + "/" + std::string(user_id_data, user_id_data + user_id.size()));
}

base::Optional<std::string> EncodeRpId(const std::string& secret,
                                       const std::string& rp_id) {
  // Encrypt with a fixed nonce to make the result deterministic while still
  // allowing the RP ID to be recovered from the ciphertext later.
  static constexpr std::array<uint8_t, kNonceLength> fixed_zero_nonce = {};
  base::span<const uint8_t> pt(reinterpret_cast<const uint8_t*>(rp_id.data()),
                               rp_id.size());
  std::string empty_ad;
  // Using AES-GCM with a fixed nonce would break confidentiality, so this uses
  // AES-GCM-SIV instead.
  base::Optional<std::string> ct = Cryptor(secret).Seal(
      Cryptor::Algorithm::kAes256GcmSiv, fixed_zero_nonce, pt, empty_ad);
  if (!ct) {
    return base::nullopt;
  }
  // The keychain field that stores the encrypted RP ID only accepts NSString
  // (not NSData), so we HexEncode to ensure the result is UTF-8-decodable.
  return base::HexEncode(ct->data(), ct->size());
}

base::Optional<std::string> DecodeRpId(const std::string& secret,
                                       const std::string& ciphertext) {
  std::vector<uint8_t> ct;
  if (!base::HexStringToBytes(ciphertext, &ct)) {
    return base::nullopt;
  }
  static constexpr std::array<uint8_t, kNonceLength> fixed_zero_nonce = {};
  std::string empty_ad;
  return Cryptor(secret).Unseal(Cryptor::Algorithm::kAes256GcmSiv,
                                fixed_zero_nonce, ct, empty_ad);
}

base::Optional<std::vector<uint8_t>> SealLegacyV0CredentialIdForTestingOnly(
    const std::string& secret,
    const std::string& rp_id,
    const std::vector<uint8_t>& user_id,
    const std::string& user_name,
    const std::string& user_display_name) {
  constexpr uint8_t version = 0x00;
  //    | version  |    nonce   | AEAD(pt=CBOR(user_entity), |
  //    | (1 byte) | (12 bytes) |      nonce=nonce,          |
  //    |          |            |      ad=(version, rpID))   |
  std::vector<uint8_t> result(13);
  result[0] = version;
  base::span<uint8_t> nonce(result.data() + 1, 12);
  RAND_bytes(nonce.data(), nonce.size());  // RAND_bytes always returns 1.

  Value::ArrayValue cbor_user;
  cbor_user.emplace_back(Value(user_id));
  cbor_user.emplace_back(Value(user_name, Value::Type::BYTE_STRING));
  cbor_user.emplace_back(Value(user_display_name, Value::Type::BYTE_STRING));
  base::Optional<std::vector<uint8_t>> pt =
      Writer::Write(Value(std::move(cbor_user)));
  if (!pt) {
    return base::nullopt;
  }
  std::string aad = std::string(1, version) + rp_id;
  base::Optional<std::string> ciphertext =
      Cryptor(secret).Seal(Cryptor::Algorithm::kAes256Gcm, nonce, *pt, aad);
  if (!ciphertext) {
    return base::nullopt;
  }
  base::span<const char> cts(reinterpret_cast<const char*>(ciphertext->data()),
                             ciphertext->size());
  result.insert(result.end(), cts.begin(), cts.end());
  return result;
}

}  // namespace mac
}  // namespace fido
}  // namespace device
