// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/keypair.h"

#include "base/logging.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
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

bool IsSupportedEvpId(int evp_id) {
  return evp_id == EVP_PKEY_RSA || evp_id == EVP_PKEY_EC;
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
  OpenSSLErrStackTracer err_tracer(FROM_HERE);

  bssl::UniquePtr<EC_KEY> ec_key(
      EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  CHECK(ec_key);
  CHECK(EC_KEY_generate_key(ec_key.get()));

  bssl::UniquePtr<EVP_PKEY> key(EVP_PKEY_new());
  CHECK(EVP_PKEY_set1_EC_KEY(key.get(), ec_key.get()));
  return PrivateKey(std::move(key));
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

std::vector<uint8_t> PrivateKey::ToSubjectPublicKeyInfo() const {
  return ExportEVPPublicKey(key_.get());
}

std::vector<uint8_t> PrivateKey::ToUncompressedForm() const {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);

  std::vector<uint8_t> buf(65);
  EC_KEY* ec_key = EVP_PKEY_get0_EC_KEY(key_.get());
  CHECK(EC_POINT_point2oct(
      EC_KEY_get0_group(ec_key), EC_KEY_get0_public_key(ec_key),
      POINT_CONVERSION_UNCOMPRESSED, buf.data(), buf.size(), /*ctx=*/nullptr));

  return buf;
}

PrivateKey::PrivateKey(bssl::UniquePtr<EVP_PKEY> key) : key_(std::move(key)) {}

bool PrivateKey::IsRsa() const {
  return EVP_PKEY_id(key_.get()) == EVP_PKEY_RSA;
}

bool PrivateKey::IsEc() const {
  return EVP_PKEY_id(key_.get()) == EVP_PKEY_EC;
}

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

std::vector<uint8_t> PublicKey::ToSubjectPublicKeyInfo() const {
  return ExportEVPPublicKey(key_.get());
}

bool PublicKey::IsRsa() const {
  return EVP_PKEY_id(key_.get()) == EVP_PKEY_RSA;
}

bool PublicKey::IsEc() const {
  return EVP_PKEY_id(key_.get()) == EVP_PKEY_EC;
}

PublicKey::PublicKey(bssl::UniquePtr<EVP_PKEY> key) : key_(std::move(key)) {}

}  // namespace crypto::keypair
