// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/fido/mac/credential_metadata.h"

#include <ostream>
#include <string_view>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"
#include "third_party/boringssl/src/include/openssl/rand.h"

namespace device::fido::mac {

static constexpr size_t kNonceLength = 12;

namespace {

// MakeAad returns the concatenation of |version| and |rp_id|,
// which is used as the additional authenticated data (AAD) input to the AEAD.
std::vector<uint8_t> MakeAad(CredentialMetadata::Version version,
                             const std::string& rp_id) {
  std::vector<uint8_t> result = {static_cast<uint8_t>(version)};
  result.insert(result.end(), rp_id.data(), rp_id.data() + rp_id.size());
  return result;
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

  std::vector<uint8_t> Seal(Algorithm alg,
                            base::span<const uint8_t> nonce,
                            base::span<const uint8_t> plaintext,
                            base::span<const uint8_t> authenticated_data) const;

  std::optional<std::vector<uint8_t>> Unseal(
      Algorithm alg,
      base::span<const uint8_t> nonce,
      base::span<const uint8_t> ciphertext,
      base::span<const uint8_t> authenticated_data) const;

  std::string HmacForStorage(std::string_view data) const;

 private:
  static std::optional<crypto::Aead::AeadAlgorithm> ToAeadAlgorithm(
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

std::vector<uint8_t> Cryptor::Seal(
    Algorithm algorithm,
    base::span<const uint8_t> nonce,
    base::span<const uint8_t> plaintext,
    base::span<const uint8_t> authenticated_data) const {
  const std::string key = DeriveKey(algorithm);
  crypto::Aead aead(*ToAeadAlgorithm(algorithm));
  aead.Init(&key);
  return aead.Seal(plaintext, nonce, authenticated_data);
}

std::optional<std::vector<uint8_t>> Cryptor::Unseal(
    Algorithm algorithm,
    base::span<const uint8_t> nonce,
    base::span<const uint8_t> ciphertext,
    base::span<const uint8_t> authenticated_data) const {
  const std::string key = DeriveKey(algorithm);
  crypto::Aead aead(*ToAeadAlgorithm(algorithm));
  aead.Init(&key);
  return aead.Open(ciphertext, nonce, authenticated_data);
}

std::string Cryptor::HmacForStorage(std::string_view data) const {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  const std::string key = DeriveKey(Algorithm::kHmacSha256);
  std::vector<uint8_t> digest(hmac.DigestLength());
  CHECK(hmac.Init(key));
  CHECK(hmac.Sign(data, digest.data(), hmac.DigestLength()));

  // The keychain fields that store RP ID and User ID seem to only accept
  // NSString (not NSData), so we HexEncode to ensure the result to be
  // UTF-8-decodable.
  return base::HexEncode(digest);
}

// static
std::optional<crypto::Aead::AeadAlgorithm> Cryptor::ToAeadAlgorithm(
    Algorithm alg) {
  switch (alg) {
    case Algorithm::kAes256Gcm:
      return crypto::Aead::AES_256_GCM;
    case Algorithm::kAes256GcmSiv:
      return crypto::Aead::AES_256_GCM_SIV;
    case Algorithm::kHmacSha256:
      NOTREACHED() << "invalid AEAD";
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
CredentialMetadata::Version CredentialMetadata::CurrentVersion() {
  return CredentialMetadata::Version::kV4;
}

// static
CredentialMetadata CredentialMetadata::FromPublicKeyCredentialUserEntity(
    const PublicKeyCredentialUserEntity& user,
    bool is_resident) {
  return CredentialMetadata(
      /*version=*/CurrentVersion(),
      /*user_id=*/user.id,
      /*user_name=*/user.name.value_or(""),
      /*user_display_name=*/user.display_name.value_or(""),
      /*is_resident=*/is_resident,
      // All new credentials use zero counters:
      CredentialMetadata::SignCounter::kZero);
}

PublicKeyCredentialUserEntity
CredentialMetadata::ToPublicKeyCredentialUserEntity() const {
  PublicKeyCredentialUserEntity user_entity(user_id);
  if (!user_name.empty()) {
    user_entity.name = user_name;
  }
  if (!user_display_name.empty()) {
    user_entity.display_name = user_display_name;
  }
  return user_entity;
}

CredentialMetadata::CredentialMetadata(Version version,
                                       std::vector<uint8_t> user_id,
                                       std::string user_name,
                                       std::string user_display_name,
                                       bool is_resident,
                                       SignCounter counter_type)
    : version(version),
      user_id(std::move(user_id)),
      user_name(std::move(user_name)),
      user_display_name(std::move(user_display_name)),
      is_resident(is_resident),
      sign_counter_type(counter_type) {}

CredentialMetadata::CredentialMetadata(const CredentialMetadata&) = default;
CredentialMetadata::CredentialMetadata(CredentialMetadata&&) = default;
CredentialMetadata& CredentialMetadata::operator=(const CredentialMetadata&) =
    default;
CredentialMetadata& CredentialMetadata::operator=(CredentialMetadata&&) =
    default;
CredentialMetadata::~CredentialMetadata() = default;

bool CredentialMetadata::operator==(const CredentialMetadata& other) const {
  return version == other.version && user_id == other.user_id &&
         user_name == other.user_name &&
         user_display_name == other.user_display_name &&
         is_resident == other.is_resident &&
         sign_counter_type == other.sign_counter_type;
}

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

std::vector<uint8_t> SealCredentialMetadata(
    const std::string& secret,
    const std::string& rp_id,
    const CredentialMetadata& metadata) {
  // We only encrypt the most recent CredentialMetadata scheme in practice,
  // except for tests.
  DCHECK_GE(metadata.version, CredentialMetadata::Version::kV3);

  // CBOR-encode the CredentialMetadata. Then AES-GCM encrypt, and authenticate
  // with the RP ID.
  cbor::Value::ArrayValue cbor_metadata;
  cbor_metadata.emplace_back(cbor::Value(metadata.user_id));
  cbor_metadata.emplace_back(
      cbor::Value(MaybeTruncateWithTrailingEllipsis(metadata.user_name),
                  cbor::Value::Type::BYTE_STRING));
  cbor_metadata.emplace_back(
      cbor::Value(MaybeTruncateWithTrailingEllipsis(metadata.user_display_name),
                  cbor::Value::Type::BYTE_STRING));
  cbor_metadata.emplace_back(cbor::Value(metadata.is_resident));
  cbor_metadata.emplace_back(
      cbor::Value(static_cast<uint8_t>(metadata.sign_counter_type)));
  std::optional<std::vector<uint8_t>> pt =
      cbor::Writer::Write(cbor::Value(std::move(cbor_metadata)));
  DCHECK(pt);

  std::vector<uint8_t> nonce(kNonceLength);
  RAND_bytes(nonce.data(), nonce.size());  // RAND_bytes always returns 1.
  const std::vector<uint8_t> ct =
      Cryptor(secret).Seal(Cryptor::Algorithm::kAes256Gcm, nonce, *pt,
                           MakeAad(metadata.version, rp_id));

  // The Credential ID is the concatenation of nonce and ciphertext.
  nonce.insert(nonce.end(), ct.begin(), ct.end());
  return nonce;
}

// UnsealLegacyCredentialId attempts to decrypt a credential ID that has been
// encrypted under the scheme for version 0 or 1, which is:
//    | version  |    nonce   | AEAD(pt=CBOR(metadata),    |
//    | (1 byte) | (12 bytes) |      nonce=nonce,          |
//    |          |            |      ad=(version, rpID))   |
// In these versions, the `version` field is not part of the AEAD pt. Version 0
// also lacks the `is_resident` boolean inside the metadata (i.e. all V0
// credentials are non-resident).
static std::optional<CredentialMetadata> UnsealLegacyCredentialId(
    const std::string& secret,
    const std::string& rp_id,
    base::span<const uint8_t> credential_id) {
  // Recover the nonce and check for the correct version byte. Then try to
  // decrypt the remaining bytes.
  if (credential_id.size() <= 1 + kNonceLength ||
      (credential_id[0] !=
           static_cast<uint8_t>(CredentialMetadata::Version::kV0) &&
       credential_id[0] !=
           static_cast<uint8_t>(CredentialMetadata::Version::kV1))) {
    return std::nullopt;
  }

  auto version = static_cast<CredentialMetadata::Version>(credential_id[0]);

  std::optional<std::vector<uint8_t>> plaintext = Cryptor(secret).Unseal(
      Cryptor::Algorithm::kAes256Gcm, credential_id.subspan(1, kNonceLength),
      credential_id.subspan(1 + kNonceLength), MakeAad(version, rp_id));
  if (!plaintext) {
    return std::nullopt;
  }

  // The recovered plaintext should decode into the CredentialMetadata struct.
  std::optional<cbor::Value> maybe_array = cbor::Reader::Read(*plaintext);
  if (!maybe_array || !maybe_array->is_array()) {
    return std::nullopt;
  }
  const cbor::Value::ArrayValue& array = maybe_array->GetArray();
  if (array.size() < 3 || !array[0].is_bytestring() ||
      !array[1].is_bytestring() || !array[2].is_bytestring()) {
    return std::nullopt;
  }
  auto user_id = array[0].GetBytestring();
  auto user_name = array[1].GetBytestringAsString();
  auto user_display_name = array[2].GetBytestringAsString();
  bool is_resident = false;

  DCHECK(version == CredentialMetadata::Version::kV0 ||
         version == CredentialMetadata::Version::kV1);
  if (version == CredentialMetadata::Version::kV0 && array.size() != 3) {
    return std::nullopt;
  }
  if (version == CredentialMetadata::Version::kV1) {
    if (array.size() != 4 || !array[3].is_bool()) {
      return std::nullopt;
    }
    is_resident = array[3].GetBool();
  }

  return CredentialMetadata(
      /*version=*/version,
      /*user_id=*/user_id,
      /*user_name=*/std::string(user_name),
      /*user_display_name=*/std::string(user_display_name),
      /*is_resident=*/is_resident,
      // V0 and V1 credentials implicitly use a timestamp counter.
      CredentialMetadata::SignCounter::kTimestamp);
}

// Attempts to unseal metadata V2 or later, which dropped the unencrypted
// version prefix. Since the metadata version is still part of the AEAD's
// authenticated data, this is generally called iteratively for each potential
// version. Returns nullopt if unsealing fails.
static std::optional<CredentialMetadata> UnsealV2OrLaterCredentialMetadata(
    CredentialMetadata::Version version,
    const std::string& secret,
    const std::string& rp_id,
    base::span<const uint8_t> credential_id) {
  DCHECK_GE(version, CredentialMetadata::Version::kV2);
  if (credential_id.size() <= kNonceLength) {
    return std::nullopt;
  }

  std::optional<std::vector<uint8_t>> plaintext = Cryptor(secret).Unseal(
      Cryptor::Algorithm::kAes256Gcm, credential_id.first(kNonceLength),
      credential_id.subspan(kNonceLength), MakeAad(version, rp_id));
  if (!plaintext) {
    return std::nullopt;
  }

  std::optional<cbor::Value> maybe_array = cbor::Reader::Read(base::make_span(
      reinterpret_cast<const uint8_t*>(plaintext->data()), plaintext->size()));
  if (!maybe_array || !maybe_array->is_array()) {
    return std::nullopt;
  }
  const cbor::Value::ArrayValue& array = maybe_array->GetArray();
  if (array.size() < 4 || !array[0].is_bytestring() ||
      !array[1].is_bytestring() || !array[2].is_bytestring() ||
      !array[3].is_bool()) {
    return std::nullopt;
  }
  if (version == CredentialMetadata::Version::kV2) {
    if (array.size() != 4) {
      return std::nullopt;
    }
    return CredentialMetadata(
        CredentialMetadata::Version::kV2, array[0].GetBytestring(),
        std::string(array[1].GetBytestringAsString()),
        std::string(array[2].GetBytestringAsString()), array[3].GetBool(),
        // V2 credentials implicitly use a zero counter.
        CredentialMetadata::SignCounter::kZero);
  }

  static_assert(
      CredentialMetadata::Version::MAX_VERSION ==
          CredentialMetadata::Version::kV4,
      "Ensure unsealing code is able to handle added CredentialMetadata "
      "versions");
  DCHECK_GE(version, CredentialMetadata::Version::kV3);
  if (array.size() != 5) {
    return std::nullopt;
  }
  // Decode SignCounter enum:
  const int64_t counter_type = array[4].GetUnsigned();
  if (counter_type < 1) {
    return std::nullopt;
  }
  return CredentialMetadata(version, array[0].GetBytestring(),
                            std::string(array[1].GetBytestringAsString()),
                            std::string(array[2].GetBytestringAsString()),
                            array[3].GetBool(),
                            CredentialMetadata::SignCounter(counter_type));
}

std::optional<CredentialMetadata> UnsealMetadataFromLegacyCredentialId(
    const std::string& secret,
    const std::string& rp_id,
    base::span<const uint8_t> credential_id) {
  // Trial decrypt under V2 first, and if that fails try again with V0/V1.
  std::optional<CredentialMetadata> credential_metadata =
      UnsealV2OrLaterCredentialMetadata(CredentialMetadata::Version::kV2,
                                        secret, rp_id, credential_id);
  if (credential_metadata) {
    return credential_metadata;
  }
  return UnsealLegacyCredentialId(secret, rp_id, credential_id);
}

std::optional<CredentialMetadata> UnsealMetadataFromApplicationTag(
    const std::string& secret,
    const std::string& rp_id,
    base::span<const uint8_t> application_tag) {
  static_assert(
      CredentialMetadata::Version::MAX_VERSION ==
          CredentialMetadata::Version::kV4,
      "Ensure unsealing code is able to handle added CredentialMetadata "
      "versions");

  // kSecAttrApplicationTag only stores >= V3 metadata. This needs trial
  // decryption because the version is part of the AEAD authententication tag.
  for (const auto version :
       {CredentialMetadata::Version::kV3, CredentialMetadata::Version::kV4}) {
    if (std::optional<CredentialMetadata> metadata =
            UnsealV2OrLaterCredentialMetadata(version, secret, rp_id,
                                              application_tag)) {
      return metadata;
    }
  }
  return std::nullopt;
}

std::string EncodeRpIdAndUserIdDeprecated(const std::string& secret,
                                          const std::string& rp_id,
                                          base::span<const uint8_t> user_id) {
  // Encoding RP ID along with the user ID hides whether the same user ID was
  // reused on different RPs.
  const auto* user_id_data = reinterpret_cast<const char*>(user_id.data());
  return Cryptor(secret).HmacForStorage(
      rp_id + "/" + std::string(user_id_data, user_id_data + user_id.size()));
}

std::string EncodeRpId(const std::string& secret, const std::string& rp_id) {
  // Encrypt with a fixed nonce to make the result deterministic while still
  // allowing the RP ID to be recovered from the ciphertext later.
  static constexpr std::array<uint8_t, kNonceLength> fixed_zero_nonce = {};
  base::span<const uint8_t> pt(reinterpret_cast<const uint8_t*>(rp_id.data()),
                               rp_id.size());
  // Using AES-GCM with a fixed nonce would break confidentiality, so this uses
  // AES-GCM-SIV instead.
  const std::vector<uint8_t> ct =
      Cryptor(secret).Seal(Cryptor::Algorithm::kAes256GcmSiv, fixed_zero_nonce,
                           pt, /*authenticated_data=*/{});

  // HexEncode to ensure that the result is valid UTF-8. The result of this
  // function will be converted to an NSString via SysUTF8ToNSString and
  // therefore must be valid for that.
  return base::HexEncode(ct);
}

std::optional<std::string> DecodeRpId(const std::string& secret,
                                      const std::string& ciphertext) {
  std::vector<uint8_t> ct;
  if (!base::HexStringToBytes(ciphertext, &ct)) {
    return std::nullopt;
  }
  static constexpr std::array<uint8_t, kNonceLength> fixed_zero_nonce = {};
  std::optional<std::vector<uint8_t>> pt = Cryptor(secret).Unseal(
      Cryptor::Algorithm::kAes256GcmSiv, fixed_zero_nonce, ct,
      /*authenticated_data=*/{});
  if (!pt) {
    return std::nullopt;
  }
  return std::string(pt->begin(), pt->end());
}

std::vector<uint8_t> SealLegacyCredentialIdForTestingOnly(
    CredentialMetadata::Version version,
    const std::string& secret,
    const std::string& rp_id,
    const std::vector<uint8_t>& user_id,
    const std::string& user_name,
    const std::string& user_display_name,
    bool is_resident) {
  DCHECK_LT(version, CredentialMetadata::Version::kV3);

  std::vector<uint8_t> result;
  if (version < CredentialMetadata::Version::kV2) {
    result.push_back(static_cast<uint8_t>(version));
  }
  auto nonce_begin = result.insert(result.end(), 12, 0);
  base::span<uint8_t> nonce(nonce_begin, result.end());
  DCHECK_EQ(nonce.size(), 12u);
  RAND_bytes(nonce.data(), nonce.size());  // RAND_bytes always returns 1.

  // Only V1 includes the `is_resident` bit. `sign_counter_type=kTimestamp` was
  // implicit before V3 and thus not encoded.
  cbor::Value::ArrayValue cbor_metadata;
  cbor_metadata.emplace_back(cbor::Value(user_id));
  cbor_metadata.emplace_back(
      cbor::Value(user_name, cbor::Value::Type::BYTE_STRING));
  cbor_metadata.emplace_back(
      cbor::Value(user_display_name, cbor::Value::Type::BYTE_STRING));
  DCHECK(version > CredentialMetadata::Version::kV0 || !is_resident);
  if (version > CredentialMetadata::Version::kV0) {
    cbor_metadata.emplace_back(cbor::Value(is_resident));
  }
  std::optional<std::vector<uint8_t>> pt =
      cbor::Writer::Write(cbor::Value(std::move(cbor_metadata)));
  DCHECK(pt);

  std::vector<uint8_t> aad;
  aad.push_back(static_cast<uint8_t>(version));
  aad.insert(aad.end(), rp_id.data(), rp_id.data() + rp_id.size());
  const std::vector<uint8_t> ct =
      Cryptor(secret).Seal(Cryptor::Algorithm::kAes256Gcm, nonce, *pt, aad);
  result.insert(result.end(), ct.begin(), ct.end());
  return result;
}

}  // namespace device::fido::mac
