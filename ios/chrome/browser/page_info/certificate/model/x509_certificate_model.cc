// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/page_info/certificate/model/x509_certificate_model.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/span.h"
#include "components/certificate_model/x509_certificate_constants.h"
#include "crypto/sha2.h"
#include "ios/chrome/grit/ios_strings.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/pki/input.h"
#include "third_party/boringssl/src/pki/parse_name.h"
#include "third_party/boringssl/src/pki/parser.h"
#include "third_party/boringssl/src/pki/signature_algorithm.h"
#include "ui/base/l10n/l10n_util.h"

namespace x509_certificate_model {

namespace {
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

std::string ProcessRawBytes(base::span<const uint8_t> data) {
  return ProcessRawBytesWithSeparators(data, ' ', ' ');
}

std::optional<std::string> GetOidText(bssl::der::Input oid) {
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

}  // namespace x509_certificate_model
