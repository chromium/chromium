// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/keypair.h"

#include "base/logging.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace crypto::keypair {

namespace {

bssl::UniquePtr<EVP_PKEY> GenerateRsa(size_t bits) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);

  bssl::UniquePtr<RSA> rsa_key(RSA_new());
  bssl::UniquePtr<BIGNUM> bn(BN_new());

  CHECK(rsa_key.get());
  CHECK(bn.get());
  CHECK(BN_set_word(bn.get(), 65537L));

  CHECK(RSA_generate_key_ex(rsa_key.get(), bits, bn.get(), nullptr));

  bssl::UniquePtr<EVP_PKEY> key(EVP_PKEY_new());
  CHECK(EVP_PKEY_set1_RSA(key.get(), rsa_key.get()));

  return key;
}

bssl::UniquePtr<EVP_PKEY> GenerateEc(int nid) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);

  bssl::UniquePtr<EC_KEY> ec_key(EC_KEY_new_by_curve_name(nid));
  CHECK(ec_key);
  CHECK(EC_KEY_generate_key(ec_key.get()));

  bssl::UniquePtr<EVP_PKEY> key(EVP_PKEY_new());
  CHECK(EVP_PKEY_set1_EC_KEY(key.get(), ec_key.get()));
  return key;
}

bool IsSupportedEvpId(int evp_id) {
  return evp_id == EVP_PKEY_RSA || evp_id == EVP_PKEY_EC ||
         evp_id == EVP_PKEY_ED25519;
}

std::vector<uint8_t> ExportEVPPublicKey(EVP_PKEY* pkey) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  bssl::ScopedCBB cbb;

  CHECK(CBB_init(cbb.get(), 0));
  CHECK(EVP_marshal_public_key(cbb.get(), pkey));

  uint8_t* data;
  size_t len;
  CHECK(CBB_finish(cbb.get(), &data, &len));

  std::vector<uint8_t> result(len);
  // SAFETY: OpenSSL freshly allocated data for us and ensured it pointed to at
  // least len bytes.
  UNSAFE_BUFFERS(result.assign(data, data + len));
  OPENSSL_free(data);
  return result;
}

bssl::UniquePtr<EVP_PKEY> EVP_PKEYFromEcPoint(const EC_GROUP* group,
                                              base::span<const uint8_t> p) {
  bssl::UniquePtr<EC_KEY> ec(EC_KEY_new());
  CHECK(ec);
  CHECK(EC_KEY_set_group(ec.get(), group));

  if (!EC_KEY_oct2key(ec.get(), p.data(), p.size(), nullptr)) {
    return nullptr;
  }

  // The only failure mode for EVP_PKEY_new() is memory allocation failures,
  // and the only failure mode for EVP_PKEY_set1_EC_KEY() is being passed a null
  // key or EC_KEY object.
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  CHECK(pkey);
  CHECK(EVP_PKEY_set1_EC_KEY(pkey.get(), ec.get()));
  return pkey;
}

std::vector<uint8_t> EvpToUncompressedX962Point(EVP_PKEY* key) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);

  std::vector<uint8_t> ec_buffer(255);
  EC_KEY* ec_key = EVP_PKEY_get0_EC_KEY(key);
  size_t len = EC_POINT_point2oct(
      EC_KEY_get0_group(ec_key), EC_KEY_get0_public_key(ec_key),
      POINT_CONVERSION_UNCOMPRESSED, ec_buffer.data(), ec_buffer.size(),
      /*ctx=*/nullptr);
  CHECK(len);
  ec_buffer.resize(len);

  return ec_buffer;
}

}  // namespace

PrivateKey::PrivateKey(bssl::UniquePtr<EVP_PKEY> key, crypto::SubtlePassKey)
    : PrivateKey(std::move(key)) {}
PrivateKey::~PrivateKey() = default;
PrivateKey::PrivateKey(PrivateKey&& other) = default;
PrivateKey::PrivateKey(const PrivateKey& other)
    : key_(bssl::UpRef(const_cast<PrivateKey&>(other).key())) {}
PrivateKey& PrivateKey::operator=(PrivateKey&& other) = default;
PrivateKey& PrivateKey::operator=(const PrivateKey& other) {
  key_ = bssl::UpRef(const_cast<PrivateKey&>(other).key());
  return *this;
}

// static
PrivateKey PrivateKey::GenerateRsa2048() {
  return PrivateKey(GenerateRsa(2048));
}

// static
PrivateKey PrivateKey::GenerateRsa4096() {
  return PrivateKey(GenerateRsa(4096));
}

// static
PrivateKey PrivateKey::GenerateEcP256() {
  return PrivateKey(GenerateEc(NID_X9_62_prime256v1));
}

// static
PrivateKey PrivateKey::GenerateEcP384() {
  return PrivateKey(GenerateEc(NID_secp384r1));
}

// static
PrivateKey PrivateKey::GenerateEcP521() {
  return PrivateKey(GenerateEc(NID_secp521r1));
}

