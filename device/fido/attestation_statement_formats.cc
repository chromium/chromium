// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/attestation_statement_formats.h"

#include <utility>

#include "base/logging.h"
#include "device/fido/fido_parsing_utils.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"

namespace device {

namespace {
constexpr char kFidoFormatName[] = "fido-u2f";
constexpr char kPackedAttestationFormat[] = "packed";
constexpr char kAlgorithmKey[] = "alg";
constexpr char kSignatureKey[] = "sig";
constexpr char kX509CertKey[] = "x5c";

bool IsCertificateInappropriatelyIdentifying(
    const std::vector<uint8_t>& der_bytes) {
  constexpr int kVersionTag = 0;
  CBS cert(der_bytes);
  CBS top_level, to_be_signed_cert, issuer;
  if (!CBS_get_asn1(&cert, &top_level, CBS_ASN1_SEQUENCE) ||
      CBS_len(&cert) != 0 ||
      !CBS_get_asn1(&top_level, &to_be_signed_cert, CBS_ASN1_SEQUENCE) ||
      // version, explicitly tagged with tag zero.
      !CBS_get_optional_asn1(
          &to_be_signed_cert, NULL, NULL,
          CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC | kVersionTag) ||
      // serialNumber
      !CBS_get_asn1(&to_be_signed_cert, NULL, CBS_ASN1_INTEGER) ||
      // signature algorithm
      !CBS_get_asn1(&to_be_signed_cert, NULL, CBS_ASN1_SEQUENCE) ||
      !CBS_get_asn1(&to_be_signed_cert, &issuer, CBS_ASN1_SEQUENCE)) {
    return false;
  }

  while (CBS_len(&issuer) != 0) {
    CBS relative_distinguished_names;
    if (!CBS_get_asn1(&issuer, &relative_distinguished_names, CBS_ASN1_SET)) {
      return false;
    }
    while (CBS_len(&relative_distinguished_names) != 0) {
      CBS relative_distinguished_name, object_id;
      if (!CBS_get_asn1(&relative_distinguished_names,
                        &relative_distinguished_name, CBS_ASN1_SEQUENCE) ||
          !CBS_get_asn1(&relative_distinguished_name, &object_id,
                        CBS_ASN1_OBJECT)) {
        return false;
      }

      // Encoding of OID 2.5.4.3 in DER form. See "OBJECT IDENTIFER" in
      // http://luca.ntop.org/Teaching/Appunti/asn1.html
      static constexpr uint8_t kCommonNameOID[] = {40 * 2 + 5, 4, 3};
      if (!CBS_mem_equal(&object_id, kCommonNameOID, sizeof(kCommonNameOID))) {
        continue;
      }

      CBS value;
      unsigned tag;
      if (!CBS_get_any_asn1(&relative_distinguished_name, &value, &tag)) {
        return false;
      }

      static constexpr uint8_t kCommonName[] = "FT FIDO 0100";
      if ((tag == CBS_ASN1_IA5STRING || tag == CBS_ASN1_UTF8STRING ||
           tag == CBS_ASN1_PRINTABLESTRING) &&
          CBS_mem_equal(&value, kCommonName, sizeof(kCommonName) - 1)) {
        return true;
      }
    }
  }

  return false;
}

}  // namespace

// static
std::unique_ptr<FidoAttestationStatement>
FidoAttestationStatement::CreateFromU2fRegisterResponse(
    base::span<const uint8_t> u2f_data) {
  CBS response, cert;
  CBS_init(&response, u2f_data.data(), u2f_data.size());

  // The format of |u2f_data| is specified here:
  // https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-raw-message-formats-v1.2-ps-20170411.html#registration-response-message-success
  uint8_t credential_length;
  if (!CBS_skip(&response, kU2fKeyHandleLengthOffset) ||
      !CBS_get_u8(&response, &credential_length) ||
      !CBS_skip(&response, credential_length) ||
      !CBS_get_asn1_element(&response, &cert, CBS_ASN1_SEQUENCE)) {
    DLOG(ERROR)
        << "Invalid U2F response. Unable to unpack attestation statement.";
    return nullptr;
  }

  std::vector<std::vector<uint8_t>> x509_certificates;
  x509_certificates.emplace_back(CBS_data(&cert),
                                 CBS_data(&cert) + CBS_len(&cert));

  // The remaining bytes are the signature.
  std::vector<uint8_t> signature(CBS_data(&response),
                                 CBS_data(&response) + CBS_len(&response));
  return std::make_unique<FidoAttestationStatement>(
      std::move(signature), std::move(x509_certificates));
}

FidoAttestationStatement::FidoAttestationStatement(
    std::vector<uint8_t> signature,
    std::vector<std::vector<uint8_t>> x509_certificates)
    : AttestationStatement(kFidoFormatName),
      signature_(std::move(signature)),
      x509_certificates_(std::move(x509_certificates)) {}

FidoAttestationStatement::~FidoAttestationStatement() = default;

cbor::Value FidoAttestationStatement::AsCBOR() const {
  cbor::Value::MapValue attestation_statement_map;
  attestation_statement_map[cbor::Value(kSignatureKey)] =
      cbor::Value(signature_);

  std::vector<cbor::Value> certificate_array;
  for (const auto& cert : x509_certificates_) {
    certificate_array.push_back(cbor::Value(cert));
  }

  attestation_statement_map[cbor::Value(kX509CertKey)] =
      cbor::Value(std::move(certificate_array));

  return cbor::Value(std::move(attestation_statement_map));
}

bool FidoAttestationStatement::IsSelfAttestation() {
  return false;
}

bool FidoAttestationStatement::
    IsAttestationCertificateInappropriatelyIdentifying() {
  // An attestation certificate is considered inappropriately identifying if it
  // contains a common name of "FT FIDO 0100". See "Inadequately batched
  // attestation certificates" on https://www.chromium.org/security-keys
  for (const auto& der_bytes : x509_certificates_) {
    if (IsCertificateInappropriatelyIdentifying(der_bytes)) {
      return true;
    }
  }

  return false;
}

base::Optional<base::span<const uint8_t>>
FidoAttestationStatement::GetLeafCertificate() const {
  if (x509_certificates_.empty()) {
    return base::nullopt;
  }
  return x509_certificates_[0];
}

PackedAttestationStatement::PackedAttestationStatement(
    CoseAlgorithmIdentifier algorithm,
    std::vector<uint8_t> signature,
    std::vector<std::vector<uint8_t>> x509_certificates)
    : AttestationStatement(kPackedAttestationFormat),
      algorithm_(algorithm),
      signature_(signature),
      x509_certificates_(std::move(x509_certificates)) {
  DCHECK(!signature_.empty());
}

PackedAttestationStatement::~PackedAttestationStatement() = default;

cbor::Value PackedAttestationStatement::AsCBOR() const {
  cbor::Value::MapValue attestation_statement_map;
  // alg
  attestation_statement_map[cbor::Value(kAlgorithmKey)] =
      cbor::Value(static_cast<int>(algorithm_));
  // sig
  attestation_statement_map[cbor::Value(kSignatureKey)] =
      cbor::Value(signature_);
  // x5c (optional)
  if (!x509_certificates_.empty()) {
    std::vector<cbor::Value> certificate_array;
    for (const auto& cert : x509_certificates_) {
      certificate_array.push_back(cbor::Value(cert));
    }
    attestation_statement_map[cbor::Value(kX509CertKey)] =
        cbor::Value(std::move(certificate_array));
  }
  return cbor::Value(std::move(attestation_statement_map));
}

bool PackedAttestationStatement::IsSelfAttestation() {
  return x509_certificates_.empty();
}

bool PackedAttestationStatement::
    IsAttestationCertificateInappropriatelyIdentifying() {
  for (const auto& der_bytes : x509_certificates_) {
    if (IsCertificateInappropriatelyIdentifying(der_bytes)) {
      return true;
    }
  }

  return false;
}

base::Optional<base::span<const uint8_t>>
PackedAttestationStatement::GetLeafCertificate() const {
  if (x509_certificates_.empty()) {
    return base::nullopt;
  }
  return x509_certificates_[0];
}

}  // namespace device
