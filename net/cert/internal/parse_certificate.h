// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_PARSE_CERTIFICATE_H_
#define NET_CERT_INTERNAL_PARSE_CERTIFICATE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/optional.h"
#include "net/base/net_export.h"
#include "net/der/input.h"
#include "net/der/parse_values.h"

namespace net {

namespace der {
class Parser;
}

class CertErrors;
struct ParsedTbsCertificate;

// Returns true if the given serial number (CertificateSerialNumber in RFC 5280)
// is valid:
//
//    CertificateSerialNumber  ::=  INTEGER
//
// The input to this function is the (unverified) value octets of the INTEGER.
// This function will verify that:
//
//   * The octets are a valid DER-encoding of an INTEGER (for instance, minimal
//     encoding length).
//
//   * No more than 20 octets are used.
//
// Note that it DOES NOT reject non-positive values (zero or negative).
//
// For reference, here is what RFC 5280 section 4.1.2.2 says:
//
//     Given the uniqueness requirements above, serial numbers can be
//     expected to contain long integers.  Certificate users MUST be able to
//     handle serialNumber values up to 20 octets.  Conforming CAs MUST NOT
//     use serialNumber values longer than 20 octets.
//
//     Note: Non-conforming CAs may issue certificates with serial numbers
//     that are negative or zero.  Certificate users SHOULD be prepared to
//     gracefully handle such certificates.
//
// |errors| must be a non-null destination for any errors/warnings. If
// |warnings_only| is set to true, then what would ordinarily be errors are
// instead added as warnings.
NET_EXPORT bool VerifySerialNumber(const der::Input& value,
                                   bool warnings_only,
                                   CertErrors* errors) WARN_UNUSED_RESULT;

// Consumes a "Time" value (as defined by RFC 5280) from |parser|. On success
// writes the result to |*out| and returns true. On failure no guarantees are
// made about the state of |parser|.
//
// From RFC 5280:
//
//     Time ::= CHOICE {
//          utcTime        UTCTime,
//          generalTime    GeneralizedTime }
NET_EXPORT bool ReadUTCOrGeneralizedTime(der::Parser* parser,
                                         der::GeneralizedTime* out)
    WARN_UNUSED_RESULT;

struct NET_EXPORT ParseCertificateOptions {
  // If set to true, then parsing will skip checks on the certificate's serial
  // number. The only requirement will be that the serial number is an INTEGER,
  // however it is not required to be a valid DER-encoding (i.e. minimal
  // encoding), nor is it required to be constrained to any particular length.
  bool allow_invalid_serial_numbers = false;
};

// Parses a DER-encoded "Certificate" as specified by RFC 5280. Returns true on
// success and sets the results in the |out_*| parameters. On both the failure
// and success case, if |out_errors| was non-null it may contain extra error
// information.
//
// Note that on success the out parameters alias data from the input
// |certificate_tlv|.  Hence the output values are only valid as long as
// |certificate_tlv| remains valid.
//
// On failure the out parameters have an undefined state, except for
// out_errors. Some of them may have been updated during parsing, whereas
// others may not have been changed.
//
// The out parameters represent each field of the Certificate SEQUENCE:
//       Certificate  ::=  SEQUENCE  {
//
// The |out_tbs_certificate_tlv| parameter corresponds with "tbsCertificate"
// from RFC 5280:
//         tbsCertificate       TBSCertificate,
//
// This contains the full (unverified) Tag-Length-Value for a SEQUENCE. No
// guarantees are made regarding the value of this SEQUENCE.
// This can be further parsed using ParseTbsCertificate().
//
// The |out_signature_algorithm_tlv| parameter corresponds with
// "signatureAlgorithm" from RFC 5280:
//         signatureAlgorithm   AlgorithmIdentifier,
//
// This contains the full (unverified) Tag-Length-Value for a SEQUENCE. No
// guarantees are made regarding the value of this SEQUENCE.
// This can be further parsed using SignatureValue::Create().
//
// The |out_signature_value| parameter corresponds with "signatureValue" from
// RFC 5280:
//         signatureValue       BIT STRING  }
//
// Parsing guarantees that this is a valid BIT STRING.
NET_EXPORT bool ParseCertificate(const der::Input& certificate_tlv,
                                 der::Input* out_tbs_certificate_tlv,
                                 der::Input* out_signature_algorithm_tlv,
                                 der::BitString* out_signature_value,
                                 CertErrors* out_errors) WARN_UNUSED_RESULT;

// Parses a DER-encoded "TBSCertificate" as specified by RFC 5280. Returns true
// on success and sets the results in |out|. Certain invalid inputs may
// be accepted based on the provided |options|.
//
// If |errors| was non-null then any warnings/errors that occur during parsing
// are added to it.
//
// Note that on success |out| aliases data from the input |tbs_tlv|.
// Hence the fields of the ParsedTbsCertificate are only valid as long as
// |tbs_tlv| remains valid.
//
// On failure |out| has an undefined state. Some of its fields may have been
// updated during parsing, whereas others may not have been changed.
//
// Refer to the per-field documentation of ParsedTbsCertificate for details on
// what validity checks parsing performs.
//
//       TBSCertificate  ::=  SEQUENCE  {
//            version         [0]  EXPLICIT Version DEFAULT v1,
//            serialNumber         CertificateSerialNumber,
//            signature            AlgorithmIdentifier,
//            issuer               Name,
//            validity             Validity,
//            subject              Name,
//            subjectPublicKeyInfo SubjectPublicKeyInfo,
//            issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL,
//                                 -- If present, version MUST be v2 or v3
//            subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL,
//                                 -- If present, version MUST be v2 or v3
//            extensions      [3]  EXPLICIT Extensions OPTIONAL
//                                 -- If present, version MUST be v3
//            }
NET_EXPORT bool ParseTbsCertificate(const der::Input& tbs_tlv,
                                    const ParseCertificateOptions& options,
                                    ParsedTbsCertificate* out,
                                    CertErrors* errors) WARN_UNUSED_RESULT;

// Represents a "Version" from RFC 5280:
//         Version  ::=  INTEGER  {  v1(0), v2(1), v3(2)  }
enum class CertificateVersion {
  V1,
  V2,
  V3,
};

// ParsedTbsCertificate contains pointers to the main fields of a DER-encoded
// RFC 5280 "TBSCertificate".
//
// ParsedTbsCertificate is expected to be filled by ParseTbsCertificate(), so
// subsequent field descriptions are in terms of what ParseTbsCertificate()
// sets.
struct NET_EXPORT ParsedTbsCertificate {
  ParsedTbsCertificate();
  ~ParsedTbsCertificate();

