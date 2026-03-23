// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/page_info/certificate/model/x509_certificate_model_ios.h"

#import "base/check.h"
#import "base/containers/adapters.h"
#import "base/containers/fixed_flat_map.h"
#import "base/containers/span.h"
#import "base/hash/sha1.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/string_util.h"
#import "base/strings/string_view_util.h"
#import "base/strings/stringprintf.h"
#import "components/strings/grit/components_strings.h"
#import "crypto/sha2.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/cert/ct_objects_extractor.h"
#import "net/cert/time_conversions.h"
#import "net/cert/x509_certificate.h"
#import "third_party/boringssl/src/include/openssl/bytestring.h"
#import "third_party/boringssl/src/include/openssl/evp.h"
#import "third_party/boringssl/src/include/openssl/mem.h"
#import "third_party/boringssl/src/pki/cert_errors.h"
#import "third_party/boringssl/src/pki/certificate_policies.h"
#import "third_party/boringssl/src/pki/extended_key_usage.h"
#import "third_party/boringssl/src/pki/general_names.h"
#import "third_party/boringssl/src/pki/input.h"
#import "third_party/boringssl/src/pki/parse_certificate.h"
#import "third_party/boringssl/src/pki/parse_name.h"
#import "third_party/boringssl/src/pki/parse_values.h"
#import "third_party/boringssl/src/pki/signature_algorithm.h"
#import "ui/base/l10n/l10n_util.h"

