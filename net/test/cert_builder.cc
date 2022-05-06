// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/cert_builder.h"

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "crypto/openssl_util.h"
#include "crypto/rsa_private_key.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_util.h"
#include "net/der/encode_values.h"
#include "net/der/input.h"
#include "net/der/parse_values.h"
#include "net/der/parser.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "url/gurl.h"

namespace net {

namespace {

std::string MakeRandomHexString(size_t num_bytes) {
  std::vector<char> rand_bytes;
  rand_bytes.resize(num_bytes);

  base::RandBytes(rand_bytes.data(), rand_bytes.size());
  return base::HexEncode(rand_bytes.data(), rand_bytes.size());
}

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

// Adds bytes (specified as a StringPiece) to the given CBB.
// The argument ordering follows the boringssl CBB_* api style.
bool CBBAddBytes(CBB* cbb, base::StringPiece bytes) {
  return CBB_add_bytes(cbb, reinterpret_cast<const uint8_t*>(bytes.data()),
                       bytes.size());
}

// Adds bytes (from fixed size array) to the given CBB.
// The argument ordering follows the boringssl CBB_* api style.
template <size_t N>
bool CBBAddBytes(CBB* cbb, const uint8_t (&data)[N]) {
  return CBB_add_bytes(cbb, data, N);
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

}  // namespace

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
      LoadPrivateKeyFromFile(cert_and_key_file));
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
  base::StringPiece subject_tlv;
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
      LoadPrivateKeyFromFile(cert_and_key_file));
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

  CBS cbs;
  CBS_init(&cbs, spki_der.data(), spki_der.size());
  builder->key_ = bssl::UniquePtr<EVP_PKEY>(EVP_parse_public_key(&cbs));
  // Check that there was no error in `EVP_parse_public_key` and that it
  // consumed the entire public key.
  if (!builder->key_ || (CBS_len(&cbs) != 0))
    return nullptr;

  return builder;
}

CertBuilder::~CertBuilder() = default;

// static
void CertBuilder::CreateSimpleChain(
    std::unique_ptr<CertBuilder>* out_leaf,
    std::unique_ptr<CertBuilder>* out_intermediate,
    std::unique_ptr<CertBuilder>* out_root) {
  const char kHostname[] = "www.example.com";
  base::FilePath certs_dir =
      GetTestNetDataDirectory()
          .AppendASCII("verify_certificate_chain_unittest")
          .AppendASCII("target-and-intermediate");

  CertificateList orig_certs = CreateCertificateListFromFile(
      certs_dir, "chain.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, orig_certs.size());

  // Build slightly modified variants of |orig_certs|.
  *out_root =
      std::make_unique<CertBuilder>(orig_certs[2]->cert_buffer(), nullptr);
  *out_intermediate = std::make_unique<CertBuilder>(
      orig_certs[1]->cert_buffer(), out_root->get());
  (*out_intermediate)->EraseExtension(der::Input(kCrlDistributionPointsOid));
  (*out_intermediate)->EraseExtension(der::Input(kAuthorityInfoAccessOid));
  *out_leaf = std::make_unique<CertBuilder>(orig_certs[0]->cert_buffer(),
                                            out_intermediate->get());
  (*out_leaf)->SetSubjectAltName(kHostname);
  (*out_leaf)->EraseExtension(der::Input(kCrlDistributionPointsOid));
  (*out_leaf)->EraseExtension(der::Input(kAuthorityInfoAccessOid));
}

void CertBuilder::CreateSimpleChain(std::unique_ptr<CertBuilder>* out_leaf,
                                    std::unique_ptr<CertBuilder>* out_root) {
  const char kHostname[] = "www.example.com";
  base::FilePath certs_dir = GetTestCertsDirectory();

  auto orig_root = ImportCertFromFile(certs_dir, "root_ca_cert.pem");
  ASSERT_TRUE(orig_root);
  auto orig_leaf = ImportCertFromFile(certs_dir, "ok_cert.pem");
  ASSERT_TRUE(orig_leaf);

  // Build slightly modified variants of |orig_certs|.
  *out_root = std::make_unique<CertBuilder>(orig_root->cert_buffer(), nullptr);

  *out_leaf =
      std::make_unique<CertBuilder>(orig_leaf->cert_buffer(), out_root->get());
  (*out_leaf)->SetSubjectAltName(kHostname);
}

void CertBuilder::SetExtension(const der::Input& oid,
                               std::string value,
                               bool critical) {
  auto& extension_value = extensions_[oid.AsString()];
  extension_value.critical = critical;
  extension_value.value = std::move(value);

  Invalidate();
}

void CertBuilder::EraseExtension(const der::Input& oid) {
  extensions_.erase(oid.AsString());

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

  SetExtension(der::Input(kBasicConstraintsOid), FinishCBB(cbb.get()),
               /*critical=*/true);
}

void CertBuilder::SetCaIssuersUrl(const GURL& url) {
  SetCaIssuersAndOCSPUrls({url}, {});
}

void CertBuilder::SetCaIssuersAndOCSPUrls(
    const std::vector<GURL>& ca_issuers_urls,
    const std::vector<GURL>& ocsp_urls) {
  std::vector<std::pair<der::Input, GURL>> entries;
  for (const auto& url : ca_issuers_urls)
    entries.emplace_back(der::Input(kAdCaIssuersOid), url);
  for (const auto& url : ocsp_urls)
    entries.emplace_back(der::Input(kAdOcspOid), url);
  ASSERT_GT(entries.size(), 0U);

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
    ASSERT_TRUE(CBBAddBytes(&access_method, entry.first.AsStringPiece()));
    ASSERT_TRUE(CBB_add_asn1(&access_description, &access_location,
                             CBS_ASN1_CONTEXT_SPECIFIC | 6));
    ASSERT_TRUE(CBBAddBytes(&access_location, entry.second.spec()));
    ASSERT_TRUE(CBB_flush(&aia));
  }

  SetExtension(der::Input(kAuthorityInfoAccessOid), FinishCBB(cbb.get()));
}