  // Corresponds with "version" from RFC 5280:
  //         version         [0]  EXPLICIT Version DEFAULT v1,
  //
  // Parsing guarantees that the version is one of v1, v2, or v3.
  CertificateVersion version = CertificateVersion::V1;

  // Corresponds with "serialNumber" from RFC 5280:
  //         serialNumber         CertificateSerialNumber,
  //
  // This field specifically contains the content bytes of the INTEGER. So for
  // instance if the serial number was 1000 then this would contain bytes
  // {0x03, 0xE8}.
  //
  // The serial number may or may not be a valid DER-encoded INTEGER:
  //
  // If the option |allow_invalid_serial_numbers=true| was used during
  // parsing, then nothing further can be assumed about these bytes.
  //
  // Otherwise if |allow_invalid_serial_numbers=false| then in addition
  // to being a valid DER-encoded INTEGER, parsing guarantees that
  // the serial number is at most 20 bytes long. Parsing does NOT guarantee
  // that the integer is positive (might be zero or negative).
  der::Input serial_number;

  // Corresponds with "signatureAlgorithm" from RFC 5280:
  //         signatureAlgorithm   AlgorithmIdentifier,
  //
  // This contains the full (unverified) Tag-Length-Value for a SEQUENCE. No
  // guarantees are made regarding the value of this SEQUENCE.
  //
  // This can be further parsed using SignatureValue::Create().
  der::Input signature_algorithm_tlv;