namespace x509_certificate_model {

namespace {

// Additional OID constants not provided by the base class.

// Business Category OID: 2.5.4.15
constexpr uint8_t KTypeBusinessCategoryOid[] = {0x55, 0x04, 0x0f};
// Jurisdiction Country OID: 1.3.6.1.4.1.311.60.2.1.3
constexpr uint8_t KTypeJurisdictionCountryOid[] = {
    0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x3c, 0x02, 0x01, 0x03};
// Jurisdiction State OID: 1.3.6.1.4.1.311.60.2.1.2
constexpr uint8_t KTypeJurisdictionStateOid[] = {
    0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x3c, 0x02, 0x01, 0x02};

// OCSP Access Method OID: 1.3.6.1.5.5.7.48.1
constexpr uint8_t KTypeAdOcspOid[] = {0x2B, 0x06, 0x01, 0x05,
                                      0x05, 0x07, 0x30, 0x01};
// CA Issuers Access Method OID: 1.3.6.1.5.5.7.48.2
constexpr uint8_t KTypeAdCaIssuersOid[] = {0x2B, 0x06, 0x01, 0x05,
                                           0x05, 0x07, 0x30, 0x02};

// Known extension OIDs that are handled by specific methods.
const bssl::der::Input kKnownExtensionOids[] = {
    bssl::der::Input(bssl::kBasicConstraintsOid),
    bssl::der::Input(bssl::kKeyUsageOid),
    bssl::der::Input(bssl::kExtKeyUsageOid),
    bssl::der::Input(bssl::kSubjectKeyIdentifierOid),
    bssl::der::Input(bssl::kAuthorityKeyIdentifierOid),
    bssl::der::Input(bssl::kAuthorityInfoAccessOid),
    bssl::der::Input(bssl::kSubjectAltNameOid),
    bssl::der::Input(bssl::kCertificatePoliciesOid),
    bssl::der::Input(bssl::kCrlDistributionPointsOid),
    bssl::der::Input(net::ct::kEmbeddedSCTOid),
};

// Converts a bssl::der::Input to a std::string_view.
std::string_view InputToStringView(bssl::der::Input in) {
  return std::string_view(reinterpret_cast<const char*>(in.data()), in.size());
}

// Converts an OID (DER-encoded) to dotted decimal notation (e.g., "2.5.29.32")
std::string OidToString(bssl::der::Input oid) {
  CBS cbs;
  CBS_init(&cbs, oid.data(), oid.size());
  bssl::UniquePtr<char> text(CBS_asn1_oid_to_text(&cbs));
  if (!text) {
    return std::string();
  }
  return std::string(text.get());
}

// Format byte data as hex string with separator.
std::string FormatHexWithSeparator(base::span<const uint8_t> data,
                                   char separator) {
  std::string hex = base::HexEncode(data);
  if (hex.empty()) {
    return std::string();
  }
  std::string result;
  result.reserve(hex.length() + hex.length() / 2);
  for (size_t i = 0; i < hex.length(); i += 2) {
    if (i > 0) {
      result += separator;
    }
    result += hex.substr(i, 2);
  }
  return result;
}

constexpr auto kOidStringMap = base::MakeFixedFlatMap<bssl::der::Input, int>({
    // Extended Key Usage fields:
    {bssl::der::Input(bssl::kServerAuth), IDS_IOS_CERT_EKU_SERVER_AUTH},
    {bssl::der::Input(bssl::kClientAuth), IDS_IOS_CERT_EKU_CLIENT_AUTH},
    {bssl::der::Input(bssl::kOCSPSigning), IDS_IOS_CERT_EKU_OCSP_SIGNING},
    {bssl::der::Input(bssl::kDocumentSigning),
     IDS_IOS_CERT_EKU_DOCUMENT_SIGNING},
    {bssl::der::Input(bssl::kRcsMlsClient), IDS_IOS_CERT_EKU_RCS_MLS_CLIENT},
    // Authority Information Access fields:
    {bssl::der::Input(KTypeAdOcspOid), IDS_IOS_CERT_AIA_ACCESS_METHOD_OCSP},
    {bssl::der::Input(KTypeAdCaIssuersOid),
     IDS_IOS_CERT_AIA_ACCESS_METHOD_CA_ISSUERS},
    // Certificate Policy Qualifier fields:
    {bssl::der::Input(bssl::kCpsPointerId), IDS_IOS_CERT_POLICY_QUALIFIER_CPS},
    {bssl::der::Input(bssl::kUserNoticeId),
     IDS_IOS_CERT_PKIX_USER_NOTICE_QUALIFIER_DESCRIPTION},
});

std::optional<std::string> GetOidText(bssl::der::Input oid) {
  const auto i = kOidStringMap.find(oid);
  if (i != kOidStringMap.end()) {
    return l10n_util::GetStringUTF8(i->second);
  }

  // Fall through to common OIDs shared with other platforms.
  std::optional<int> common_id =
      x509_certificate_model::GetCommonOidStringId(oid);
  if (common_id.has_value()) {
    return l10n_util::GetStringUTF8(*common_id);
  }

  return std::nullopt;
}

// Parses the SPKI to extract the algorithm OID and an algorithm parameters
// parser via output parameters. Returns false if parsing fails.
bool ParseSpkiAlgorithm(bssl::der::Input spki_tlv,
                        bssl::der::Input* algorithm_oid,
                        bssl::der::Parser* algorithm_parser) {
  bssl::der::Parser parser(spki_tlv);
  bssl::der::Parser spki_parser;
  if (!parser.ReadSequence(&spki_parser)) {
    return false;
  }

  // Read AlgorithmIdentifier
  if (!spki_parser.ReadSequence(algorithm_parser)) {
    return false;
  }

  if (!algorithm_parser->ReadTag(CBS_ASN1_OBJECT, algorithm_oid)) {
    return false;
  }

  return true;
}

std::string ProcessRawBytes(bssl::der::Input value) {
  return base::ToUpperASCII(FormatHexWithSeparator(
      base::as_byte_span(base::as_string_view(value)), ' '));
}

std::optional<std::string> ProcessUserNotice(bssl::der::Input qualifier) {
  bssl::der::Parser outer_parser(qualifier);
  bssl::der::Parser parser;
  if (!outer_parser.ReadSequence(&parser) || outer_parser.HasMore()) {
    return std::nullopt;
  }

  std::optional<bssl::der::Input> notice_ref_value;
  if (!parser.ReadOptionalTag(CBS_ASN1_SEQUENCE, &notice_ref_value)) {
    return std::nullopt;
  }

  std::string rv;
  if (notice_ref_value) {
    bssl::der::Parser notice_ref_parser(*notice_ref_value);
    CBS_ASN1_TAG organization_tag;
    bssl::der::Input organization_value;
    if (!notice_ref_parser.ReadTagAndValue(&organization_tag,
                                           &organization_value)) {
      return std::nullopt;
    }
    std::optional<std::string> s =
        ProcessUserNoticeDisplayText(organization_tag, organization_value);
    if (!s) {
      return std::nullopt;
    }
    rv += *s;
    rv += " - ";

    bssl::der::Parser notice_numbers_parser;
    if (!notice_ref_parser.ReadSequence(&notice_numbers_parser)) {
      return std::nullopt;
    }
    bool first = true;
    while (notice_numbers_parser.HasMore()) {
      bssl::der::Input notice_number;
      if (!notice_numbers_parser.ReadTag(CBS_ASN1_INTEGER, &notice_number)) {
        return std::nullopt;
      }
      if (!first) {
        rv += ", ";
      }
      rv += '#';
      uint64_t number;
      if (bssl::der::ParseUint64(notice_number, &number)) {
        rv += base::NumberToString(number);
      } else {
        rv += ProcessRawBytes(notice_number);
      }
      first = false;
    }
  }

  if (parser.HasMore()) {
    CBS_ASN1_TAG explicit_text_tag;
    bssl::der::Input explicit_text_value;
    if (!parser.ReadTagAndValue(&explicit_text_tag, &explicit_text_value)) {
      return std::nullopt;
    }

    std::optional<std::string> s =
        ProcessUserNoticeDisplayText(explicit_text_tag, explicit_text_value);
    if (!s) {
      return std::nullopt;
    }
    rv += *s;
  }

  if (parser.HasMore()) {
    return std::nullopt;
  }

  return rv;
}

}  // namespace

// PolicyQualifier implementation
X509CertificateModel::PolicyQualifier::PolicyQualifier() = default;
X509CertificateModel::PolicyQualifier::~PolicyQualifier() = default;
X509CertificateModel::PolicyQualifier::PolicyQualifier(const PolicyQualifier&) =
    default;
X509CertificateModel::PolicyQualifier&
X509CertificateModel::PolicyQualifier::operator=(const PolicyQualifier&) =
    default;

// CertificatePolicy implementation
X509CertificateModel::CertificatePolicy::CertificatePolicy() = default;
X509CertificateModel::CertificatePolicy::~CertificatePolicy() = default;
X509CertificateModel::CertificatePolicy::CertificatePolicy(
    const CertificatePolicy&) = default;
X509CertificateModel::CertificatePolicy&
X509CertificateModel::CertificatePolicy::operator=(const CertificatePolicy&) =
    default;

// X509CertificateModel implementation
X509CertificateModel::X509CertificateModel(
    bssl::UniquePtr<CRYPTO_BUFFER> cert_data)
    : X509CertificateModelBase(std::move(cert_data)) {
  if (!cert_data_) {
    return;
  }

  bssl::CertErrors unused_errors;
  if (!bssl::ParseCertificate(
          bssl::der::Input(CRYPTO_BUFFER_data(cert_data_.get()),
                           CRYPTO_BUFFER_len(cert_data_.get())),
          &tbs_certificate_tlv_, &signature_algorithm_tlv_, &signature_value_,
          &unused_errors)) {
    return;
  }

  bssl::ParseCertificateOptions options;
  options.allow_invalid_serial_numbers = true;

  if (!bssl::ParseTbsCertificate(tbs_certificate_tlv_, options, &tbs_,
                                 &unused_errors)) {
    return;
  }

  if (!bssl::ParseName(tbs_.subject_tlv, &subject_rdns_)) {
    return;
  }

  if (!bssl::ParseName(tbs_.issuer_tlv, &issuer_rdns_)) {
    return;
  }

  // Parse extensions if present.
  if (tbs_.extensions_tlv) {
    ParseExtensions(*tbs_.extensions_tlv);
  }

  // Parse SubjectAltName extension if present. Use find() instead of
  // `ConsumeExtension()` to avoid removing the extension from the map, so that
  // `HasSubjectAlternativeNames()` continues to work.
  auto san_it = extensions_.find(bssl::der::Input(bssl::kSubjectAltNameOid));
  if (san_it != extensions_.end()) {
    // The extension value is the OCTET STRING contents, which contains a
    // GeneralNames SEQUENCE TLV. Use Create() which expects the full TLV.
    bssl::CertErrors errors;
    subject_alt_names_ =
        bssl::GeneralNames::Create(san_it->second.value, &errors);
  }

  parsed_successfully_ = true;
}

X509CertificateModel::X509CertificateModel(const net::X509Certificate* cert)
    : X509CertificateModel(cert ? bssl::UpRef(cert->cert_buffer()) : nullptr) {}

X509CertificateModel::X509CertificateModel(X509CertificateModel&& other) =
    default;

X509CertificateModel::~X509CertificateModel() = default;

std::string X509CertificateModel::HashCertSHA256() const {
  if (!cert_data_) {
    return std::string();
  }

  std::string hash = crypto::SHA256HashString(std::string_view(
      reinterpret_cast<const char*>(CRYPTO_BUFFER_data(cert_data_.get())),
      CRYPTO_BUFFER_len(cert_data_.get())));

  return base::ToUpperASCII(
      FormatHexWithSeparator(base::as_byte_span(hash), ' '));
}

std::string X509CertificateModel::HashSpkiSHA256() const {
  CHECK(is_valid());
  std::string spki_hash =
      crypto::SHA256HashString(InputToStringView(tbs_.spki_tlv));

  return base::ToUpperASCII(
      FormatHexWithSeparator(base::as_byte_span(spki_hash), ' '));
}

int X509CertificateModel::GetVersion() const {
  CHECK(is_valid());
  switch (tbs_.version) {
    case bssl::CertificateVersion::V1:
      return 1;
    case bssl::CertificateVersion::V2:
      return 2;
    case bssl::CertificateVersion::V3:
      return 3;
  }
  return 0;
}

std::string X509CertificateModel::GetSerialNumberHexified() const {
  CHECK(is_valid());
  std::string serial(InputToStringView(tbs_.serial_number));
  std::string result = FormatHexWithSeparator(base::as_byte_span(serial), ' ');
  return base::ToUpperASCII(result);
}

OptionalStringOrError X509CertificateModel::GetSubjectCountryName() const {
  CHECK(is_valid());
  return FindFirstNameOfType(bssl::der::Input(bssl::kTypeCountryNameOid),
                             subject_rdns_);
}

OptionalStringOrError X509CertificateModel::GetSubjectLocalityName() const {
  CHECK(is_valid());
  return FindFirstNameOfType(bssl::der::Input(bssl::kTypeLocalityNameOid),
                             subject_rdns_);
}

OptionalStringOrError X509CertificateModel::GetSubjectStateName() const {
  CHECK(is_valid());
  return FindFirstNameOfType(
      bssl::der::Input(bssl::kTypeStateOrProvinceNameOid), subject_rdns_);
}

OptionalStringOrError X509CertificateModel::GetSubjectBusinessCategory() const {
  CHECK(is_valid());
  return FindFirstNameOfType(bssl::der::Input(KTypeBusinessCategoryOid),
                             subject_rdns_);
}

OptionalStringOrError X509CertificateModel::GetSubjectJurisdictionCountryName()
    const {
  CHECK(is_valid());
  return FindFirstNameOfType(bssl::der::Input(KTypeJurisdictionCountryOid),
                             subject_rdns_);
}

OptionalStringOrError X509CertificateModel::GetSubjectJurisdictionStateName()
    const {
  CHECK(is_valid());
  return FindFirstNameOfType(bssl::der::Input(KTypeJurisdictionStateOid),
                             subject_rdns_);
}

OptionalStringOrError X509CertificateModel::GetSubjectSerialNumber() const {
  CHECK(is_valid());
  return FindFirstNameOfType(bssl::der::Input(bssl::kTypeSerialNumberOid),
                             subject_rdns_);
}

OptionalStringOrError X509CertificateModel::GetIssuerCountryName() const {
  CHECK(is_valid());
  return FindFirstNameOfType(bssl::der::Input(bssl::kTypeCountryNameOid),
                             issuer_rdns_);
}

std::optional<size_t> X509CertificateModel::GetPublicKeySize() const {
  CHECK(is_valid());

  CBS cbs;
  CBS_init(&cbs, tbs_.spki_tlv.data(), tbs_.spki_tlv.size());
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_parse_public_key(&cbs));

