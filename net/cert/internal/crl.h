// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_CRL_H_
#define NET_CERT_INTERNAL_CRL_H_

#include "base/optional.h"
#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/cert/internal/general_names.h"
#include "net/cert/internal/parsed_certificate.h"
#include "net/der/input.h"
#include "net/der/parse_values.h"

namespace net {

struct ParsedCrlTbsCertList;
struct ParsedDistributionPoint;

// TODO(https://crbug.com/749276): This is the same enum with the same meaning
// as OCSPRevocationStatus, maybe they should be merged?
enum class CRLRevocationStatus {
  GOOD = 0,
  REVOKED = 1,
  UNKNOWN = 2,
  MAX_VALUE = UNKNOWN
};

// Parses a DER-encoded CRL "CertificateList" as specified by RFC 5280 Section
// 5.1. Returns true on success and sets the results in the |out_*| parameters.
// The contents of the output data is not validated.
//
// Note that on success the out parameters alias data from the input |crl_tlv|.
// Hence the output values are only valid as long as |crl_tlv| remains valid.
//
// On failure the out parameters have an undefined state. Some of them may have
// been updated during parsing, whereas others may not have been changed.
//
//    CertificateList  ::=  SEQUENCE  {
//         tbsCertList          TBSCertList,
//         signatureAlgorithm   AlgorithmIdentifier,
//         signatureValue       BIT STRING  }
NET_EXPORT_PRIVATE bool ParseCrlCertificateList(
    const der::Input& crl_tlv,
    der::Input* out_tbs_cert_list_tlv,
    der::Input* out_signature_algorithm_tlv,
    der::BitString* out_signature_value) WARN_UNUSED_RESULT;

// Parses a DER-encoded "TBSCertList" as specified by RFC 5280 Section 5.1.
// Returns true on success and sets the results in |out|.
//
// Note that on success |out| aliases data from the input |tbs_tlv|.
// Hence the fields of the ParsedCrlTbsCertList are only valid as long as
// |tbs_tlv| remains valid.
//
// On failure |out| has an undefined state. Some of its fields may have been
// updated during parsing, whereas others may not have been changed.
//
// Refer to the per-field documentation of ParsedCrlTbsCertList for details on
// what validity checks parsing performs.
//
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
NET_EXPORT_PRIVATE bool ParseCrlTbsCertList(const der::Input& tbs_tlv,
                                            ParsedCrlTbsCertList* out)
    WARN_UNUSED_RESULT;

// Represents a CRL "Version" from RFC 5280. TBSCertList reuses the same
// Version definition from TBSCertificate, however only v1(not present) and
// v2(1) are valid values, so a unique enum is used to avoid confusion.
enum class CrlVersion {
  V1,
  V2,
};

// Corresponds with "TBSCertList" from RFC 5280 Section 5.1:
struct NET_EXPORT_PRIVATE ParsedCrlTbsCertList {
  ParsedCrlTbsCertList();
  ~ParsedCrlTbsCertList();

  //         version                 Version OPTIONAL,
  //                                      -- if present, MUST be v2
  //
  // Parsing guarantees that the version is one of v1 or v2.
  CrlVersion version = CrlVersion::V1;

  //         signature               AlgorithmIdentifier,
  //
  // This contains the full (unverified) Tag-Length-Value for a SEQUENCE. No
  // guarantees are made regarding the value of this SEQUENCE.
  //
  // This can be further parsed using SignatureValue::Create().
  der::Input signature_algorithm_tlv;

  //         issuer               Name,
  //
  // This contains the full (unverified) Tag-Length-Value for a SEQUENCE. No
  // guarantees are made regarding the value of this SEQUENCE.
  der::Input issuer_tlv;

  //         thisUpdate              Time,
  //         nextUpdate              Time OPTIONAL,
  //
  // Parsing guarantees that thisUpdate and nextUpdate(if present) are valid
  // DER-encoded dates, however it DOES NOT guarantee anything about their
  // values. For instance notAfter could be before notBefore, or the dates
  // could indicate an expired CRL.
  der::GeneralizedTime this_update;
  base::Optional<der::GeneralizedTime> next_update;

  //         revokedCertificates     SEQUENCE OF SEQUENCE  {
  //              userCertificate         CertificateSerialNumber,
  //              revocationDate          Time,
  //              crlEntryExtensions      Extensions OPTIONAL
  //                                       -- if present, version MUST be v2
  //                                   }  OPTIONAL,
  //
  // This contains the full (unverified) Tag-Length-Value for a SEQUENCE. No
  // guarantees are made regarding the value of this SEQUENCE.
  base::Optional<der::Input> revoked_certificates_tlv;