  // Corresponds with "issuer" from RFC 5280:
  //         issuer               Name,
  //
  // This contains the full (unverified) Tag-Length-Value for a SEQUENCE. No
  // guarantees are made regarding the value of this SEQUENCE.
  der::Input issuer_tlv;

  // Corresponds with "validity" from RFC 5280:
  //         validity             Validity,
  //
  // Where Validity is defined as:
  //
  //   Validity ::= SEQUENCE {
  //        notBefore      Time,
  //        notAfter       Time }
  //
  // Parsing guarantees that notBefore (validity_not_before) and notAfter
  // (validity_not_after) are valid DER-encoded dates, however it DOES NOT
  // gurantee anything about their values. For instance notAfter could be
  // before notBefore, or the dates could indicate an expired certificate.
  // Consumers are responsible for testing expiration.
  der::GeneralizedTime validity_not_before;
  der::GeneralizedTime validity_not_after;

  // Corresponds with "subject" from RFC 5280:
  //         subject              Name,
  //
  // This contains the full (unverified) Tag-Length-Value for a SEQUENCE. No
  // guarantees are made regarding the value of this SEQUENCE.
  der::Input subject_tlv;

  // Corresponds with "subjectPublicKeyInfo" from RFC 5280:
  //         subjectPublicKeyInfo SubjectPublicKeyInfo,
  //
  // This contains the full (unverified) Tag-Length-Value for a SEQUENCE. No
  // guarantees are made regarding the value of this SEQUENCE.
  der::Input spki_tlv;

  // Corresponds with "issuerUniqueID" from RFC 5280:
  //         issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL,
  //                              -- If present, version MUST be v2 or v3
  //
  // Parsing guarantees that if issuer_unique_id is present it is a valid BIT
  // STRING, and that the version is either v2 or v3
  bool has_issuer_unique_id = false;
  der::BitString issuer_unique_id;

  // Corresponds with "subjectUniqueID" from RFC 5280:
  //         subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL,
  //                              -- If present, version MUST be v2 or v3
  //
  // Parsing guarantees that if subject_unique_id is present it is a valid BIT
  // STRING, and that the version is either v2 or v3
  bool has_subject_unique_id = false;
  der::BitString subject_unique_id;