void CertBuilder::SetCrlDistributionPointUrl(const GURL& url) {
  SetCrlDistributionPointUrls({url});
}

void CertBuilder::SetCrlDistributionPointUrls(const std::vector<GURL>& urls) {
  bssl::ScopedCBB cbb;
  ASSERT_TRUE(CBB_init(cbb.get(), 64));
  CBB dps, dp, dp_name, dp_fullname;

  //    CRLDistributionPoints ::= SEQUENCE SIZE (1..MAX) OF DistributionPoint
  ASSERT_TRUE(CBB_add_asn1(cbb.get(), &dps, CBS_ASN1_SEQUENCE));

  //    DistributionPoint ::= SEQUENCE {
  //         distributionPoint       [0]     DistributionPointName OPTIONAL,
  //         reasons                 [1]     ReasonFlags OPTIONAL,
  //         cRLIssuer               [2]     GeneralNames OPTIONAL }
  ASSERT_TRUE(CBB_add_asn1(&dps, &dp, CBS_ASN1_SEQUENCE));
  ASSERT_TRUE(CBB_add_asn1(
      &dp, &dp_name, CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0));

  //    DistributionPointName ::= CHOICE {
  //         fullName                [0]     GeneralNames,
  //         nameRelativeToCRLIssuer [1]     RelativeDistinguishedName }
  ASSERT_TRUE(
      CBB_add_asn1(&dp_name, &dp_fullname,
                   CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0));

  //   GeneralNames ::= SEQUENCE SIZE (1..MAX) OF GeneralName
  //   GeneralName ::= CHOICE {
  // uniformResourceIdentifier       [6]     IA5String,
  for (const auto& url : urls) {
    CBB dp_url;
    ASSERT_TRUE(
        CBB_add_asn1(&dp_fullname, &dp_url, CBS_ASN1_CONTEXT_SPECIFIC | 6));
    ASSERT_TRUE(CBBAddBytes(&dp_url, url.spec()));
    ASSERT_TRUE(CBB_flush(&dp_fullname));
  }

  SetExtension(der::Input(kCrlDistributionPointsOid), FinishCBB(cbb.get()));
}

void CertBuilder::SetSubjectCommonName(base::StringPiece common_name) {
  // See RFC 4519.
  static const uint8_t kCommonName[] = {0x55, 0x04, 0x03};

  // See RFC 5280, section 4.1.2.4.
  bssl::ScopedCBB cbb;
  CBB rdns, rdn, attr, type, value;
  ASSERT_TRUE(CBB_init(cbb.get(), 64));
  ASSERT_TRUE(CBB_add_asn1(cbb.get(), &rdns, CBS_ASN1_SEQUENCE));
  ASSERT_TRUE(CBB_add_asn1(&rdns, &rdn, CBS_ASN1_SET));
  ASSERT_TRUE(CBB_add_asn1(&rdn, &attr, CBS_ASN1_SEQUENCE));
  ASSERT_TRUE(CBB_add_asn1(&attr, &type, CBS_ASN1_OBJECT));
  ASSERT_TRUE(CBBAddBytes(&type, kCommonName));
  ASSERT_TRUE(CBB_add_asn1(&attr, &value, CBS_ASN1_UTF8STRING));
  ASSERT_TRUE(CBBAddBytes(&value, common_name));

  subject_tlv_ = FinishCBB(cbb.get());
  Invalidate();
}

