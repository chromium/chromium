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

  // No parameters, or explicit ASN.1 NULL -- both mean "none".
  if (parameters.size() == 0) {
    return l10n_util::GetStringUTF8(IDS_IOS_CERT_DETAILS_PARAMETERS_NONE);
  }
  bssl::der::Parser parser(parameters);
  std::optional<bssl::der::Input> null_value;
  if (parser.ReadOptionalTag(CBS_ASN1_NULL, &null_value) && null_value &&
      null_value->size() == 0 && !parser.HasMore()) {
    return l10n_util::GetStringUTF8(IDS_IOS_CERT_DETAILS_PARAMETERS_NONE);
  }

  return ProcessRawBytes(parameters);
}

std::string X509CertificateModel::GetSignatureData() const {
  CHECK(is_valid());
  return ProcessRawBytes(signature_value_.bytes());
}

}  // namespace x509_certificate_model