  // Corresponds with "extensions" from RFC 5280:
  //         extensions      [3]  EXPLICIT Extensions OPTIONAL
  //                              -- If present, version MUST be v3
  //
  //
  // This contains the full (unverified) Tag-Length-Value for a SEQUENCE. No
  // guarantees are made regarding the value of this SEQUENCE. (Note that the
  // EXPLICIT outer tag is stripped.)
  //
  // Parsing guarantees that if extensions is present the version is v3.
  bool has_extensions = false;
  der::Input extensions_tlv;
};

// ParsedExtension represents a parsed "Extension" from RFC 5280. It contains
// der:Inputs which are not owned so the associated data must be kept alive.
//
//    Extension  ::=  SEQUENCE  {
//            extnID      OBJECT IDENTIFIER,
//            critical    BOOLEAN DEFAULT FALSE,
//            extnValue   OCTET STRING
//                        -- contains the DER encoding of an ASN.1 value
//                        -- corresponding to the extension type identified
//                        -- by extnID
//            }
struct NET_EXPORT ParsedExtension {
  der::Input oid;
  // |value| will contain the contents of the OCTET STRING. For instance for
  // basicConstraints it will be the TLV for a SEQUENCE.
  der::Input value;
  bool critical = false;
};

// Parses a DER-encoded "Extension" as specified by RFC 5280. Returns true on
// success and sets the results in |out|.
//
// Note that on success |out| aliases data from the input |extension_tlv|.
// Hence the fields of the ParsedExtension are only valid as long as
// |extension_tlv| remains valid.
//
// On failure |out| has an undefined state. Some of its fields may have been
// updated during parsing, whereas others may not have been changed.
NET_EXPORT bool ParseExtension(const der::Input& extension_tlv,
                               ParsedExtension* out) WARN_UNUSED_RESULT;

// From RFC 5280:
//
//     id-ce-subjectKeyIdentifier OBJECT IDENTIFIER ::=  { id-ce 14 }
//
// In dotted notation: 2.5.29.14
NET_EXPORT der::Input SubjectKeyIdentifierOid();

// From RFC 5280:
//
//     id-ce-keyUsage OBJECT IDENTIFIER ::=  { id-ce 15 }
//
// In dotted notation: 2.5.29.15
NET_EXPORT der::Input KeyUsageOid();

// From RFC 5280:
//
//     id-ce-subjectAltName OBJECT IDENTIFIER ::=  { id-ce 17 }
//
// In dotted notation: 2.5.29.17
NET_EXPORT der::Input SubjectAltNameOid();

// From RFC 5280:
//
//     id-ce-basicConstraints OBJECT IDENTIFIER ::=  { id-ce 19 }
//
// In dotted notation: 2.5.29.19
NET_EXPORT der::Input BasicConstraintsOid();

// From RFC 5280:
//
//     id-ce-nameConstraints OBJECT IDENTIFIER ::=  { id-ce 30 }
//
// In dotted notation: 2.5.29.30
NET_EXPORT der::Input NameConstraintsOid();

// From RFC 5280:
//
//     id-ce-certificatePolicies OBJECT IDENTIFIER ::=  { id-ce 32 }
//
// In dotted notation: 2.5.29.32
NET_EXPORT der::Input CertificatePoliciesOid();

// From RFC 5280:
//
//     id-ce-authorityKeyIdentifier OBJECT IDENTIFIER ::=  { id-ce 35 }
//
// In dotted notation: 2.5.29.35
NET_EXPORT der::Input AuthorityKeyIdentifierOid();

// From RFC 5280:
//
//     id-ce-policyConstraints OBJECT IDENTIFIER ::=  { id-ce 36 }
//
// In dotted notation: 2.5.29.36
NET_EXPORT der::Input PolicyConstraintsOid();

// From RFC 5280:
//
//     id-ce-extKeyUsage OBJECT IDENTIFIER ::= { id-ce 37 }
//
// In dotted notation: 2.5.29.37
NET_EXPORT der::Input ExtKeyUsageOid();

// From RFC 5280:
//
//     id-pe-authorityInfoAccess OBJECT IDENTIFIER ::= { id-pe 1 }
//
// In dotted notation: 1.3.6.1.5.5.7.1.1
NET_EXPORT der::Input AuthorityInfoAccessOid();

// From RFC 5280:
//
//     id-ad-caIssuers OBJECT IDENTIFIER ::= { id-ad 2 }
//
// In dotted notation: 1.3.6.1.5.5.7.48.2
NET_EXPORT der::Input AdCaIssuersOid();

// From RFC 5280:
//
//     id-ad-ocsp OBJECT IDENTIFIER ::= { id-ad 1 }
//
// In dotted notation: 1.3.6.1.5.5.7.48.1
NET_EXPORT der::Input AdOcspOid();

// From RFC 5280:
//
//     id-ce-cRLDistributionPoints OBJECT IDENTIFIER ::=  { id-ce 31 }
//
// In dotted notation: 2.5.29.31
NET_EXPORT der::Input CrlDistributionPointsOid();

// Parses the Extensions sequence as defined by RFC 5280. Extensions are added
// to the map |extensions| keyed by the OID. Parsing guarantees that each OID
// is unique. Note that certificate verification must consume each extension
// marked as critical.
//
// Returns true on success and fills |extensions|. The output will reference
// bytes in |extensions_tlv|, so that data must be kept alive.
// On failure |extensions| may be partially written to and should not be used.
NET_EXPORT bool ParseExtensions(
    const der::Input& extensions_tlv,
    std::map<der::Input, ParsedExtension>* extensions) WARN_UNUSED_RESULT;

// Removes the extension with OID |oid| from |unconsumed_extensions| and fills
// |extension| with the matching extension value. If there was no extension
// matching |oid| then returns |false|.
NET_EXPORT bool ConsumeExtension(
    const der::Input& oid,
    std::map<der::Input, ParsedExtension>* unconsumed_extensions,
    ParsedExtension* extension) WARN_UNUSED_RESULT;

struct ParsedBasicConstraints {
  bool is_ca = false;
  bool has_path_len = false;
  uint8_t path_len = 0;
};

// Parses the BasicConstraints extension as defined by RFC 5280:
//
//    BasicConstraints ::= SEQUENCE {
//         cA                      BOOLEAN DEFAULT FALSE,
//         pathLenConstraint       INTEGER (0..MAX) OPTIONAL }
//
// The maximum allowed value of pathLenConstraints will be whatever can fit
// into a uint8_t.
NET_EXPORT bool ParseBasicConstraints(const der::Input& basic_constraints_tlv,
                                      ParsedBasicConstraints* out)
    WARN_UNUSED_RESULT;

// KeyUsageBit contains the index for a particular key usage. The index is
// measured from the most significant bit of a bit string.
//
// From RFC 5280 section 4.2.1.3:
//
//     KeyUsage ::= BIT STRING {
//          digitalSignature        (0),
//          nonRepudiation          (1), -- recent editions of X.509 have
//                               -- renamed this bit to contentCommitment
//          keyEncipherment         (2),
//          dataEncipherment        (3),
//          keyAgreement            (4),
//          keyCertSign             (5),
//          cRLSign                 (6),
//          encipherOnly            (7),
//          decipherOnly            (8) }
enum KeyUsageBit {
  KEY_USAGE_BIT_DIGITAL_SIGNATURE = 0,
  KEY_USAGE_BIT_NON_REPUDIATION = 1,
  KEY_USAGE_BIT_KEY_ENCIPHERMENT = 2,
  KEY_USAGE_BIT_DATA_ENCIPHERMENT = 3,
  KEY_USAGE_BIT_KEY_AGREEMENT = 4,
  KEY_USAGE_BIT_KEY_CERT_SIGN = 5,
  KEY_USAGE_BIT_CRL_SIGN = 6,
  KEY_USAGE_BIT_ENCIPHER_ONLY = 7,
  KEY_USAGE_BIT_DECIPHER_ONLY = 8,
};

// Parses the KeyUsage extension as defined by RFC 5280. Returns true on
// success, and |key_usage| will alias data in |key_usage_tlv|. On failure
// returns false, and |key_usage| may have been modified.
//
// In addition to validating that key_usage_tlv is a BIT STRING, this does
// additional KeyUsage specific validations such as requiring at least 1 bit to
// be set.
//
// To test if a particular key usage is set, call, e.g.:
//     key_usage->AssertsBit(KEY_USAGE_BIT_DIGITAL_SIGNATURE);
NET_EXPORT bool ParseKeyUsage(const der::Input& key_usage_tlv,
                              der::BitString* key_usage) WARN_UNUSED_RESULT;

// Parses the Authority Information Access extension defined by RFC 5280.
// Returns true on success, and |out_ca_issuers_uris| and |out_ocsp_uris| will
// alias data in |authority_info_access_tlv|. On failure returns false, and
// |out_ca_issuers_uris| and |out_ocsp_uris| may have been partially filled.
//
// |out_ca_issuers_uris| is filled with the accessLocations of type
// uniformResourceIdentifier for the accessMethod id-ad-caIssuers.
// |out_ocsp_uris| is filled with the accessLocations of type
// uniformResourceIdentifier for the accessMethod id-ad-ocsp.
//
// The values in |out_ca_issuers_uris| and |out_ocsp_uris| are checked to be
// IA5String (ASCII strings), but no other validation is performed on them.
//
// accessMethods other than id-ad-caIssuers and id-ad-ocsp are silently ignored.
// accessLocation types other than uniformResourceIdentifier are silently
// ignored.
NET_EXPORT bool ParseAuthorityInfoAccess(
    const der::Input& authority_info_access_tlv,
    std::vector<base::StringPiece>* out_ca_issuers_uris,
    std::vector<base::StringPiece>* out_ocsp_uris) WARN_UNUSED_RESULT;

// ParsedDistributionPoint represents a parsed DistributionPoint from RFC 5280.
// It is simplified compared to that from RFC 5280 as it make assumptions about
// which OPTIONAL fields are present, and which CHOICEs are used.
//
//   DistributionPoint ::= SEQUENCE {
//    distributionPoint       [0]     DistributionPointName OPTIONAL,
//    reasons                 [1]     ReasonFlags OPTIONAL,
//    cRLIssuer               [2]     GeneralNames OPTIONAL }
struct NET_EXPORT ParsedDistributionPoint {
  ParsedDistributionPoint();
  ParsedDistributionPoint(ParsedDistributionPoint&& other);
  ~ParsedDistributionPoint();

