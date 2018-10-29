// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "net/cert/internal/certificate_policies.h"

#include "net/cert/internal/cert_error_params.h"
#include "net/cert/internal/cert_errors.h"
#include "net/der/input.h"
#include "net/der/parse_values.h"
#include "net/der/parser.h"
#include "net/der/tag.h"

namespace net {

namespace {

// ---------------------------------------------------------------
// Errors
// ---------------------------------------------------------------

DEFINE_CERT_ERROR_ID(kPolicyQualifiersEmptySequence,
                     "The policy qualifiers SEQUENCE is empty");
DEFINE_CERT_ERROR_ID(kUnknownPolicyQualifierOid,
                     "Unknown policy qualifier OID (not CPS or User Notice)");
DEFINE_CERT_ERROR_ID(kPoliciesEmptySequence, "Policies is an empty SEQUENCE");
DEFINE_CERT_ERROR_ID(kPoliciesDuplicateOid, "Policies contains duplicate OIDs");
DEFINE_CERT_ERROR_ID(kPolicyInformationTrailingData,
                     "PolicyInformation has trailing data");
DEFINE_CERT_ERROR_ID(kFailedParsingPolicyQualifiers,
                     "Failed parsing policy qualifiers");
DEFINE_CERT_ERROR_ID(kMissingQualifier,
                     "PolicyQualifierInfo is missing qualifier");
DEFINE_CERT_ERROR_ID(kPolicyQualifierInfoTrailingData,
                     "PolicyQualifierInfo has trailing data");

// -- policyQualifierIds for Internet policy qualifiers
//
// id-qt          OBJECT IDENTIFIER ::=  { id-pkix 2 }
// id-qt-cps      OBJECT IDENTIFIER ::=  { id-qt 1 }
//
// In dotted decimal form: 1.3.6.1.5.5.7.2.1
const der::Input CpsPointerId() {
  static const uint8_t cps_pointer_id[] = {0x2b, 0x06, 0x01, 0x05,
                                           0x05, 0x07, 0x02, 0x01};
  return der::Input(cps_pointer_id);
}

// id-qt-unotice  OBJECT IDENTIFIER ::=  { id-qt 2 }
//
// In dotted decimal form: 1.3.6.1.5.5.7.2.2
const der::Input UserNoticeId() {
  static const uint8_t user_notice_id[] = {0x2b, 0x06, 0x01, 0x05,
                                           0x05, 0x07, 0x02, 0x02};
  return der::Input(user_notice_id);
}

// Ignores the policyQualifiers, but does some minimal correctness checking.
// TODO(mattm): parse and return the policyQualifiers, since the cert viewer
// still needs to display them.
bool ParsePolicyQualifiers(bool restrict_to_known_qualifiers,
                           der::Parser* policy_qualifiers_sequence_parser,
                           CertErrors* errors) {
  DCHECK(errors);

  // If it is present, the policyQualifiers sequence should have at least 1
  // element.
  //
  //      policyQualifiers   SEQUENCE SIZE (1..MAX) OF
  //                              PolicyQualifierInfo OPTIONAL }
  if (!policy_qualifiers_sequence_parser->HasMore()) {
    errors->AddError(kPolicyQualifiersEmptySequence);
    return false;
  }
  while (policy_qualifiers_sequence_parser->HasMore()) {
    // PolicyQualifierInfo ::= SEQUENCE {
    der::Parser policy_information_parser;
    if (!policy_qualifiers_sequence_parser->ReadSequence(
            &policy_information_parser)) {
      return false;
    }
    //      policyQualifierId  PolicyQualifierId,
    der::Input qualifier_oid;
    if (!policy_information_parser.ReadTag(der::kOid, &qualifier_oid))
      return false;
    if (restrict_to_known_qualifiers && qualifier_oid != CpsPointerId() &&
        qualifier_oid != UserNoticeId()) {
      errors->AddError(kUnknownPolicyQualifierOid,
                       CreateCertErrorParams1Der("oid", qualifier_oid));
      return false;
    }
    //      qualifier          ANY DEFINED BY policyQualifierId }
    der::Tag tag;
    der::Input value;
    if (!policy_information_parser.ReadTagAndValue(&tag, &value)) {
      errors->AddError(kMissingQualifier);
      return false;
    }
    // Should not have trailing data after qualifier.
    if (policy_information_parser.HasMore()) {
      errors->AddError(kPolicyQualifierInfoTrailingData);
      return false;
    }
  }
  return true;
}

}  // namespace

const der::Input AnyPolicy() {
  // id-ce OBJECT IDENTIFIER  ::=  {joint-iso-ccitt(2) ds(5) 29}
  //
  // id-ce-certificatePolicies OBJECT IDENTIFIER ::=  { id-ce 32 }
  //
  // anyPolicy OBJECT IDENTIFIER ::= { id-ce-certificatePolicies 0 }
  //
  // In dotted decimal form: 2.5.29.32.0
  static const uint8_t any_policy[] = {0x55, 0x1D, 0x20, 0x00};
  return der::Input(any_policy);
}

der::Input InhibitAnyPolicyOid() {
  // From RFC 5280:
  //
  //     id-ce-inhibitAnyPolicy OBJECT IDENTIFIER ::=  { id-ce 54 }
  //
  // In dotted notation: 2.5.29.54
  static const uint8_t oid[] = {0x55, 0x1d, 0x36};
  return der::Input(oid);
}

der::Input PolicyMappingsOid() {
  // From RFC 5280:
  //
  //     id-ce-policyMappings OBJECT IDENTIFIER ::=  { id-ce 33 }
  //
  // In dotted notation: 2.5.29.33
  static const uint8_t oid[] = {0x55, 0x1d, 0x21};
  return der::Input(oid);
}

// RFC 5280 section 4.2.1.4.  Certificate Policies:
//
// certificatePolicies ::= SEQUENCE SIZE (1..MAX) OF PolicyInformation
//
// PolicyInformation ::= SEQUENCE {
//      policyIdentifier   CertPolicyId,
//      policyQualifiers   SEQUENCE SIZE (1..MAX) OF
//                              PolicyQualifierInfo OPTIONAL }
//
// CertPolicyId ::= OBJECT IDENTIFIER
//
// PolicyQualifierInfo ::= SEQUENCE {
//      policyQualifierId  PolicyQualifierId,
//      qualifier          ANY DEFINED BY policyQualifierId }
//
// PolicyQualifierId ::= OBJECT IDENTIFIER ( id-qt-cps | id-qt-unotice )
//
// Qualifier ::= CHOICE {
//      cPSuri           CPSuri,
//      userNotice       UserNotice }
//
// CPSuri ::= IA5String
//
// UserNotice ::= SEQUENCE {
//      noticeRef        NoticeReference OPTIONAL,
//      explicitText     DisplayText OPTIONAL }
//
// NoticeReference ::= SEQUENCE {
//      organization     DisplayText,
//      noticeNumbers    SEQUENCE OF INTEGER }
//
// DisplayText ::= CHOICE {
//      ia5String        IA5String      (SIZE (1..200)),
//      visibleString    VisibleString  (SIZE (1..200)),
//      bmpString        BMPString      (SIZE (1..200)),
//      utf8String       UTF8String     (SIZE (1..200)) }
bool ParseCertificatePoliciesExtension(const der::Input& extension_value,
                                       bool fail_parsing_unknown_qualifier_oids,
                                       std::vector<der::Input>* policies,
                                       CertErrors* errors) {
  DCHECK(errors);
  // certificatePolicies ::= SEQUENCE SIZE (1..MAX) OF PolicyInformation
  der::Parser extension_parser(extension_value);
  der::Parser policies_sequence_parser;
  if (!extension_parser.ReadSequence(&policies_sequence_parser))
    return false;
  // Should not have trailing data after certificatePolicies sequence.
  if (extension_parser.HasMore())
    return false;
  // The certificatePolicies sequence should have at least 1 element.
  if (!policies_sequence_parser.HasMore()) {
    errors->AddError(kPoliciesEmptySequence);
    return false;
  }

  policies->clear();

  while (policies_sequence_parser.HasMore()) {
    // PolicyInformation ::= SEQUENCE {
    der::Parser policy_information_parser;
    if (!policies_sequence_parser.ReadSequence(&policy_information_parser))
      return false;
    //      policyIdentifier   CertPolicyId,
    der::Input policy_oid;
    if (!policy_information_parser.ReadTag(der::kOid, &policy_oid))
      return false;

    // Build the |policies| vector in sorted order (sorted on DER encoded policy
    // OID). Use a binary search to check whether a duplicate policy is present,
    // and if not, where to insert the policy to maintain the sorted order.
    auto i = std::lower_bound(policies->begin(), policies->end(), policy_oid);
    // RFC 5280 section 4.2.1.4: A certificate policy OID MUST NOT appear more
    // than once in a certificate policies extension.
    if (i != policies->end() && *i == policy_oid) {
      errors->AddError(kPoliciesDuplicateOid,
                       CreateCertErrorParams1Der("oid", policy_oid));
      return false;
    }

    policies->insert(i, policy_oid);

    if (!policy_information_parser.HasMore())
      continue;

    //      policyQualifiers   SEQUENCE SIZE (1..MAX) OF
    //                              PolicyQualifierInfo OPTIONAL }
    der::Parser policy_qualifiers_sequence_parser;
    if (!policy_information_parser.ReadSequence(
            &policy_qualifiers_sequence_parser)) {
      return false;
    }
    // Should not have trailing data after policyQualifiers sequence.
    if (policy_information_parser.HasMore()) {
      errors->AddError(kPolicyInformationTrailingData);
      return false;
    }

    // RFC 5280 section 4.2.1.4: When qualifiers are used with the special
    // policy anyPolicy, they MUST be limited to the qualifiers identified in
    // this section.
    if (!ParsePolicyQualifiers(
            fail_parsing_unknown_qualifier_oids || policy_oid == AnyPolicy(),
            &policy_qualifiers_sequence_parser, errors)) {
      errors->AddError(kFailedParsingPolicyQualifiers);
      return false;
    }
  }

  return true;
}

// From RFC 5280:
//
//   PolicyConstraints ::= SEQUENCE {
//        requireExplicitPolicy           [0] SkipCerts OPTIONAL,
//        inhibitPolicyMapping            [1] SkipCerts OPTIONAL }
//
//   SkipCerts ::= INTEGER (0..MAX)
bool ParsePolicyConstraints(const der::Input& policy_constraints_tlv,
                            ParsedPolicyConstraints* out) {
  der::Parser parser(policy_constraints_tlv);

  //   PolicyConstraints ::= SEQUENCE {
  der::Parser sequence_parser;
  if (!parser.ReadSequence(&sequence_parser))
    return false;

  // RFC 5280 prohibits CAs from issuing PolicyConstraints as an empty sequence:
  //
  //   Conforming CAs MUST NOT issue certificates where policy constraints
  //   is an empty sequence.  That is, either the inhibitPolicyMapping field
  //   or the requireExplicitPolicy field MUST be present.  The behavior of
  //   clients that encounter an empty policy constraints field is not
  //   addressed in this profile.
  if (!sequence_parser.HasMore())
    return false;

  der::Input value;
  if (!sequence_parser.ReadOptionalTag(der::ContextSpecificPrimitive(0), &value,
                                       &out->has_require_explicit_policy)) {
    return false;
  }

  if (out->has_require_explicit_policy) {
    if (!ParseUint8(value, &out->require_explicit_policy)) {
      // TODO(eroman): Surface reason for failure if length was longer than
      // uint8.
      return false;
    }
  } else {
    out->require_explicit_policy = 0;
  }

  if (!sequence_parser.ReadOptionalTag(der::ContextSpecificPrimitive(1), &value,
                                       &out->has_inhibit_policy_mapping)) {
    return false;
  }

  if (out->has_inhibit_policy_mapping) {
    if (!ParseUint8(value, &out->inhibit_policy_mapping)) {
      // TODO(eroman): Surface reason for failure if length was longer than
      // uint8.
      return false;
    }
  } else {
    out->inhibit_policy_mapping = 0;
  }

  // There should be no remaining data.
  if (sequence_parser.HasMore() || parser.HasMore())
    return false;

  return true;
}

// From RFC 5280:
//
//   InhibitAnyPolicy ::= SkipCerts
//
//   SkipCerts ::= INTEGER (0..MAX)
bool ParseInhibitAnyPolicy(const der::Input& inhibit_any_policy_tlv,
                           uint8_t* num_certs) {
  der::Parser parser(inhibit_any_policy_tlv);

  // TODO(eroman): Surface reason for failure if length was longer than uint8.
  if (!parser.ReadUint8(num_certs))
    return false;

  // There should be no remaining data.
  if (parser.HasMore())
    return false;

  return true;
}

// From RFC 5280:
//
//   PolicyMappings ::= SEQUENCE SIZE (1..MAX) OF SEQUENCE {
//        issuerDomainPolicy      CertPolicyId,
//        subjectDomainPolicy     CertPolicyId }
bool ParsePolicyMappings(const der::Input& policy_mappings_tlv,
                         std::vector<ParsedPolicyMapping>* mappings) {
  mappings->clear();

  der::Parser parser(policy_mappings_tlv);

  //   PolicyMappings ::= SEQUENCE SIZE (1..MAX) OF SEQUENCE {
  der::Parser sequence_parser;
  if (!parser.ReadSequence(&sequence_parser))
    return false;

  // Must be at least 1 mapping.
  if (!sequence_parser.HasMore())
    return false;

  while (sequence_parser.HasMore()) {
    der::Parser mapping_parser;
    if (!sequence_parser.ReadSequence(&mapping_parser))
      return false;

    ParsedPolicyMapping mapping;
    if (!mapping_parser.ReadTag(der::kOid, &mapping.issuer_domain_policy))
      return false;
    if (!mapping_parser.ReadTag(der::kOid, &mapping.subject_domain_policy))
      return false;

    // There shouldn't be extra unconsumed data.
    if (mapping_parser.HasMore())
      return false;

    mappings->push_back(mapping);
  }

  // There shouldn't be extra unconsumed data.
  if (parser.HasMore())
    return false;

  return true;
}

}  // namespace net