  if (pkey) {
    int bits = EVP_PKEY_bits(pkey.get());
    if (bits > 0) {
      return static_cast<size_t>(bits);
    }
  }

  return std::nullopt;
}

std::string X509CertificateModel::GetPublicKeyAlgorithm() const {
  CHECK(is_valid());
  bssl::der::Input algorithm_oid;
  bssl::der::Parser algorithm_parser;
  if (!ParseSpkiAlgorithm(tbs_.spki_tlv, &algorithm_oid, &algorithm_parser)) {
    return std::string();
  }

  std::optional<std::string> oid_text = GetOidText(algorithm_oid);
  if (oid_text) {
    return *oid_text;
  }

  return std::string();
}

std::string X509CertificateModel::GetPublicKeyData() const {
  CHECK(is_valid());
  bssl::der::Input spki_tlv = tbs_.spki_tlv;
  bssl::der::Parser parser(spki_tlv);
  bssl::der::Parser spki_parser;
  if (!parser.ReadSequence(&spki_parser)) {
    return std::string();
  }

  // Skip AlgorithmIdentifier
  if (!spki_parser.SkipTag(CBS_ASN1_SEQUENCE)) {
    return std::string();
  }

  // Read subjectPublicKey BIT STRING
  auto public_key_bit_string = spki_parser.ReadBitString();
  if (!public_key_bit_string) {
    return std::string();
  }

  std::string public_key_bytes(public_key_bit_string->bytes().begin(),
                               public_key_bit_string->bytes().end());
  return base::ToUpperASCII(
      FormatHexWithSeparator(base::as_byte_span(public_key_bytes), ' '));
}

