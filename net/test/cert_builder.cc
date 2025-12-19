// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/cert_builder.h"

#include <algorithm>
#include <bit>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/extend.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/time/time.h"
#include "crypto/evp.h"
#include "crypto/hash.h"
#include "crypto/keypair.h"
#include "crypto/sha2.h"
#include "crypto/subtle_passkey.h"
#include "net/base/hash_value.h"
#include "net/cert/asn1_util.h"
#include "net/cert/ct_objects_extractor.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/qwac.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/time_conversions.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/key_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/pki/certificate_policies.h"
#include "third_party/boringssl/src/pki/extended_key_usage.h"
#include "third_party/boringssl/src/pki/input.h"
#include "third_party/boringssl/src/pki/merkle_tree.h"
#include "third_party/boringssl/src/pki/parse_certificate.h"
#include "third_party/boringssl/src/pki/parse_values.h"
#include "third_party/boringssl/src/pki/parser.h"
#include "third_party/boringssl/src/pki/trust_store.h"
#include "third_party/boringssl/src/pki/verify_signed_data.h"
#include "url/gurl.h"

namespace net {

namespace {

constexpr char kSimpleChainHostname[] = "www.example.com";

std::string Sha256WithRSAEncryption() {
  const uint8_t kSha256WithRSAEncryption[] = {0x30, 0x0D, 0x06, 0x09, 0x2a,
                                              0x86, 0x48, 0x86, 0xf7, 0x0d,
                                              0x01, 0x01, 0x0b, 0x05, 0x00};
  return std::string(std::begin(kSha256WithRSAEncryption),
                     std::end(kSha256WithRSAEncryption));
}

std::string Sha1WithRSAEncryption() {
  const uint8_t kSha1WithRSAEncryption[] = {0x30, 0x0D, 0x06, 0x09, 0x2a,
                                            0x86, 0x48, 0x86, 0xf7, 0x0d,
                                            0x01, 0x01, 0x05, 0x05, 0x00};
  return std::string(std::begin(kSha1WithRSAEncryption),
                     std::end(kSha1WithRSAEncryption));
}

std::string EcdsaWithSha256() {
  const uint8_t kDer[] = {0x30, 0x0a, 0x06, 0x08, 0x2a, 0x86,
                          0x48, 0xce, 0x3d, 0x04, 0x03, 0x02};
  return std::string(std::begin(kDer), std::end(kDer));
}

std::string EcdsaWithSha1() {
  const uint8_t kDer[] = {0x30, 0x09, 0x06, 0x07, 0x2a, 0x86,
                          0x48, 0xce, 0x3d, 0x04, 0x01};
  return std::string(std::begin(kDer), std::end(kDer));
}

// Adds bytes to the given CBB.
// The argument ordering follows the boringssl CBB_* api style.
bool CBBAddBytes(CBB* cbb, base::span<const uint8_t> bytes) {
  return CBB_add_bytes(cbb, bytes.data(), bytes.size());
}

// Adds bytes (specified as a std::string_view) to the given CBB.
// The argument ordering follows the boringssl CBB_* api style.
bool CBBAddBytes(CBB* cbb, std::string_view bytes) {
  return CBBAddBytes(cbb, base::as_byte_span(bytes));
}

// Adds bytes (from fixed size array) to the given CBB.
// The argument ordering follows the boringssl CBB_* api style.
template <size_t N>
bool CBBAddBytes(CBB* cbb, const uint8_t (&data)[N]) {
  return CBB_add_bytes(cbb, data, N);
}

// Adds tagged element to the given CBB.
// The argument ordering follows the boringssl CBB_* api style.
bool CBBAddAsn1Element(CBB* cbb,
                       CBS_ASN1_TAG tag,
                       base::span<const uint8_t> bytes) {
  return CBB_add_asn1_element(cbb, tag, bytes.data(), bytes.size());
}

// Finalizes the CBB to a std::string.
std::string FinishCBB(CBB* cbb) {
  size_t cbb_len;
  uint8_t* cbb_bytes;

  if (!CBB_finish(cbb, &cbb_bytes, &cbb_len)) {
    ADD_FAILURE() << "CBB_finish() failed";
    return std::string();
  }

  bssl::UniquePtr<uint8_t> delete_bytes(cbb_bytes);
  return std::string(reinterpret_cast<char*>(cbb_bytes), cbb_len);
}

// Finalizes the CBB to a std::vector.
std::vector<uint8_t> FinishCBBToVector(CBB* cbb) {
  size_t cbb_len;
  uint8_t* cbb_bytes;

  if (!CBB_finish(cbb, &cbb_bytes, &cbb_len)) {
    ADD_FAILURE() << "CBB_finish() failed";
    return {};
  }

  bssl::UniquePtr<uint8_t> delete_bytes(cbb_bytes);
  return std::vector<uint8_t>(cbb_bytes, UNSAFE_TODO(cbb_bytes + cbb_len));
}

}  // namespace

CertBuilder::SctConfig::SctConfig() = default;
CertBuilder::SctConfig::SctConfig(std::string log_id,
                                  bssl::UniquePtr<EVP_PKEY> log_key,
                                  base::Time timestamp)
    : log_id(std::move(log_id)),
      log_key(std::move(log_key)),
      timestamp(timestamp) {}
CertBuilder::SctConfig::SctConfig(const SctConfig& other)
    : SctConfig(other.log_id,
                bssl::UpRef(other.log_key.get()),
                other.timestamp) {}
CertBuilder::SctConfig::SctConfig(SctConfig&&) = default;
CertBuilder::SctConfig::~SctConfig() = default;
CertBuilder::SctConfig& CertBuilder::SctConfig::operator=(
    const SctConfig& other) {
  log_id = other.log_id;
  log_key = bssl::UpRef(other.log_key.get());
  timestamp = other.timestamp;
  return *this;
}
CertBuilder::SctConfig& CertBuilder::SctConfig::operator=(SctConfig&&) =
    default;

CertBuilder::CertBuilder(CRYPTO_BUFFER* orig_cert, CertBuilder* issuer)
    : CertBuilder(orig_cert, issuer, /*unique_subject_key_identifier=*/true) {}

// static
std::unique_ptr<CertBuilder> CertBuilder::FromFile(
    const base::FilePath& cert_and_key_file,
    CertBuilder* issuer) {
  scoped_refptr<X509Certificate> cert = ImportCertFromFile(cert_and_key_file);
  if (!cert)
    return nullptr;

  bssl::UniquePtr<EVP_PKEY> private_key(
      key_util::LoadEVP_PKEYFromPEM(cert_and_key_file));
  if (!private_key)
    return nullptr;

  auto builder = base::WrapUnique(new CertBuilder(cert->cert_buffer(), issuer));
  builder->key_ = std::move(private_key);
  return builder;
}

// static
std::unique_ptr<CertBuilder> CertBuilder::FromStaticCert(CRYPTO_BUFFER* cert,
                                                         EVP_PKEY* key) {
  std::unique_ptr<CertBuilder> builder = base::WrapUnique(
      new CertBuilder(cert, nullptr, /*unique_subject_key_identifier=*/false));
  // |cert_|, |key_|, and |subject_tlv_| must be initialized for |builder| to
  // function as the |issuer| of another CertBuilder.
  builder->cert_ = bssl::UpRef(cert);
  builder->key_ = bssl::UpRef(key);
  std::string_view subject_tlv;
  CHECK(asn1::ExtractSubjectFromDERCert(
      x509_util::CryptoBufferAsStringPiece(cert), &subject_tlv));
  builder->subject_tlv_ = std::string(subject_tlv);
  return builder;
}

// static
std::unique_ptr<CertBuilder> CertBuilder::FromStaticCertFile(
    const base::FilePath& cert_and_key_file) {
  scoped_refptr<X509Certificate> cert = ImportCertFromFile(cert_and_key_file);
  if (!cert)
    return nullptr;

  bssl::UniquePtr<EVP_PKEY> private_key(
      key_util::LoadEVP_PKEYFromPEM(cert_and_key_file));
  if (!private_key)
    return nullptr;

  return CertBuilder::FromStaticCert(cert->cert_buffer(), private_key.get());
}

// static
std::unique_ptr<CertBuilder> CertBuilder::FromSubjectPublicKeyInfo(
    base::span<const uint8_t> spki_der,
    CertBuilder* issuer) {
  DCHECK(issuer);
  auto builder = std::make_unique<CertBuilder>(/*orig_cert=*/nullptr, issuer);

  builder->key_ = crypto::evp::PublicKeyFromBytes(spki_der);
  if (!builder->key_) {
    return nullptr;
  }

  return builder;
}

CertBuilder::~CertBuilder() = default;

// static
std::vector<std::unique_ptr<CertBuilder>> CertBuilder::CreateSimpleChain(
    size_t chain_length) {
  std::vector<std::unique_ptr<CertBuilder>> chain;
  base::Time not_before = base::Time::Now() - base::Days(7);
  base::Time not_after = base::Time::Now() + base::Days(7);
  CertBuilder* parent_builder = nullptr;
  for (size_t remaining_chain_length = chain_length; remaining_chain_length;
       remaining_chain_length--) {
    auto builder = std::make_unique<CertBuilder>(nullptr, parent_builder);
    builder->SetValidity(not_before, not_after);
    if (remaining_chain_length > 1) {
      // CA properties:
      builder->SetBasicConstraints(/*is_ca=*/true, /*path_len=*/-1);
      builder->SetKeyUsages(
          {bssl::KEY_USAGE_BIT_KEY_CERT_SIGN, bssl::KEY_USAGE_BIT_CRL_SIGN});
    } else {
      // Leaf properties:
      builder->SetBasicConstraints(/*is_ca=*/false, /*path_len=*/-1);
      builder->SetKeyUsages({bssl::KEY_USAGE_BIT_DIGITAL_SIGNATURE});
      builder->SetExtendedKeyUsages({bssl::der::Input(bssl::kServerAuth)});
      builder->SetSubjectAltName(kSimpleChainHostname);
    }
    parent_builder = builder.get();
    chain.push_back(std::move(builder));
  }
  std::ranges::reverse(chain);
  return chain;
}

// static
std::array<std::unique_ptr<CertBuilder>, 3> CertBuilder::CreateSimpleChain3() {
  auto chain = CreateSimpleChain(3);
  return {std::move(chain[0]), std::move(chain[1]), std::move(chain[2])};
}

// static
std::array<std::unique_ptr<CertBuilder>, 2> CertBuilder::CreateSimpleChain2() {
  auto chain = CreateSimpleChain(2);
  return {std::move(chain[0]), std::move(chain[1])};
}

// static
std::optional<bssl::SignatureAlgorithm>
CertBuilder::DefaultSignatureAlgorithmForKey(EVP_PKEY* key) {
  if (EVP_PKEY_id(key) == EVP_PKEY_RSA)
    return bssl::SignatureAlgorithm::kRsaPkcs1Sha256;
  if (EVP_PKEY_id(key) == EVP_PKEY_EC)
    return bssl::SignatureAlgorithm::kEcdsaSha256;
  return std::nullopt;
}

// static
bool CertBuilder::SignData(bssl::SignatureAlgorithm signature_algorithm,
                           std::string_view tbs_data,
                           EVP_PKEY* key,
                           CBB* out_signature) {
  if (!key)
    return false;

  int expected_pkey_id = 1;
  const EVP_MD* digest;
  switch (signature_algorithm) {
    case bssl::SignatureAlgorithm::kRsaPkcs1Sha1:
      expected_pkey_id = EVP_PKEY_RSA;
      digest = EVP_sha1();
      break;
    case bssl::SignatureAlgorithm::kRsaPkcs1Sha256:
      expected_pkey_id = EVP_PKEY_RSA;
      digest = EVP_sha256();
      break;
    case bssl::SignatureAlgorithm::kRsaPkcs1Sha384:
      expected_pkey_id = EVP_PKEY_RSA;
      digest = EVP_sha384();
      break;
    case bssl::SignatureAlgorithm::kRsaPkcs1Sha512:
      expected_pkey_id = EVP_PKEY_RSA;
      digest = EVP_sha512();
      break;

    case bssl::SignatureAlgorithm::kEcdsaSha1:
      expected_pkey_id = EVP_PKEY_EC;
      digest = EVP_sha1();
      break;
    case bssl::SignatureAlgorithm::kEcdsaSha256:
      expected_pkey_id = EVP_PKEY_EC;
      digest = EVP_sha256();
      break;
    case bssl::SignatureAlgorithm::kEcdsaSha384:
      expected_pkey_id = EVP_PKEY_EC;
      digest = EVP_sha384();
      break;
    case bssl::SignatureAlgorithm::kEcdsaSha512:
      expected_pkey_id = EVP_PKEY_EC;
      digest = EVP_sha512();
      break;

    default:
      // Unsupported algorithms.
      return false;
  }

  return expected_pkey_id == EVP_PKEY_id(key) &&
         SignDataWithDigest(digest, tbs_data, key, out_signature);
}

// static
bool CertBuilder::SignDataWithDigest(const EVP_MD* digest,
                                     std::string_view tbs_data,
                                     EVP_PKEY* key,
                                     CBB* out_signature) {
  const uint8_t* tbs_bytes = reinterpret_cast<const uint8_t*>(tbs_data.data());
  bssl::ScopedEVP_MD_CTX ctx;
  uint8_t* sig_out;
  size_t sig_len;

  return EVP_DigestSignInit(ctx.get(), nullptr, digest, nullptr, key) &&
         EVP_DigestSign(ctx.get(), nullptr, &sig_len, tbs_bytes,
                        tbs_data.size()) &&
         CBB_reserve(out_signature, &sig_out, sig_len) &&
         EVP_DigestSign(ctx.get(), sig_out, &sig_len, tbs_bytes,
                        tbs_data.size()) &&
         CBB_did_write(out_signature, sig_len);
}

// static
std::string CertBuilder::SignatureAlgorithmToDer(
    bssl::SignatureAlgorithm signature_algorithm) {
  switch (signature_algorithm) {
    case bssl::SignatureAlgorithm::kRsaPkcs1Sha1:
      return Sha1WithRSAEncryption();
    case bssl::SignatureAlgorithm::kRsaPkcs1Sha256:
      return Sha256WithRSAEncryption();
    case bssl::SignatureAlgorithm::kEcdsaSha1:
      return EcdsaWithSha1();
    case bssl::SignatureAlgorithm::kEcdsaSha256:
      return EcdsaWithSha256();
    default:
      ADD_FAILURE();
      return std::string();
  }
}

// static
std::string CertBuilder::MakeRandomHexString(size_t num_bytes) {
  std::vector<uint8_t> rand_bytes(num_bytes);
  base::RandBytes(rand_bytes);
  return base::HexEncode(rand_bytes);
}

// static
std::vector<uint8_t> CertBuilder::BuildNameWithCommonNameOfType(
    std::string_view common_name,
    unsigned common_name_tag) {
  // See RFC 4519.
  static const uint8_t kCommonName[] = {0x55, 0x04, 0x03};

  // See RFC 5280, section 4.1.2.4.
  bssl::ScopedCBB cbb;
  CBB rdns, rdn, attr, type, value;
  if (!CBB_init(cbb.get(), 64) ||
      !CBB_add_asn1(cbb.get(), &rdns, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&rdns, &rdn, CBS_ASN1_SET) ||
      !CBB_add_asn1(&rdn, &attr, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&attr, &type, CBS_ASN1_OBJECT) ||
      !CBBAddBytes(&type, kCommonName) ||
      !CBB_add_asn1(&attr, &value, common_name_tag) ||
      !CBBAddBytes(&value, common_name)) {
    ADD_FAILURE();
    return {};
  }

  return FinishCBBToVector(cbb.get());
}

// static
std::vector<uint8_t> CertBuilder::BuildSequenceOfOid(
    std::vector<bssl::der::Input> oids) {
  bssl::ScopedCBB cbb;
  CBB sequence;
  if (!CBB_init(cbb.get(), 64) ||
      !CBB_add_asn1(cbb.get(), &sequence, CBS_ASN1_SEQUENCE)) {
    ADD_FAILURE();
    return {};
  }
  for (const auto& oid_value : oids) {
    CBB oid;
    if (!CBB_add_asn1(&sequence, &oid, CBS_ASN1_OBJECT) ||
        !CBBAddBytes(&oid, oid_value) || !CBB_flush(&sequence)) {
      ADD_FAILURE();
      return {};
    }
  }
  return FinishCBBToVector(cbb.get());
}

void CertBuilder::SetCertificateVersion(bssl::CertificateVersion version) {
  version_ = version;
  Invalidate();
}

void CertBuilder::SetExtension(const bssl::der::Input& oid,
                               std::string value,
                               bool critical) {
  auto& extension_value = extensions_[std::string(base::as_string_view(oid))];
  extension_value.critical = critical;
  extension_value.value = std::move(value);

  Invalidate();
}

void CertBuilder::EraseExtension(const bssl::der::Input& oid) {
  extensions_.erase(std::string(base::as_string_view(oid)));

  Invalidate();
}

void CertBuilder::ClearExtensions() {
  extensions_.clear();
  Invalidate();
}

void CertBuilder::SetBasicConstraints(bool is_ca, int path_len) {
  // From RFC 5280:
  //
  //   BasicConstraints ::= SEQUENCE {
  //        cA                      BOOLEAN DEFAULT FALSE,
  //        pathLenConstraint       INTEGER (0..MAX) OPTIONAL }
  bssl::ScopedCBB cbb;
  CBB basic_constraints;
  ASSERT_TRUE(CBB_init(cbb.get(), 64));
  ASSERT_TRUE(CBB_add_asn1(cbb.get(), &basic_constraints, CBS_ASN1_SEQUENCE));
  if (is_ca)
    ASSERT_TRUE(CBB_add_asn1_bool(&basic_constraints, true));
  if (path_len >= 0)
    ASSERT_TRUE(CBB_add_asn1_uint64(&basic_constraints, path_len));

  SetExtension(bssl::der::Input(bssl::kBasicConstraintsOid),
               FinishCBB(cbb.get()),
               /*critical=*/true);
}

namespace {
void AddNameConstraintsSubTrees(CBB* cbb,
                                const std::vector<std::string>& dns_names) {
  CBB subtrees;
  ASSERT_TRUE(CBB_add_asn1(
      cbb, &subtrees, CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0));
  for (const auto& name : dns_names) {
    CBB subtree;
    ASSERT_TRUE(CBB_add_asn1(&subtrees, &subtree, CBS_ASN1_SEQUENCE));
    CBB general_name;
    ASSERT_TRUE(
        CBB_add_asn1(&subtree, &general_name, CBS_ASN1_CONTEXT_SPECIFIC | 2));
    ASSERT_TRUE(CBBAddBytes(&general_name, name));
    ASSERT_TRUE(CBB_flush(&subtrees));
  }
  ASSERT_TRUE(CBB_flush(cbb));
}
}  // namespace

void CertBuilder::SetNameConstraintsDnsNames(
    const std::vector<std::string>& permitted_dns_names,
    const std::vector<std::string>& excluded_dns_names) {
  // From RFC 5280:
  //
  //   id-ce-nameConstraints OBJECT IDENTIFIER ::=  { id-ce 30 }
  //
  //   NameConstraints ::= SEQUENCE {
  //        permittedSubtrees       [0]     GeneralSubtrees OPTIONAL,
  //        excludedSubtrees        [1]     GeneralSubtrees OPTIONAL }
  //
  //   GeneralSubtrees ::= SEQUENCE SIZE (1..MAX) OF GeneralSubtree
  //
  //   GeneralSubtree ::= SEQUENCE {
  //        base                    GeneralName,
  //        minimum         [0]     BaseDistance DEFAULT 0,
  //        maximum         [1]     BaseDistance OPTIONAL }
  //
  //   BaseDistance ::= INTEGER (0..MAX)

  if (permitted_dns_names.empty() && excluded_dns_names.empty()) {
    EraseExtension(bssl::der::Input(bssl::kNameConstraintsOid));
    return;
  }

  bssl::ScopedCBB cbb;
  CBB name_constraints;
  ASSERT_TRUE(CBB_init(cbb.get(), 64));
  ASSERT_TRUE(CBB_add_asn1(cbb.get(), &name_constraints, CBS_ASN1_SEQUENCE));
  if (!permitted_dns_names.empty()) {
    ASSERT_NO_FATAL_FAILURE(
        AddNameConstraintsSubTrees(&name_constraints, permitted_dns_names));
  }
  if (!excluded_dns_names.empty()) {
    ASSERT_NO_FATAL_FAILURE(
        AddNameConstraintsSubTrees(&name_constraints, excluded_dns_names));
  }
  SetExtension(bssl::der::Input(bssl::kNameConstraintsOid),
               FinishCBB(cbb.get()),
               /*critical=*/true);
}

void CertBuilder::SetCaIssuersUrl(const GURL& url) {
  SetCaIssuersAndOCSPUrls({url}, {});
}

void CertBuilder::SetCaIssuersAndOCSPUrls(
    const std::vector<GURL>& ca_issuers_urls,
    const std::vector<GURL>& ocsp_urls) {
  std::vector<std::pair<bssl::der::Input, std::string_view>> entries;
  for (const auto& url : ca_issuers_urls) {
    entries.emplace_back(bssl::der::Input(bssl::kAdCaIssuersOid),
                         url.possibly_invalid_spec());
  }
  for (const auto& url : ocsp_urls) {
    entries.emplace_back(bssl::der::Input(bssl::kAdOcspOid),
                         url.possibly_invalid_spec());
  }
  SetCaIssuersAndOCSPUrls(entries);
}

void CertBuilder::SetCaIssuersAndOCSPUrls(
    const std::vector<std::string>& ca_issuers_urls,
    const std::vector<std::string>& ocsp_urls) {
  std::vector<std::pair<bssl::der::Input, std::string_view>> entries;
  for (const auto& url : ca_issuers_urls) {
    entries.emplace_back(bssl::der::Input(bssl::kAdCaIssuersOid), url);
  }
  for (const auto& url : ocsp_urls) {
    entries.emplace_back(bssl::der::Input(bssl::kAdOcspOid), url);
  }
  SetCaIssuersAndOCSPUrls(entries);
}

void CertBuilder::SetCaIssuersAndOCSPUrls(
    const std::vector<std::pair<bssl::der::Input, std::string_view>>& entries) {
  if (entries.empty()) {
    EraseExtension(bssl::der::Input(bssl::kAuthorityInfoAccessOid));
    return;
  }

  // From RFC 5280:
  //
  //   AuthorityInfoAccessSyntax  ::=
  //           SEQUENCE SIZE (1..MAX) OF AccessDescription
  //
  //   AccessDescription  ::=  SEQUENCE {
  //           accessMethod          OBJECT IDENTIFIER,
  //           accessLocation        GeneralName  }
  bssl::ScopedCBB cbb;
  CBB aia;
  ASSERT_TRUE(CBB_init(cbb.get(), 64));
  ASSERT_TRUE(CBB_add_asn1(cbb.get(), &aia, CBS_ASN1_SEQUENCE));

  for (const auto& entry : entries) {
    CBB access_description, access_method, access_location;
    ASSERT_TRUE(CBB_add_asn1(&aia, &access_description, CBS_ASN1_SEQUENCE));
    ASSERT_TRUE(
        CBB_add_asn1(&access_description, &access_method, CBS_ASN1_OBJECT));
    ASSERT_TRUE(CBBAddBytes(&access_method, entry.first));
    ASSERT_TRUE(CBB_add_asn1(&access_description, &access_location,
                             CBS_ASN1_CONTEXT_SPECIFIC | 6));
    ASSERT_TRUE(CBBAddBytes(&access_location, entry.second));
    ASSERT_TRUE(CBB_flush(&aia));
  }

  SetExtension(bssl::der::Input(bssl::kAuthorityInfoAccessOid),
               FinishCBB(cbb.get()));
}

void CertBuilder::SetCrlDistributionPointUrl(const GURL& url) {
  SetCrlDistributionPointUrls({url});
}

void CertBuilder::SetCrlDistributionPointUrl(const std::string_view& url) {
  SetCrlDistributionPointUrls(std::vector<std::string>{std::string(url)});
}

void CertBuilder::SetCrlDistributionPointUrls(const std::vector<GURL>& urls) {
  std::vector<std::string> string_urls;
  for (const auto& url : urls) {
    string_urls.push_back(url.possibly_invalid_spec());
  }
  SetCrlDistributionPointUrls(string_urls);
}

void CertBuilder::SetCrlDistributionPointUrls(
    const std::vector<std::string>& urls) {
  bssl::ScopedCBB cbb;
  ASSERT_TRUE(CBB_init(cbb.get(), 64));
  CBB dps, dp, dp_name, dp_fullname;

  //    CRLDistributionPoints ::= SEQUENCE SIZE (1..MAX) OF DistributionPoint
  ASSERT_TRUE(CBB_add_asn1(cbb.get(), &dps, CBS_ASN1_SEQUENCE));

  //    DistributionPoint ::= SEQUENCE {
  //         distributionPoint       [0]     DistributionPointName OPTIONAL,
  //         reasons                 [1]     ReasonFlags OPTIONAL,
  //         cRLIssuer               [2]     bssl::GeneralNames OPTIONAL }
  ASSERT_TRUE(CBB_add_asn1(&dps, &dp, CBS_ASN1_SEQUENCE));
  ASSERT_TRUE(CBB_add_asn1(
      &dp, &dp_name, CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0));

  //    DistributionPointName ::= CHOICE {
  //         fullName                [0]     bssl::GeneralNames,
  //         nameRelativeToCRLIssuer [1]     bssl::RelativeDistinguishedName }
  ASSERT_TRUE(
      CBB_add_asn1(&dp_name, &dp_fullname,
                   CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0));

  //   bssl::GeneralNames ::= SEQUENCE SIZE (1..MAX) OF GeneralName
  //   GeneralName ::= CHOICE {
  // uniformResourceIdentifier       [6]     IA5String,
  for (const auto& url : urls) {
    CBB dp_url;
    ASSERT_TRUE(
        CBB_add_asn1(&dp_fullname, &dp_url, CBS_ASN1_CONTEXT_SPECIFIC | 6));
    ASSERT_TRUE(CBBAddBytes(&dp_url, url));
    ASSERT_TRUE(CBB_flush(&dp_fullname));
  }

  SetExtension(bssl::der::Input(bssl::kCrlDistributionPointsOid),
               FinishCBB(cbb.get()));
}

void CertBuilder::SetIssuerTLV(base::span<const uint8_t> issuer_tlv) {
  if (issuer_tlv.empty())
    issuer_tlv_ = std::nullopt;
  else
    issuer_tlv_ = std::string(issuer_tlv.begin(), issuer_tlv.end());
  Invalidate();
}

void CertBuilder::SetSubjectCommonName(std::string_view common_name) {
  SetSubjectTLV(
      BuildNameWithCommonNameOfType(common_name, CBS_ASN1_UTF8STRING));
  Invalidate();
}

void CertBuilder::SetSubjectTLV(base::span<const uint8_t> subject_tlv) {
  subject_tlv_.assign(subject_tlv.begin(), subject_tlv.end());
  Invalidate();
}

void CertBuilder::SetSubjectAltName(std::string_view dns_name) {
  SetSubjectAltNames({std::string(dns_name)}, {});
}

void CertBuilder::SetSubjectAltNames(
    const std::vector<std::string>& dns_names,
    const std::vector<IPAddress>& ip_addresses) {
  // From RFC 5280:
  //
  //   SubjectAltName ::= bssl::GeneralNames
  //
  //   bssl::GeneralNames ::= SEQUENCE SIZE (1..MAX) OF GeneralName
  //
  //   GeneralName ::= CHOICE {
  //        ...
  //        dNSName                         [2]     IA5String,
  //        ...
  //        iPAddress                       [7]     OCTET STRING,
  //        ... }
  ASSERT_GT(dns_names.size() + ip_addresses.size(), 0U);
  bssl::ScopedCBB cbb;
  CBB general_names;
  ASSERT_TRUE(CBB_init(cbb.get(), 64));
  ASSERT_TRUE(CBB_add_asn1(cbb.get(), &general_names, CBS_ASN1_SEQUENCE));
  if (!dns_names.empty()) {
    for (const auto& name : dns_names) {
      CBB general_name;
      ASSERT_TRUE(CBB_add_asn1(&general_names, &general_name,
                               CBS_ASN1_CONTEXT_SPECIFIC | 2));
      ASSERT_TRUE(CBBAddBytes(&general_name, name));
      ASSERT_TRUE(CBB_flush(&general_names));
    }
  }
  if (!ip_addresses.empty()) {
    for (const auto& addr : ip_addresses) {
      CBB general_name;
      ASSERT_TRUE(CBB_add_asn1(&general_names, &general_name,
                               CBS_ASN1_CONTEXT_SPECIFIC | 7));
      ASSERT_TRUE(
          CBB_add_bytes(&general_name, addr.bytes().data(), addr.size()));
      ASSERT_TRUE(CBB_flush(&general_names));
    }
  }
  SetExtension(bssl::der::Input(bssl::kSubjectAltNameOid),
               FinishCBB(cbb.get()));
}

void CertBuilder::SetKeyUsages(const std::vector<bssl::KeyUsageBit>& usages) {
  ASSERT_GT(usages.size(), 0U);
  int number_of_unused_bits = 0;
  std::vector<uint8_t> bytes;
  for (auto usage : usages) {
    int bit_index = static_cast<int>(usage);

    // Index of the byte that contains the bit.
    size_t byte_index = bit_index / 8;

    if (byte_index + 1 > bytes.size()) {
      bytes.resize(byte_index + 1);
      number_of_unused_bits = 8;
    }

    // Within a byte, bits are ordered from most significant to least
    // significant. Convert |bit_index| to an index within the |byte_index|
    // byte, measured from its least significant bit.
    uint8_t bit_index_in_byte = 7 - (bit_index - byte_index * 8);

    if (byte_index + 1 == bytes.size() &&
        bit_index_in_byte < number_of_unused_bits) {
      number_of_unused_bits = bit_index_in_byte;
    }

    bytes[byte_index] |= (1 << bit_index_in_byte);
  }

  // From RFC 5290:
  //   KeyUsage ::= BIT STRING {...}
  bssl::ScopedCBB cbb;
  CBB ku_cbb;
  ASSERT_TRUE(CBB_init(cbb.get(), bytes.size() + 1));
  ASSERT_TRUE(CBB_add_asn1(cbb.get(), &ku_cbb, CBS_ASN1_BITSTRING));
  ASSERT_TRUE(CBB_add_u8(&ku_cbb, number_of_unused_bits));
  ASSERT_TRUE(CBB_add_bytes(&ku_cbb, bytes.data(), bytes.size()));
  SetExtension(bssl::der::Input(bssl::kKeyUsageOid), FinishCBB(cbb.get()),
               /*critical=*/true);
}

void CertBuilder::SetExtendedKeyUsages(
    const std::vector<bssl::der::Input>& purpose_oids) {
  // From RFC 5280:
  //   ExtKeyUsageSyntax ::= SEQUENCE SIZE (1..MAX) OF KeyPurposeId
  //   KeyPurposeId ::= OBJECT IDENTIFIER
  ASSERT_GT(purpose_oids.size(), 0U);
  bssl::ScopedCBB cbb;
  CBB eku;
  ASSERT_TRUE(CBB_init(cbb.get(), 64));
  ASSERT_TRUE(CBB_add_asn1(cbb.get(), &eku, CBS_ASN1_SEQUENCE));

  for (const auto& oid : purpose_oids) {
    CBB purpose_cbb;
    ASSERT_TRUE(CBB_add_asn1(&eku, &purpose_cbb, CBS_ASN1_OBJECT));
    ASSERT_TRUE(CBBAddBytes(&purpose_cbb, oid));
    ASSERT_TRUE(CBB_flush(&eku));
  }
  SetExtension(bssl::der::Input(bssl::kExtKeyUsageOid), FinishCBB(cbb.get()));
}

void CertBuilder::SetCertificatePolicies(
    const std::vector<std::string>& policy_oids) {
  // From RFC 5280:
  //    certificatePolicies ::= SEQUENCE SIZE (1..MAX) OF PolicyInformation
  //
  //    PolicyInformation ::= SEQUENCE {
  //         policyIdentifier   CertPolicyId,
  //         policyQualifiers   SEQUENCE SIZE (1..MAX) OF
  //                                 PolicyQualifierInfo OPTIONAL }
  //
  //    CertPolicyId ::= OBJECT IDENTIFIER
  if (policy_oids.empty()) {
    EraseExtension(bssl::der::Input(bssl::kCertificatePoliciesOid));
    return;
  }

  bssl::ScopedCBB cbb;
  CBB certificate_policies;
  ASSERT_TRUE(CBB_init(cbb.get(), 64));
  ASSERT_TRUE(
      CBB_add_asn1(cbb.get(), &certificate_policies, CBS_ASN1_SEQUENCE));
  for (const auto& oid : policy_oids) {
    CBB policy_information, policy_identifier;
    ASSERT_TRUE(CBB_add_asn1(&certificate_policies, &policy_information,
                             CBS_ASN1_SEQUENCE));
    ASSERT_TRUE(
        CBB_add_asn1(&policy_information, &policy_identifier, CBS_ASN1_OBJECT));
    ASSERT_TRUE(
        CBB_add_asn1_oid_from_text(&policy_identifier, oid.data(), oid.size()));
    ASSERT_TRUE(CBB_flush(&certificate_policies));
  }

  SetExtension(bssl::der::Input(bssl::kCertificatePoliciesOid),
               FinishCBB(cbb.get()));
}

void CertBuilder::SetPolicyMappings(
    const std::vector<std::pair<std::string, std::string>>& policy_mappings) {
  // From RFC 5280:
  //   PolicyMappings ::= SEQUENCE SIZE (1..MAX) OF SEQUENCE {
  //        issuerDomainPolicy      CertPolicyId,
  //        subjectDomainPolicy     CertPolicyId }
  if (policy_mappings.empty()) {
    EraseExtension(bssl::der::Input(bssl::kPolicyMappingsOid));
    return;
  }

  bssl::ScopedCBB cbb;
  CBB mappings_sequence;
  ASSERT_TRUE(CBB_init(cbb.get(), 64));
  ASSERT_TRUE(CBB_add_asn1(cbb.get(), &mappings_sequence, CBS_ASN1_SEQUENCE));
  for (const auto& [issuer_domain_policy, subject_domain_policy] :
       policy_mappings) {
    CBB mapping_sequence;
    CBB issuer_policy_object;
    CBB subject_policy_object;
    ASSERT_TRUE(
        CBB_add_asn1(&mappings_sequence, &mapping_sequence, CBS_ASN1_SEQUENCE));

    ASSERT_TRUE(CBB_add_asn1(&mapping_sequence, &issuer_policy_object,
                             CBS_ASN1_OBJECT));
    ASSERT_TRUE(CBB_add_asn1_oid_from_text(&issuer_policy_object,
                                           issuer_domain_policy.data(),
                                           issuer_domain_policy.size()));

    ASSERT_TRUE(CBB_add_asn1(&mapping_sequence, &subject_policy_object,
                             CBS_ASN1_OBJECT));
    ASSERT_TRUE(CBB_add_asn1_oid_from_text(&subject_policy_object,
                                           subject_domain_policy.data(),
                                           subject_domain_policy.size()));

    ASSERT_TRUE(CBB_flush(&mappings_sequence));
  }

  SetExtension(bssl::der::Input(bssl::kPolicyMappingsOid), FinishCBB(cbb.get()),
               /*critical=*/true);
}

void CertBuilder::SetPolicyConstraints(
    std::optional<uint64_t> require_explicit_policy,
    std::optional<uint64_t> inhibit_policy_mapping) {
  if (!require_explicit_policy.has_value() &&
      !inhibit_policy_mapping.has_value()) {
    EraseExtension(bssl::der::Input(bssl::kPolicyConstraintsOid));
    return;
  }

  // From RFC 5280:
  //   PolicyConstraints ::= SEQUENCE {
  //        requireExplicitPolicy           [0] SkipCerts OPTIONAL,
  //        inhibitPolicyMapping            [1] SkipCerts OPTIONAL }
  //
  //   SkipCerts ::= INTEGER (0..MAX)
  bssl::ScopedCBB cbb;
  CBB policy_constraints;
  ASSERT_TRUE(CBB_init(cbb.get(), 64));
  ASSERT_TRUE(CBB_add_asn1(cbb.get(), &policy_constraints, CBS_ASN1_SEQUENCE));
  if (require_explicit_policy.has_value()) {
    ASSERT_TRUE(CBB_add_asn1_uint64_with_tag(&policy_constraints,
                                             *require_explicit_policy,
                                             CBS_ASN1_CONTEXT_SPECIFIC | 0));
  }
  if (inhibit_policy_mapping.has_value()) {
    ASSERT_TRUE(CBB_add_asn1_uint64_with_tag(&policy_constraints,
                                             *inhibit_policy_mapping,
                                             CBS_ASN1_CONTEXT_SPECIFIC | 1));
  }

  SetExtension(bssl::der::Input(bssl::kPolicyConstraintsOid),
               FinishCBB(cbb.get()),
               /*critical=*/true);
}

void CertBuilder::SetInhibitAnyPolicy(uint64_t skip_certs) {
  // From RFC 5280:
  //   id-ce-inhibitAnyPolicy OBJECT IDENTIFIER ::=  { id-ce 54 }
  //
  //   InhibitAnyPolicy ::= SkipCerts
  //
  //   SkipCerts ::= INTEGER (0..MAX)
  bssl::ScopedCBB cbb;
  ASSERT_TRUE(CBB_init(cbb.get(), 64));
  ASSERT_TRUE(CBB_add_asn1_uint64(cbb.get(), skip_certs));
  SetExtension(bssl::der::Input(bssl::kInhibitAnyPolicyOid),
               FinishCBB(cbb.get()),
               /*critical=*/true);
}

void CertBuilder::SetQcStatements(std::vector<QcStatement> qc_statements) {
  // From RFC 3739 A.1:
  //
  //   QCStatements ::= SEQUENCE OF QCStatement
  //
  //   QCStatement ::= SEQUENCE {
  //       statementId        OBJECT IDENTIFIER,
  //       statementInfo      ANY DEFINED BY statementId OPTIONAL}
  bssl::ScopedCBB cbb;
  ASSERT_TRUE(CBB_init(cbb.get(), 64));
  CBB qc_statements_sequence;
  ASSERT_TRUE(
      CBB_add_asn1(cbb.get(), &qc_statements_sequence, CBS_ASN1_SEQUENCE));

  for (const auto& statement : qc_statements) {
    CBB qc_statement_sequence;
    ASSERT_TRUE(CBB_add_asn1(&qc_statements_sequence, &qc_statement_sequence,
                             CBS_ASN1_SEQUENCE));
    CBB statement_id;
    ASSERT_TRUE(
        CBB_add_asn1(&qc_statement_sequence, &statement_id, CBS_ASN1_OBJECT));
    ASSERT_TRUE(CBBAddBytes(&statement_id, statement.id));
    ASSERT_TRUE(CBBAddBytes(&qc_statement_sequence, statement.info));
    ASSERT_TRUE(CBB_flush(&qc_statements_sequence));
  }

  SetExtension(bssl::der::Input(kQcStatementsOid), FinishCBB(cbb.get()));
}

void CertBuilder::SetQwacQcStatements(std::vector<bssl::der::Input> qc_types) {
  std::vector<uint8_t> qc_type_info = CertBuilder::BuildSequenceOfOid(qc_types);
  SetQcStatements({
      {bssl::der::Input(kEtsiQcsQcComplianceOid), {}},
      {bssl::der::Input(kEtsiQcsQcTypeOid), bssl::der::Input(qc_type_info)},
  });
}

void CertBuilder::SetValidity(base::Time not_before, base::Time not_after) {
  // From RFC 5280:
  //   Validity ::= SEQUENCE {
  //        notBefore      Time,
  //        notAfter       Time }
  bssl::ScopedCBB cbb;
  CBB validity;
  ASSERT_TRUE(CBB_init(cbb.get(), 64));
  ASSERT_TRUE(CBB_add_asn1(cbb.get(), &validity, CBS_ASN1_SEQUENCE));
  ASSERT_TRUE(x509_util::CBBAddTime(&validity, not_before));
  ASSERT_TRUE(x509_util::CBBAddTime(&validity, not_after));
  validity_tlv_ = FinishCBB(cbb.get());
  Invalidate();
}

void CertBuilder::SetSubjectKeyIdentifier(
    const std::string& subject_key_identifier) {
  ASSERT_FALSE(subject_key_identifier.empty());

  // From RFC 5280:
  //   KeyIdentifier ::= OCTET STRING
  //   SubjectKeyIdentifier ::= KeyIdentifier
  bssl::ScopedCBB cbb;
  ASSERT_TRUE(CBB_init(cbb.get(), 32));

  ASSERT_TRUE(CBB_add_asn1_octet_string(
      cbb.get(),
      reinterpret_cast<const uint8_t*>(subject_key_identifier.data()),
      subject_key_identifier.size()));

  // Replace the existing SKI. Note it MUST be non-critical, per RFC 5280.
  SetExtension(bssl::der::Input(bssl::kSubjectKeyIdentifierOid),
               FinishCBB(cbb.get()),
               /*critical=*/false);
}

void CertBuilder::SetAuthorityKeyIdentifier(
    const std::string& authority_key_identifier) {
  // If an empty AKI is presented, simply erase the existing one. Creating
  // an empty AKI is technically valid, but there's no use case for this.
  // An empty AKI would an empty (ergo, non-unique) SKI on the issuer,
  // which would violate RFC 5280, so using the empty value as a placeholder
  // unless and until a use case emerges is fine.
  if (authority_key_identifier.empty()) {
    EraseExtension(bssl::der::Input(bssl::kAuthorityKeyIdentifierOid));
    return;
  }

  // From RFC 5280:
  //
  //   AuthorityKeyIdentifier ::= SEQUENCE {
  //       keyIdentifier             [0] KeyIdentifier           OPTIONAL,
  //       authorityCertIssuer       [1] bssl::GeneralNames            OPTIONAL,
  //       authorityCertSerialNumber [2] CertificateSerialNumber OPTIONAL  }
  //
  //   KeyIdentifier ::= OCTET STRING
  bssl::ScopedCBB cbb;
  CBB aki, aki_value;
  ASSERT_TRUE(CBB_init(cbb.get(), 32));
  ASSERT_TRUE(CBB_add_asn1(cbb.get(), &aki, CBS_ASN1_SEQUENCE));
  ASSERT_TRUE(CBB_add_asn1(&aki, &aki_value, CBS_ASN1_CONTEXT_SPECIFIC | 0));
  ASSERT_TRUE(CBBAddBytes(&aki_value, authority_key_identifier));
  ASSERT_TRUE(CBB_flush(&aki));

  SetExtension(bssl::der::Input(bssl::kAuthorityKeyIdentifierOid),
               FinishCBB(cbb.get()));
}

void CertBuilder::SetSignatureAlgorithm(
    bssl::SignatureAlgorithm signature_algorithm) {
  signature_algorithm_ = signature_algorithm;
  Invalidate();
}

void CertBuilder::SetSignatureAlgorithmTLV(
    std::string_view signature_algorithm_tlv) {
  SetOuterSignatureAlgorithmTLV(signature_algorithm_tlv);
  SetTBSSignatureAlgorithmTLV(signature_algorithm_tlv);
}

void CertBuilder::SetOuterSignatureAlgorithmTLV(
    std::string_view signature_algorithm_tlv) {
  outer_signature_algorithm_tlv_ = std::string(signature_algorithm_tlv);
  Invalidate();
}

void CertBuilder::SetTBSSignatureAlgorithmTLV(
    std::string_view signature_algorithm_tlv) {
  tbs_signature_algorithm_tlv_ = std::string(signature_algorithm_tlv);
  Invalidate();
}

void CertBuilder::SetSerialNumber(uint64_t serial_number) {
  serial_number_ = serial_number;
  Invalidate();
}

void CertBuilder::SetRandomSerialNumber() {
  serial_number_ = base::RandUint64();
  Invalidate();
}

void CertBuilder::SetSctConfig(
    std::vector<CertBuilder::SctConfig> sct_configs) {
  sct_configs_ = std::move(sct_configs);
  Invalidate();
}

CRYPTO_BUFFER* CertBuilder::GetCertBuffer() {
  if (!cert_)
    GenerateCertificate();
  return cert_.get();
}

bssl::UniquePtr<CRYPTO_BUFFER> CertBuilder::DupCertBuffer() {
  return bssl::UpRef(GetCertBuffer());
}

const std::string& CertBuilder::GetSubject() {
  if (subject_tlv_.empty())
    GenerateSubject();
  return subject_tlv_;
}

uint64_t CertBuilder::GetSerialNumber() {
  if (!serial_number_)
    serial_number_ = base::RandUint64();
  return serial_number_;
}

std::string CertBuilder::GetSubjectKeyIdentifier() {
  std::string ski_oid(base::as_string_view(bssl::kSubjectKeyIdentifierOid));
  if (extensions_.find(ski_oid) == extensions_.end()) {
    // If no SKI is present, this means that the certificate was either
    // created by FromStaticCert() and lacked one, or it was explicitly
    // deleted as an extension.
    return std::string();
  }

  auto& extension_value = extensions_[ski_oid];
  bssl::der::Input ski_value;
  if (!bssl::ParseSubjectKeyIdentifier(bssl::der::Input(extension_value.value),
                                       &ski_value)) {
    return std::string();
  }
  return std::string(base::as_string_view(ski_value));
}

bool CertBuilder::GetValidity(base::Time* not_before,
                              base::Time* not_after) const {
  bssl::der::GeneralizedTime not_before_generalized_time;
  bssl::der::GeneralizedTime not_after_generalized_time;
  if (!bssl::ParseValidity(bssl::der::Input(validity_tlv_),
                           &not_before_generalized_time,
                           &not_after_generalized_time) ||
      !GeneralizedTimeToTime(not_before_generalized_time, not_before) ||
      !GeneralizedTimeToTime(not_after_generalized_time, not_after)) {
    return false;
  }
  return true;
}

EVP_PKEY* CertBuilder::GetKey() {
  if (!key_) {
    switch (default_pkey_id_) {
      case EVP_PKEY_RSA:
        GenerateRSAKey();
        break;
      case EVP_PKEY_EC:
        GenerateECKey();
        break;
    }
  }
  return key_.get();
}

scoped_refptr<X509Certificate> CertBuilder::GetX509Certificate() {
  return X509Certificate::CreateFromBuffer(DupCertBuffer(), {});
}

scoped_refptr<X509Certificate> CertBuilder::GetX509CertificateChain() {
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  // Add intermediates, not including the self-signed root.
  for (CertBuilder* cert = issuer_; cert && cert != cert->issuer_;
       cert = cert->issuer_) {
    intermediates.push_back(cert->DupCertBuffer());
  }
  return X509Certificate::CreateFromBuffer(DupCertBuffer(),
                                           std::move(intermediates));
}

scoped_refptr<X509Certificate> CertBuilder::GetX509CertificateFullChain() {
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  // Add intermediates and the self-signed root.
  for (CertBuilder* cert = issuer_; cert; cert = cert->issuer_) {
    intermediates.push_back(cert->DupCertBuffer());
    if (cert == cert->issuer_)
      break;
  }
  return X509Certificate::CreateFromBuffer(DupCertBuffer(),
                                           std::move(intermediates));
}

std::string CertBuilder::GetDER() {
  return std::string(x509_util::CryptoBufferAsStringPiece(GetCertBuffer()));
}

std::string CertBuilder::GetPEM() {
  std::string pem_encoded;
  EXPECT_TRUE(X509Certificate::GetPEMEncoded(GetCertBuffer(), &pem_encoded));
  return pem_encoded;
}

std::string CertBuilder::GetPEMFullChain() {
  std::vector<std::string> pems;
  CertBuilder* cert = this;
  while (cert) {
    pems.push_back(cert->GetPEM());
    if (cert == cert->issuer_)
      break;
    cert = cert->issuer_;
  }
  return base::JoinString(pems, "\n");
}

std::string CertBuilder::GetPrivateKeyPEM() {
  std::string pem_encoded = key_util::PEMFromPrivateKey(GetKey());
  EXPECT_FALSE(pem_encoded.empty());
  return pem_encoded;
}

CertBuilder::CertBuilder(CRYPTO_BUFFER* orig_cert,
                         CertBuilder* issuer,
                         bool unique_subject_key_identifier)
    : issuer_(issuer) {
  if (!issuer_)
    issuer_ = this;

  if (orig_cert)
    InitFromCert(
        bssl::der::Input(x509_util::CryptoBufferAsStringPiece(orig_cert)));

  if (unique_subject_key_identifier) {
    GenerateSubjectKeyIdentifier();
    SetAuthorityKeyIdentifier(issuer_->GetSubjectKeyIdentifier());
  }
}

void CertBuilder::Invalidate() {
  cert_.reset();
}

void CertBuilder::GenerateECKey() {
  auto private_key = crypto::keypair::PrivateKey::GenerateEcP256();
  SetKey(bssl::UpRef(private_key.key()));
}

void CertBuilder::GenerateRSAKey() {
  // TODO(https://crbug.com/426228064): Can we just use a hardcoded key here?
  auto private_key = crypto::keypair::PrivateKey::GenerateRsa2048();
  SetKey(bssl::UpRef(private_key.key()));
}

bool CertBuilder::UseKeyFromFile(const base::FilePath& key_file) {
  bssl::UniquePtr<EVP_PKEY> private_key(
      key_util::LoadEVP_PKEYFromPEM(key_file));
  if (!private_key)
    return false;
  SetKey(std::move(private_key));
  return true;
}

void CertBuilder::SetKey(bssl::UniquePtr<EVP_PKEY> key) {
  key_ = std::move(key);
  Invalidate();
}

void CertBuilder::GenerateSubjectKeyIdentifier() {
  // 20 bytes are chosen here for no other reason than it's compatible with
  // systems that assume the SKI is SHA-1(SPKI), which RFC 5280 notes as one
  // mechanism for generating an SKI, while also noting that random/unique
  // SKIs are also fine.
  std::string random_ski = base::RandBytesAsString(20);
  SetSubjectKeyIdentifier(random_ski);
}

void CertBuilder::GenerateSubject() {
  ASSERT_TRUE(subject_tlv_.empty());

  // Use a random common name comprised of 12 bytes in hex.
  std::string common_name = MakeRandomHexString(12);

  SetSubjectCommonName(common_name);
}

void CertBuilder::InitFromCert(const bssl::der::Input& cert) {
  extensions_.clear();
  Invalidate();

  // From RFC 5280, section 4.1
  //    Certificate  ::=  SEQUENCE  {
  //      tbsCertificate       TBSCertificate,
  //      signatureAlgorithm   AlgorithmIdentifier,
  //      signatureValue       BIT STRING  }

  // TBSCertificate  ::=  SEQUENCE  {
  //      version         [0]  EXPLICIT Version DEFAULT v1,
  //      serialNumber         CertificateSerialNumber,
  //      signature            AlgorithmIdentifier,
  //      issuer               Name,
  //      validity             Validity,
  //      subject              Name,
  //      subjectPublicKeyInfo SubjectPublicKeyInfo,
  //      issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL,
  //                           -- If present, version MUST be v2 or v3
  //      subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL,
  //                           -- If present, version MUST be v2 or v3
  //      extensions      [3]  EXPLICIT Extensions OPTIONAL
  //                           -- If present, version MUST be v3
  //      }
  bssl::der::Parser parser(cert);
  bssl::der::Parser certificate;
  bssl::der::Parser tbs_certificate;
  ASSERT_TRUE(parser.ReadSequence(&certificate));
  ASSERT_TRUE(certificate.ReadSequence(&tbs_certificate));

  // version
  bool has_version;
  ASSERT_TRUE(tbs_certificate.SkipOptionalTag(
      CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC | 0, &has_version));
  if (has_version) {
    // TODO(mattm): could actually parse the version here instead of assuming
    // V3.
    version_ = bssl::CertificateVersion::V3;
  } else {
    version_ = bssl::CertificateVersion::V1;
  }

  // serialNumber
  ASSERT_TRUE(tbs_certificate.SkipTag(CBS_ASN1_INTEGER));

  // signature
  bssl::der::Input signature_algorithm_tlv;
  ASSERT_TRUE(tbs_certificate.ReadRawTLV(&signature_algorithm_tlv));
  auto signature_algorithm =
      bssl::ParseSignatureAlgorithm(signature_algorithm_tlv);
  ASSERT_TRUE(signature_algorithm);
  signature_algorithm_ = *signature_algorithm;

  // issuer
  ASSERT_TRUE(tbs_certificate.SkipTag(CBS_ASN1_SEQUENCE));

  // validity
  bssl::der::Input validity_tlv;
  ASSERT_TRUE(tbs_certificate.ReadRawTLV(&validity_tlv));
  validity_tlv_ = base::as_string_view(validity_tlv);

  // subject
  ASSERT_TRUE(tbs_certificate.SkipTag(CBS_ASN1_SEQUENCE));

  // subjectPublicKeyInfo
  bssl::der::Input spki_tlv;
  ASSERT_TRUE(tbs_certificate.ReadRawTLV(&spki_tlv));
  bssl::UniquePtr<EVP_PKEY> public_key;
  ASSERT_TRUE(bssl::ParsePublicKey(spki_tlv, &public_key));
  default_pkey_id_ = EVP_PKEY_id(public_key.get());

  // issuerUniqueID
  bool unused;
  ASSERT_TRUE(
      tbs_certificate.SkipOptionalTag(CBS_ASN1_CONTEXT_SPECIFIC | 1, &unused));
  // subjectUniqueID
  ASSERT_TRUE(
      tbs_certificate.SkipOptionalTag(CBS_ASN1_CONTEXT_SPECIFIC | 2, &unused));

  // extensions
  std::optional<bssl::der::Input> extensions_tlv;
  ASSERT_TRUE(tbs_certificate.ReadOptionalTag(
      CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 3, &extensions_tlv));
  if (extensions_tlv) {
    std::map<bssl::der::Input, bssl::ParsedExtension> parsed_extensions;
    ASSERT_TRUE(ParseExtensions(extensions_tlv.value(), &parsed_extensions));

    for (const auto& parsed_extension : parsed_extensions) {
      SetExtension(
          parsed_extension.second.oid,
          std::string(base::as_string_view(parsed_extension.second.value)),
          parsed_extension.second.critical);
    }
  }
}

void CertBuilder::GetEncodedExtensions(std::vector<uint8_t>* out) {
  if (extensions_.empty()) {
    out->clear();
    return;
  }

  bssl::ScopedCBB cbb;
  CBB extensions_context, extensions;

  ASSERT_TRUE(CBB_init(cbb.get(), 64));
  ASSERT_TRUE(
      CBB_add_asn1(cbb.get(), &extensions_context,
                   CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 3));
  ASSERT_TRUE(
      CBB_add_asn1(&extensions_context, &extensions, CBS_ASN1_SEQUENCE));

  //   Extension  ::=  SEQUENCE  {
  //        extnID      OBJECT IDENTIFIER,
  //        critical    BOOLEAN DEFAULT FALSE,
  //        extnValue   OCTET STRING
  //                    -- contains the DER encoding of an ASN.1 value
  //                    -- corresponding to the extension type identified
  //                    -- by extnID
  //        }
  for (const auto& [extension_id, extension_value] : extensions_) {
    CBB extension_seq, oid, extn_value;
    ASSERT_TRUE(CBB_add_asn1(&extensions, &extension_seq, CBS_ASN1_SEQUENCE));
    ASSERT_TRUE(CBB_add_asn1(&extension_seq, &oid, CBS_ASN1_OBJECT));
    ASSERT_TRUE(CBBAddBytes(&oid, extension_id));
    if (extension_value.critical) {
      ASSERT_TRUE(CBB_add_asn1_bool(&extension_seq, true));
    }

    ASSERT_TRUE(
        CBB_add_asn1(&extension_seq, &extn_value, CBS_ASN1_OCTETSTRING));
    ASSERT_TRUE(CBBAddBytes(&extn_value, extension_value.value));
    ASSERT_TRUE(CBB_flush(&extensions));
  }
  *out = FinishCBBToVector(cbb.get());
}

void CertBuilder::BuildTBSCertificate(std::string_view signature_algorithm_tlv,
                                      std::string* out) {
  bssl::ScopedCBB cbb;
  CBB tbs_cert, version;

  ASSERT_TRUE(CBB_init(cbb.get(), 64));
  ASSERT_TRUE(CBB_add_asn1(cbb.get(), &tbs_cert, CBS_ASN1_SEQUENCE));
  if (version_ != bssl::CertificateVersion::V1) {
    ASSERT_TRUE(
        CBB_add_asn1(&tbs_cert, &version,
                     CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0));
    switch (version_) {
      case bssl::CertificateVersion::V2:
        ASSERT_TRUE(CBB_add_asn1_uint64(&version, 1));
        break;
      case bssl::CertificateVersion::V3:
        ASSERT_TRUE(CBB_add_asn1_uint64(&version, 2));
        break;
      case bssl::CertificateVersion::V1:
        NOTREACHED();
    }
  }
  ASSERT_TRUE(CBB_add_asn1_uint64(&tbs_cert, GetSerialNumber()));
  ASSERT_TRUE(CBBAddBytes(&tbs_cert, signature_algorithm_tlv));
  ASSERT_TRUE(CBBAddBytes(&tbs_cert, issuer_tlv_.has_value()
                                         ? *issuer_tlv_
                                         : issuer_->GetSubject()));
  ASSERT_TRUE(CBBAddBytes(&tbs_cert, validity_tlv_));
  ASSERT_TRUE(CBBAddBytes(&tbs_cert, GetSubject()));
  ASSERT_TRUE(GetKey());
  ASSERT_TRUE(EVP_marshal_public_key(&tbs_cert, GetKey()));

  // Serialize all the extensions (encoded_extensions will be empty if there
  // are no extensions).
  std::vector<uint8_t> encoded_extensions;
  GetEncodedExtensions(&encoded_extensions);
  ASSERT_TRUE(CBBAddBytes(&tbs_cert, encoded_extensions));

  *out = FinishCBB(cbb.get());
}

void CertBuilder::BuildSctListExtension(const std::string& pre_tbs_certificate,
                                        std::string* out) {
  std::vector<std::string> encoded_scts;
  for (const SctConfig& sct_config : sct_configs_) {
    ct::SignedEntryData entry;
    entry.type = ct::SignedEntryData::LOG_ENTRY_TYPE_PRECERT;
    std::vector<uint8_t> issuer_spki =
        crypto::evp::PublicKeyToBytes(issuer_->GetKey());
    entry.issuer_key_hash = crypto::hash::Sha256(issuer_spki);
    entry.tbs_certificate = pre_tbs_certificate;

    std::string serialized_log_entry;
    std::string serialized_data;
    ASSERT_TRUE(ct::EncodeSignedEntry(entry, &serialized_log_entry));
    ASSERT_TRUE(ct::EncodeV1SCTSignedData(sct_config.timestamp,
                                          serialized_log_entry,
                                          /*extensions=*/"", &serialized_data));

    scoped_refptr<ct::SignedCertificateTimestamp> sct =
        base::MakeRefCounted<ct::SignedCertificateTimestamp>();
    sct->log_id = sct_config.log_id;
    sct->timestamp = sct_config.timestamp;
    sct->signature.hash_algorithm = ct::DigitallySigned::HASH_ALGO_SHA256;
    sct->signature.signature_algorithm = ct::DigitallySigned::SIG_ALGO_ECDSA;

    bssl::ScopedCBB sct_signature_cbb;
    ASSERT_TRUE(CBB_init(sct_signature_cbb.get(), 0));
    ASSERT_TRUE(SignData(bssl::SignatureAlgorithm::kEcdsaSha256,
                         serialized_data, sct_config.log_key.get(),
                         sct_signature_cbb.get()));
    sct->signature.signature_data = FinishCBB(sct_signature_cbb.get());

    sct->origin = ct::SignedCertificateTimestamp::SCT_EMBEDDED;

    std::string encoded_sct;
    ASSERT_TRUE(ct::EncodeSignedCertificateTimestamp(sct, &encoded_sct));
    encoded_scts.push_back(std::move(encoded_sct));
  }
  std::string encoded_sct_list;
  ASSERT_TRUE(ct::EncodeSCTListForTesting(encoded_scts, &encoded_sct_list));

  bssl::ScopedCBB sct_extension_cbb;
  ASSERT_TRUE(CBB_init(sct_extension_cbb.get(), 32));
  ASSERT_TRUE(CBB_add_asn1_octet_string(
      sct_extension_cbb.get(),
      reinterpret_cast<const uint8_t*>(encoded_sct_list.data()),
      encoded_sct_list.size()));

  *out = FinishCBB(sct_extension_cbb.get());
}

void CertBuilder::GenerateCertificate() {
  ASSERT_FALSE(cert_);

  std::optional<bssl::SignatureAlgorithm> signature_algorithm =
      signature_algorithm_;
  if (!signature_algorithm)
    signature_algorithm = DefaultSignatureAlgorithmForKey(issuer_->GetKey());
  ASSERT_TRUE(signature_algorithm.has_value());

  std::string signature_algorithm_tlv =
      !outer_signature_algorithm_tlv_.empty()
          ? outer_signature_algorithm_tlv_
          : SignatureAlgorithmToDer(*signature_algorithm);
  ASSERT_FALSE(signature_algorithm_tlv.empty());

  std::string tbs_signature_algorithm_tlv =
      !tbs_signature_algorithm_tlv_.empty()
          ? tbs_signature_algorithm_tlv_
          : SignatureAlgorithmToDer(*signature_algorithm);
  ASSERT_FALSE(tbs_signature_algorithm_tlv.empty());

  if (!sct_configs_.empty()) {
    EraseExtension(bssl::der::Input(ct::kEmbeddedSCTOid));
    std::string pre_tbs_certificate;
    BuildTBSCertificate(tbs_signature_algorithm_tlv, &pre_tbs_certificate);
    std::string sct_extension;
    BuildSctListExtension(pre_tbs_certificate, &sct_extension);
    SetExtension(bssl::der::Input(ct::kEmbeddedSCTOid), sct_extension,
                 /*critical=*/false);
  }

  std::string tbs_cert;
  BuildTBSCertificate(tbs_signature_algorithm_tlv, &tbs_cert);

  // Sign the TBSCertificate and write the entire certificate.
  bssl::ScopedCBB cbb;
  CBB cert, signature;

  ASSERT_TRUE(CBB_init(cbb.get(), tbs_cert.size()));
  ASSERT_TRUE(CBB_add_asn1(cbb.get(), &cert, CBS_ASN1_SEQUENCE));
  ASSERT_TRUE(CBBAddBytes(&cert, tbs_cert));
  ASSERT_TRUE(CBBAddBytes(&cert, signature_algorithm_tlv));
  ASSERT_TRUE(CBB_add_asn1(&cert, &signature, CBS_ASN1_BITSTRING));
  ASSERT_TRUE(CBB_add_u8(&signature, 0 /* no unused bits */));
  ASSERT_TRUE(
      SignData(*signature_algorithm, tbs_cert, issuer_->GetKey(), &signature));

  auto cert_der = FinishCBB(cbb.get());
  cert_ = x509_util::CreateCryptoBuffer(base::as_byte_span(cert_der));
}

namespace {

enum MerkleTreeCertEntryType {
  kNullEntry = 0,
  kTbsCertEntry = 1,
};

uint64_t bit_length(uint64_t n) {
  return std::numeric_limits<uint64_t>::digits - std::countl_zero(n);
}

// Returns one or two subtrees that cover the interval [start, end).
//
// This is based on the "find_subtrees" function from the MTC draft:
// https://davidben.github.io/merkle-tree-certs/draft-davidben-tls-merkle-tree-certs.html#section-4.5-5
std::vector<bssl::Subtree> SubtreesForLandmarkRange(
    MtcLogBuilder::LogIndex start,
    MtcLogBuilder::LogIndex end) {
  const uint64_t last = end - 1;

  if (start == last) {
    return {{start, end}};
  }

  // Find where start and last's tree paths diverge. The two
  // subtrees will be on either side of the split.
  const uint64_t split = bit_length(start ^ last) - 1;
  const uint64_t mask = (1 << split) - 1;
  const uint64_t mid = last & ~mask;
  // Maximize the left endpoint. This is just before start's
  // path leaves the right edge of its new subtree.
  const uint64_t left_split = bit_length(~start & mask);
  const uint64_t left_start = start & ~((1 << left_split) - 1);

  return {{left_start, mid}, {mid, end}};
}

// For initial experimentation, early implementations of this design will use
// the OID 1.3.6.1.4.1.44363.47.0 instead of id-alg-mtcProof.
// This is the DER encoding of an AlgorithmIdentifier with the algorithm OID
// set and the parameters omitted, eg:
// SEQUENCE { OBJECT_IDENTIFIER { 1.3.6.1.4.1.44363.47.0 } }
constexpr uint8_t kMtcSignatureAlgorithmIdentifier[] = {
    0x30, 0x0c, 0x06, 0x0a, 0x2b, 0x06, 0x01,
    0x04, 0x01, 0x82, 0xda, 0x4b, 0x2f, 0x00};

}  // namespace

class MtcLogBuilder::Data {
 public:
  explicit Data(std::vector<uint8_t> log_id)
      : log_id_(log_id), encoded_log_name_(GetEncodedLogName()) {
    // Entry 0 is always the null entry.
    merkle_tree_.Append(base::U16ToBigEndian(kNullEntry));
    log_entries_.push_back(MtcLogEntry::NullEntry());
  }