void CertBuilder::SetSubject(base::span<const uint8_t> subject_tlv) {
  subject_tlv_.assign(subject_tlv.begin(), subject_tlv.end());
  Invalidate();
}

void CertBuilder::SetSubjectAltName(const std::string& dns_name) {
  SetSubjectAltNames({dns_name}, {});
}

void CertBuilder::SetSubjectAltNames(
    const std::vector<std::string>& dns_names,
    const std::vector<IPAddress>& ip_addresses) {
  // From RFC 5280:
  //
  //   SubjectAltName ::= GeneralNames
  //
  //   GeneralNames ::= SEQUENCE SIZE (1..MAX) OF GeneralName
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
  SetExtension(der::Input(kSubjectAltNameOid), FinishCBB(cbb.get()));
}

void CertBuilder::SetKeyUsages(const std::vector<KeyUsageBit>& usages) {
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
  SetExtension(der::Input(kKeyUsageOid), FinishCBB(cbb.get()));
}

void CertBuilder::SetExtendedKeyUsages(
    const std::vector<der::Input>& purpose_oids) {
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
    ASSERT_TRUE(CBBAddBytes(&purpose_cbb, oid.AsStringPiece()));
    ASSERT_TRUE(CBB_flush(&eku));
  }
  SetExtension(der::Input(kExtKeyUsageOid), FinishCBB(cbb.get()));
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

  SetExtension(der::Input(kCertificatePoliciesOid), FinishCBB(cbb.get()));
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
  SetExtension(der::Input(kSubjectKeyIdentifierOid), FinishCBB(cbb.get()),
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
    EraseExtension(der::Input(kAuthorityKeyIdentifierOid));
    return;
  }

  // From RFC 5280:
  //
  //   AuthorityKeyIdentifier ::= SEQUENCE {
  //       keyIdentifier             [0] KeyIdentifier           OPTIONAL,
  //       authorityCertIssuer       [1] GeneralNames            OPTIONAL,
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

  SetExtension(der::Input(kAuthorityKeyIdentifierOid), FinishCBB(cbb.get()));
}

void CertBuilder::SetSignatureAlgorithmRsaPkca1(DigestAlgorithm digest) {
  switch (digest) {
    case DigestAlgorithm::Sha256: {
      SetSignatureAlgorithm(Sha256WithRSAEncryption());
      break;
    }

    case DigestAlgorithm::Sha1: {
      SetSignatureAlgorithm(Sha1WithRSAEncryption());
      break;
    }

    default:
      ASSERT_TRUE(false);
  }
}

void CertBuilder::SetSignatureAlgorithm(std::string algorithm_tlv) {
  signature_algorithm_tlv_ = std::move(algorithm_tlv);
  Invalidate();
}

void CertBuilder::SetRandomSerialNumber() {
  serial_number_ = base::RandUint64();
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
  std::string ski_oid = der::Input(kSubjectKeyIdentifierOid).AsString();
  if (extensions_.find(ski_oid) == extensions_.end()) {
    // If no SKI is present, this means that the certificate was either
    // created by FromStaticCert() and lacked one, or it was explicitly
    // deleted as an extension.
    return std::string();
  }

  auto& extension_value = extensions_[ski_oid];
  der::Input ski_value;
  if (!ParseSubjectKeyIdentifier(der::Input(&extension_value.value),
                                 &ski_value)) {
    return std::string();
  }
  return ski_value.AsString();
}

bool CertBuilder::GetValidity(base::Time* not_before,
                              base::Time* not_after) const {
  der::GeneralizedTime not_before_generalized_time;
  der::GeneralizedTime not_after_generalized_time;
  if (!ParseValidity(der::Input(&validity_tlv_), &not_before_generalized_time,
                     &not_after_generalized_time) ||
      !GeneralizedTimeToTime(not_before_generalized_time, not_before) ||
      !GeneralizedTimeToTime(not_after_generalized_time, not_after)) {
    return false;
  }
  return true;
}