bool X509CertificateModel::HasBasicConstraints() const {
  CHECK(is_valid());
  return extensions_.find(bssl::der::Input(bssl::kBasicConstraintsOid)) !=
         extensions_.end();
}

bool X509CertificateModel::IsBasicConstraintsCritical() const {
  CHECK(is_valid());
  auto it = extensions_.find(bssl::der::Input(bssl::kBasicConstraintsOid));
  if (it == extensions_.end()) {
    return false;
  }
  return it->second.critical;
}

bool X509CertificateModel::IsBasicConstraintsCa() const {
  CHECK(is_valid());
  auto it = extensions_.find(bssl::der::Input(bssl::kBasicConstraintsOid));
  if (it == extensions_.end()) {
    return false;
  }

  bssl::ParsedBasicConstraints basic_constraints;
  if (!bssl::ParseBasicConstraints(it->second.value, &basic_constraints)) {
    return false;
  }

  return basic_constraints.is_ca;
}

std::optional<uint8_t> X509CertificateModel::GetBasicConstraintsPathLen()
    const {
  CHECK(is_valid());
  auto it = extensions_.find(bssl::der::Input(bssl::kBasicConstraintsOid));
  if (it == extensions_.end()) {
    return std::nullopt;
  }

  bssl::ParsedBasicConstraints basic_constraints;
  if (!bssl::ParseBasicConstraints(it->second.value, &basic_constraints)) {
    return std::nullopt;
  }

  if (!basic_constraints.has_path_len) {
    return std::nullopt;
  }

  return basic_constraints.path_len;
}

