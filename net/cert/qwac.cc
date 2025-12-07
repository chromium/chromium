// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/qwac.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "third_party/boringssl/src/pki/parser.h"

namespace net {

// RFC 3739 A.1:
//
//   QCStatements ::= SEQUENCE OF QCStatement
//
//   QCStatement ::= SEQUENCE {
//       statementId        OBJECT IDENTIFIER,
//       statementInfo      ANY DEFINED BY statementId OPTIONAL}
std::optional<std::vector<QcStatement>> ParseQcStatements(
    bssl::der::Input extension_value) {
  std::vector<QcStatement> results;

  bssl::der::Parser parser(extension_value);
  bssl::der::Parser statements_parser;
  if (!parser.ReadSequence(&statements_parser)) {
    return std::nullopt;
  }

  while (statements_parser.HasMore()) {
    bssl::der::Parser statement_parser;
    if (!statements_parser.ReadSequence(&statement_parser)) {
      return std::nullopt;
    }
    bssl::der::Input statement_id;
    if (!statement_parser.ReadTag(CBS_ASN1_OBJECT, &statement_id)) {
      return std::nullopt;
    }
    bssl::der::Input statement_info;
    if (statement_parser.HasMore()) {
      if (!statement_parser.ReadRawTLV(&statement_info)) {
        return std::nullopt;
      }
    }
    if (statement_parser.HasMore()) {
      return std::nullopt;
    }
    results.emplace_back(statement_id, statement_info);
  }

  return results;
}

std::optional<std::vector<bssl::der::Input>> ParseQcTypeInfo(
    bssl::der::Input statement_info) {
  // QcType::= SEQUENCE OF OBJECT IDENTIFIER (id-etsi-qct-esign |
  //     id-etsi-qct-eseal | id-etsi-qct-web, ...)
  bssl::der::Parser info_parser(statement_info);
  bssl::der::Parser qctype_parser;
  if (!info_parser.ReadSequence(&qctype_parser)) {
    return std::nullopt;
  }
  std::vector<bssl::der::Input> results;
  while (qctype_parser.HasMore()) {
    bssl::der::Input qctype_id;
    if (!qctype_parser.ReadTag(CBS_ASN1_OBJECT, &qctype_id)) {
      return std::nullopt;
    }
    results.push_back(qctype_id);
  }
  if (info_parser.HasMore()) {
    return std::nullopt;
  }
  return results;
}

QwacQcStatementsStatus HasQwacQcStatements(
    const std::vector<QcStatement>& qc_statements) {
  // ETSI TS 119 411-5 - V2.1.1 - section 6.1.2:
  //   the QWAC includes QCStatements as specified in clause 4.2 of ETSI EN 319
  //   412-4 [4]
  //
  // ETSI EN 319 412-4 - V1.3.2 - section 4.2:
  //   QCS-4.2-1: When certificates are issued as EU Qualified Certificates,
  //   they shall include QCStatements as specified in clauses 4 and 5 of ETSI
  //   EN 319 412-5 [1].
  //
  // ETSI EN 319 412-5 - V2.4.1 - section 5:
  //   clause 4.2.1, statement esi4-qcStatement-1, is Mandatory
  //
  // ETSI EN 319 412-5 - V2.4.1 - section 4.2.1:
  //   esi4-qcStatement-1 QC-STATEMENT ::=
  //     { IDENTIFIED BY id-etsi-qcs-QcCompliance }
  //   id-etsi-qcs-QcCompliance OBJECT IDENTIFIER ::= { id-etsi-qcs 1 }
  //   The precise meaning of this statement is enhanced by:
  //     a) the QC type statement defined in clause 4.2.3 according to table 1
  //
  // ETSI EN 319 412-5 - V2.4.1 - section 4.2.3:
  //   This QCStatement declares that a certificate is issued as one and only
  //   one of the purposes of electronic signature, electronic seal or web site
  //   authentication
  //   ...
  //   id-etsi-qct-web OBJECT IDENTIFIER ::= { id-etsi-qcs-QcType 3 }
  //   -- Certificate for website authentication as defined in Regulation (EU)
  //      No 910/2014
  bool has_qc_compliance = false;
  bool has_qctype_web = false;
  for (const auto& statement : qc_statements) {
    if (statement.id == bssl::der::Input(kEtsiQcsQcComplianceOid)) {
      has_qc_compliance = true;
    } else if (statement.id == bssl::der::Input(kEtsiQcsQcTypeOid)) {
      std::optional<std::vector<bssl::der::Input>> qc_types =
          ParseQcTypeInfo(statement.info);
      if (!qc_types.has_value()) {
        return QwacQcStatementsStatus::kNotQwac;
      }
      for (const auto& qc_type_id : qc_types.value()) {
        if (qc_type_id == bssl::der::Input(kEtsiQctWebOid)) {
          has_qctype_web = true;
        }
      }
    }
  }

  if (has_qc_compliance && has_qctype_web) {
    return QwacQcStatementsStatus::kHasQwacStatements;
  } else if (has_qc_compliance || has_qctype_web) {
    return QwacQcStatementsStatus::kInconsistent;
  }
  return QwacQcStatementsStatus::kNotQwac;
}

QwacPoliciesStatus Has1QwacPolicies(
    const std::set<bssl::der::Input>& policy_set) {
  // ETSI TS 119 411-5 - V2.1.1 - section 4.1.1:
  //   The 1-QWAC certificate shall be issued in accordance with one of the
  //   following certificate policies as specified in ETSI EN 319 411-2 [3]:
  //     a)QEVCP-w; or
  //     b)QNCP-w.
  //
  // ETSI EN 319 411-2 - V2.6.0 - section 4.2.2:
  //   5) A policy for EU qualified website certificates (QEVCP-w) that
  //   conforms to the latest version of EVCG [i.7], offering at a minimum the
  //   "Extended Validated" level of assurance as defined by the CA/Browser
  //   Forum, and the level of quality defined in Regulation (EU) No 910/2014
  //   [i.1] for EU qualified certificates used in support of websites
  //   authentication
  //
  //   6) A policy for EU qualified website certificates (QNCP-w) that conforms
  //   to the latest version of BRG [i.3], offering at a minimum the
  //   "Organization Validated" or "Individual Validated" level of assurance as
  //   defined by the CA/Browser Forum and the level of quality defined in
  //   Regulation (EU) No 910/2014 [i.1] for EU qualified certificates used in
  //   support of websites authentication

  const bool has_ev = policy_set.contains(bssl::der::Input(kCabfBrEvOid));
  const bool has_iv = policy_set.contains(bssl::der::Input(kCabfBrIvOid));
  const bool has_ov = policy_set.contains(bssl::der::Input(kCabfBrOvOid));

  const bool has_qevcpw = policy_set.contains(bssl::der::Input(kQevcpwOid));
  const bool has_qncpw = policy_set.contains(bssl::der::Input(kQncpwOid));

  if (has_ev && has_qevcpw) {
    return QwacPoliciesStatus::kHasQwacPolicies;
  } else if ((has_ov || has_iv) && has_qncpw) {
    return QwacPoliciesStatus::kHasQwacPolicies;
  } else if (has_qevcpw || has_qncpw) {
    return QwacPoliciesStatus::kInconsistent;
  }
  return QwacPoliciesStatus::kNotQwac;
}

QwacPoliciesStatus Has2QwacPolicies(
    const std::set<bssl::der::Input>& policy_set) {
  // ETSI TS 119 411-5 V2.1.1 - 4.2.1:
  // The 2-QWAC certificate shall be issued in accordance with the QNCP-w-gen
  // certificate policy
  //
  // ETSI EN 319 411-2 - V2.6.1 - section 4.2.2:
  // A policy for EU qualified website certificates (QNCP-w-gen) offering the
  // level of quality defined in Regulation (EU) No 910/2014 [i.1] for EU
  // qualified certificates used in support of websites authentication for
  // general purpose certificate for qualified website authentication
  return policy_set.contains(bssl::der::Input(kQncpwgenOid))
             ? QwacPoliciesStatus::kHasQwacPolicies
             : QwacPoliciesStatus::kNotQwac;
}

QwacEkuStatus Has2QwacEku(const bssl::ParsedCertificate* cert) {
  // ETSI TS 119 411-5 V2.1.1 - 4.2.2:
  // the extKeyUsage value shall only assert the extendedKeyUsage purpose of
  // id-kp-tls-binding as specified in Annex A.
  if (!cert->has_extended_key_usage()) {
    return QwacEkuStatus::kNotQwac;
  }
  if (!base::Contains(cert->extended_key_usage(),
                      bssl::der::Input(kIdKpTlsBinding))) {
    return QwacEkuStatus::kNotQwac;
  }
  if (cert->extended_key_usage().size() != 1) {
    return QwacEkuStatus::kInconsistent;
    ;
  }
  return QwacEkuStatus::kHasQwacEku;
}

}  // namespace net
