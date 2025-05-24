// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_QWAC_H_
#define NET_CERT_QWAC_H_

#include <stdint.h>

#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "net/base/net_export.h"
#include "third_party/boringssl/src/pki/input.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"

namespace net {

// Note: some things in this file aren't really QWAC specific, but are just
// here since QWAC processing is the only place they are used currently. They
// could be moved somewhere else later if necessary.

// https://cabforum.org/resources/object-registry/
//
// extended-validation(1) - 2.23.140.1.1
inline constexpr uint8_t kCabfBrEvOid[] = {0x67, 0x81, 0x0c, 0x01, 0x01};

// organization-validated(2) - 2.23.140.1.2.2
inline constexpr uint8_t kCabfBrOvOid[] = {0x67, 0x81, 0x0c, 0x01, 0x02, 0x02};

// individual-validated(3) - 2.23.140.1.2.3
inline constexpr uint8_t kCabfBrIvOid[] = {0x67, 0x81, 0x0c, 0x01, 0x02, 0x03};

// ETSI EN 319 411-2 - V2.6.0 - 5.3.e:
// QEVCP-w: itu-t(0) identified-organization(4) etsi(0)
//     qualified-certificate-policies(194112) policy-identifiers(1) qcp-web (4)
// which is 0.4.0.194112.1.4
inline constexpr uint8_t kQevcpwOid[] = {0x04, 0x00, 0x8b, 0xec,
                                         0x40, 0x01, 0x04};

// ETSI EN 319 411-2 - V2.6.0 - 5.3.f:
// QNCP-w: itu-t(0) identified-organization(4) etsi(0)
//     qualified-certificate-policies(194112) policy-identifiers(1) qncp-web (5)
// which is 0.4.0.194112.1.5
inline constexpr uint8_t kQncpwOid[] = {0x04, 0x00, 0x8b, 0xec,
                                        0x40, 0x01, 0x05};

// ETSI EN 319 411-2 - V2.6.1 - 5.3.g:
// QNCP-w-gen: itu-t(0) identified-organization(4) etsi(0)
//     qualified-certificate-policies(194112) policy-identifiers(1)
//     qncp-web-gen (6)
// which is 0.4.0.194112.1.6
inline constexpr uint8_t kQncpwgenOid[] = {0x04, 0x00, 0x8b, 0xec,
                                           0x40, 0x01, 0x06};

// ETSI TS 119 411-5 V2.1.1 - Annex A:
// id-tlsBinding OBJECT IDENTIFIER ::= { itu-t(0) identified-organization(4)
//     etsi(0) id-qwacImplementation(194115) tls-binding (1) }
// id-kp-tls-binding OBJECT IDENTIFIER ::= { id-tlsBinding
//     id-kp-tls-binding(0) }
// which is 0.4.0.194115.1.0
inline constexpr uint8_t kIdKpTlsBinding[] = {0x04, 0x00, 0x8b, 0xec,
                                              0x43, 0x01, 0x00};

// RFC 7299 section 2:
// id-pkix OBJECT IDENTIFIER ::= { iso(1) identified-organization(3)
//                 dod(6) internet(1) security(5) mechanisms(5) pkix(7) }
// id-pe   OBJECT IDENTIFIER ::= { id-pkix 1 }
//
// RFC 3739 appendix A.2:
// id-pe-qcStatements     OBJECT IDENTIFIER ::= { id-pe 3 }
// which is 1.3.6.1.5.5.7.1.3
inline constexpr uint8_t kQcStatementsOid[] = {0x2b, 0x06, 0x01, 0x05,
                                               0x05, 0x07, 0x01, 0x03};

// ETSI EN 319 412-5 Annex B:
//
// id-etsi-qcs OBJECT IDENTIFIER ::=
//    { itu-t(0) identified-organization(4) etsi(0) id-qc-profile(1862) 1 }
//
// id-etsi-qcs-QcCompliance OBJECT IDENTIFIER ::= { id-etsi-qcs 1 }
// which is 0.4.0.1862.1.1
inline constexpr uint8_t kEtsiQcsQcComplianceOid[] = {0x04, 0x00, 0x8e,
                                                      0x46, 0x01, 0x01};

// id-etsi-qcs-QcType OBJECT IDENTIFIER ::= { id-etsi-qcs 6 }
// which is 0.4.0.1862.1.6
inline constexpr uint8_t kEtsiQcsQcTypeOid[] = {0x04, 0x00, 0x8e,
                                                0x46, 0x01, 0x06};

// id-etsi-qct-web OBJECT IDENTIFIER ::= { id-etsi-qcs-QcType 3 }
// which is 0.4.0.1862.1.6.3
inline constexpr uint8_t kEtsiQctWebOid[] = {0x04, 0x00, 0x8e, 0x46,
                                             0x01, 0x06, 0x03};

struct NET_EXPORT QcStatement {
  // The statementId OID value as DER bytes. Does not include tag&length.
  bssl::der::Input id;

  // The raw bytes of statementInfo.
  bssl::der::Input info;
};

// Parses a QcStatements extension as specified in RFC 3739. Returns nullopt if
// parsing failed.
// Each pair in the vector contains a statementId object identifier and the
// optional statementInfo if present. The statementInfo is returned as the raw
// DER bytes of the statementInfo value and the caller is responsible for
// parsing it as defined by the corresponding statementId.
NET_EXPORT
std::optional<std::vector<QcStatement>> ParseQcStatements(
    bssl::der::Input extension_value);

// Parses the statementInfo of a etsi-qcs-QcType statement. Returns a vector of
// the OID values, or nullopt on error.
NET_EXPORT std::optional<std::vector<bssl::der::Input>> ParseQcTypeInfo(
    bssl::der::Input statement_info);

enum class QwacQcStatementsStatus {
  kNotQwac,
  kInconsistent,
  kHasQwacStatements,
};
// Returns kHasQwacStatements if the given QcStatements extension (as returned
// by ParseQcStatements) indicates the certificate is a QWAC.
NET_EXPORT_PRIVATE QwacQcStatementsStatus
HasQwacQcStatements(const std::vector<QcStatement>& qc_statements);

enum class QwacPoliciesStatus {
  kNotQwac,
  kInconsistent,
  kHasQwacPolicies,
};

// Returns kHasQwacPolicies if the set of policy OIDs contains a suitable
// combination of policies to be a 1-QWAC.
NET_EXPORT_PRIVATE QwacPoliciesStatus
Has1QwacPolicies(const std::set<bssl::der::Input>& policy_set);

// Returns kHasQwacPolicies if the set of policy OIDs contains a suitable
// combination of policies to be a 2-QWAC.
NET_EXPORT_PRIVATE QwacPoliciesStatus
Has2QwacPolicies(const std::set<bssl::der::Input>& policy_set);

enum class QwacEkuStatus {
  kNotQwac,
  kInconsistent,
  kHasQwacEku,
};

// Returns kHasQwacEku if the set of policy EKUs is suitable to be a 2-QWAC.
NET_EXPORT_PRIVATE QwacEkuStatus
Has2QwacEku(const bssl::ParsedCertificate* cert);

}  // namespace net

#endif  // NET_CERT_QWAC_H_