bool X509CertificateModel::HasKeyUsage() const {
  CHECK(is_valid());
  return extensions_.find(bssl::der::Input(bssl::kKeyUsageOid)) !=
         extensions_.end();
}

bool X509CertificateModel::IsKeyUsageCritical() const {
  CHECK(is_valid());
  auto it = extensions_.find(bssl::der::Input(bssl::kKeyUsageOid));
  if (it == extensions_.end()) {
    return false;
  }
  return it->second.critical;
}

std::string X509CertificateModel::GetKeyUsageString() const {
  CHECK(is_valid());
  auto it = extensions_.find(bssl::der::Input(bssl::kKeyUsageOid));
  if (it == extensions_.end()) {
    return std::string();
  }

  bssl::der::BitString key_usage;
  if (!bssl::ParseKeyUsage(it->second.value, &key_usage)) {
    return std::string();
  }

  std::vector<std::string> usages;

  if (key_usage.AssertsBit(bssl::KEY_USAGE_BIT_DIGITAL_SIGNATURE)) {
    usages.push_back(l10n_util::GetStringUTF8(
        IDS_IOS_CERT_X509_KEY_USAGE_DIGITAL_SIGNATURE));
  }
  if (key_usage.AssertsBit(bssl::KEY_USAGE_BIT_NON_REPUDIATION)) {
    usages.push_back(l10n_util::GetStringUTF8(IDS_CERT_X509_KEY_USAGE_NONREP));
  }
  if (key_usage.AssertsBit(bssl::KEY_USAGE_BIT_KEY_ENCIPHERMENT)) {
    usages.push_back(
        l10n_util::GetStringUTF8(IDS_CERT_X509_KEY_USAGE_ENCIPHERMENT));
  }
  if (key_usage.AssertsBit(bssl::KEY_USAGE_BIT_DATA_ENCIPHERMENT)) {
    usages.push_back(
        l10n_util::GetStringUTF8(IDS_CERT_X509_KEY_USAGE_DATA_ENCIPHERMENT));
  }
  if (key_usage.AssertsBit(bssl::KEY_USAGE_BIT_KEY_AGREEMENT)) {
    usages.push_back(
        l10n_util::GetStringUTF8(IDS_CERT_X509_KEY_USAGE_KEY_AGREEMENT));
  }
  if (key_usage.AssertsBit(bssl::KEY_USAGE_BIT_KEY_CERT_SIGN)) {
    usages.push_back(
        l10n_util::GetStringUTF8(IDS_CERT_X509_KEY_USAGE_CERT_SIGNER));
  }
  if (key_usage.AssertsBit(bssl::KEY_USAGE_BIT_CRL_SIGN)) {
    usages.push_back(
        l10n_util::GetStringUTF8(IDS_CERT_X509_KEY_USAGE_CRL_SIGNER));
  }
  if (key_usage.AssertsBit(bssl::KEY_USAGE_BIT_ENCIPHER_ONLY)) {
    usages.push_back(
        l10n_util::GetStringUTF8(IDS_CERT_X509_KEY_USAGE_ENCIPHER_ONLY));
  }
  if (key_usage.AssertsBit(bssl::KEY_USAGE_BIT_DECIPHER_ONLY)) {
    usages.push_back(
        l10n_util::GetStringUTF8(IDS_CERT_X509_KEY_USAGE_DECIPHER_ONLY));
  }

  return base::JoinString(usages, ", ");
}

bool X509CertificateModel::HasExtendedKeyUsage() const {
  CHECK(is_valid());
  return extensions_.find(bssl::der::Input(bssl::kExtKeyUsageOid)) !=
         extensions_.end();
}

bool X509CertificateModel::IsExtendedKeyUsageCritical() const {
  CHECK(is_valid());
  auto it = extensions_.find(bssl::der::Input(bssl::kExtKeyUsageOid));
  if (it == extensions_.end()) {
    return false;
  }
  return it->second.critical;
}

std::vector<std::string> X509CertificateModel::GetExtendedKeyUsagePurposes()
    const {
  CHECK(is_valid());
  std::vector<std::string> purposes;

  auto it = extensions_.find(bssl::der::Input(bssl::kExtKeyUsageOid));
  if (it == extensions_.end()) {
    return purposes;
  }

  std::vector<bssl::der::Input> eku_oids;
  if (!bssl::ParseEKUExtension(it->second.value, &eku_oids)) {
    return purposes;
  }

  for (const auto& oid : eku_oids) {
    std::optional<std::string> oid_text = GetOidText(oid);
    if (oid_text) {
      purposes.push_back(*oid_text);
    } else {
      purposes.push_back(
          std::string(reinterpret_cast<const char*>(oid.data()), oid.size()));
    }
  }
  return purposes;
}