  // The possibly-empty list of URIs from distributionPoint.
  std::vector<base::StringPiece> uris;

  // TODO(eroman): Include the actual cRLIssuer.
  bool has_crl_issuer = false;
};

// Parses the value of a CRL Distribution Points extension (sequence of
// DistributionPoint). Return true on success, and fills |distribution_points|
// with values that reference data in |distribution_points_tlv|.
//
// Some simplifications are made during parsing.
//
//  * Skips DistributionPoints that lack a "distributionPoint" (name) field.
//
//  * Skips DistributionPoints that contain a "reasons" field. This is
//    reasonable under RFC 5280's profile which requires that conforming CAs
//    "MUST include at least one DistributionPoint that points to a CRL that
//    covers the certificate for all reasons".
//
//  * Only parses URIs from the GeneralNames "distributionPoint". If the
//    DistributionPoint uses "nameRelativeToCRLIssuer" rather than "fullName" it
//    is skipped. And if "fullName" includes names ofther than
//    "uniformResourceIdentifier" they are also skipped.
NET_EXPORT bool ParseCrlDistributionPoints(
    const der::Input& distribution_points_tlv,
    std::vector<ParsedDistributionPoint>* distribution_points)
    WARN_UNUSED_RESULT;

// Represents the AuthorityKeyIdentifier extension defined by RFC 5280 section
// 4.2.1.1.
//
//    AuthorityKeyIdentifier ::= SEQUENCE {
//       keyIdentifier             [0] KeyIdentifier           OPTIONAL,
//       authorityCertIssuer       [1] GeneralNames            OPTIONAL,
//       authorityCertSerialNumber [2] CertificateSerialNumber OPTIONAL  }
//
//    KeyIdentifier ::= OCTET STRING
struct NET_EXPORT ParsedAuthorityKeyIdentifier {
  ParsedAuthorityKeyIdentifier();
  ~ParsedAuthorityKeyIdentifier();
  ParsedAuthorityKeyIdentifier(ParsedAuthorityKeyIdentifier&& other);
  ParsedAuthorityKeyIdentifier& operator=(ParsedAuthorityKeyIdentifier&& other);