// static
PrivateKey PrivateKey::GenerateEd25519() {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);

  std::array<uint8_t, ED25519_PUBLIC_KEY_LEN> unused_pubkey;
  std::array<uint8_t, ED25519_PRIVATE_KEY_LEN> privkey;

  ED25519_keypair(unused_pubkey.data(), privkey.data());

  // EVP_PKEY_new_raw_public_key() takes only the 32-byte RFC 8032 "seed" at the
  // start of the private key, not the BoringSSL-format "full" private key.
  return FromEd25519PrivateKey(base::span(privkey).first<32>());
}

// static
std::optional<PrivateKey> PrivateKey::FromPrivateKeyInfo(
    base::span<const uint8_t> pki) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);

  CBS cbs(pki);
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_parse_private_key(&cbs));
  if (!pkey || CBS_len(&cbs) != 0) {
    LOG(WARNING) << "Malformed PrivateKeyInfo or trailing data";
    return std::nullopt;
  }

  auto id = EVP_PKEY_id(pkey.get());
  if (!IsSupportedEvpId(id)) {
    LOG(WARNING) << "Unsupported key type (EVP ID: " << id << ")";
    return std::nullopt;
  }

  return std::optional<PrivateKey>(PrivateKey(std::move(pkey)));
}

// static
PrivateKey PrivateKey::FromEd25519PrivateKey(
    base::span<const uint8_t, 32> key) {
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new_raw_private_key(
      EVP_PKEY_ED25519, nullptr, key.data(), key.size()));
  CHECK(pkey);
  return PrivateKey(std::move(pkey));
}

std::vector<uint8_t> PrivateKey::ToPrivateKeyInfo() const {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  bssl::ScopedCBB cbb;

  CHECK(CBB_init(cbb.get(), 0));
  CHECK(EVP_marshal_private_key(cbb.get(), key_.get()));

  uint8_t* data;
  size_t len;
  CHECK(CBB_finish(cbb.get(), &data, &len));

  std::vector<uint8_t> result(len);
  // SAFETY: OpenSSL freshly allocated data for us and ensured it pointed to at
  // least len bytes.
  UNSAFE_BUFFERS(result.assign(data, data + len));
  OPENSSL_free(data);
  return result;
}

std::array<uint8_t, 32> PrivateKey::ToEd25519PrivateKey() const {
  CHECK(IsEd25519());
  std::array<uint8_t, 32> result;
  size_t len = std::size(result);
  CHECK(EVP_PKEY_get_raw_private_key(key_.get(), result.data(), &len));
  CHECK(len == std::size(result));
  return result;
}

std::vector<uint8_t> PrivateKey::ToSubjectPublicKeyInfo() const {
  return ExportEVPPublicKey(key_.get());
}

std::vector<uint8_t> PrivateKey::ToUncompressedX962Point() const {
  return EvpToUncompressedX962Point(key_.get());
}

std::array<uint8_t, 32> PrivateKey::ToEd25519PublicKey() const {
  CHECK(IsEd25519());
  std::array<uint8_t, 32> result;
  size_t len = std::size(result);
  CHECK(EVP_PKEY_get_raw_public_key(key_.get(), result.data(), &len));
  CHECK(len == std::size(result));
  return result;
}

bool PrivateKey::IsRsa() const {
  return EVP_PKEY_id(key_.get()) == EVP_PKEY_RSA;
}

bool PrivateKey::IsEc() const {
  return EVP_PKEY_id(key_.get()) == EVP_PKEY_EC;
}

bool PrivateKey::IsEd25519() const {
  return EVP_PKEY_id(key_.get()) == EVP_PKEY_ED25519;
}

bool PrivateKey::IsEcP256() const {
  return EVP_PKEY_get_ec_curve_nid(key_.get()) == NID_X9_62_prime256v1;
}

bool PrivateKey::IsEcP384() const {
  return EVP_PKEY_get_ec_curve_nid(key_.get()) == NID_secp384r1;
}

bool PrivateKey::IsEcP521() const {
  return EVP_PKEY_get_ec_curve_nid(key_.get()) == NID_secp521r1;
}

PrivateKey::PrivateKey(bssl::UniquePtr<EVP_PKEY> key) : key_(std::move(key)) {}

PublicKey::PublicKey(bssl::UniquePtr<EVP_PKEY> key, crypto::SubtlePassKey)
    : PublicKey(std::move(key)) {}
PublicKey::~PublicKey() = default;
PublicKey::PublicKey(PublicKey&& other) = default;
PublicKey::PublicKey(const PublicKey& other)
    : key_(bssl::UpRef(const_cast<PublicKey&>(other).key())) {}
PublicKey& PublicKey::operator=(PublicKey&& other) = default;
PublicKey& PublicKey::operator=(const PublicKey& other) {
  key_ = bssl::UpRef(const_cast<PublicKey&>(other).key());
  return *this;
}

// static
PublicKey PublicKey::FromPrivateKey(const PrivateKey& key) {
  return *FromSubjectPublicKeyInfo(key.ToSubjectPublicKeyInfo());
}