bool X509CertificateModel::HasSubjectKeyIdentifier() const {
  CHECK(is_valid());
  return extensions_.find(bssl::der::Input(bssl::kSubjectKeyIdentifierOid)) !=
         extensions_.end();
}

bool X509CertificateModel::IsSubjectKeyIdentifierCritical() const {
  CHECK(is_valid());
  auto it = extensions_.find(bssl::der::Input(bssl::kSubjectKeyIdentifierOid));
  if (it == extensions_.end()) {
    return false;
  }
  return it->second.critical;
}

std::string X509CertificateModel::GetSubjectKeyIdentifier() const {
  CHECK(is_valid());
  auto it = extensions_.find(bssl::der::Input(bssl::kSubjectKeyIdentifierOid));
  if (it == extensions_.end()) {
    return std::string();
  }

  bssl::der::Input subject_key_id;
  if (!bssl::ParseSubjectKeyIdentifier(it->second.value, &subject_key_id)) {
    return std::string();
  }

  return base::ToUpperASCII(FormatHexWithSeparator(
      base::as_byte_span(base::as_string_view(subject_key_id)), ' '));
}

std::string X509CertificateModel::GetSignatureAlgorithm() const {
  CHECK(is_valid());

  bssl::der::Input algorithm_oid;
  bssl::der::Input parameters;
  if (bssl::ParseAlgorithmIdentifier(signature_algorithm_tlv_, &algorithm_oid,
                                     &parameters)) {
    std::optional<std::string> oid_text = GetOidText(algorithm_oid);
    if (oid_text) {
      return *oid_text;
    }
  }
  return std::string();
}

std::string X509CertificateModel::GetSignatureParameters() const {
  CHECK(is_valid());

  // Parse the AlgorithmIdentifier to get parameters
  bssl::der::Input algorithm;
  bssl::der::Input parameters;

  if (!bssl::ParseAlgorithmIdentifier(signature_algorithm_tlv_, &algorithm,
                                      &parameters)) {
    return l10n_util::GetStringUTF8(IDS_IOS_CERT_DETAILS_PARAMETERS_NONE);
  }

  // If parameters are empty, return the localized "none" string.
  if (parameters.size() == 0) {
    return l10n_util::GetStringUTF8(IDS_IOS_CERT_DETAILS_PARAMETERS_NONE);
  }

  // Check if it's ASN.1 NULL (tag 0x05, length 0x00) - also treated as no
  // parameters.
  auto params_span = base::as_byte_span(base::as_string_view(parameters));
  if (parameters.size() == 2 && params_span[0] == 0x05 &&
      params_span[1] == 0x00) {
    return l10n_util::GetStringUTF8(IDS_IOS_CERT_DETAILS_PARAMETERS_NONE);
  }

  // For other parameters, show hex representation
  return base::ToUpperASCII(FormatHexWithSeparator(params_span, ' '));
}

std::string X509CertificateModel::GetSignatureData() const {
  CHECK(is_valid());
  auto signature_bytes = signature_value_.bytes();
  if (signature_bytes.empty()) {
    return std::string();
  }
  return base::ToUpperASCII(FormatHexWithSeparator(signature_bytes, ' '));
}

bool X509CertificateModel::HasAuthorityKeyIdentifier() const {
  CHECK(is_valid());
  return extensions_.find(bssl::der::Input(bssl::kAuthorityKeyIdentifierOid)) !=
         extensions_.end();
}

bool X509CertificateModel::IsAuthorityKeyIdentifierCritical() const {
  CHECK(is_valid());
  auto it =
      extensions_.find(bssl::der::Input(bssl::kAuthorityKeyIdentifierOid));
  if (it == extensions_.end()) {
    return false;
  }
  return it->second.critical;
}

std::string X509CertificateModel::GetAuthorityKeyIdentifier() const {
  CHECK(is_valid());
  auto it =
      extensions_.find(bssl::der::Input(bssl::kAuthorityKeyIdentifierOid));
  if (it == extensions_.end()) {
    return std::string();
  }

  bssl::ParsedAuthorityKeyIdentifier authority_key_id;
  if (!bssl::ParseAuthorityKeyIdentifier(it->second.value, &authority_key_id)) {
    return std::string();
  }

  if (!authority_key_id.key_identifier.has_value()) {
    return std::string();
  }

  return base::ToUpperASCII(FormatHexWithSeparator(
      base::as_byte_span(
          base::as_string_view(*authority_key_id.key_identifier)),
      ' '));
}

bool X509CertificateModel::HasAuthorityInformationAccess() const {
  CHECK(is_valid());
  return extensions_.find(bssl::der::Input(bssl::kAuthorityInfoAccessOid)) !=
         extensions_.end();
}

bool X509CertificateModel::IsAuthorityInformationAccessCritical() const {
  CHECK(is_valid());
  auto it = extensions_.find(bssl::der::Input(bssl::kAuthorityInfoAccessOid));
  if (it == extensions_.end()) {
    return false;
  }
  return it->second.critical;
}