  // The keyIdentifier, which is an OCTET STRING.
  base::Optional<der::Input> key_identifier;

  // The authorityCertIssuer, which should be a GeneralNames, but this is not
  // enforced by ParseAuthorityKeyIdentifier.
  base::Optional<der::Input> authority_cert_issuer;

  // The DER authorityCertSerialNumber, which should be a
  // CertificateSerialNumber (an INTEGER) but this is not enforced by
  // ParseAuthorityKeyIdentifier.
  base::Optional<der::Input> authority_cert_serial_number;
};

// Parses the value of an authorityKeyIdentifier extension. Returns true on
// success and fills |authority_key_identifier| with values that reference data
// in |extension_value|. On failure the state of |authority_key_identifier| is
// not guaranteed.
NET_EXPORT bool ParseAuthorityKeyIdentifier(
    const der::Input& extension_value,
    ParsedAuthorityKeyIdentifier* authority_key_identifier) WARN_UNUSED_RESULT;

// Parses the value of a subjectKeyIdentifier extension. Returns true on
// success and |subject_key_identifier| references data in |extension_value|.
// On failure the state of |subject_key_identifier| is not guaranteed.
NET_EXPORT bool ParseSubjectKeyIdentifier(const der::Input& extension_value,
                                          der::Input* subject_key_identifier)
    WARN_UNUSED_RESULT;

}  // namespace net

#endif  // NET_CERT_INTERNAL_PARSE_CERTIFICATE_H_
