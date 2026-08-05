// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/page_info/certificate/model/x509_certificate_model.h"

#include <array>
#include <memory>
#include <string_view>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/certificate_model/x509_certificate_constants.h"
#include "components/strings/grit/components_strings.h"
#include "crypto/sha2.h"
#include "ios/chrome/grit/ios_strings.h"
#include "net/base/ip_address.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/certificate_policies.h"
#include "third_party/boringssl/src/pki/extended_key_usage.h"
#include "third_party/boringssl/src/pki/general_names.h"
#include "third_party/boringssl/src/pki/input.h"
#include "third_party/boringssl/src/pki/parse_certificate.h"
#include "third_party/boringssl/src/pki/parse_name.h"
#include "third_party/boringssl/src/pki/parse_values.h"
#include "third_party/boringssl/src/pki/parser.h"
#include "third_party/boringssl/src/pki/signature_algorithm.h"
#include "ui/base/l10n/l10n_util.h"

namespace x509_certificate_model {

namespace {
// Converts an OID (DER-encoded) to dotted decimal notation (e.g., "2.5.29.32")
// Unlike the `OidToNumericString` method in the base class, due to specific
// requirements of the iOS platform, there is no need to display the "OID."
// prefix.
std::string OidToString(bssl::der::Input oid) {
  CBS cbs;
  CBS_init(&cbs, oid.data(), oid.size());
  bssl::UniquePtr<char> text(CBS_asn1_oid_to_text(&cbs));
  if (!text) {
    return std::string();
  }
  return std::string(text.get());
}

std::string ProcessRawBytes(base::span<const uint8_t> data) {
  return ProcessRawBytesWithSeparators(data, ' ', ' ');
}

constexpr auto kOidStringMap = base::MakeFixedFlatMap<bssl::der::Input, int>({
    // Extended Key Usage field OIDs:
    {bssl::der::Input(bssl::kDocumentSigning),
     IDS_IOS_CERT_EKU_DOCUMENT_SIGNING},
    {bssl::der::Input(bssl::kRcsMlsClient), IDS_IOS_CERT_EKU_RCS_MLS_CLIENT},
    // Authority Information Access method OIDs:
    {bssl::der::Input(bssl::kAdCaIssuersOid),
     IDS_IOS_CERT_AIA_ACCESS_METHOD_CA_ISSUERS},
    {bssl::der::Input(bssl::kAdOcspOid), IDS_IOS_CERT_AIA_ACCESS_METHOD_OCSP},
});

std::optional<std::string> GetOidText(bssl::der::Input oid) {
  const auto i = kOidStringMap.find(oid);
  if (i != kOidStringMap.end()) {
    return l10n_util::GetStringUTF8(i->second);
  }

  // Fall through to common OIDs shared across platforms.
  std::optional<int> common_id = GetCommonOidStringId(oid);
  if (common_id.has_value()) {
    return l10n_util::GetStringUTF8(*common_id);
  }
  return std::nullopt;
}

// Like GetOidText(), but falls back to the OID's dotted decimal notation
// (e.g. "2.5.29.32") when no friendly name is known.
std::string GetOidTextOrOid(bssl::der::Input oid) {
  std::optional<std::string> text = GetOidText(oid);
  return text.has_value() ? *std::move(text) : OidToString(oid);
}

// Looks up an extension by OID in the base class's `extensions_` vector.
// Returns nullptr if not found.
const bssl::ParsedExtension* FindExtension(
    const std::vector<bssl::ParsedExtension>& extensions,
    bssl::der::Input oid) {
  for (const bssl::ParsedExtension& extension : extensions) {
    if (extension.oid == oid) {
      return &extension;
    }
  }
  return nullptr;
}

// Returns true if the extension identified by `oid` is present and marked
// critical.
bool IsExtensionCritical(const std::vector<bssl::ParsedExtension>& extensions,
                         bssl::der::Input oid) {
  const bssl::ParsedExtension* extension = FindExtension(extensions, oid);
  return extension && extension->critical;
}

// Finds and parses the BasicConstraints extension. Returns nullopt if the
// extension is absent or could not be parsed.
std::optional<bssl::ParsedBasicConstraints> ParseBasicConstraintsExtension(
    const std::vector<bssl::ParsedExtension>& extensions) {
  const bssl::ParsedExtension* extension =
      FindExtension(extensions, bssl::der::Input(bssl::kBasicConstraintsOid));
  if (!extension) {
    return std::nullopt;
  }
  bssl::ParsedBasicConstraints basic_constraints;
  if (!bssl::ParseBasicConstraints(extension->value, &basic_constraints)) {
    return std::nullopt;
  }
  return basic_constraints;
}

// Finds and parses the AuthorityKeyIdentifier extension. Returns nullopt if the
// extension is absent or could not be parsed.
std::optional<bssl::ParsedAuthorityKeyIdentifier>
ParseAuthorityKeyIdentifierExtension(
    const std::vector<bssl::ParsedExtension>& extensions) {
  const bssl::ParsedExtension* extension = FindExtension(
      extensions, bssl::der::Input(bssl::kAuthorityKeyIdentifierOid));
  if (!extension) {
    return std::nullopt;
  }
  bssl::ParsedAuthorityKeyIdentifier authority_key_id;
  if (!bssl::ParseAuthorityKeyIdentifier(extension->value, &authority_key_id)) {
    return std::nullopt;
  }
  return authority_key_id;
}

// Parses `spki_tlv` as a SubjectPublicKeyInfo, writing its AlgorithmIdentifier
// TLV to `algorithm_tlv`. If `subject_public_key` is non-null, the
// subjectPublicKey BIT STRING (with its unused-bits byte stripped) is also
// written to it. Returns false if parsing fails.
bool ParseSubjectPublicKeyInfo(
    bssl::der::Input spki_tlv,
    bssl::der::Input* algorithm_tlv,
    bssl::der::BitString* subject_public_key = nullptr) {
  bssl::der::Parser parser(spki_tlv);

  // SubjectPublicKeyInfo  ::=  SEQUENCE  {
  //      algorithm            AlgorithmIdentifier,
  //      subjectPublicKey     BIT STRING  }
  bssl::der::Parser spki_parser;
  if (!parser.ReadSequence(&spki_parser)) {
    return false;
  }
  if (!spki_parser.ReadRawTLV(algorithm_tlv)) {
    return false;
  }
  std::optional<bssl::der::BitString> bit_string = spki_parser.ReadBitString();
  if (!bit_string.has_value()) {
    return false;
  }
  if (subject_public_key) {
    *subject_public_key = *bit_string;
  }
  return true;
}

// Renders an AlgorithmIdentifier `parameters` field for display. An absent
// parameters field or an explicit ASN.1 NULL both render as "none". Any other
// value renders as the raw TLV bytes in hex.
std::string FormatAlgorithmParameters(bssl::der::Input parameters) {
  if (parameters.size() == 0) {
    return l10n_util::GetStringUTF8(IDS_IOS_CERT_DETAILS_PARAMETERS_NONE);
  }
  bssl::der::Parser parser(parameters);
  std::optional<bssl::der::Input> null_tag;
  if (parser.ReadOptionalTag(CBS_ASN1_NULL, &null_tag) && null_tag &&
      null_tag->size() == 0 && !parser.HasMore()) {
    return l10n_util::GetStringUTF8(IDS_IOS_CERT_DETAILS_PARAMETERS_NONE);
  }
  return ProcessRawBytes(parameters);
}

// Builds the ordered, presentation-ready attribute list for a DN. Preserves
// the original DER ordering so the UI can render attributes in the same order
// they appear in the certificate data.
std::vector<X509CertificateModel::RDNAttribute> ToOrderedAttributeList(
    const bssl::RDNSequence& rdns) {
  std::vector<X509CertificateModel::RDNAttribute> entries;
  for (const auto& rdn : rdns) {
    for (const bssl::X509NameAttribute& attr : rdn) {
      X509CertificateModel::RDNAttribute entry;
      entry.oid = OidToString(attr.type);
      entry.label = GetOidTextOrOid(attr.type);
      std::string value;
      if (attr.ValueAsStringWithUnsafeOptions(kNameStringHandling, &value)) {
        entry.value = std::move(value);
      } else {
        // Fallback to hex of the raw value bytes.
        entry.value = ProcessRawBytes(attr.value);
      }
      entries.push_back(std::move(entry));
    }
  }
  return entries;
}

// Decodes the value of an otherName: `wrapped_value` is the content of the
// `[0] EXPLICIT` wrapper, itself a single string TLV. Returns the decoded text
// for any recognized string type, or a hex dump otherwise.
std::string DecodeOtherNameValue(bssl::der::Input wrapped_value) {
  bssl::der::Parser parser(wrapped_value);
  CBS_ASN1_TAG tag;
  bssl::der::Input inner_value;
  if (!parser.ReadTagAndValue(&tag, &inner_value) || parser.HasMore()) {
    return ProcessRawBytes(wrapped_value);
  }
  // Reuse the DN attribute string decoder, which handles every string tag. The
  // `type` field is irrelevant here, so leave it empty.
  std::string text;
  if (bssl::X509NameAttribute(bssl::der::Input(), tag, inner_value)
          .ValueAsStringWithUnsafeOptions(kNameStringHandling, &text)) {
    return text;
  }
  return ProcessRawBytes(wrapped_value);
}

// Converts a parsed bssl::GeneralNames into presentation-ready GeneralName
// entries (the structured analog of desktop's ProcessGeneralNames). Names are
// grouped by type, matching the order the buckets appear in bssl::GeneralNames;
// the original cross-type DER ordering is not recoverable.
std::vector<X509CertificateModel::GeneralName> ToGeneralNameList(
    const bssl::GeneralNames& names) {
  using GeneralName = X509CertificateModel::GeneralName;
  std::vector<GeneralName> result;
  for (const bssl::der::Input& other_name : names.other_names) {
    GeneralName entry;
    entry.type = GeneralName::Type::kOtherName;
    bssl::der::Input type_id;
    bssl::der::Input wrapped_value;
    if (ParseOtherName(other_name, &type_id, &wrapped_value)) {
      entry.other_name_oid = GetOidTextOrOid(type_id);
      entry.value = DecodeOtherNameValue(wrapped_value);
    } else {
      // Fallback to the whole-bytes hex dump.
      entry.value = ProcessRawBytes(other_name);
    }
    result.push_back(std::move(entry));
  }
  for (std::string_view rfc822_name : names.rfc822_names) {
    result.push_back({.type = GeneralName::Type::kRFC822Name,
                      .value = std::string(rfc822_name)});
  }
  for (std::string_view dns_name : names.dns_names) {
    result.push_back(
        {.type = GeneralName::Type::kDNSName, .value = std::string(dns_name)});
  }
  for (const bssl::der::Input& x400_address : names.x400_addresses) {
    result.push_back({.type = GeneralName::Type::kX400Address,
                      .value = ProcessRawBytes(x400_address)});
  }
  for (const bssl::der::Input& directory_name : names.directory_names) {
    GeneralName entry;
    entry.type = GeneralName::Type::kDirectoryName;
    bssl::RDNSequence rdns;
    if (bssl::ParseNameValue(directory_name, &rdns)) {
      entry.directory_name = ToOrderedAttributeList(rdns);
    } else {
      entry.value = ProcessRawBytes(directory_name);
    }
    result.push_back(std::move(entry));
  }
  for (const bssl::der::Input& edi_party_name : names.edi_party_names) {
    result.push_back({.type = GeneralName::Type::kEDIPartyName,
                      .value = ProcessRawBytes(edi_party_name)});
  }
  for (std::string_view uri : names.uniform_resource_identifiers) {
    result.push_back(
        {.type = GeneralName::Type::kURI, .value = std::string(uri)});
  }
  for (const bssl::der::Input& ip_address : names.ip_addresses) {
    result.push_back({.type = GeneralName::Type::kIPAddress,
                      .value = net::IPAddress(ip_address).ToString()});
  }
  for (const bssl::der::Input& registered_id : names.registered_ids) {
    result.push_back({.type = GeneralName::Type::kRegisteredID,
                      .value = OidToString(registered_id)});
  }
  return result;
}

std::optional<std::vector<X509CertificateModel::GeneralName>>
ProcessGeneralNamesValue(bssl::der::Input general_names_value) {
  bssl::CertErrors unused_errors;
  std::unique_ptr<bssl::GeneralNames> general_names =
      bssl::GeneralNames::CreateFromValue(general_names_value, &unused_errors);
  if (!general_names) {
    return std::nullopt;
  }
  return ToGeneralNameList(*general_names);
}

std::optional<std::vector<X509CertificateModel::GeneralName>>
ProcessGeneralNamesTlv(bssl::der::Input extension_data) {
  bssl::CertErrors unused_errors;
  std::unique_ptr<bssl::GeneralNames> general_names =
      bssl::GeneralNames::Create(extension_data, &unused_errors);
  if (!general_names) {
    return std::nullopt;
  }
  return ToGeneralNameList(*general_names);
}

std::optional<X509CertificateModel::UserNotice> ProcessUserNotice(
    bssl::der::Input qualifier) {
  // RFC 5280 section 4.2.1.4:
  //
  //    UserNotice ::= SEQUENCE {
  //         noticeRef        NoticeReference OPTIONAL,
  //         explicitText     DisplayText OPTIONAL }
  //
  //    NoticeReference ::= SEQUENCE {
  //         organization     DisplayText,
  //         noticeNumbers    SEQUENCE OF INTEGER }
  //
  //    DisplayText ::= CHOICE {
  //         ia5String        IA5String      (SIZE (1..200)),
  //         visibleString    VisibleString  (SIZE (1..200)),
  //         bmpString        BMPString      (SIZE (1..200)),
  //         utf8String       UTF8String     (SIZE (1..200)) }
  bssl::der::Parser outer_parser(qualifier);
  bssl::der::Parser parser;
  if (!outer_parser.ReadSequence(&parser) || outer_parser.HasMore()) {
    return std::nullopt;
  }

  std::optional<bssl::der::Input> notice_ref_value;
  if (!parser.ReadOptionalTag(CBS_ASN1_SEQUENCE, &notice_ref_value)) {
    return std::nullopt;
  }

  X509CertificateModel::UserNotice notice;
  if (notice_ref_value) {
    bssl::der::Parser notice_ref_parser(*notice_ref_value);
    CBS_ASN1_TAG organization_tag;
    bssl::der::Input organization_value;
    if (!notice_ref_parser.ReadTagAndValue(&organization_tag,
                                           &organization_value)) {
      return std::nullopt;
    }
    std::optional<std::string> organization =
        ProcessUserNoticeDisplayText(organization_tag, organization_value);
    if (!organization) {
      return std::nullopt;
    }
    notice.organization = *std::move(organization);

    bssl::der::Parser notice_numbers_parser;
    if (!notice_ref_parser.ReadSequence(&notice_numbers_parser)) {
      return std::nullopt;
    }
    while (notice_numbers_parser.HasMore()) {
      bssl::der::Input notice_number;
      if (!notice_numbers_parser.ReadTag(CBS_ASN1_INTEGER, &notice_number)) {
        return std::nullopt;
      }
      uint64_t number;
      if (bssl::der::ParseUint64(notice_number, &number)) {
        notice.notice_numbers.push_back(base::NumberToString(number));
      } else {
        // The integer does not fit in a uint64, so surface it as hex.
        notice.notice_numbers.push_back(ProcessRawBytes(notice_number));
      }
    }
  }

  if (parser.HasMore()) {
    CBS_ASN1_TAG explicit_text_tag;
    bssl::der::Input explicit_text_value;
    if (!parser.ReadTagAndValue(&explicit_text_tag, &explicit_text_value)) {
      return std::nullopt;
    }
    std::optional<std::string> explicit_text =
        ProcessUserNoticeDisplayText(explicit_text_tag, explicit_text_value);
    if (!explicit_text) {
      return std::nullopt;
    }
    notice.explicit_text = *std::move(explicit_text);
  }

  if (parser.HasMore()) {
    return std::nullopt;
  }
  return notice;
}

// Returns false when a known qualifier fails to decode. In that case, its type
// is preserved in `qualifier`, but its decoded content is left empty.
bool ProcessPolicyQualifier(const bssl::PolicyQualifierInfo& qualifier_info,
                            X509CertificateModel::PolicyQualifier* qualifier) {
  if (qualifier_info.qualifier_oid == bssl::der::Input(bssl::kCpsPointerId)) {
    qualifier->type = X509CertificateModel::PolicyQualifier::Type::kCpsUri;
    std::optional<std::string> cps_uri =
        ProcessIA5String(qualifier_info.qualifier);
    if (!cps_uri) {
      return false;
    }
    qualifier->cps_uri = *std::move(cps_uri);
    return true;
  }

  if (qualifier_info.qualifier_oid == bssl::der::Input(bssl::kUserNoticeId)) {
    qualifier->type = X509CertificateModel::PolicyQualifier::Type::kUserNotice;
    std::optional<X509CertificateModel::UserNotice> user_notice =
        ProcessUserNotice(qualifier_info.qualifier);
    if (!user_notice) {
      return false;
    }
    qualifier->user_notice = *std::move(user_notice);
    return true;
  }

  qualifier->type = X509CertificateModel::PolicyQualifier::Type::kUnknown;
  qualifier->raw_value = ProcessRawBytes(qualifier_info.qualifier);
  return true;
}

}  // namespace

// X509CertificateModel implementation
X509CertificateModel::X509CertificateModel(
    bssl::UniquePtr<CRYPTO_BUFFER> cert_data)
    : X509CertificateModelBase(std::move(cert_data)) {}

X509CertificateModel::X509CertificateModel(const net::X509Certificate* cert)
    : X509CertificateModel(bssl::UpRef(CHECK_DEREF(cert).cert_buffer())) {}

X509CertificateModel::X509CertificateModel(X509CertificateModel&& other) =
    default;

X509CertificateModel::~X509CertificateModel() = default;

std::string X509CertificateModel::GetExtensionRawHex(
    bssl::der::Input oid) const {
  CHECK(is_valid());
  const bssl::ParsedExtension* extension = FindExtension(extensions_, oid);
  if (!extension) {
    return std::string();
  }
  return ProcessRawBytes(extension->value);
}

std::string X509CertificateModel::HashCertSHA256() const {
  auto hash =
      crypto::SHA256Hash(net::x509_util::CryptoBufferAsSpan(cert_buffer()));
  return ProcessRawBytes(hash);
}

std::string X509CertificateModel::HashSpkiSHA256() const {
  CHECK(is_valid());
  return ProcessRawBytes(crypto::SHA256Hash(tbs_.spki_tlv));
}

std::string X509CertificateModel::GetSerialNumberHexified() const {
  CHECK(is_valid());
  return ProcessRawBytes(tbs_.serial_number);
}

std::vector<X509CertificateModel::RDNAttribute>
X509CertificateModel::GetSubjectAttributesInOrder() const {
  CHECK(is_valid());
  return ToOrderedAttributeList(subject_rdns_);
}

std::vector<X509CertificateModel::RDNAttribute>
X509CertificateModel::GetIssuerAttributesInOrder() const {
  CHECK(is_valid());
  return ToOrderedAttributeList(issuer_rdns_);
}

std::string X509CertificateModel::GetSignatureAlgorithm() const {
  CHECK(is_valid());
  bssl::der::Input algorithm;
  bssl::der::Input parameters;
  if (!bssl::ParseAlgorithmIdentifier(signature_algorithm_tlv_, &algorithm,
                                      &parameters)) {
    return std::string();
  }
  return GetOidTextOrOid(algorithm);
}

std::string X509CertificateModel::GetSignatureParameters() const {
  CHECK(is_valid());
  bssl::der::Input algorithm;
  bssl::der::Input parameters;
  if (!bssl::ParseAlgorithmIdentifier(signature_algorithm_tlv_, &algorithm,
                                      &parameters)) {
    return std::string();
  }
  return FormatAlgorithmParameters(parameters);
}

std::string X509CertificateModel::GetSignatureData() const {
  CHECK(is_valid());
  return ProcessRawBytes(signature_value_.bytes());
}

std::string X509CertificateModel::GetPublicKeyAlgorithm() const {
  CHECK(is_valid());
  bssl::der::Input algorithm_tlv;
  if (!ParseSubjectPublicKeyInfo(tbs_.spki_tlv, &algorithm_tlv)) {
    return std::string();
  }
  bssl::der::Input algorithm;
  bssl::der::Input parameters;
  if (!bssl::ParseAlgorithmIdentifier(algorithm_tlv, &algorithm, &parameters)) {
    return std::string();
  }
  return GetOidTextOrOid(algorithm);
}

std::string X509CertificateModel::GetPublicKeyParameters() const {
  CHECK(is_valid());
  bssl::der::Input algorithm_tlv;
  if (!ParseSubjectPublicKeyInfo(tbs_.spki_tlv, &algorithm_tlv)) {
    return std::string();
  }
  bssl::der::Input algorithm;
  bssl::der::Input parameters;
  if (!bssl::ParseAlgorithmIdentifier(algorithm_tlv, &algorithm, &parameters)) {
    return std::string();
  }
  return FormatAlgorithmParameters(parameters);
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

std::string X509CertificateModel::GetPublicKeyData() const {
  CHECK(is_valid());
  bssl::der::Input algorithm_tlv;
  bssl::der::BitString subject_public_key;
  if (!ParseSubjectPublicKeyInfo(tbs_.spki_tlv, &algorithm_tlv,
                                 &subject_public_key)) {
    return std::string();
  }
  return ProcessRawBytes(subject_public_key.bytes());
}

std::vector<bssl::der::Input> X509CertificateModel::GetExtensionOidsInOrder()
    const {
  CHECK(is_valid());
  std::vector<bssl::der::Input> oids;
  oids.reserve(extensions_.size());
  for (const bssl::ParsedExtension& extension : extensions_) {
    oids.push_back(extension.oid);
  }
  return oids;
}

bool X509CertificateModel::IsKeyUsageCritical() const {
  CHECK(is_valid());
  return IsExtensionCritical(extensions_, bssl::der::Input(bssl::kKeyUsageOid));
}

std::optional<std::string> X509CertificateModel::GetKeyUsageString() const {
  CHECK(is_valid());
  const bssl::ParsedExtension* key_usage_extension =
      FindExtension(extensions_, bssl::der::Input(bssl::kKeyUsageOid));
  if (!key_usage_extension) {
    return std::nullopt;
  }

  bssl::der::BitString key_usage;
  if (!bssl::ParseKeyUsage(key_usage_extension->value, &key_usage)) {
    return std::nullopt;
  }

  // The string IDs are indexed by the KeyUsage bit position, matching
  // bssl::KeyUsageBit:
  //   KEY_USAGE_BIT_DIGITAL_SIGNATURE = 0,
  //   KEY_USAGE_BIT_NON_REPUDIATION   = 1,
  //   KEY_USAGE_BIT_KEY_ENCIPHERMENT  = 2,
  //   KEY_USAGE_BIT_DATA_ENCIPHERMENT = 3,
  //   KEY_USAGE_BIT_KEY_AGREEMENT     = 4,
  //   KEY_USAGE_BIT_KEY_CERT_SIGN     = 5,
  //   KEY_USAGE_BIT_CRL_SIGN          = 6,
  //   KEY_USAGE_BIT_ENCIPHER_ONLY     = 7,
  //   KEY_USAGE_BIT_DECIPHER_ONLY     = 8,
  static constexpr auto kUsageStringIds = std::to_array<int>({
      IDS_CERT_X509_KEY_USAGE_SIGNING,
      IDS_CERT_X509_KEY_USAGE_NONREP,
      IDS_CERT_X509_KEY_USAGE_ENCIPHERMENT,
      IDS_CERT_X509_KEY_USAGE_DATA_ENCIPHERMENT,
      IDS_CERT_X509_KEY_USAGE_KEY_AGREEMENT,
      IDS_CERT_X509_KEY_USAGE_CERT_SIGNER,
      IDS_CERT_X509_KEY_USAGE_CRL_SIGNER,
      IDS_CERT_X509_KEY_USAGE_ENCIPHER_ONLY,
      IDS_CERT_X509_KEY_USAGE_DECIPHER_ONLY,
  });

  std::vector<std::string> usages;
  for (size_t bit = 0; bit < kUsageStringIds.size(); ++bit) {
    if (key_usage.AssertsBit(bit)) {
      usages.push_back(l10n_util::GetStringUTF8(kUsageStringIds[bit]));
    }
  }

  return base::JoinString(usages, ", ");
}

std::string X509CertificateModel::GetKeyUsageStringRaw() const {
  return GetExtensionRawHex(bssl::der::Input(bssl::kKeyUsageOid));
}

bool X509CertificateModel::IsExtendedKeyUsageCritical() const {
  CHECK(is_valid());
  return IsExtensionCritical(extensions_,
                             bssl::der::Input(bssl::kExtKeyUsageOid));
}

std::optional<std::vector<std::string>>
X509CertificateModel::GetExtendedKeyUsagePurposes() const {
  CHECK(is_valid());
  const bssl::ParsedExtension* eku_extension =
      FindExtension(extensions_, bssl::der::Input(bssl::kExtKeyUsageOid));
  if (!eku_extension) {
    return std::nullopt;
  }

  std::vector<bssl::der::Input> eku_oids;
  if (!bssl::ParseEKUExtension(eku_extension->value, &eku_oids)) {
    return std::nullopt;
  }

  std::vector<std::string> purposes;
  purposes.reserve(eku_oids.size());
  for (const auto& oid : eku_oids) {
    purposes.push_back(GetOidTextOrOid(oid));
  }
  return purposes;
}

std::string X509CertificateModel::GetExtendedKeyUsagePurposesRaw() const {
  return GetExtensionRawHex(bssl::der::Input(bssl::kExtKeyUsageOid));
}

bool X509CertificateModel::IsBasicConstraintsCritical() const {
  CHECK(is_valid());
  return IsExtensionCritical(extensions_,
                             bssl::der::Input(bssl::kBasicConstraintsOid));
}

bool X509CertificateModel::IsBasicConstraintsCA() const {
  CHECK(is_valid());
  std::optional<bssl::ParsedBasicConstraints> basic_constraints =
      ParseBasicConstraintsExtension(extensions_);
  return basic_constraints && basic_constraints->is_ca;
}

std::optional<uint8_t> X509CertificateModel::GetBasicConstraintsPathLen()
    const {
  CHECK(is_valid());
  std::optional<bssl::ParsedBasicConstraints> basic_constraints =
      ParseBasicConstraintsExtension(extensions_);
  if (!basic_constraints || !basic_constraints->has_path_len) {
    return std::nullopt;
  }
  return basic_constraints->path_len;
}

bool X509CertificateModel::IsSubjectKeyIdentifierCritical() const {
  CHECK(is_valid());
  return IsExtensionCritical(extensions_,
                             bssl::der::Input(bssl::kSubjectKeyIdentifierOid));
}

std::optional<std::string> X509CertificateModel::GetSubjectKeyIdentifier()
    const {
  CHECK(is_valid());
  const bssl::ParsedExtension* extension = FindExtension(
      extensions_, bssl::der::Input(bssl::kSubjectKeyIdentifierOid));
  if (!extension) {
    return std::nullopt;
  }
  bssl::der::Input subject_key_id;
  if (!bssl::ParseSubjectKeyIdentifier(extension->value, &subject_key_id)) {
    return std::nullopt;
  }
  return ProcessRawBytes(subject_key_id);
}

std::string X509CertificateModel::GetSubjectKeyIdentifierRaw() const {
  return GetExtensionRawHex(bssl::der::Input(bssl::kSubjectKeyIdentifierOid));
}

bool X509CertificateModel::IsAuthorityKeyIdentifierCritical() const {
  CHECK(is_valid());
  return IsExtensionCritical(
      extensions_, bssl::der::Input(bssl::kAuthorityKeyIdentifierOid));
}

std::optional<std::string> X509CertificateModel::GetAuthorityKeyIdentifier()
    const {
  CHECK(is_valid());
  std::optional<bssl::ParsedAuthorityKeyIdentifier> authority_key_id =
      ParseAuthorityKeyIdentifierExtension(extensions_);
  if (!authority_key_id) {
    return std::nullopt;
  }
  if (!authority_key_id->key_identifier.has_value()) {
    return std::string();
  }
  return ProcessRawBytes(*authority_key_id->key_identifier);
}

std::optional<std::vector<X509CertificateModel::GeneralName>>
X509CertificateModel::GetAuthorityKeyIdentifierIssuer() const {
  CHECK(is_valid());
  std::optional<bssl::ParsedAuthorityKeyIdentifier> authority_key_id =
      ParseAuthorityKeyIdentifierExtension(extensions_);
  if (!authority_key_id) {
    return std::nullopt;
  }
  if (!authority_key_id->authority_cert_issuer.has_value()) {
    return std::vector<GeneralName>();
  }
  return ProcessGeneralNamesValue(*authority_key_id->authority_cert_issuer);
}

std::optional<std::string>
X509CertificateModel::GetAuthorityKeyIdentifierSerial() const {
  CHECK(is_valid());
  std::optional<bssl::ParsedAuthorityKeyIdentifier> authority_key_id =
      ParseAuthorityKeyIdentifierExtension(extensions_);
  if (!authority_key_id) {
    return std::nullopt;
  }
  if (!authority_key_id->authority_cert_serial_number.has_value()) {
    return std::string();
  }
  return ProcessRawBytes(*authority_key_id->authority_cert_serial_number);
}

std::string X509CertificateModel::GetAuthorityKeyIdentifierRaw() const {
  return GetExtensionRawHex(bssl::der::Input(bssl::kAuthorityKeyIdentifierOid));
}

bool X509CertificateModel::IsAuthorityInformationAccessCritical() const {
  CHECK(is_valid());
  return IsExtensionCritical(extensions_,
                             bssl::der::Input(bssl::kAuthorityInfoAccessOid));
}

std::optional<std::vector<X509CertificateModel::AccessDescription>>
X509CertificateModel::GetAuthorityInformationAccess() const {
  CHECK(is_valid());
  const bssl::ParsedExtension* extension = FindExtension(
      extensions_, bssl::der::Input(bssl::kAuthorityInfoAccessOid));
  if (!extension) {
    return std::nullopt;
  }
  std::vector<bssl::AuthorityInfoAccessDescription> access_descriptions;
  if (!bssl::ParseAuthorityInfoAccess(extension->value, &access_descriptions)) {
    return std::nullopt;
  }

  std::vector<AccessDescription> result;
  result.reserve(access_descriptions.size());
  for (const bssl::AuthorityInfoAccessDescription& desc : access_descriptions) {
    AccessDescription access = {GetOidTextOrOid(desc.access_method_oid),
                                std::nullopt};
    bssl::GeneralNames names;
    bssl::CertErrors errors;
    if (bssl::ParseGeneralName(desc.access_location,
                               bssl::GeneralNames::IP_ADDRESS_ONLY, &names,
                               &errors)) {
      // accessLocation is a single GeneralName. ToGeneralNameList() is the
      // shared converter that returns a vector, but one parsed GeneralName
      // yields exactly one element, so front() is that element.
      std::vector<GeneralName> location = ToGeneralNameList(names);
      if (!location.empty()) {
        access.location = std::move(location.front());
      }
    }
    // A single accessLocation that fails to decode keeps the method and
    // surfaces the raw bytes, rather than dropping the whole description.
    if (!access.location) {
      access.raw_location = ProcessRawBytes(desc.access_location);
    }
    result.push_back(std::move(access));
  }
  return result;
}

std::string X509CertificateModel::GetAuthorityInformationAccessRaw() const {
  return GetExtensionRawHex(bssl::der::Input(bssl::kAuthorityInfoAccessOid));
}

bool X509CertificateModel::IsSubjectAlternativeNameCritical() const {
  CHECK(is_valid());
  return IsExtensionCritical(extensions_,
                             bssl::der::Input(bssl::kSubjectAltNameOid));
}

std::optional<std::vector<X509CertificateModel::GeneralName>>
X509CertificateModel::GetSubjectAlternativeNames() const {
  CHECK(is_valid());
  // The base class already parsed the SubjectAltName extension into
  // `subject_alt_names_`. It is null only when the extension is absent: a
  // SubjectAltName that fails to decode makes the whole certificate invalid, so
  // is_valid() would be false and this getter would not be reached.
  if (!subject_alt_names_) {
    return std::nullopt;
  }
  return ToGeneralNameList(*subject_alt_names_);
}

bool X509CertificateModel::IsIssuerAlternativeNameCritical() const {
  CHECK(is_valid());
  return IsExtensionCritical(extensions_, bssl::der::Input(kIssuerAltNameOid));
}

std::optional<std::vector<X509CertificateModel::GeneralName>>
X509CertificateModel::GetIssuerAlternativeNames() const {
  CHECK(is_valid());
  const bssl::ParsedExtension* extension =
      FindExtension(extensions_, bssl::der::Input(kIssuerAltNameOid));
  if (!extension) {
    return std::nullopt;
  }
  // Unlike SubjectAltName, the base class does not cache IssuerAltName, so
  // parse the extension's GeneralNames value here.
  return ProcessGeneralNamesTlv(extension->value);
}

std::string X509CertificateModel::GetIssuerAlternativeNamesRaw() const {
  return GetExtensionRawHex(bssl::der::Input(kIssuerAltNameOid));
}

bool X509CertificateModel::IsCRLDistributionPointsCritical() const {
  CHECK(is_valid());
  return IsExtensionCritical(extensions_,
                             bssl::der::Input(bssl::kCrlDistributionPointsOid));
}

std::optional<std::vector<X509CertificateModel::GeneralName>>
X509CertificateModel::GetCRLDistributionPointsFullNames() const {
  CHECK(is_valid());
  const bssl::ParsedExtension* extension = FindExtension(
      extensions_, bssl::der::Input(bssl::kCrlDistributionPointsOid));
  if (!extension) {
    return std::nullopt;
  }
  std::vector<bssl::ParsedDistributionPoint> distribution_points;
  if (!bssl::ParseCrlDistributionPoints(extension->value,
                                        &distribution_points)) {
    return std::nullopt;
  }
  // Only the distributionPoint fullName is surfaced. The relativeName,
  // reasons, and cRLIssuer fields are skipped.
  std::vector<GeneralName> result;
  for (const bssl::ParsedDistributionPoint& dp : distribution_points) {
    if (dp.distribution_point_fullname) {
      auto names = ToGeneralNameList(*dp.distribution_point_fullname);
      result.insert(result.end(), std::make_move_iterator(names.begin()),
                    std::make_move_iterator(names.end()));
    }
  }
  return result;
}

std::string X509CertificateModel::GetCRLDistributionPointsFullNamesRaw() const {
  return GetExtensionRawHex(bssl::der::Input(bssl::kCrlDistributionPointsOid));
}

bool X509CertificateModel::IsCertificatePoliciesCritical() const {
  CHECK(is_valid());
  return IsExtensionCritical(extensions_,
                             bssl::der::Input(bssl::kCertificatePoliciesOid));
}

X509CertificateModel::CertificatePoliciesResult
X509CertificateModel::GetCertificatePolicies() const {
  CHECK(is_valid());
  CertificatePoliciesResult result;
  const bssl::ParsedExtension* extension = FindExtension(
      extensions_, bssl::der::Input(bssl::kCertificatePoliciesOid));
  if (!extension) {
    return result;
  }
  result.raw_der = ProcessRawBytes(extension->value);
  std::vector<bssl::PolicyInformation> policies;
  bssl::CertErrors unused_errors;
  if (!bssl::ParseCertificatePoliciesExtension(extension->value, &policies,
                                               &unused_errors)) {
    result.has_error = true;
    return result;
  }
  for (const bssl::PolicyInformation& policy_info : policies) {
    CertificatePolicy policy;
    policy.policy_oid = GetOidTextOrOid(policy_info.policy_oid);
    // Keep a known qualifier that fails to decode, then stop decoding.
    for (const bssl::PolicyQualifierInfo& qualifier_info :
         policy_info.policy_qualifiers) {
      PolicyQualifier qualifier;
      const bool qualifier_decoded =
          ProcessPolicyQualifier(qualifier_info, &qualifier);
      policy.qualifiers.push_back(std::move(qualifier));
      if (!qualifier_decoded) {
        result.has_error = true;
        break;
      }
    }
    result.policies.push_back(std::move(policy));
    if (result.has_error) {
      break;
    }
  }
  return result;
}

}  // namespace x509_certificate_model