  uint64_t Size() const { return log_entries_.size(); }

  uint64_t AddEntry(MtcLogEntry entry) {
    merkle_tree_.Append(
        entry.BuildMerkleTreeCertEntryTbsCertEntry(encoded_log_name_));

    log_entries_.push_back(std::move(entry));

    return log_entries_.size() - 1;
  }

  std::vector<uint8_t> BuildTBSCertificate(uint64_t index) {
    return log_entries_[index].BuildTBSCertificate(encoded_log_name_, index);
  }

  std::optional<bssl::TreeHash> SubtreeHash(const bssl::Subtree& subtree) {
    return merkle_tree_.SubtreeHash(subtree);
  }

  std::vector<uint8_t> InclusionProof(uint64_t index,
                                      const bssl::Subtree& subtree) {
    return merkle_tree_.SubtreeInclusionProof(index, subtree);
  }

 private:
  std::vector<uint8_t> GetEncodedLogName() const {
    // TODO(crbug.com/469624806): this is duplicates code in MTCAnchor class
    // for encoding the log id into the synthetic cert subject. Can we
    // deduplicate this somehow?
    bssl::ScopedCBB cbb;
    CBB subject_seq, subject_set, subject_log;

    CHECK(CBB_init(cbb.get(), 32));
    CHECK(CBB_add_asn1(cbb.get(), &subject_seq, CBS_ASN1_SEQUENCE));
    CHECK(CBB_add_asn1(&subject_seq, &subject_set, CBS_ASN1_SET));
    CHECK(CBB_add_asn1(&subject_set, &subject_log, CBS_ASN1_SEQUENCE));
    // Section 5.2: Use OID 1.3.6.1.4.1.44363.47.1 as the attribute type for the
    // log ID's name. Note that this is the early experimentation OID in the
    // draft rather than the real value of `id-rdna-trustAnchorID`.
    static uint8_t log_attr_oid[] = {0x2b, 0x06, 0x01, 0x04, 0x01,
                                     0x82, 0xda, 0x4b, 0x2f, 0x01};
    CHECK(CBB_add_asn1_element(&subject_log, CBS_ASN1_OBJECT, log_attr_oid,
                               sizeof(log_attr_oid)));

    std::string log_id_text = x509_util::RelativeOidToString(log_id_);
    CHECK(CBBAddAsn1Element(&subject_log, CBS_ASN1_UTF8STRING,
                            base::as_byte_span(log_id_text)));

    return FinishCBBToVector(cbb.get());
  }