  //         crlExtensions           [0]  EXPLICIT Extensions OPTIONAL
  //                                       -- if present, version MUST be v2
  //
  // This contains the full (unverified) Tag-Length-Value for a SEQUENCE. No
  // guarantees are made regarding the value of this SEQUENCE. (Note that the
  // EXPLICIT outer tag is stripped.)
  //
  // Parsing guarantees that if extensions is present the version is v2.
  base::Optional<der::Input> crl_extensions_tlv;
};

// Represents the IssuingDistributionPoint certificate type constraints:
enum class ContainedCertsType {
  // Neither onlyContainsUserCerts or onlyContainsCACerts was present.
  ANY_CERTS,
  // onlyContainsUserCerts      [1] BOOLEAN DEFAULT FALSE,
  USER_CERTS,
  // onlyContainsCACerts        [2] BOOLEAN DEFAULT FALSE,
  CA_CERTS,
};

// Parses a DER-encoded IssuingDistributionPoint extension value.
// Returns true on success and sets the results in the |out_*| parameters.
//
// If the IssuingDistributionPoint contains a distributionPoint fullName field,
// |out_distribution_point_names| will contain the parsed representation.
// If the distributionPoint type is nameRelativeToCRLIssuer, parsing will fail.
//
// |out_only_contains_cert_type| will contain the logical representation of the
// onlyContainsUserCerts and onlyContainsCACerts fields (or their absence).
//
// indirectCRL and onlyContainsAttributeCerts are not supported and parsing will
// fail if they are present.
//
// Note that on success |out_distribution_point_names| aliases data from the
// input |extension_value|.
//
// On failure the |out_*| parameters have undefined state.
//
// IssuingDistributionPoint ::= SEQUENCE {
//     distributionPoint          [0] DistributionPointName OPTIONAL,
//     onlyContainsUserCerts      [1] BOOLEAN DEFAULT FALSE,
//     onlyContainsCACerts        [2] BOOLEAN DEFAULT FALSE,
//     onlySomeReasons            [3] ReasonFlags OPTIONAL,
//     indirectCRL                [4] BOOLEAN DEFAULT FALSE,
//     onlyContainsAttributeCerts [5] BOOLEAN DEFAULT FALSE }
NET_EXPORT_PRIVATE bool ParseIssuingDistributionPoint(
    const der::Input& extension_value,
    std::unique_ptr<GeneralNames>* out_distribution_point_names,
    ContainedCertsType* out_only_contains_cert_type) WARN_UNUSED_RESULT;

NET_EXPORT_PRIVATE CRLRevocationStatus
GetCRLStatusForCert(const der::Input& cert_serial,
                    CrlVersion crl_version,
                    const base::Optional<der::Input>& revoked_certificates_tlv);

// Checks the revocation status of the certificate |cert| by using the
// DER-encoded |raw_crl|. |cert| must already have passed certificate path
// validation.
//
// Returns GOOD if the CRL indicates the certificate is not revoked,
// REVOKED if it indicates it is revoked, or UNKNOWN for all other cases.
//
//  * |raw_crl|: A DER encoded CRL CertificateList.
//  * |valid_chain|: The validated certificate chain containing the target cert.
//  * |target_cert_index|: The index into |valid_chain| of the certificate being
//        checked for revocation.
//  * |cert_dp|: The distribution point from the target certificate's CRL
//        distribution points extension that |raw_crl| corresponds to. If
//        |raw_crl| was not specified in a distribution point, the caller must
//        synthesize a ParsedDistributionPoint object as specified by RFC 5280
//        6.3.3.
//  * |verify_time|: The time to use when checking revocation status.
//  * |max_age|: The maximum age for a CRL, implemented as time since
//        the |thisUpdate| field in the CRL TBSCertList. Responses older than
//        |max_age| will be considered invalid.
NET_EXPORT CRLRevocationStatus
CheckCRL(base::StringPiece raw_crl,
         const ParsedCertificateList& valid_chain,
         size_t target_cert_index,
         const ParsedDistributionPoint& cert_dp,
         const base::Time& verify_time,
         const base::TimeDelta& max_age) WARN_UNUSED_RESULT;

}  // namespace net

#endif  // NET_CERT_INTERNAL_CRL_H_