std::vector<X509CertificateModel::AccessDescription>
X509CertificateModel::GetAuthorityInformationAccess() const {
  CHECK(is_valid());
  std::vector<AccessDescription> result;
  auto it = extensions_.find(bssl::der::Input(bssl::kAuthorityInfoAccessOid));
  if (it == extensions_.end()) {
    return result;
  }

  std::vector<bssl::AuthorityInfoAccessDescription> access_descriptions;
  if (!bssl::ParseAuthorityInfoAccess(it->second.value, &access_descriptions)) {
    return result;
  }

  for (const auto& desc : access_descriptions) {
    AccessDescription access_desc;

    // Determine the access method using GetOidText
    std::optional<std::string> oid_text = GetOidText(desc.access_method_oid);
    if (oid_text) {
      access_desc.method = *oid_text;
    } else {
      // Unknown method - show OID
      access_desc.method = std::string(
          reinterpret_cast<const char*>(desc.access_method_oid.data()),
          desc.access_method_oid.size());
    }

    // Extract the URI from accessLocation (should be a
    // uniformResourceIdentifier).
    // accessLocation is a GeneralName, which for URI is [6] IA5String.
    bssl::der::Parser parser(desc.access_location);
    bssl::der::Input uri;

    // Tag [6] for uniformResourceIdentifier (context-specific, primitive, tag
    // 6)
    if (parser.ReadTag(CBS_ASN1_CONTEXT_SPECIFIC | 6, &uri)) {
      access_desc.location =
          std::string(reinterpret_cast<const char*>(uri.data()), uri.size());
    } else {
      // If it's not a URI, show the hex representation
      access_desc.location = base::ToUpperASCII(FormatHexWithSeparator(
          base::as_byte_span(base::as_string_view(desc.access_location)), ' '));
    }

    result.push_back(std::move(access_desc));
  }

  return result;
}

bool X509CertificateModel::HasCRLDistributionPoints() const {
  CHECK(is_valid());
  auto it = extensions_.find(bssl::der::Input(bssl::kCrlDistributionPointsOid));
  return it != extensions_.end();
}

bool X509CertificateModel::IsCRLDistributionPointsCritical() const {
  if (!HasCRLDistributionPoints()) {
    return false;
  }

  auto it = extensions_.find(bssl::der::Input(bssl::kCrlDistributionPointsOid));
  return it->second.critical;
}

std::vector<std::string> X509CertificateModel::GetCRLDistributionPoints()
    const {
  std::vector<std::string> result;

  if (!HasCRLDistributionPoints()) {
    return result;
  }

  auto it = extensions_.find(bssl::der::Input(bssl::kCrlDistributionPointsOid));

  // Parse the CRL Distribution Points extension
  std::vector<bssl::ParsedDistributionPoint> distribution_points;
  if (!bssl::ParseCrlDistributionPoints(it->second.value,
                                        &distribution_points)) {
    return result;
  }

  // Extract URIs from each distribution point
  for (const auto& dp : distribution_points) {
    if (!dp.distribution_point_fullname) {
      continue;
    }

    // Extract URIs from the GeneralNames
    const auto& general_names = dp.distribution_point_fullname;
    for (const auto& uri : general_names->uniform_resource_identifiers) {
      result.push_back(std::string(uri));
    }
  }

  return result;
}

bool X509CertificateModel::HasCertificatePolicies() const {
  CHECK(is_valid());
  auto it = extensions_.find(bssl::der::Input(bssl::kCertificatePoliciesOid));
  return it != extensions_.end();
}

bool X509CertificateModel::IsCertificatePoliciesCritical() const {
  if (!HasCertificatePolicies()) {
    return false;
  }

  auto it = extensions_.find(bssl::der::Input(bssl::kCertificatePoliciesOid));
  return it->second.critical;
}

