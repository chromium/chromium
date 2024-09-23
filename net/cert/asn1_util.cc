// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/asn1_util.h"

#include <optional>
#include <string_view>

#include "third_party/boringssl/src/pki/input.h"
#include "third_party/boringssl/src/pki/parse_certificate.h"
#include "third_party/boringssl/src/pki/parser.h"

namespace net::asn1 {

namespace {

// Parses input |in| which should point to the beginning of a Certificate, and
// sets |*tbs_certificate| ready to parse the Subject. If parsing
// fails, this function returns false and |*tbs_certificate| is left in an
// undefined state.
bool SeekToSubject(bssl::der::Input in, bssl::der::Parser* tbs_certificate) {
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
  //      ... }

  bssl::der::Parser parser(in);
  bssl::der::Parser certificate;
  if (!parser.ReadSequence(&certificate))
    return false;

  // We don't allow junk after the certificate.
  if (parser.HasMore())
    return false;

  if (!certificate.ReadSequence(tbs_certificate))
    return false;

  bool unused;
  if (!tbs_certificate->SkipOptionalTag(
          CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC | 0, &unused)) {
    return false;
  }

  // serialNumber
  if (!tbs_certificate->SkipTag(CBS_ASN1_INTEGER)) {
    return false;
  }
  // signature
  if (!tbs_certificate->SkipTag(CBS_ASN1_SEQUENCE)) {
    return false;
  }
  // issuer
  if (!tbs_certificate->SkipTag(CBS_ASN1_SEQUENCE)) {
    return false;
  }
  // validity
  if (!tbs_certificate->SkipTag(CBS_ASN1_SEQUENCE)) {
    return false;
  }
  return true;
}

// Parses input |in| which should point to the beginning of a Certificate, and
// sets |*tbs_certificate| ready to parse the SubjectPublicKeyInfo. If parsing
// fails, this function returns false and |*tbs_certificate| is left in an
// undefined state.
bool SeekToSPKI(bssl::der::Input in, bssl::der::Parser* tbs_certificate) {
  return SeekToSubject(in, tbs_certificate) &&
         // Skip over Subject.
         tbs_certificate->SkipTag(CBS_ASN1_SEQUENCE);
}

// Parses input |in| which should point to the beginning of a
// Certificate. If parsing fails, this function returns false, with
// |*extensions_present| and |*extensions_parser| left in an undefined
// state. If parsing succeeds and extensions are present, this function
// sets |*extensions_present| to true and sets |*extensions_parser|
// ready to parse the Extensions. If extensions are not present, it sets
// |*extensions_present| to false and |*extensions_parser| is left in an
// undefined state.
bool SeekToExtensions(bssl::der::Input in,
                      bool* extensions_present,
                      bssl::der::Parser* extensions_parser) {
  bool present;
  bssl::der::Parser tbs_cert_parser;
  if (!SeekToSPKI(in, &tbs_cert_parser))
    return false;

  // From RFC 5280, section 4.1
  // TBSCertificate  ::=  SEQUENCE  {
  //      ...
  //      subjectPublicKeyInfo SubjectPublicKeyInfo,
  //      issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL,
  //      subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL,
  //      extensions      [3]  EXPLICIT Extensions OPTIONAL }

  // subjectPublicKeyInfo
  if (!tbs_cert_parser.SkipTag(CBS_ASN1_SEQUENCE)) {
    return false;
  }
  // issuerUniqueID
  if (!tbs_cert_parser.SkipOptionalTag(CBS_ASN1_CONTEXT_SPECIFIC | 1,
                                       &present)) {
    return false;
  }
  // subjectUniqueID
  if (!tbs_cert_parser.SkipOptionalTag(CBS_ASN1_CONTEXT_SPECIFIC | 2,
                                       &present)) {
    return false;
  }

  std::optional<bssl::der::Input> extensions;
  if (!tbs_cert_parser.ReadOptionalTag(
          CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC | 3, &extensions)) {
    return false;
  }

  if (!extensions) {
    *extensions_present = false;
    return true;
  }

  // Extensions  ::=  SEQUENCE SIZE (1..MAX) OF Extension
  // Extension   ::=  SEQUENCE  {
  //      extnID      OBJECT IDENTIFIER,
  //      critical    BOOLEAN DEFAULT FALSE,
  //      extnValue   OCTET STRING }

  // |extensions| was EXPLICITly tagged, so we still need to remove the
  // ASN.1 SEQUENCE header.
  bssl::der::Parser explicit_extensions_parser(extensions.value());
  if (!explicit_extensions_parser.ReadSequence(extensions_parser))
    return false;

  if (explicit_extensions_parser.HasMore())
    return false;

  *extensions_present = true;
  return true;
}

// Parse a DER-encoded, X.509 certificate in |cert| and find an extension with
// the given OID. Returns false on parse error or true if the parse was
// successful. |*out_extension_present| will be true iff the extension was
// found. In the case where it was found, |*out_extension| will describe the
// extension, or is undefined on parse error or if the extension is missing.
bool ExtractExtensionWithOID(std::string_view cert,
                             bssl::der::Input extension_oid,
                             bool* out_extension_present,
                             bssl::ParsedExtension* out_extension) {
  bssl::der::Parser extensions;
  bool extensions_present;
  if (!SeekToExtensions(bssl::der::Input(cert), &extensions_present,
                        &extensions)) {
    return false;
  }
  if (!extensions_present) {
    *out_extension_present = false;
    return true;
  }

  while (extensions.HasMore()) {
    bssl::der::Input extension_tlv;
    if (!extensions.ReadRawTLV(&extension_tlv) ||
        !ParseExtension(extension_tlv, out_extension)) {
      return false;
    }

    if (out_extension->oid == extension_oid) {
      *out_extension_present = true;
      return true;
    }
  }

  *out_extension_present = false;
  return true;
}

}  // namespace

bool ExtractSubjectFromDERCert(std::string_view cert,
                               std::string_view* subject_out) {
  bssl::der::Parser parser;
  if (!SeekToSubject(bssl::der::Input(cert), &parser)) {
    return false;
  }
  bssl::der::Input subject;
  if (!parser.ReadRawTLV(&subject))
    return false;
  *subject_out = subject.AsStringView();
  return true;
}

bool ExtractSPKIFromDERCert(std::string_view cert, std::string_view* spki_out) {
  bssl::der::Parser parser;
  if (!SeekToSPKI(bssl::der::Input(cert), &parser)) {
    return false;
  }
  bssl::der::Input spki;
  if (!parser.ReadRawTLV(&spki))
    return false;
  *spki_out = spki.AsStringView();
  return true;
}

bool ExtractSubjectPublicKeyFromSPKI(std::string_view spki,
                                     std::string_view* spk_out) {
  // From RFC 5280, Section 4.1
  //   SubjectPublicKeyInfo  ::=  SEQUENCE  {
  //     algorithm            AlgorithmIdentifier,
  //     subjectPublicKey     BIT STRING  }
  //
  //   AlgorithmIdentifier  ::=  SEQUENCE  {
  //     algorithm               OBJECT IDENTIFIER,
  //     parameters              ANY DEFINED BY algorithm OPTIONAL  }

  // Step into SubjectPublicKeyInfo sequence.
  bssl::der::Parser parser((bssl::der::Input(spki)));
  bssl::der::Parser spki_parser;
  if (!parser.ReadSequence(&spki_parser))
    return false;

  // Step over algorithm field (a SEQUENCE).
  if (!spki_parser.SkipTag(CBS_ASN1_SEQUENCE)) {
    return false;
  }

  // Extract the subjectPublicKey field.
  bssl::der::Input spk;
  if (!spki_parser.ReadTag(CBS_ASN1_BITSTRING, &spk)) {
    return false;
  }
  *spk_out = spk.AsStringView();
  return true;
}

bool HasCanSignHttpExchangesDraftExtension(std::string_view cert) {
  // kCanSignHttpExchangesDraftOid is the DER encoding of the OID for
  // canSignHttpExchangesDraft defined in:
  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html
  static const uint8_t kCanSignHttpExchangesDraftOid[] = {
      0x2B, 0x06, 0x01, 0x04, 0x01, 0xd6, 0x79, 0x02, 0x01, 0x16};

  bool extension_present;
  bssl::ParsedExtension extension;
  if (!ExtractExtensionWithOID(cert,
                               bssl::der::Input(kCanSignHttpExchangesDraftOid),
                               &extension_present, &extension) ||
      !extension_present) {
    return false;
  }

  // The extension should have contents NULL.
  static const uint8_t kNull[] = {0x05, 0x00};
  return extension.value == bssl::der::Input(kNull);
}

bool ExtractSignatureAlgorithmsFromDERCert(
    std::string_view cert,
    std::string_view* cert_signature_algorithm_sequence,
    std::string_view* tbs_signature_algorithm_sequence) {
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
  //      ... }

  bssl::der::Parser parser((bssl::der::Input(cert)));
  bssl::der::Parser certificate;
  if (!parser.ReadSequence(&certificate))
    return false;

  bssl::der::Parser tbs_certificate;
  if (!certificate.ReadSequence(&tbs_certificate))
    return false;

  bool unused;
  if (!tbs_certificate.SkipOptionalTag(
          CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC | 0, &unused)) {
    return false;
  }

  // serialNumber
  if (!tbs_certificate.SkipTag(CBS_ASN1_INTEGER)) {
    return false;
  }
  // signature
  bssl::der::Input tbs_algorithm;
  if (!tbs_certificate.ReadRawTLV(&tbs_algorithm))
    return false;

  bssl::der::Input cert_algorithm;
  if (!certificate.ReadRawTLV(&cert_algorithm))
    return false;

  *cert_signature_algorithm_sequence = cert_algorithm.AsStringView();
  *tbs_signature_algorithm_sequence = tbs_algorithm.AsStringView();
  return true;
}

bool ExtractExtensionFromDERCert(std::string_view cert,
                                 std::string_view extension_oid,
                                 bool* out_extension_present,
                                 bool* out_extension_critical,
                                 std::string_view* out_contents) {
  *out_extension_present = false;
  *out_extension_critical = false;
  *out_contents = std::string_view();

  bssl::ParsedExtension extension;
  if (!ExtractExtensionWithOID(cert, bssl::der::Input(extension_oid),
                               out_extension_present, &extension)) {
    return false;
  }
  if (!*out_extension_present)
    return true;

  *out_extension_critical = extension.critical;
  *out_contents = extension.value.AsStringView();
  return true;
}

}  // namespace net::asn1
