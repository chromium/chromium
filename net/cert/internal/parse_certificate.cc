// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/parse_certificate.h"

#include <utility>

#include "base/strings/string_util.h"
#include "net/cert/internal/cert_error_params.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/general_names.h"
#include "net/der/input.h"
#include "net/der/parse_values.h"
#include "net/der/parser.h"

namespace net {

namespace {

DEFINE_CERT_ERROR_ID(kCertificateNotSequence,
                     "Failed parsing Certificate SEQUENCE");
DEFINE_CERT_ERROR_ID(kUnconsumedDataInsideCertificateSequence,
                     "Unconsumed data inside Certificate SEQUENCE");
DEFINE_CERT_ERROR_ID(kUnconsumedDataAfterCertificateSequence,
                     "Unconsumed data after Certificate SEQUENCE");
DEFINE_CERT_ERROR_ID(kTbsCertificateNotSequence,
                     "Couldn't read tbsCertificate as SEQUENCE");
DEFINE_CERT_ERROR_ID(
    kSignatureAlgorithmNotSequence,
    "Couldn't read Certificate.signatureAlgorithm as SEQUENCE");
DEFINE_CERT_ERROR_ID(kSignatureValueNotBitString,
                     "Couldn't read Certificate.signatureValue as BIT STRING");
DEFINE_CERT_ERROR_ID(kUnconsumedDataInsideTbsCertificateSequence,
                     "Unconsumed data inside TBSCertificate");
DEFINE_CERT_ERROR_ID(kTbsNotSequence, "Failed parsing TBSCertificate SEQUENCE");
DEFINE_CERT_ERROR_ID(kFailedReadingVersion, "Failed reading version");
DEFINE_CERT_ERROR_ID(kFailedParsingVersion, "Failed parsing version");
DEFINE_CERT_ERROR_ID(kVersionExplicitlyV1,
                     "Version explicitly V1 (should be omitted)");
DEFINE_CERT_ERROR_ID(kFailedReadingSerialNumber, "Failed reading serialNumber");
DEFINE_CERT_ERROR_ID(kFailedReadingSignatureValue, "Failed reading signature");
DEFINE_CERT_ERROR_ID(kFailedReadingIssuer, "Failed reading issuer");
DEFINE_CERT_ERROR_ID(kFailedReadingValidity, "Failed reading validity");
DEFINE_CERT_ERROR_ID(kFailedParsingValidity, "Failed parsing validity");
DEFINE_CERT_ERROR_ID(kFailedReadingSubject, "Failed reading subject");
DEFINE_CERT_ERROR_ID(kFailedReadingSpki, "Failed reading subjectPublicKeyInfo");
DEFINE_CERT_ERROR_ID(kFailedReadingIssuerUniqueId,
                     "Failed reading issuerUniqueId");
DEFINE_CERT_ERROR_ID(kFailedParsingIssuerUniqueId,
                     "Failed parsing issuerUniqueId");
DEFINE_CERT_ERROR_ID(
    kIssuerUniqueIdNotExpected,
    "Unexpected issuerUniqueId (must be V2 or V3 certificate)");
DEFINE_CERT_ERROR_ID(kFailedReadingSubjectUniqueId,
                     "Failed reading subjectUniqueId");
DEFINE_CERT_ERROR_ID(kFailedParsingSubjectUniqueId,
                     "Failed parsing subjectUniqueId");
DEFINE_CERT_ERROR_ID(
    kSubjectUniqueIdNotExpected,
    "Unexpected subjectUniqueId (must be V2 or V3 certificate)");
DEFINE_CERT_ERROR_ID(kFailedReadingExtensions,
                     "Failed reading extensions SEQUENCE");
DEFINE_CERT_ERROR_ID(kUnexpectedExtensions,
                     "Unexpected extensions (must be V3 certificate)");
DEFINE_CERT_ERROR_ID(kSerialNumberIsNegative, "Serial number is negative");
DEFINE_CERT_ERROR_ID(kSerialNumberIsZero, "Serial number is zero");
DEFINE_CERT_ERROR_ID(kSerialNumberLengthOver20,
                     "Serial number is longer than 20 octets");
DEFINE_CERT_ERROR_ID(kSerialNumberNotValidInteger,
                     "Serial number is not a valid INTEGER");

// Returns true if |input| is a SEQUENCE and nothing else.
WARN_UNUSED_RESULT bool IsSequenceTLV(const der::Input& input) {
  der::Parser parser(input);
  der::Parser unused_sequence_parser;
  if (!parser.ReadSequence(&unused_sequence_parser))
    return false;
  // Should by a single SEQUENCE by definition of the function.
  return !parser.HasMore();
}

// Reads a SEQUENCE from |parser| and writes the full tag-length-value into
// |out|. On failure |parser| may or may not have been advanced.
WARN_UNUSED_RESULT bool ReadSequenceTLV(der::Parser* parser, der::Input* out) {
  return parser->ReadRawTLV(out) && IsSequenceTLV(*out);
}

// Parses a Version according to RFC 5280:
//
//     Version  ::=  INTEGER  {  v1(0), v2(1), v3(2)  }
//
// No value other that v1, v2, or v3 is allowed (and if given will fail). RFC
// 5280 minimally requires the handling of v3 (and overwhelmingly these are the
// certificate versions in use today):
//
//     Implementations SHOULD be prepared to accept any version certificate.
//     At a minimum, conforming implementations MUST recognize version 3
//     certificates.
WARN_UNUSED_RESULT bool ParseVersion(const der::Input& in,
                                     CertificateVersion* version) {
  der::Parser parser(in);
  uint64_t version64;
  if (!parser.ReadUint64(&version64))
    return false;

  switch (version64) {
    case 0:
      *version = CertificateVersion::V1;
      break;
    case 1:
      *version = CertificateVersion::V2;
      break;
    case 2:
      *version = CertificateVersion::V3;
      break;
    default:
      // Don't allow any other version identifier.
      return false;
  }

  // By definition the input to this function was a single INTEGER, so there
  // shouldn't be anything else after it.
  return !parser.HasMore();
}

// Parses a DER-encoded "Validity" as specified by RFC 5280. Returns true on
// success and sets the results in |not_before| and |not_after|:
//
//       Validity ::= SEQUENCE {
//            notBefore      Time,
//            notAfter       Time }
//
// Note that upon success it is NOT guaranteed that |*not_before <= *not_after|.
bool ParseValidity(const der::Input& validity_tlv,
                   der::GeneralizedTime* not_before,
                   der::GeneralizedTime* not_after) {
  der::Parser parser(validity_tlv);

  //     Validity ::= SEQUENCE {
  der::Parser validity_parser;
  if (!parser.ReadSequence(&validity_parser))
    return false;

  //          notBefore      Time,
  if (!ReadUTCOrGeneralizedTime(&validity_parser, not_before))
    return false;

  //          notAfter       Time }
  if (!ReadUTCOrGeneralizedTime(&validity_parser, not_after))
    return false;

  // By definition the input was a single Validity sequence, so there shouldn't
  // be unconsumed data.
  if (parser.HasMore())
    return false;

  // The Validity type does not have an extension point.
  if (validity_parser.HasMore())
    return false;

  // Note that RFC 5280 doesn't require notBefore to be <=
  // notAfter, so that will not be considered a "parsing" error here. Instead it
  // will be considered an expired certificate later when testing against the
  // current timestamp.
  return true;
}

// Returns true if every bit in |bits| is zero (including empty).
WARN_UNUSED_RESULT bool BitStringIsAllZeros(const der::BitString& bits) {
  // Note that it is OK to read from the unused bits, since BitString parsing
  // guarantees they are all zero.
  for (size_t i = 0; i < bits.bytes().Length(); ++i) {
    if (bits.bytes().UnsafeData()[i] != 0)
      return false;
  }
  return true;
}

// Parses a DistributionPointName.
//
// Currently this implementation is only concerned with URIs encoded in
// fullName and skips the rest (it does not fully parse the GeneralNames).
//
// URIs found in fullName are appended to |uris|.
//
// From RFC 5280:
//
//    DistributionPointName ::= CHOICE {
//      fullName                [0]     GeneralNames,
//      nameRelativeToCRLIssuer [1]     RelativeDistinguishedName }
bool ParseDistributionPointName(const der::Input& dp_name,
                                std::vector<base::StringPiece>* uris) {
  bool has_full_name;
  der::Input der_full_name;
  if (!der::Parser(dp_name).ReadOptionalTag(
          der::kTagContextSpecific | der::kTagConstructed | 0, &der_full_name,
          &has_full_name)) {
    return false;
  }
  if (!has_full_name) {
    // Only process DistributionPoints which provide "fullName".
    return true;
  }

  // TODO(mattm): surface the CertErrors.
  CertErrors errors;
  std::unique_ptr<GeneralNames> full_name =
      GeneralNames::CreateFromValue(der_full_name, &errors);
  if (!full_name)
    return false;

  // This code is only interested in extracting the URIs from fullName.
  *uris = full_name->uniform_resource_identifiers;
  return true;
}

// RFC 5280, section 4.2.1.13.
//
// DistributionPoint ::= SEQUENCE {
//  distributionPoint       [0]     DistributionPointName OPTIONAL,
//  reasons                 [1]     ReasonFlags OPTIONAL,
//  cRLIssuer               [2]     GeneralNames OPTIONAL }
bool ParseAndAddDistributionPoint(
    der::Parser* parser,
    std::vector<ParsedDistributionPoint>* distribution_points) {
  ParsedDistributionPoint distribution_point;

  // DistributionPoint ::= SEQUENCE {
  der::Parser distrib_point_parser;
  if (!parser->ReadSequence(&distrib_point_parser))
    return false;

  //  distributionPoint       [0]     DistributionPointName OPTIONAL,
  bool distribution_point_present;
  der::Input name;
  if (!distrib_point_parser.ReadOptionalTag(
          der::kTagContextSpecific | der::kTagConstructed | 0, &name,
          &distribution_point_present)) {
    return false;
  }

  if (!distribution_point_present) {
    // Only process DistributionPoints which provide a "distributionPoint".
    return true;
  }

  //  reasons                 [1]     ReasonFlags OPTIONAL,
  bool reasons_present;
  if (!distrib_point_parser.SkipOptionalTag(der::kTagContextSpecific | 1,
                                            &reasons_present)) {
    return false;
  }

  // If it contains a subset of reasons then we skip it. We aren't
  // interested in subsets of CRLs and the RFC states that there MUST be
  // a CRL that covers all reasons.
  if (reasons_present) {
    return true;
  }

  // Extract the URIs from the DistributionPointName.
  if (!ParseDistributionPointName(name, &distribution_point.uris))
    return false;

  //  cRLIssuer               [2]     GeneralNames OPTIONAL }
  bool crl_issuer_present;
  der::Input crl_issuer;
  if (!distrib_point_parser.ReadOptionalTag(
          der::kTagContextSpecific | der::kTagConstructed | 2, &crl_issuer,
          &crl_issuer_present)) {
    return false;
  }

  distribution_point.has_crl_issuer = crl_issuer_present;
  // TODO(eroman): Parse "cRLIssuer".

  if (distrib_point_parser.HasMore())
    return false;

  distribution_points->push_back(std::move(distribution_point));
  return true;
}

}  // namespace

ParsedTbsCertificate::ParsedTbsCertificate() = default;

ParsedTbsCertificate::~ParsedTbsCertificate() = default;

bool VerifySerialNumber(const der::Input& value,
                        bool warnings_only,
                        CertErrors* errors) {
  // If |warnings_only| was set to true, the exact same errors will be logged,
  // only they will be logged with a lower severity (warning rather than error).
  CertError::Severity error_severity =
      warnings_only ? CertError::SEVERITY_WARNING : CertError::SEVERITY_HIGH;

  bool negative;
  if (!der::IsValidInteger(value, &negative)) {
    errors->Add(error_severity, kSerialNumberNotValidInteger, nullptr);
    return false;
  }

  // RFC 5280 section 4.1.2.2:
  //
  //    Note: Non-conforming CAs may issue certificates with serial numbers
  //    that are negative or zero.  Certificate users SHOULD be prepared to
  //    gracefully handle such certificates.
  if (negative)
    errors->AddWarning(kSerialNumberIsNegative);
  if (value.Length() == 1 && value.UnsafeData()[0] == 0)
    errors->AddWarning(kSerialNumberIsZero);

  // RFC 5280 section 4.1.2.2:
  //
  //    Certificate users MUST be able to handle serialNumber values up to 20
  //    octets. Conforming CAs MUST NOT use serialNumber values longer than 20
  //    octets.
  if (value.Length() > 20) {
    errors->Add(error_severity, kSerialNumberLengthOver20,
                CreateCertErrorParams1SizeT("length", value.Length()));
    return false;
  }

  return true;
}

bool ReadUTCOrGeneralizedTime(der::Parser* parser, der::GeneralizedTime* out) {
  der::Input value;
  der::Tag tag;

  if (!parser->ReadTagAndValue(&tag, &value))
    return false;

  if (tag == der::kUtcTime)
    return der::ParseUTCTime(value, out);

  if (tag == der::kGeneralizedTime)
    return der::ParseGeneralizedTime(value, out);

  // Unrecognized tag.
  return false;
}

bool ParseCertificate(const der::Input& certificate_tlv,
                      der::Input* out_tbs_certificate_tlv,
                      der::Input* out_signature_algorithm_tlv,
                      der::BitString* out_signature_value,
                      CertErrors* out_errors) {
  // |out_errors| is optional. But ensure it is non-null for the remainder of
  // this function.
  if (!out_errors) {
    CertErrors unused_errors;
    return ParseCertificate(certificate_tlv, out_tbs_certificate_tlv,
                            out_signature_algorithm_tlv, out_signature_value,
                            &unused_errors);
  }

  der::Parser parser(certificate_tlv);

  //   Certificate  ::=  SEQUENCE  {
  der::Parser certificate_parser;
  if (!parser.ReadSequence(&certificate_parser)) {
    out_errors->AddError(kCertificateNotSequence);
    return false;
  }

  //        tbsCertificate       TBSCertificate,
  if (!ReadSequenceTLV(&certificate_parser, out_tbs_certificate_tlv)) {
    out_errors->AddError(kTbsCertificateNotSequence);
    return false;
  }

  //        signatureAlgorithm   AlgorithmIdentifier,
  if (!ReadSequenceTLV(&certificate_parser, out_signature_algorithm_tlv)) {
    out_errors->AddError(kSignatureAlgorithmNotSequence);
    return false;
  }

  //        signatureValue       BIT STRING  }
  if (!certificate_parser.ReadBitString(out_signature_value)) {
    out_errors->AddError(kSignatureValueNotBitString);
    return false;
  }

  // There isn't an extension point at the end of Certificate.
  if (certificate_parser.HasMore()) {
    out_errors->AddError(kUnconsumedDataInsideCertificateSequence);
    return false;
  }

  // By definition the input was a single Certificate, so there shouldn't be
  // unconsumed data.
  if (parser.HasMore()) {
    out_errors->AddError(kUnconsumedDataAfterCertificateSequence);
    return false;
  }

  return true;
}

// From RFC 5280 section 4.1:
//
//   TBSCertificate  ::=  SEQUENCE  {
//        version         [0]  EXPLICIT Version DEFAULT v1,
//        serialNumber         CertificateSerialNumber,
//        signature            AlgorithmIdentifier,
//        issuer               Name,
//        validity             Validity,
//        subject              Name,
//        subjectPublicKeyInfo SubjectPublicKeyInfo,
//        issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL,
//                             -- If present, version MUST be v2 or v3
//        subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL,
//                             -- If present, version MUST be v2 or v3
//        extensions      [3]  EXPLICIT Extensions OPTIONAL
//                             -- If present, version MUST be v3
//        }
bool ParseTbsCertificate(const der::Input& tbs_tlv,
                         const ParseCertificateOptions& options,
                         ParsedTbsCertificate* out,
                         CertErrors* errors) {
  // The rest of this function assumes that |errors| is non-null.
  if (!errors) {
    CertErrors unused_errors;
    return ParseTbsCertificate(tbs_tlv, options, out, &unused_errors);
  }

  // TODO(crbug.com/634443): Add useful error information to |errors|.

  der::Parser parser(tbs_tlv);

  //   TBSCertificate  ::=  SEQUENCE  {
  der::Parser tbs_parser;
  if (!parser.ReadSequence(&tbs_parser)) {
    errors->AddError(kTbsNotSequence);
    return false;
  }

  //        version         [0]  EXPLICIT Version DEFAULT v1,
  der::Input version;
  bool has_version;
  if (!tbs_parser.ReadOptionalTag(der::ContextSpecificConstructed(0), &version,
                                  &has_version)) {
    errors->AddError(kFailedReadingVersion);
    return false;
  }
  if (has_version) {
    if (!ParseVersion(version, &out->version)) {
      errors->AddError(kFailedParsingVersion);
      return false;
    }
    if (out->version == CertificateVersion::V1) {
      errors->AddError(kVersionExplicitlyV1);
      // The correct way to specify v1 is to omit the version field since v1 is
      // the DEFAULT.
      return false;
    }
  } else {
    out->version = CertificateVersion::V1;
  }

  //        serialNumber         CertificateSerialNumber,
  if (!tbs_parser.ReadTag(der::kInteger, &out->serial_number)) {
    errors->AddError(kFailedReadingSerialNumber);
    return false;
  }
  if (!VerifySerialNumber(out->serial_number,
                          options.allow_invalid_serial_numbers, errors)) {
    // Invalid serial numbers are only considered fatal failures if
    // |!allow_invalid_serial_numbers|.
    if (!options.allow_invalid_serial_numbers)
      return false;
  }

  //        signature            AlgorithmIdentifier,
  if (!ReadSequenceTLV(&tbs_parser, &out->signature_algorithm_tlv)) {
    errors->AddError(kFailedReadingSignatureValue);
    return false;
  }

  //        issuer               Name,
  if (!ReadSequenceTLV(&tbs_parser, &out->issuer_tlv)) {
    errors->AddError(kFailedReadingIssuer);
    return false;
  }

  //        validity             Validity,
  der::Input validity_tlv;
  if (!tbs_parser.ReadRawTLV(&validity_tlv)) {
    errors->AddError(kFailedReadingValidity);
    return false;
  }
  if (!ParseValidity(validity_tlv, &out->validity_not_before,
                     &out->validity_not_after)) {
    errors->AddError(kFailedParsingValidity);
    return false;
  }

  //        subject              Name,
  if (!ReadSequenceTLV(&tbs_parser, &out->subject_tlv)) {
    errors->AddError(kFailedReadingSubject);
    return false;
  }

  //        subjectPublicKeyInfo SubjectPublicKeyInfo,
  if (!ReadSequenceTLV(&tbs_parser, &out->spki_tlv)) {
    errors->AddError(kFailedReadingSpki);
    return false;
  }

  //        issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL,
  //                             -- If present, version MUST be v2 or v3
  der::Input issuer_unique_id;
  if (!tbs_parser.ReadOptionalTag(der::ContextSpecificPrimitive(1),
                                  &issuer_unique_id,
                                  &out->has_issuer_unique_id)) {
    errors->AddError(kFailedReadingIssuerUniqueId);
    return false;
  }
  if (out->has_issuer_unique_id) {
    if (!der::ParseBitString(issuer_unique_id, &out->issuer_unique_id)) {
      errors->AddError(kFailedParsingIssuerUniqueId);
      return false;
    }
    if (out->version != CertificateVersion::V2 &&
        out->version != CertificateVersion::V3) {
      errors->AddError(kIssuerUniqueIdNotExpected);
      return false;
    }
  }

  //        subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL,
  //                             -- If present, version MUST be v2 or v3
  der::Input subject_unique_id;
  if (!tbs_parser.ReadOptionalTag(der::ContextSpecificPrimitive(2),
                                  &subject_unique_id,
                                  &out->has_subject_unique_id)) {
    errors->AddError(kFailedReadingSubjectUniqueId);
    return false;
  }
  if (out->has_subject_unique_id) {
    if (!der::ParseBitString(subject_unique_id, &out->subject_unique_id)) {
      errors->AddError(kFailedParsingSubjectUniqueId);
      return false;
    }
    if (out->version != CertificateVersion::V2 &&
        out->version != CertificateVersion::V3) {
      errors->AddError(kSubjectUniqueIdNotExpected);
      return false;
    }
  }

  //        extensions      [3]  EXPLICIT Extensions OPTIONAL
  //                             -- If present, version MUST be v3
  if (!tbs_parser.ReadOptionalTag(der::ContextSpecificConstructed(3),
                                  &out->extensions_tlv, &out->has_extensions)) {
    errors->AddError(kFailedReadingExtensions);
    return false;
  }
  if (out->has_extensions) {
    // extensions_tlv must be a single element. Also check that it is a
    // SEQUENCE.
    if (!IsSequenceTLV(out->extensions_tlv)) {
      errors->AddError(kFailedReadingExtensions);
      return false;
    }
    if (out->version != CertificateVersion::V3) {
      errors->AddError(kUnexpectedExtensions);
      return false;
    }
  }

  // Note that there IS an extension point at the end of TBSCertificate
  // (according to RFC 5912), so from that interpretation, unconsumed data would
  // be allowed in |tbs_parser|.
  //
  // However because only v1, v2, and v3 certificates are supported by the
  // parsing, there shouldn't be any subsequent data in those versions, so
  // reject.
  if (tbs_parser.HasMore()) {
    errors->AddError(kUnconsumedDataInsideTbsCertificateSequence);
    return false;
  }

  // By definition the input was a single TBSCertificate, so there shouldn't be
  // unconsumed data.
  if (parser.HasMore())
    return false;

  return true;
}

// From RFC 5280:
//
//    Extension  ::=  SEQUENCE  {
//            extnID      OBJECT IDENTIFIER,
//            critical    BOOLEAN DEFAULT FALSE,
//            extnValue   OCTET STRING
//                        -- contains the DER encoding of an ASN.1 value
//                        -- corresponding to the extension type identified
//                        -- by extnID
//            }
bool ParseExtension(const der::Input& extension_tlv, ParsedExtension* out) {
  der::Parser parser(extension_tlv);

  //    Extension  ::=  SEQUENCE  {
  der::Parser extension_parser;
  if (!parser.ReadSequence(&extension_parser))
    return false;

  //            extnID      OBJECT IDENTIFIER,
  if (!extension_parser.ReadTag(der::kOid, &out->oid))
    return false;

  //            critical    BOOLEAN DEFAULT FALSE,
  out->critical = false;
  bool has_critical;
  der::Input critical;
  if (!extension_parser.ReadOptionalTag(der::kBool, &critical, &has_critical))
    return false;
  if (has_critical) {
    if (!der::ParseBool(critical, &out->critical))
      return false;
    if (!out->critical)
      return false;  // DER-encoding requires DEFAULT values be omitted.
  }

  //            extnValue   OCTET STRING
  if (!extension_parser.ReadTag(der::kOctetString, &out->value))
    return false;

  // The Extension type does not have an extension point (everything goes in
  // extnValue).
  if (extension_parser.HasMore())
    return false;

  // By definition the input was a single Extension sequence, so there shouldn't
  // be unconsumed data.
  if (parser.HasMore())
    return false;

  return true;
}

der::Input SubjectKeyIdentifierOid() {
  // From RFC 5280:
  //
  //     id-ce-subjectKeyIdentifier OBJECT IDENTIFIER ::=  { id-ce 14 }
  //
  // In dotted notation: 2.5.29.14
  static const uint8_t oid[] = {0x55, 0x1d, 0x0e};
  return der::Input(oid);
}

der::Input KeyUsageOid() {
  // From RFC 5280:
  //
  //     id-ce-keyUsage OBJECT IDENTIFIER ::=  { id-ce 15 }
  //
  // In dotted notation: 2.5.29.15
  static const uint8_t oid[] = {0x55, 0x1d, 0x0f};
  return der::Input(oid);
}

der::Input SubjectAltNameOid() {
  // From RFC 5280:
  //
  //     id-ce-subjectAltName OBJECT IDENTIFIER ::=  { id-ce 17 }
  //
  // In dotted notation: 2.5.29.17
  static const uint8_t oid[] = {0x55, 0x1d, 0x11};
  return der::Input(oid);
}

der::Input BasicConstraintsOid() {
  // From RFC 5280:
  //
  //     id-ce-basicConstraints OBJECT IDENTIFIER ::=  { id-ce 19 }
  //
  // In dotted notation: 2.5.29.19
  static const uint8_t oid[] = {0x55, 0x1d, 0x13};
  return der::Input(oid);
}

der::Input NameConstraintsOid() {
  // From RFC 5280:
  //
  //     id-ce-nameConstraints OBJECT IDENTIFIER ::=  { id-ce 30 }
  //
  // In dotted notation: 2.5.29.30
  static const uint8_t oid[] = {0x55, 0x1d, 0x1e};
  return der::Input(oid);
}

der::Input CertificatePoliciesOid() {
  // From RFC 5280:
  //
  //     id-ce-certificatePolicies OBJECT IDENTIFIER ::=  { id-ce 32 }
  //
  // In dotted notation: 2.5.29.32
  static const uint8_t oid[] = {0x55, 0x1d, 0x20};
  return der::Input(oid);
}

der::Input AuthorityKeyIdentifierOid() {
  // From RFC 5280:
  //
  //     id-ce-authorityKeyIdentifier OBJECT IDENTIFIER ::=  { id-ce 35 }
  //
  // In dotted notation: 2.5.29.35
  static const uint8_t oid[] = {0x55, 0x1d, 0x23};
  return der::Input(oid);
}

der::Input PolicyConstraintsOid() {
  // From RFC 5280:
  //
  //     id-ce-policyConstraints OBJECT IDENTIFIER ::=  { id-ce 36 }
  //
  // In dotted notation: 2.5.29.36
  static const uint8_t oid[] = {0x55, 0x1d, 0x24};
  return der::Input(oid);
}

der::Input ExtKeyUsageOid() {
  // From RFC 5280:
  //
  //     id-ce-extKeyUsage OBJECT IDENTIFIER ::= { id-ce 37 }
  //
  // In dotted notation: 2.5.29.37
  static const uint8_t oid[] = {0x55, 0x1d, 0x25};
  return der::Input(oid);
}

der::Input AuthorityInfoAccessOid() {
  // From RFC 5280:
  //
  //     id-pe-authorityInfoAccess OBJECT IDENTIFIER ::= { id-pe 1 }
  //
  // In dotted notation: 1.3.6.1.5.5.7.1.1
  static const uint8_t oid[] = {0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x01, 0x01};
  return der::Input(oid);
}

der::Input AdCaIssuersOid() {
  // From RFC 5280:
  //
  //     id-ad-caIssuers OBJECT IDENTIFIER ::= { id-ad 2 }
  //
  // In dotted notation: 1.3.6.1.5.5.7.48.2
  static const uint8_t oid[] = {0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x02};
  return der::Input(oid);
}

der::Input AdOcspOid() {
  // From RFC 5280:
  //
  //     id-ad-ocsp OBJECT IDENTIFIER ::= { id-ad 1 }
  //
  // In dotted notation: 1.3.6.1.5.5.7.48.1
  static const uint8_t oid[] = {0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01};
  return der::Input(oid);
}

der::Input CrlDistributionPointsOid() {
  // From RFC 5280:
  //
  //     id-ce-cRLDistributionPoints OBJECT IDENTIFIER ::=  { id-ce 31 }
  //
  // In dotted notation: 2.5.29.31
  static const uint8_t oid[] = {0x55, 0x1d, 0x1f};
  return der::Input(oid);
}

NET_EXPORT bool ParseExtensions(
    const der::Input& extensions_tlv,
    std::map<der::Input, ParsedExtension>* extensions) {
  der::Parser parser(extensions_tlv);

  //    Extensions  ::=  SEQUENCE SIZE (1..MAX) OF Extension
  der::Parser extensions_parser;
  if (!parser.ReadSequence(&extensions_parser))
    return false;

  // The Extensions SEQUENCE must contains at least 1 element (otherwise it
  // should have been omitted).
  if (!extensions_parser.HasMore())
    return false;

  extensions->clear();

  while (extensions_parser.HasMore()) {
    ParsedExtension extension;

    der::Input extension_tlv;
    if (!extensions_parser.ReadRawTLV(&extension_tlv))
      return false;

    if (!ParseExtension(extension_tlv, &extension))
      return false;

    bool is_duplicate =
        !extensions->insert(std::make_pair(extension.oid, extension)).second;

    // RFC 5280 says that an extension should not appear more than once.
    if (is_duplicate)
      return false;
  }

  // By definition the input was a single Extensions sequence, so there
  // shouldn't be unconsumed data.
  if (parser.HasMore())
    return false;

  return true;
}

NET_EXPORT bool ConsumeExtension(
    const der::Input& oid,
    std::map<der::Input, ParsedExtension>* unconsumed_extensions,
    ParsedExtension* extension) {
  auto it = unconsumed_extensions->find(oid);
  if (it == unconsumed_extensions->end())
    return false;

  *extension = it->second;
  unconsumed_extensions->erase(it);
  return true;
}

bool ParseBasicConstraints(const der::Input& basic_constraints_tlv,
                           ParsedBasicConstraints* out) {
  der::Parser parser(basic_constraints_tlv);

  //    BasicConstraints ::= SEQUENCE {
  der::Parser sequence_parser;
  if (!parser.ReadSequence(&sequence_parser))
    return false;

  //         cA                      BOOLEAN DEFAULT FALSE,
  out->is_ca = false;
  bool has_ca;
  der::Input ca;
  if (!sequence_parser.ReadOptionalTag(der::kBool, &ca, &has_ca))
    return false;
  if (has_ca) {
    if (!der::ParseBool(ca, &out->is_ca))
      return false;
    // TODO(eroman): Should reject if CA was set to false, since
    // DER-encoding requires DEFAULT values be omitted. In
    // practice however there are a lot of certificates that use
    // the broken encoding.
  }

  //         pathLenConstraint       INTEGER (0..MAX) OPTIONAL }
  der::Input encoded_path_len;
  if (!sequence_parser.ReadOptionalTag(der::kInteger, &encoded_path_len,
                                       &out->has_path_len)) {
    return false;
  }
  if (out->has_path_len) {
    // TODO(eroman): Surface reason for failure if length was longer than uint8.
    if (!der::ParseUint8(encoded_path_len, &out->path_len))
      return false;
  } else {
    // Default initialize to 0 as a precaution.
    out->path_len = 0;
  }

  // There shouldn't be any unconsumed data in the extension.
  if (sequence_parser.HasMore())
    return false;

  // By definition the input was a single BasicConstraints sequence, so there
  // shouldn't be unconsumed data.
  if (parser.HasMore())
    return false;

  return true;
}

bool ParseKeyUsage(const der::Input& key_usage_tlv, der::BitString* key_usage) {
  der::Parser parser(key_usage_tlv);
  if (!parser.ReadBitString(key_usage))
    return false;

  // By definition the input was a single BIT STRING.
  if (parser.HasMore())
    return false;

  // RFC 5280 section 4.2.1.3:
  //
  //     When the keyUsage extension appears in a certificate, at least
  //     one of the bits MUST be set to 1.
  if (BitStringIsAllZeros(*key_usage))
    return false;

  return true;
}

bool ParseAuthorityInfoAccess(
    const der::Input& authority_info_access_tlv,
    std::vector<base::StringPiece>* out_ca_issuers_uris,
    std::vector<base::StringPiece>* out_ocsp_uris) {
  der::Parser parser(authority_info_access_tlv);

  out_ca_issuers_uris->clear();
  out_ocsp_uris->clear();

  //    AuthorityInfoAccessSyntax  ::=
  //            SEQUENCE SIZE (1..MAX) OF AccessDescription
  der::Parser sequence_parser;
  if (!parser.ReadSequence(&sequence_parser))
    return false;
  if (!sequence_parser.HasMore())
    return false;

  while (sequence_parser.HasMore()) {
    //    AccessDescription  ::=  SEQUENCE {
    der::Parser access_description_sequence_parser;
    if (!sequence_parser.ReadSequence(&access_description_sequence_parser))
      return false;

    //            accessMethod          OBJECT IDENTIFIER,
    der::Input access_method_oid;
    if (!access_description_sequence_parser.ReadTag(der::kOid,
                                                    &access_method_oid))
      return false;

    //            accessLocation        GeneralName  }
    der::Tag access_location_tag;
    der::Input access_location_value;
    if (!access_description_sequence_parser.ReadTagAndValue(
            &access_location_tag, &access_location_value))
      return false;

    // GeneralName ::= CHOICE {
    if (access_location_tag == der::ContextSpecificPrimitive(6)) {
      // uniformResourceIdentifier       [6]     IA5String,
      base::StringPiece uri = access_location_value.AsStringPiece();
      if (!base::IsStringASCII(uri))
        return false;

      if (access_method_oid == AdCaIssuersOid())
        out_ca_issuers_uris->push_back(uri);
      else if (access_method_oid == AdOcspOid())
        out_ocsp_uris->push_back(uri);
    }
  }

  return true;
}

ParsedDistributionPoint::ParsedDistributionPoint() = default;
ParsedDistributionPoint::ParsedDistributionPoint(
    ParsedDistributionPoint&& other) = default;
ParsedDistributionPoint::~ParsedDistributionPoint() = default;

bool ParseCrlDistributionPoints(
    const der::Input& extension_value,
    std::vector<ParsedDistributionPoint>* distribution_points) {
  distribution_points->clear();

  // RFC 5280, section 4.2.1.13.
  //
  // CRLDistributionPoints ::= SEQUENCE SIZE (1..MAX) OF DistributionPoint
  der::Parser extension_value_parser(extension_value);
  der::Parser distribution_points_parser;
  if (!extension_value_parser.ReadSequence(&distribution_points_parser))
    return false;
  if (extension_value_parser.HasMore())
    return false;

  // Sequence must have a minimum of 1 item.
  if (!distribution_points_parser.HasMore())
    return false;

  while (distribution_points_parser.HasMore()) {
    if (!ParseAndAddDistributionPoint(&distribution_points_parser,
                                      distribution_points))
      return false;
  }

  return true;
}

ParsedAuthorityKeyIdentifier::ParsedAuthorityKeyIdentifier() = default;
ParsedAuthorityKeyIdentifier::~ParsedAuthorityKeyIdentifier() = default;
ParsedAuthorityKeyIdentifier::ParsedAuthorityKeyIdentifier(
    ParsedAuthorityKeyIdentifier&& other) = default;
ParsedAuthorityKeyIdentifier& ParsedAuthorityKeyIdentifier::operator=(
    ParsedAuthorityKeyIdentifier&& other) = default;

bool ParseAuthorityKeyIdentifier(
    const der::Input& extension_value,
    ParsedAuthorityKeyIdentifier* authority_key_identifier) {
  // RFC 5280, section 4.2.1.1.
  //    AuthorityKeyIdentifier ::= SEQUENCE {
  //       keyIdentifier             [0] KeyIdentifier           OPTIONAL,
  //       authorityCertIssuer       [1] GeneralNames            OPTIONAL,
  //       authorityCertSerialNumber [2] CertificateSerialNumber OPTIONAL  }
  //
  //    KeyIdentifier ::= OCTET STRING

  der::Parser extension_value_parser(extension_value);
  der::Parser aki_parser;
  if (!extension_value_parser.ReadSequence(&aki_parser))
    return false;
  if (extension_value_parser.HasMore())
    return false;

  // TODO(mattm): Should having an empty AuthorityKeyIdentifier SEQUENCE be an
  // error? RFC 5280 doesn't explicitly say it.

  //       keyIdentifier             [0] KeyIdentifier           OPTIONAL,
  if (!aki_parser.ReadOptionalTag(der::ContextSpecificPrimitive(0),
                                  &authority_key_identifier->key_identifier)) {
    return false;
  }

  //       authorityCertIssuer       [1] GeneralNames            OPTIONAL,
  if (!aki_parser.ReadOptionalTag(
          der::ContextSpecificConstructed(1),
          &authority_key_identifier->authority_cert_issuer)) {
    return false;
  }

  //       authorityCertSerialNumber [2] CertificateSerialNumber OPTIONAL  }
  if (!aki_parser.ReadOptionalTag(
          der::ContextSpecificPrimitive(2),
          &authority_key_identifier->authority_cert_serial_number)) {
    return false;
  }

  //     -- authorityCertIssuer and authorityCertSerialNumber MUST both
  //     -- be present or both be absent
  if (authority_key_identifier->authority_cert_issuer.has_value() !=
      authority_key_identifier->authority_cert_serial_number.has_value()) {
    return false;
  }

  // There shouldn't be any unconsumed data in the AuthorityKeyIdentifier
  // SEQUENCE.
  if (aki_parser.HasMore())
    return false;

  return true;
}

bool ParseSubjectKeyIdentifier(const der::Input& extension_value,
                               der::Input* subject_key_identifier) {
  //    SubjectKeyIdentifier ::= KeyIdentifier
  //
  //    KeyIdentifier ::= OCTET STRING
  der::Parser extension_value_parser(extension_value);
  if (!extension_value_parser.ReadTag(der::kOctetString,
                                      subject_key_identifier)) {
    return false;
  }

  // There shouldn't be any unconsumed data in the extension SEQUENCE.
  if (extension_value_parser.HasMore())
    return false;

  return true;
}

}  // namespace net