EVP_PKEY* CertBuilder::GetKey() {
  if (!key_)
    GenerateKey();
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

std::string CertBuilder::GetDER() {
  return std::string(x509_util::CryptoBufferAsStringPiece(GetCertBuffer()));
}

CertBuilder::CertBuilder(CRYPTO_BUFFER* orig_cert,
                         CertBuilder* issuer,
                         bool unique_subject_key_identifier)
    : issuer_(issuer) {
  if (!issuer_)
    issuer_ = this;

  crypto::EnsureOpenSSLInit();
  if (orig_cert)
    InitFromCert(der::Input(x509_util::CryptoBufferAsStringPiece(orig_cert)));

  if (unique_subject_key_identifier) {
    GenerateSubjectKeyIdentifier();
    SetAuthorityKeyIdentifier(issuer_->GetSubjectKeyIdentifier());
  }
}

void CertBuilder::Invalidate() {
  cert_.reset();
}

void CertBuilder::GenerateKey() {
  ASSERT_FALSE(key_);

  auto private_key = crypto::RSAPrivateKey::Create(2048);
  key_ = bssl::UpRef(private_key->key());
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

void CertBuilder::InitFromCert(const der::Input& cert) {
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
  der::Parser parser(cert);
  der::Parser certificate;
  der::Parser tbs_certificate;
  ASSERT_TRUE(parser.ReadSequence(&certificate));
  ASSERT_TRUE(certificate.ReadSequence(&tbs_certificate));

  // version
  bool unused;
  ASSERT_TRUE(tbs_certificate.SkipOptionalTag(
      der::kTagConstructed | der::kTagContextSpecific | 0, &unused));
  // serialNumber
  ASSERT_TRUE(tbs_certificate.SkipTag(der::kInteger));

  // signature
  der::Input signature_algorithm_tlv;
  ASSERT_TRUE(tbs_certificate.ReadRawTLV(&signature_algorithm_tlv));
  signature_algorithm_tlv_ = signature_algorithm_tlv.AsString();

  // issuer
  ASSERT_TRUE(tbs_certificate.SkipTag(der::kSequence));

  // validity
  der::Input validity_tlv;
  ASSERT_TRUE(tbs_certificate.ReadRawTLV(&validity_tlv));
  validity_tlv_ = validity_tlv.AsString();

  // subject
  ASSERT_TRUE(tbs_certificate.SkipTag(der::kSequence));
  // subjectPublicKeyInfo
  ASSERT_TRUE(tbs_certificate.SkipTag(der::kSequence));
  // issuerUniqueID
  ASSERT_TRUE(tbs_certificate.SkipOptionalTag(der::ContextSpecificPrimitive(1),
                                              &unused));
  // subjectUniqueID
  ASSERT_TRUE(tbs_certificate.SkipOptionalTag(der::ContextSpecificPrimitive(2),
                                              &unused));

  // extensions
  absl::optional<der::Input> extensions_tlv;
  ASSERT_TRUE(tbs_certificate.ReadOptionalTag(
      der::ContextSpecificConstructed(3), &extensions_tlv));
  if (extensions_tlv) {
    std::map<der::Input, ParsedExtension> parsed_extensions;
    ASSERT_TRUE(ParseExtensions(extensions_tlv.value(), &parsed_extensions));

    for (const auto& parsed_extension : parsed_extensions) {
      SetExtension(parsed_extension.second.oid,
                   parsed_extension.second.value.AsString(),
                   parsed_extension.second.critical);
    }
  }
}

void CertBuilder::BuildTBSCertificate(std::string* out) {
  bssl::ScopedCBB cbb;
  CBB tbs_cert, version, extensions_context, extensions;

  ASSERT_TRUE(CBB_init(cbb.get(), 64));
  ASSERT_TRUE(CBB_add_asn1(cbb.get(), &tbs_cert, CBS_ASN1_SEQUENCE));
  ASSERT_TRUE(
      CBB_add_asn1(&tbs_cert, &version,
                   CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0));
  // Always use v3 certificates.
  ASSERT_TRUE(CBB_add_asn1_uint64(&version, 2));
  ASSERT_TRUE(CBB_add_asn1_uint64(&tbs_cert, GetSerialNumber()));
  ASSERT_TRUE(AddSignatureAlgorithm(&tbs_cert));
  ASSERT_TRUE(CBBAddBytes(&tbs_cert, issuer_->GetSubject()));
  ASSERT_TRUE(CBBAddBytes(&tbs_cert, validity_tlv_));
  ASSERT_TRUE(CBBAddBytes(&tbs_cert, GetSubject()));
  ASSERT_TRUE(EVP_marshal_public_key(&tbs_cert, GetKey()));

  // Serialize all the extensions.
  if (!extensions_.empty()) {
    ASSERT_TRUE(
        CBB_add_asn1(&tbs_cert, &extensions_context,
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
    for (const auto& extension_it : extensions_) {
      CBB extension_seq, oid, extn_value;
      ASSERT_TRUE(CBB_add_asn1(&extensions, &extension_seq, CBS_ASN1_SEQUENCE));
      ASSERT_TRUE(CBB_add_asn1(&extension_seq, &oid, CBS_ASN1_OBJECT));
      ASSERT_TRUE(CBBAddBytes(&oid, extension_it.first));
      if (extension_it.second.critical) {
        ASSERT_TRUE(CBB_add_asn1_bool(&extension_seq, true));
      }

      ASSERT_TRUE(
          CBB_add_asn1(&extension_seq, &extn_value, CBS_ASN1_OCTETSTRING));
      ASSERT_TRUE(CBBAddBytes(&extn_value, extension_it.second.value));
      ASSERT_TRUE(CBB_flush(&extensions));
    }
  }

  *out = FinishCBB(cbb.get());
}

bool CertBuilder::AddSignatureAlgorithm(CBB* cbb) {
  return CBBAddBytes(cbb, signature_algorithm_tlv_);
}

void CertBuilder::GenerateCertificate() {
  ASSERT_FALSE(cert_);

  std::string tbs_cert;
  BuildTBSCertificate(&tbs_cert);
  const uint8_t* tbs_cert_bytes =
      reinterpret_cast<const uint8_t*>(tbs_cert.data());

  // Determine the correct digest algorithm to use (assumes RSA PKCS#1
  // signatures).
  auto signature_algorithm = SignatureAlgorithm::Create(
      der::Input(&signature_algorithm_tlv_), nullptr);
  ASSERT_TRUE(signature_algorithm);
  ASSERT_EQ(SignatureAlgorithmId::RsaPkcs1, signature_algorithm->algorithm());
  const EVP_MD* md = nullptr;

  switch (signature_algorithm->digest()) {
    case DigestAlgorithm::Sha256:
      md = EVP_sha256();
      break;

    case DigestAlgorithm::Sha1:
      md = EVP_sha1();
      break;

    default:
      ASSERT_TRUE(false) << "Only rsaEncryptionWithSha256 or "
                            "rsaEnryptionWithSha1 are supported";
      break;
  }

  // Sign the TBSCertificate and write the entire certificate.
  bssl::ScopedCBB cbb;
  CBB cert, signature;
  bssl::ScopedEVP_MD_CTX ctx;
  uint8_t* sig_out;
  size_t sig_len;

  ASSERT_TRUE(CBB_init(cbb.get(), tbs_cert.size()));
  ASSERT_TRUE(CBB_add_asn1(cbb.get(), &cert, CBS_ASN1_SEQUENCE));
  ASSERT_TRUE(CBBAddBytes(&cert, tbs_cert));
  ASSERT_TRUE(AddSignatureAlgorithm(&cert));
  ASSERT_TRUE(CBB_add_asn1(&cert, &signature, CBS_ASN1_BITSTRING));
  ASSERT_TRUE(CBB_add_u8(&signature, 0 /* no unused bits */));
  ASSERT_TRUE(
      EVP_DigestSignInit(ctx.get(), nullptr, md, nullptr, issuer_->GetKey()));
  ASSERT_TRUE(EVP_DigestSign(ctx.get(), nullptr, &sig_len, tbs_cert_bytes,
                             tbs_cert.size()));
  ASSERT_TRUE(CBB_reserve(&signature, &sig_out, sig_len));
  ASSERT_TRUE(EVP_DigestSign(ctx.get(), sig_out, &sig_len, tbs_cert_bytes,
                             tbs_cert.size()));
  ASSERT_TRUE(CBB_did_write(&signature, sig_len));

  auto cert_der = FinishCBB(cbb.get());
  cert_ =
      x509_util::CreateCryptoBuffer(base::as_bytes(base::make_span(cert_der)));
}

}  // namespace net
