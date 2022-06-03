// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/name_constraints.h"

#include <limits.h>

#include <memory>

#include "base/check.h"
#include "base/numerics/clamped_math.h"
#include "base/strings/string_util.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/common_cert_errors.h"
#include "net/cert/internal/verify_name_match.h"
#include "net/der/input.h"
#include "net/der/parser.h"
#include "net/der/tag.h"

namespace net {

namespace {

// The name types of GeneralName that are fully supported in name constraints.
//
// (The other types will have the minimal checking described by RFC 5280
// section 4.2.1.10: If a name constraints extension that is marked as critical
// imposes constraints on a particular name form, and an instance of
// that name form appears in the subject field or subjectAltName
// extension of a subsequent certificate, then the application MUST
// either process the constraint or reject the certificate.)
const int kSupportedNameTypes = GENERAL_NAME_DNS_NAME |
                                GENERAL_NAME_DIRECTORY_NAME |
                                GENERAL_NAME_IP_ADDRESS;

// Controls wildcard handling of DNSNameMatches.
// If WildcardMatchType is WILDCARD_PARTIAL_MATCH "*.bar.com" is considered to
// match the constraint "foo.bar.com". If it is WILDCARD_FULL_MATCH, "*.bar.com"
// will match "bar.com" but not "foo.bar.com".
enum WildcardMatchType { WILDCARD_PARTIAL_MATCH, WILDCARD_FULL_MATCH };

// Returns true if |name| falls in the subtree defined by |dns_constraint|.
// RFC 5280 section 4.2.1.10:
// DNS name restrictions are expressed as host.example.com. Any DNS
// name that can be constructed by simply adding zero or more labels
// to the left-hand side of the name satisfies the name constraint. For
// example, www.host.example.com would satisfy the constraint but
// host1.example.com would not.
//
// |wildcard_matching| controls handling of wildcard names (|name| starts with
// "*."). Wildcard handling is not specified by RFC 5280, but certificate
// verification allows it, name constraints must check it similarly.
bool DNSNameMatches(base::StringPiece name,
                    base::StringPiece dns_constraint,
                    WildcardMatchType wildcard_matching) {
  // Everything matches the empty DNS name constraint.
  if (dns_constraint.empty())
    return true;

  // Normalize absolute DNS names by removing the trailing dot, if any.
  if (!name.empty() && *name.rbegin() == '.')
    name.remove_suffix(1);
  if (!dns_constraint.empty() && *dns_constraint.rbegin() == '.')
    dns_constraint.remove_suffix(1);

  // Wildcard partial-match handling ("*.bar.com" matching name constraint
  // "foo.bar.com"). This only handles the case where the the dnsname and the
  // constraint match after removing the leftmost label, otherwise it is handled
  // by falling through to the check of whether the dnsname is fully within or
  // fully outside of the constraint.
  if (wildcard_matching == WILDCARD_PARTIAL_MATCH && name.size() > 2 &&
      name[0] == '*' && name[1] == '.') {
    size_t dns_constraint_dot_pos = dns_constraint.find('.');
    if (dns_constraint_dot_pos != std::string::npos) {
      base::StringPiece dns_constraint_domain(
          dns_constraint.begin() + dns_constraint_dot_pos + 1,
          dns_constraint.size() - dns_constraint_dot_pos - 1);
      base::StringPiece wildcard_domain(name.begin() + 2, name.size() - 2);
      if (base::EqualsCaseInsensitiveASCII(wildcard_domain,
                                           dns_constraint_domain)) {
        return true;
      }
    }
  }

  if (!base::EndsWith(name, dns_constraint,
                      base::CompareCase::INSENSITIVE_ASCII)) {
    return false;
  }
  // Exact match.
  if (name.size() == dns_constraint.size())
    return true;
  // If dNSName constraint starts with a dot, only subdomains should match.
  // (e.g., "foo.bar.com" matches constraint ".bar.com", but "bar.com" doesn't.)
  // RFC 5280 is ambiguous, but this matches the behavior of other platforms.
  if (!dns_constraint.empty() && dns_constraint[0] == '.')
    dns_constraint.remove_prefix(1);
  // Subtree match.
  if (name.size() > dns_constraint.size() &&
      name[name.size() - dns_constraint.size() - 1] == '.') {
    return true;
  }
  // Trailing text matches, but not in a subtree (e.g., "foobar.com" is not a
  // match for "bar.com").
  return false;
}

// Parses a GeneralSubtrees |value| and store the contents in |subtrees|.
// The individual values stored into |subtrees| are not validated by this
// function.
// NOTE: |subtrees| is not pre-initialized by the function(it is expected to be
// a default initialized object), and it will be modified regardless of the
// return value.
WARN_UNUSED_RESULT bool ParseGeneralSubtrees(const der::Input& value,
                                             GeneralNames* subtrees,
                                             CertErrors* errors) {
  DCHECK(errors);

  // GeneralSubtrees ::= SEQUENCE SIZE (1..MAX) OF GeneralSubtree
  //
  // GeneralSubtree ::= SEQUENCE {
  //      base                    GeneralName,
  //      minimum         [0]     BaseDistance DEFAULT 0,
  //      maximum         [1]     BaseDistance OPTIONAL }
  //
  // BaseDistance ::= INTEGER (0..MAX)
  der::Parser sequence_parser(value);
  // The GeneralSubtrees sequence should have at least 1 element.
  if (!sequence_parser.HasMore())
    return false;
  while (sequence_parser.HasMore()) {
    der::Parser subtree_sequence;
    if (!sequence_parser.ReadSequence(&subtree_sequence))
      return false;

    der::Input raw_general_name;
    if (!subtree_sequence.ReadRawTLV(&raw_general_name))
      return false;

    if (!ParseGeneralName(
            raw_general_name,
            GeneralNames::IP_ADDRESS_AND_NETMASK, subtrees, errors)) {
      errors->AddError(kFailedParsingGeneralName);
      return false;
    }

    // RFC 5280 section 4.2.1.10:
    // Within this profile, the minimum and maximum fields are not used with any
    // name forms, thus, the minimum MUST be zero, and maximum MUST be absent.
    // However, if an application encounters a critical name constraints
    // extension that specifies other values for minimum or maximum for a name
    // form that appears in a subsequent certificate, the application MUST
    // either process these fields or reject the certificate.

    // Note that technically failing here isn't required: rather only need to
    // fail if a name of this type actually appears in a subsequent cert and
    // this extension was marked critical. However the minimum and maximum
    // fields appear uncommon enough that implementing that isn't useful.
    if (subtree_sequence.HasMore())
      return false;
  }
  return true;
}

}  // namespace

NameConstraints::~NameConstraints() = default;

// static
std::unique_ptr<NameConstraints> NameConstraints::Create(
    const der::Input& extension_value,
    bool is_critical,
    CertErrors* errors) {
  DCHECK(errors);

  std::unique_ptr<NameConstraints> name_constraints(new NameConstraints());
  if (!name_constraints->Parse(extension_value, is_critical, errors))
    return nullptr;
  return name_constraints;
}

bool NameConstraints::Parse(const der::Input& extension_value,
                            bool is_critical,
                            CertErrors* errors) {
  DCHECK(errors);

  der::Parser extension_parser(extension_value);
  der::Parser sequence_parser;

  // NameConstraints ::= SEQUENCE {
  //      permittedSubtrees       [0]     GeneralSubtrees OPTIONAL,
  //      excludedSubtrees        [1]     GeneralSubtrees OPTIONAL }
  if (!extension_parser.ReadSequence(&sequence_parser))
    return false;
  if (extension_parser.HasMore())
    return false;

  bool had_permitted_subtrees = false;
  der::Input permitted_subtrees_value;
  if (!sequence_parser.ReadOptionalTag(der::ContextSpecificConstructed(0),
                                       &permitted_subtrees_value,
                                       &had_permitted_subtrees)) {
    return false;
  }
  if (had_permitted_subtrees &&
      !ParseGeneralSubtrees(permitted_subtrees_value, &permitted_subtrees_,
                            errors)) {
    return false;
  }
  constrained_name_types_ |=
      permitted_subtrees_.present_name_types &
      (is_critical ? GENERAL_NAME_ALL_TYPES : kSupportedNameTypes);

  bool had_excluded_subtrees = false;
  der::Input excluded_subtrees_value;
  if (!sequence_parser.ReadOptionalTag(der::ContextSpecificConstructed(1),
                                       &excluded_subtrees_value,
                                       &had_excluded_subtrees)) {
    return false;
  }
  if (had_excluded_subtrees &&
      !ParseGeneralSubtrees(excluded_subtrees_value, &excluded_subtrees_,
                            errors)) {
    return false;
  }
  constrained_name_types_ |=
      excluded_subtrees_.present_name_types &
      (is_critical ? GENERAL_NAME_ALL_TYPES : kSupportedNameTypes);

  // RFC 5280 section 4.2.1.10:
  // Conforming CAs MUST NOT issue certificates where name constraints is an
  // empty sequence. That is, either the permittedSubtrees field or the
  // excludedSubtrees MUST be present.
  if (!had_permitted_subtrees && !had_excluded_subtrees)
    return false;

  if (sequence_parser.HasMore())
    return false;

  return true;
}

void NameConstraints::IsPermittedCert(const der::Input& subject_rdn_sequence,
                                      const GeneralNames* subject_alt_names,
                                      CertErrors* errors) const {
  // Checking NameConstraints is O(number_of_names * number_of_constraints).
  // Impose a hard limit to mitigate the use of name constraints as a DoS
  // mechanism.
  const size_t kMaxChecks = 1048576;  // 1 << 20
  base::ClampedNumeric<size_t> check_count = 0;

  if (subject_alt_names) {
    check_count +=
        base::ClampMul(subject_alt_names->dns_names.size(),
                       base::ClampAdd(excluded_subtrees_.dns_names.size(),
                                      permitted_subtrees_.dns_names.size()));
    check_count += base::ClampMul(
        subject_alt_names->directory_names.size(),
        base::ClampAdd(excluded_subtrees_.directory_names.size(),
                       permitted_subtrees_.directory_names.size()));
    check_count += base::ClampMul(
        subject_alt_names->ip_addresses.size(),
        base::ClampAdd(excluded_subtrees_.ip_address_ranges.size(),
                       permitted_subtrees_.ip_address_ranges.size()));
  }

  if (!(subject_alt_names && subject_rdn_sequence.Length() == 0)) {
    check_count += base::ClampAdd(excluded_subtrees_.directory_names.size(),
                                  permitted_subtrees_.directory_names.size());
  }

  if (check_count > kMaxChecks) {
    errors->AddError(cert_errors::kTooManyNameConstraintChecks);
    return;
  }

  // Subject Alternative Name handling:
  //
  // RFC 5280 section 4.2.1.6:
  // id-ce-subjectAltName OBJECT IDENTIFIER ::=  { id-ce 17 }
  //
  // SubjectAltName ::= GeneralNames
  //
  // GeneralNames ::= SEQUENCE SIZE (1..MAX) OF GeneralName

  if (subject_alt_names) {
    // Check unsupported name types:
    // constrained_name_types() for the unsupported types will only be true if
    // that type of name was present in a name constraint that was marked
    // critical.
    //
    // RFC 5280 section 4.2.1.10:
    // If a name constraints extension that is marked as critical
    // imposes constraints on a particular name form, and an instance of
    // that name form appears in the subject field or subjectAltName
    // extension of a subsequent certificate, then the application MUST
    // either process the constraint or reject the certificate.
    if (constrained_name_types() & subject_alt_names->present_name_types &
        ~kSupportedNameTypes) {
      errors->AddError(cert_errors::kNotPermittedByNameConstraints);
      return;
    }

    // Check supported name types:
    for (const auto& dns_name : subject_alt_names->dns_names) {
      if (!IsPermittedDNSName(dns_name)) {
        errors->AddError(cert_errors::kNotPermittedByNameConstraints);
        return;
      }
    }

    for (const auto& directory_name : subject_alt_names->directory_names) {
      if (!IsPermittedDirectoryName(directory_name)) {
        errors->AddError(cert_errors::kNotPermittedByNameConstraints);
        return;
      }
    }

    for (const auto& ip_address : subject_alt_names->ip_addresses) {
      if (!IsPermittedIP(ip_address)) {
        errors->AddError(cert_errors::kNotPermittedByNameConstraints);
        return;
      }
    }
  }

  // Subject handling:

  // RFC 5280 section 4.2.1.10:
  // Legacy implementations exist where an electronic mail address is embedded
  // in the subject distinguished name in an attribute of type emailAddress
  // (Section 4.1.2.6). When constraints are imposed on the rfc822Name name
  // form, but the certificate does not include a subject alternative name, the
  // rfc822Name constraint MUST be applied to the attribute of type emailAddress
  // in the subject distinguished name.
  if (!subject_alt_names &&
      (constrained_name_types() & GENERAL_NAME_RFC822_NAME)) {
    bool contained_email_address = false;
    if (!NameContainsEmailAddress(subject_rdn_sequence,
                                  &contained_email_address)) {
      errors->AddError(cert_errors::kNotPermittedByNameConstraints);
      return;
    }
    if (contained_email_address) {
      errors->AddError(cert_errors::kNotPermittedByNameConstraints);
      return;
    }
  }

  // RFC 5280 4.1.2.6:
  // If subject naming information is present only in the subjectAltName
  // extension (e.g., a key bound only to an email address or URI), then the
  // subject name MUST be an empty sequence and the subjectAltName extension
  // MUST be critical.
  // This code assumes that criticality condition is checked by the caller, and
  // therefore only needs to avoid the IsPermittedDirectoryName check against an
  // empty subject in such a case.
  if (subject_alt_names && subject_rdn_sequence.Length() == 0)
    return;

  if (!IsPermittedDirectoryName(subject_rdn_sequence)) {
    errors->AddError(cert_errors::kNotPermittedByNameConstraints);
    return;
  }
}

bool NameConstraints::IsPermittedDNSName(base::StringPiece name) const {
  for (const auto& excluded_name : excluded_subtrees_.dns_names) {
    // When matching wildcard hosts against excluded subtrees, consider it a
    // match if the constraint would match any expansion of the wildcard. Eg,
    // *.bar.com should match a constraint of foo.bar.com.
    if (DNSNameMatches(name, excluded_name, WILDCARD_PARTIAL_MATCH))
      return false;
  }

  // If permitted subtrees are not constrained, any name that is not excluded is
  // allowed.
  if (!(permitted_subtrees_.present_name_types & GENERAL_NAME_DNS_NAME))
    return true;

  for (const auto& permitted_name : permitted_subtrees_.dns_names) {
    // When matching wildcard hosts against permitted subtrees, consider it a
    // match only if the constraint would match all expansions of the wildcard.
    // Eg, *.bar.com should match a constraint of bar.com, but not foo.bar.com.
    if (DNSNameMatches(name, permitted_name, WILDCARD_FULL_MATCH))
      return true;
  }

  return false;
}

bool NameConstraints::IsPermittedDirectoryName(
    const der::Input& name_rdn_sequence) const {
  for (const auto& excluded_name : excluded_subtrees_.directory_names) {
    if (VerifyNameInSubtree(name_rdn_sequence, excluded_name))
      return false;
  }

  // If permitted subtrees are not constrained, any name that is not excluded is
  // allowed.
  if (!(permitted_subtrees_.present_name_types & GENERAL_NAME_DIRECTORY_NAME))
    return true;

  for (const auto& permitted_name : permitted_subtrees_.directory_names) {
    if (VerifyNameInSubtree(name_rdn_sequence, permitted_name))
      return true;
  }

  return false;
}

bool NameConstraints::IsPermittedIP(const IPAddress& ip) const {
  for (const auto& excluded_ip : excluded_subtrees_.ip_address_ranges) {
    if (IPAddressMatchesPrefix(ip, excluded_ip.first, excluded_ip.second))
      return false;
  }

  // If permitted subtrees are not constrained, any name that is not excluded is
  // allowed.
  if (!(permitted_subtrees_.present_name_types & GENERAL_NAME_IP_ADDRESS))
    return true;

  for (const auto& permitted_ip : permitted_subtrees_.ip_address_ranges) {
    if (IPAddressMatchesPrefix(ip, permitted_ip.first, permitted_ip.second))
      return true;
  }

  return false;
}

}  // namespace net