std::vector<X509CertificateModel::CertificatePolicy>
X509CertificateModel::GetCertificatePolicies() const {
  std::vector<CertificatePolicy> result;

  if (!HasCertificatePolicies()) {
    return result;
  }

  auto it = extensions_.find(bssl::der::Input(bssl::kCertificatePoliciesOid));

  // Parse the Certificate Policies extension
  std::vector<bssl::PolicyInformation> policies;
  bssl::CertErrors errors;
  if (!bssl::ParseCertificatePoliciesExtension(it->second.value, &policies,
                                               &errors)) {
    return result;
  }

  // Extract policy information
  for (const auto& policy_info : policies) {
    CertificatePolicy policy;
    policy.identifier = OidToString(policy_info.policy_oid);

    // Process policy qualifiers
    for (const auto& qualifier_info : policy_info.policy_qualifiers) {
      PolicyQualifier qualifier;
      qualifier.oid = OidToString(qualifier_info.qualifier_oid);

      bssl::der::Input qualifier_oid = qualifier_info.qualifier_oid;
      auto qualifier_oid_span =
          base::as_byte_span(base::as_string_view(qualifier_oid));
      auto cps_pointer_span = base::span(bssl::kCpsPointerId);
      auto user_notice_span = base::span(bssl::kUserNoticeId);

      std::optional<std::string> oid_text = GetOidText(qualifier_oid);
      if (oid_text) {
        qualifier.type_name = *oid_text;
        if (qualifier_oid_span == cps_pointer_span) {
          qualifier.description =
              l10n_util::GetStringUTF8(IDS_CERT_PKIX_CPS_POINTER_QUALIFIER);
          std::optional<std::string> processed =
              ProcessIA5String(qualifier_info.qualifier);
          if (processed) {
            qualifier.value = *processed;
          }
        } else if (qualifier_oid_span == user_notice_span) {
          qualifier.description =
              l10n_util::GetStringUTF8(IDS_CERT_PKIX_USER_NOTICE_QUALIFIER);
          std::optional<std::string> processed =
              ProcessUserNotice(qualifier_info.qualifier);
          if (processed) {
            qualifier.value = *processed;
          } else {
            qualifier.value = base::ToUpperASCII(FormatHexWithSeparator(
                base::as_byte_span(
                    base::as_string_view(qualifier_info.qualifier)),
                ' '));
          }
        }
      } else {
        qualifier.type_name =
            l10n_util::GetStringUTF8(IDS_IOS_CERT_POLICY_QUALIFIER_UNKNOWN);
        qualifier.description =
            l10n_util::GetStringUTF8(IDS_IOS_CERT_POLICY_QUALIFIER_UNKNOWN);
        qualifier.value = base::ToUpperASCII(FormatHexWithSeparator(
            base::as_byte_span(base::as_string_view(qualifier_info.qualifier)),
            ' '));
      }

      policy.qualifiers.push_back(std::move(qualifier));
    }

    result.push_back(std::move(policy));
  }

  return result;
}

bool X509CertificateModel::HasSubjectAlternativeNames() const {
  CHECK(is_valid());
  auto it = extensions_.find(bssl::der::Input(bssl::kSubjectAltNameOid));
  return it != extensions_.end();
}

bool X509CertificateModel::IsSubjectAlternativeNamesCritical() const {
  if (!HasSubjectAlternativeNames()) {
    return false;
  }

  auto it = extensions_.find(bssl::der::Input(bssl::kSubjectAltNameOid));
  return it->second.critical;
}

std::vector<std::string> X509CertificateModel::GetSubjectAlternativeNamesDNS()
    const {
  std::vector<std::string> result;

  if (!subject_alt_names_) {
    return result;
  }

  // Extract DNS names
  for (const auto& dns_name : subject_alt_names_->dns_names) {
    result.push_back(std::string(dns_name));
  }

  return result;
}

bool X509CertificateModel::HasSCTList() const {
  CHECK(is_valid());

  auto it = extensions_.find(bssl::der::Input(net::ct::kEmbeddedSCTOid));
  return it != extensions_.end();
}

bool X509CertificateModel::IsSCTListCritical() const {
  if (!HasSCTList()) {
    return false;
  }

  auto it = extensions_.find(bssl::der::Input(net::ct::kEmbeddedSCTOid));
  return it->second.critical;
}

std::string X509CertificateModel::GetSCTListData() const {
  if (!HasSCTList()) {
    return std::string();
  }

  auto it = extensions_.find(bssl::der::Input(net::ct::kEmbeddedSCTOid));

  // Return the extension value as hex-encoded string
  return base::ToUpperASCII(FormatHexWithSeparator(
      base::as_byte_span(base::as_string_view(it->second.value)), ' '));
}

std::vector<X509CertificateModel::UnknownExtension>
X509CertificateModel::GetUnknownExtensions() const {
  CHECK(is_valid());
  std::vector<UnknownExtension> unknown_extensions;

  for (const auto& [oid_input, extension] : extensions_) {
    // Skip known extensions by comparing bssl::der::Input directly
    bool is_known = false;
    for (const auto& known_oid : kKnownExtensionOids) {
      if (oid_input == known_oid) {
        is_known = true;
        break;
      }
    }

    if (is_known) {
      continue;
    }

    UnknownExtension unknown;
    unknown.oid = l10n_util::GetStringUTF8(IDS_IOS_CERT_UNKNOWN_OID_PREFIX) +
                  OidToString(oid_input);
    unknown.critical = extension.critical;
    unknown.raw_data = base::ToUpperASCII(FormatHexWithSeparator(
        base::as_byte_span(base::as_string_view(extension.value)), ' '));

    unknown_extensions.push_back(std::move(unknown));
  }

  return unknown_extensions;
}

bool X509CertificateModel::ParseExtensions(
    const bssl::der::Input& extensions_tlv) {
  return bssl::ParseExtensions(extensions_tlv, &extensions_);
}

}  // namespace x509_certificate_model