// static
std::optional<PublicKey> PublicKey::FromSubjectPublicKeyInfo(
    base::span<const uint8_t> spki) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);

  CBS cbs(spki);
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_parse_public_key(&cbs));
  if (!pkey || CBS_len(&cbs) != 0) {
    LOG(WARNING) << "Malformed PublicKeyInfo or trailing data";
    return std::nullopt;
  }

  auto id = EVP_PKEY_id(pkey.get());
  if (!IsSupportedEvpId(id)) {
    LOG(WARNING) << "Unsupported key type (EVP ID: " << id << ")";
    return std::nullopt;
  }

  return std::optional<PublicKey>(PublicKey(std::move(pkey)));
}

std::optional<PublicKey> PublicKey::FromRsaPublicKeyComponents(
    base::span<const uint8_t> n,
    base::span<const uint8_t> e) {
  bssl::UniquePtr<BIGNUM> bn_n(BN_bin2bn(n.data(), n.size(), nullptr));
  bssl::UniquePtr<BIGNUM> bn_e(BN_bin2bn(e.data(), e.size(), nullptr));
  if (!bn_n || !bn_e) {
    return std::nullopt;
  }

  bssl::UniquePtr<RSA> rsa(RSA_new_public_key(bn_n.get(), bn_e.get()));
  if (!rsa) {
    return std::nullopt;
  }

  // The only failure mode for EVP_PKEY_new() is memory allocation failures,
  // and the only failure mode for EVP_PKEY_set1_RSA() is being passed a null
  // key or RSA object.
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  CHECK(pkey);
  CHECK(EVP_PKEY_set1_RSA(pkey.get(), rsa.get()));
  return PublicKey(std::move(pkey));
}

// static
std::optional<PublicKey> PublicKey::FromEcP256Point(
    base::span<const uint8_t> p) {
  auto key = EVP_PKEYFromEcPoint(EC_group_p256(), p);
  if (!key) {
    return std::nullopt;
  }
  return PublicKey(std::move(key));
}

// static
std::optional<PublicKey> PublicKey::FromEcP384Point(
    base::span<const uint8_t> p) {
  auto key = EVP_PKEYFromEcPoint(EC_group_p384(), p);
  if (!key) {
    return std::nullopt;
  }
  return PublicKey(std::move(key));
}

// static
std::optional<PublicKey> PublicKey::FromEcP521Point(
    base::span<const uint8_t> p) {
  auto key = EVP_PKEYFromEcPoint(EC_group_p521(), p);
  if (!key) {
    return std::nullopt;
  }
  return PublicKey(std::move(key));
}

// static
PublicKey PublicKey::FromEd25519PublicKey(base::span<const uint8_t, 32> key) {
  static_assert(std::size(key) == ED25519_PUBLIC_KEY_LEN);

  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr, key.data(), key.size()));
  CHECK(pkey);
  return PublicKey(std::move(pkey));
}

std::vector<uint8_t> PublicKey::ToSubjectPublicKeyInfo() const {
  return ExportEVPPublicKey(key_.get());
}

std::vector<uint8_t> PublicKey::ToUncompressedX962Point() const {
  return EvpToUncompressedX962Point(key_.get());
}

std::vector<uint8_t> PublicKey::GetRsaExponent() const {
  CHECK(IsRsa());
  RSA* rsa = EVP_PKEY_get0_RSA(key_.get());
  const BIGNUM* e = RSA_get0_e(rsa);
  std::vector<uint8_t> result(BN_num_bytes(e));
  BN_bn2bin(e, result.data());
  return result;
}

std::vector<uint8_t> PublicKey::GetRsaModulus() const {
  CHECK(IsRsa());
  RSA* rsa = EVP_PKEY_get0_RSA(key_.get());
  const BIGNUM* n = RSA_get0_n(rsa);
  std::vector<uint8_t> result(BN_num_bytes(n));
  BN_bn2bin(n, result.data());
  return result;
}

bool PublicKey::IsRsa() const {
  return EVP_PKEY_id(key_.get()) == EVP_PKEY_RSA;
}

bool PublicKey::IsEc() const {
  return EVP_PKEY_id(key_.get()) == EVP_PKEY_EC;
}

bool PublicKey::IsEd25519() const {
  return EVP_PKEY_id(key_.get()) == EVP_PKEY_ED25519;
}

bool PublicKey::IsEcP256() const {
  return EVP_PKEY_get_ec_curve_nid(key_.get()) == NID_X9_62_prime256v1;
}

bool PublicKey::IsEcP384() const {
  return EVP_PKEY_get_ec_curve_nid(key_.get()) == NID_secp384r1;
}

bool PublicKey::IsEcP521() const {
  return EVP_PKEY_get_ec_curve_nid(key_.get()) == NID_secp521r1;
}

PublicKey::PublicKey(bssl::UniquePtr<EVP_PKEY> key) : key_(std::move(key)) {}

}  // namespace crypto::keypair
