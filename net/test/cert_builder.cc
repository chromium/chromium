// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/cert_builder.h"

#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "crypto/openssl_util.h"
#include "crypto/rsa_private_key.h"
#include "net/cert/internal/parse_certificate.h"
#include "net/cert/x509_util.h"
#include "net/der/encode_values.h"
#include "net/der/input.h"
#include "net/der/parse_values.h"
#include "net/der/parser.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
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

std::string Md5WithRSAEncryption() {
  const uint8_t kMd5WithRSAEncryption[] = {0x30, 0x0d, 0x06, 0x09, 0x2a,
                                           0x86, 0x48, 0x86, 0xf7, 0x0d,
                                           0x01, 0x01, 0x04, 0x05, 0x00};
  return std::string(std::begin(kMd5WithRSAEncryption),
                     std::end(kMd5WithRSAEncryption));
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

// Adds a RFC 5280 Time value to the given CBB.
// The argument ordering follows the boringssl CBB_* api style.
bool CBBAddTime(CBB* cbb, const base::Time& time) {
  der::GeneralizedTime generalized_time;
  if (!der::EncodeTimeAsGeneralizedTime(time, &generalized_time))
    return false;
  CBB time_cbb;
  if (generalized_time.year < 2050) {
    uint8_t out[der::kUTCTimeLength];
    if (!der::EncodeUTCTime(generalized_time, out) ||
        !CBB_add_asn1(cbb, &time_cbb, CBS_ASN1_UTCTIME) ||
        !CBBAddBytes(&time_cbb, out) || !CBB_flush(cbb))
      return false;
  } else {
    uint8_t out[der::kGeneralizedTimeLength];
    if (!der::EncodeGeneralizedTime(generalized_time, out) ||
        !CBB_add_asn1(cbb, &time_cbb, CBS_ASN1_GENERALIZEDTIME) ||
        !CBBAddBytes(&time_cbb, out) || !CBB_flush(cbb))
      return false;
  }
  return true;
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
    : issuer_(issuer) {
  if (!issuer_)
    issuer_ = this;

  crypto::EnsureOpenSSLInit();
  InitFromCert(der::Input(x509_util::CryptoBufferAsStringPiece(orig_cert)));
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
  (*out_intermediate)->EraseExtension(CrlDistributionPointsOid());
  (*out_intermediate)->EraseExtension(AuthorityInfoAccessOid());
  *out_leaf = std::make_unique<CertBuilder>(orig_certs[0]->cert_buffer(),
                                            out_intermediate->get());
  (*out_leaf)->SetSubjectAltName(kHostname);
  (*out_leaf)->EraseExtension(CrlDistributionPointsOid());
  (*out_leaf)->EraseExtension(AuthorityInfoAccessOid());
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

void CertBuilder::SetCaIssuersUrl(const GURL& url) {
  SetCaIssuersAndOCSPUrls({url}, {});
}

void CertBuilder::SetCaIssuersAndOCSPUrls(
    const std::vector<GURL>& ca_issuers_urls,
    const std::vector<GURL>& ocsp_urls) {
  std::vector<std::pair<der::Input, GURL>> entries;
  for (const auto& url : ca_issuers_urls)
    entries.emplace_back(AdCaIssuersOid(), url);
  for (const auto& url : ocsp_urls)
    entries.emplace_back(AdOcspOid(), url);
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

  SetExtension(AuthorityInfoAccessOid(), FinishCBB(cbb.get()));
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

  SetExtension(CrlDistributionPointsOid(), FinishCBB(cbb.get()));
}

void CertBuilder::SetSubjectCommonName(const std::string common_name) {
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
}

void CertBuilder::SetSubjectAltName(const std::string& dns_name) {
  // From RFC 5280:
  //
  //   SubjectAltName ::= GeneralNames
  //
  //   GeneralNames ::= SEQUENCE SIZE (1..MAX) OF GeneralName
  //
  //   GeneralName ::= CHOICE {
  //        otherName                       [0]     OtherName,
  //        rfc822Name                      [1]     IA5String,
  //        dNSName                         [2]     IA5String,
  //        ... }
  bssl::ScopedCBB cbb;
  CBB general_names, general_name;
  ASSERT_TRUE(CBB_init(cbb.get(), dns_name.size()));
  ASSERT_TRUE(CBB_add_asn1(cbb.get(), &general_names, CBS_ASN1_SEQUENCE));
  ASSERT_TRUE(CBB_add_asn1(&general_names, &general_name,
                           CBS_ASN1_CONTEXT_SPECIFIC | 2));
  ASSERT_TRUE(CBBAddBytes(&general_name, dns_name));

  SetExtension(SubjectAltNameOid(), FinishCBB(cbb.get()));
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

  SetExtension(CertificatePoliciesOid(), FinishCBB(cbb.get()));
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
  ASSERT_TRUE(CBBAddTime(&validity, not_before));
  ASSERT_TRUE(CBBAddTime(&validity, not_after));
  validity_tlv_ = FinishCBB(cbb.get());
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
  return x509_util::CryptoBufferAsStringPiece(GetCertBuffer()).as_string();
}

// static
std::string CertBuilder::CreateCrl(CertBuilder* crl_issuer,
                                   const std::vector<uint64_t>& revoked_serials,
                                   DigestAlgorithm digest) {
  std::string signature_algorithm;
  const EVP_MD* md = nullptr;
  switch (digest) {
    case DigestAlgorithm::Sha256: {
      signature_algorithm = Sha256WithRSAEncryption();
      md = EVP_sha256();
      break;
    }

    case DigestAlgorithm::Sha1: {
      signature_algorithm = Sha1WithRSAEncryption();
      md = EVP_sha1();
      break;
    }

    case DigestAlgorithm::Md5: {
      signature_algorithm = Md5WithRSAEncryption();
      md = EVP_md5();
      break;
    }

    default:
      ADD_FAILURE();
      return std::string();
  }
  //    TBSCertList  ::=  SEQUENCE  {
  //         version                 Version OPTIONAL,
  //                                      -- if present, MUST be v2
  //         signature               AlgorithmIdentifier,
  //         issuer                  Name,
  //         thisUpdate              Time,
  //         nextUpdate              Time OPTIONAL,
  //         revokedCertificates     SEQUENCE OF SEQUENCE  {
  //              userCertificate         CertificateSerialNumber,
  //              revocationDate          Time,
  //              crlEntryExtensions      Extensions OPTIONAL
  //                                       -- if present, version MUST be v2
  //                                   }  OPTIONAL,
  //         crlExtensions           [0]  EXPLICIT Extensions OPTIONAL
  //                                       -- if present, version MUST be v2
  //                                   }
  bssl::ScopedCBB tbs_cbb;
  CBB tbs_cert_list, revoked_serials_cbb;
  if (!CBB_init(tbs_cbb.get(), 10) ||
      !CBB_add_asn1(tbs_cbb.get(), &tbs_cert_list, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1_uint64(&tbs_cert_list, 1 /* V2 */) ||
      !CBBAddBytes(&tbs_cert_list, signature_algorithm) ||
      !CBBAddBytes(&tbs_cert_list, crl_issuer->GetSubject()) ||
      !CBBAddTime(&tbs_cert_list,
                  base::Time::Now() - base::TimeDelta::FromDays(1)) ||
      !CBBAddTime(&tbs_cert_list,
                  base::Time::Now() + base::TimeDelta::FromDays(6))) {
    ADD_FAILURE();
    return std::string();
  }
  if (!revoked_serials.empty()) {
    if (!CBB_add_asn1(&tbs_cert_list, &revoked_serials_cbb,
                      CBS_ASN1_SEQUENCE)) {
      ADD_FAILURE();
      return std::string();
    }
    for (const int64_t revoked_serial : revoked_serials) {
      CBB revoked_serial_cbb;
      if (!CBB_add_asn1(&revoked_serials_cbb, &revoked_serial_cbb,
                        CBS_ASN1_SEQUENCE) ||
          !CBB_add_asn1_uint64(&revoked_serial_cbb, revoked_serial) ||
          !CBBAddTime(&revoked_serial_cbb,
                      base::Time::Now() - base::TimeDelta::FromDays(1)) ||
          !CBB_flush(&revoked_serials_cbb)) {
        ADD_FAILURE();
        return std::string();
      }
    }
  }

  std::string tbs_tlv = FinishCBB(tbs_cbb.get());

  //    CertificateList  ::=  SEQUENCE  {
  //         tbsCertList          TBSCertList,
  //         signatureAlgorithm   AlgorithmIdentifier,
  //         signatureValue       BIT STRING  }
  bssl::ScopedCBB crl_cbb;
  CBB cert_list, signature;
  bssl::ScopedEVP_MD_CTX ctx;
  uint8_t* sig_out;
  size_t sig_len;
  if (!CBB_init(crl_cbb.get(), 10) ||
      !CBB_add_asn1(crl_cbb.get(), &cert_list, CBS_ASN1_SEQUENCE) ||
      !CBBAddBytes(&cert_list, tbs_tlv) ||
      !CBBAddBytes(&cert_list, signature_algorithm) ||
      !CBB_add_asn1(&cert_list, &signature, CBS_ASN1_BITSTRING) ||
      !CBB_add_u8(&signature, 0 /* no unused bits */) ||
      !EVP_DigestSignInit(ctx.get(), nullptr, md, nullptr,
                          crl_issuer->GetKey()) ||
      !EVP_DigestSign(ctx.get(), nullptr, &sig_len,
                      reinterpret_cast<const uint8_t*>(tbs_tlv.data()),
                      tbs_tlv.size()) ||
      !CBB_reserve(&signature, &sig_out, sig_len) ||
      !EVP_DigestSign(ctx.get(), sig_out, &sig_len,
                      reinterpret_cast<const uint8_t*>(tbs_tlv.data()),
                      tbs_tlv.size()) ||
      !CBB_did_write(&signature, sig_len)) {
    ADD_FAILURE();
    return std::string();
  }
  return FinishCBB(crl_cbb.get());
}

void CertBuilder::Invalidate() {
  cert_.reset();
}

void CertBuilder::GenerateKey() {
  ASSERT_FALSE(key_);

  auto private_key = crypto::RSAPrivateKey::Create(2048);
  key_ = bssl::UpRef(private_key->key());
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
  bool has_extensions = false;
  der::Input extensions_tlv;
  ASSERT_TRUE(tbs_certificate.ReadOptionalTag(
      der::ContextSpecificConstructed(3), &extensions_tlv, &has_extensions));
  if (has_extensions) {
    std::map<der::Input, ParsedExtension> parsed_extensions;
    ASSERT_TRUE(ParseExtensions(extensions_tlv, &parsed_extensions));

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
  cert_ = x509_util::CreateCryptoBuffer(
      reinterpret_cast<const uint8_t*>(cert_der.data()), cert_der.size());
}

}  // namespace net