  std::vector<uint8_t> log_id_;

  std::vector<uint8_t> encoded_log_name_;

  std::vector<MtcLogEntry> log_entries_;

  bssl::MerkleTreeInMemory merkle_tree_;
};

MtcLogBuilder::MtcLogEntry::MtcLogEntry() = default;
MtcLogBuilder::MtcLogEntry::~MtcLogEntry() = default;
MtcLogBuilder::MtcLogEntry::MtcLogEntry(const MtcLogEntry&) = default;
MtcLogBuilder::MtcLogEntry& MtcLogBuilder::MtcLogEntry::operator=(
    const MtcLogEntry& other) = default;
MtcLogBuilder::MtcLogEntry::MtcLogEntry(MtcLogEntry&&) = default;
MtcLogBuilder::MtcLogEntry& MtcLogBuilder::MtcLogEntry::operator=(
    MtcLogEntry&& other) = default;

MtcLogBuilder::MtcLogEntry MtcLogBuilder::MtcLogEntry::NullEntry() {
  // TODO(crbug.com/469624806): could return a const reference to a singleton
  // (like GURL::EmptyGURL)
  MtcLogEntry result;
  return result;
}

std::vector<uint8_t>
MtcLogBuilder::MtcLogEntry::BuildMerkleTreeCertEntryTbsCertEntry(
    std::vector<uint8_t> issuer_tlv) {
  bssl::ScopedCBB cbb;
  CBB tbs_cert, version;

  CHECK(CBB_init(cbb.get(), 64));

  // MerkleTreeCertEntry type:
  CHECK(CBB_add_u16(cbb.get(), kTbsCertEntry));

  // MerkleTreeCertEntry tbs_cert_entry:
  CHECK(CBB_add_asn1(cbb.get(), &tbs_cert, CBS_ASN1_SEQUENCE));
  // TODO(crbug.com/469624806): support CertBuilder::version_?
  CHECK(CBB_add_asn1(&tbs_cert, &version,
                     CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0));
  CHECK(CBB_add_asn1_uint64(&version, 2));

  CHECK(CBBAddBytes(&tbs_cert, issuer_tlv));
  CHECK(CBBAddBytes(&tbs_cert, validity));
  CHECK(CBBAddBytes(&tbs_cert, subject));
  CHECK(CBBAddAsn1Element(&tbs_cert, CBS_ASN1_OCTETSTRING,
                          crypto::hash::Sha256(subject_public_key_info)));
  // issuerUniqueID and subjectUniqueID not present.
  CHECK(CBBAddBytes(&tbs_cert, extensions));

  return FinishCBBToVector(cbb.get());
}

std::vector<uint8_t> MtcLogBuilder::MtcLogEntry::BuildTBSCertificate(
    std::vector<uint8_t> issuer_tlv,
    uint64_t index) {
  bssl::ScopedCBB cbb;
  CBB tbs_cert, version;

  CHECK(CBB_init(cbb.get(), 64));
  CHECK(CBB_add_asn1(cbb.get(), &tbs_cert, CBS_ASN1_SEQUENCE));

  // TODO(crbug.com/469624806): support CertBuilder::version_?
  CHECK(CBB_add_asn1(&tbs_cert, &version,
                     CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0));
  CHECK(CBB_add_asn1_uint64(&version, 2));

  // serialNumber         CertificateSerialNumber
  CHECK(CBB_add_asn1_uint64(&tbs_cert, index));

  // signature            AlgorithmIdentifier,
  CHECK(CBBAddBytes(&tbs_cert, kMtcSignatureAlgorithmIdentifier));

  CHECK(CBBAddBytes(&tbs_cert, issuer_tlv));
  CHECK(CBBAddBytes(&tbs_cert, validity));
  CHECK(CBBAddBytes(&tbs_cert, subject));
  CHECK(CBBAddBytes(&tbs_cert, subject_public_key_info));
  // issuerUniqueID and subjectUniqueID not present.
  CHECK(CBBAddBytes(&tbs_cert, extensions));

  return FinishCBBToVector(cbb.get());
}

MtcLogBuilder::MtcLogBuilder(base::span<const uint8_t> log_id)
    : log_id_(base::ToVector(log_id)), data_(new Data(base::ToVector(log_id))) {
  // The first landmark, numbered zero, is always a tree size of zero.
  landmarks_.push_back(0);
}
MtcLogBuilder::~MtcLogBuilder() = default;

bool MtcLogBuilder::AdvanceLandmark() {
  if (landmarks_.back() == data_->Size()) {
    // No new entries have been added since the landmark.
    return false;
  }

  landmarks_.push_back(data_->Size());
  return true;
}

std::vector<bssl::Subtree> MtcLogBuilder::GetLandmarkSubtrees() {
  // TODO(crbug.com/469624806): could cache the subtrees when adding a landmark
  // so we don't need to be recalculated.
  std::vector<bssl::Subtree> result;

  LogIndex prev_tree_size = landmarks_.front();
  for (LogIndex tree_size : base::span(landmarks_).subspan(1u)) {
    base::Extend(result, SubtreesForLandmarkRange(prev_tree_size, tree_size));

    prev_tree_size = tree_size;
  }

  return result;
}

std::vector<bssl::TrustedSubtree> MtcLogBuilder::GetLandmarkSubtreeHashes() {
  // TODO(crbug.com/469624806): could cache the subtrees when adding a landmark
  // so they don't need to potentially be recalculated.
  std::vector<bssl::TrustedSubtree> result;

  for (const auto& subtree : GetLandmarkSubtrees()) {
    std::optional<bssl::TreeHash> hash = data_->SubtreeHash(subtree);
    CHECK(hash);
    result.emplace_back(subtree, *hash);
  }

  return result;
}

MtcLogBuilder::LogIndex MtcLogBuilder::AddEntry(CertBuilder& mtc_builder) {
  MtcLogEntry entry;
  entry.validity = base::ToVector(mtc_builder.GetEncodedValidity());
  entry.subject = base::ToVector(base::as_byte_span(mtc_builder.GetSubject()));
  entry.subject_public_key_info =
      crypto::keypair::PublicKey(bssl::UpRef(mtc_builder.GetKey()),
                                 crypto::SubtlePassKey::ForTesting())
          .ToSubjectPublicKeyInfo();
  mtc_builder.GetEncodedExtensions(&entry.extensions);

  return data_->AddEntry(std::move(entry));
}

void MtcLogBuilder::AddUnusedEntries(size_t n,
                                     base::span<const uint8_t> extra_data) {
  for (size_t i = 0; i < n; i++) {
    size_t cur_index = data_->Size();
    MtcLogEntry entry;

    base::Extend(entry.subject_public_key_info,
                 base::as_byte_span("unusedspki"));
    base::Extend(entry.subject_public_key_info, extra_data);
    base::Extend(entry.subject_public_key_info,
                 base::U64ToBigEndian(cur_index));

    data_->AddEntry(std::move(entry));
  }
}

std::vector<uint8_t> MtcLogBuilder::CreateSignaturelessMtcProof(
    LogIndex index) {
  bssl::Subtree landmark_subtree = {1, 0};  // initialize with invalid subtree
  for (const auto& subtree1 : GetLandmarkSubtrees()) {
    if (subtree1.Contains(index)) {
      landmark_subtree = subtree1;
      break;
    }
  }
  CHECK(landmark_subtree.IsValid());

  std::vector<uint8_t> inclusion_proof =
      data_->InclusionProof(index, landmark_subtree);

  std::vector<uint8_t> result;
  result.reserve(2 * sizeof(uint64_t) + 2 * sizeof(uint16_t) +
                 inclusion_proof.size());

  // struct {
  //     uint64 start;
  base::Extend(result, base::U64ToBigEndian(landmark_subtree.start));

  //     uint64 end;
  base::Extend(result, base::U64ToBigEndian(landmark_subtree.end));

  //    HashValue inclusion_proof<0..2^16-1>;
  base::Extend(result, base::U16ToBigEndian(inclusion_proof.size()));
  base::Extend(result, inclusion_proof);

  //     MTCSignature signatures<0..2^16-1>;
  // `signatures` is empty.
  base::Extend(result, base::U16ToBigEndian(0));
  //
  // } MTCProof;

  return result;
}

std::optional<std::vector<uint8_t>>
MtcLogBuilder::CreateSignaturelessCertificate(LogIndex index) {
  // Given a TBSCertificateLogEntry in the issuance log and a landmark sequence,
  // a signatureless certificate is constructed as follows:
  //
  // Wait for the first landmark to be allocated that contains the entry.
  // Determine the landmark's subtrees and select the one that contains the
  // entry.
  // Construct a certificate (Section 6.1) using the selected subtree and no
  // signatures.

  if (index >= landmarks_.back()) {
    // This entry is not included in any landmark yet, can't create a
    // signatureless certificate.
    return std::nullopt;
  }

  std::vector<uint8_t> tbs_cert = data_->BuildTBSCertificate(index);

  bssl::ScopedCBB cbb;
  CBB cert, signature;

  CHECK(CBB_init(cbb.get(), tbs_cert.size()));
  CHECK(CBB_add_asn1(cbb.get(), &cert, CBS_ASN1_SEQUENCE));
  CHECK(CBBAddBytes(&cert, tbs_cert));
  CHECK(CBBAddBytes(&cert, kMtcSignatureAlgorithmIdentifier));
  CHECK(CBB_add_asn1(&cert, &signature, CBS_ASN1_BITSTRING));
  CHECK(CBB_add_u8(&signature, 0 /* no unused bits */));
  CHECK(CBBAddBytes(&signature, CreateSignaturelessMtcProof(index)));

  return FinishCBBToVector(cbb.get());
}

bssl::UniquePtr<CRYPTO_BUFFER>
MtcLogBuilder::CreateSignaturelessCertificateBuffer(LogIndex index) {
  auto cert = CreateSignaturelessCertificate(index);
  if (cert) {
    return x509_util::CreateCryptoBuffer(*cert);
  }
  return nullptr;
}

}  // namespace net
