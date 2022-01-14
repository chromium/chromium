// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_CERTIFICATE_POLICIES_H_
#define NET_CERT_INTERNAL_CERTIFICATE_POLICIES_H_

#include <stdint.h>

#include <vector>

#include "net/base/net_export.h"
#include "net/der/input.h"

namespace net {

class CertErrors;

// Returns the DER-encoded OID, without tag or length, of the anyPolicy
// certificate policy defined in RFC 5280 section 4.2.1.4.
NET_EXPORT const der::Input AnyPolicy();

// From RFC 5280:
//
//     id-ce-inhibitAnyPolicy OBJECT IDENTIFIER ::=  { id-ce 54 }
//
// In dotted notation: 2.5.29.54
NET_EXPORT der::Input InhibitAnyPolicyOid();

// From RFC 5280:
//
//     id-ce-policyMappings OBJECT IDENTIFIER ::=  { id-ce 33 }
//
// In dotted notation: 2.5.29.33
NET_EXPORT der::Input PolicyMappingsOid();

// Parses a certificatePolicies extension and stores the policy OIDs in
// |*policies|, in sorted order.
//
// If policyQualifiers for User Notice or CPS are present then they are
// ignored (RFC 5280 section 4.2.1.4 says "optional qualifiers, which MAY
// be present, are not expected to change the definition of the policy."
//
// If a policy qualifier other than User Notice/CPS is present, parsing
// will fail if |fail_parsing_unknown_qualifier_oids| was set to true,
// otherwise the unrecognized qualifiers wil be skipped and not parsed
// any further.
//
// Returns true on success. On failure returns false and may add errors to
// |errors|, which must be non-null.
//
// The values in |policies| are only valid as long as |extension_value| is (as
// it references data).
NET_EXPORT bool ParseCertificatePoliciesExtension(
    const der::Input& extension_value,
    bool fail_parsing_unknown_qualifier_oids,
    std::vector<der::Input>* policies,
    CertErrors* errors);

struct ParsedPolicyConstraints {
  bool has_require_explicit_policy = false;
  uint8_t require_explicit_policy = 0;

  bool has_inhibit_policy_mapping = false;
  uint8_t inhibit_policy_mapping = 0;
};

// Parses a PolicyConstraints SEQUENCE as defined by RFC 5280. Returns true on
// success, and sets |out|.
[[nodiscard]] NET_EXPORT bool ParsePolicyConstraints(
    const der::Input& policy_constraints_tlv,
    ParsedPolicyConstraints* out);

// Parses an InhibitAnyPolicy as defined by RFC 5280. Returns true on success,
// and sets |num_certs|.
[[nodiscard]] NET_EXPORT bool ParseInhibitAnyPolicy(
    const der::Input& inhibit_any_policy_tlv,
    uint8_t* num_certs);

struct ParsedPolicyMapping {
  der::Input issuer_domain_policy;
  der::Input subject_domain_policy;
};

// Parses a PolicyMappings SEQUENCE as defined by RFC 5280. Returns true on
// success, and sets |mappings|.
[[nodiscard]] NET_EXPORT bool ParsePolicyMappings(
    const der::Input& policy_mappings_tlv,
    std::vector<ParsedPolicyMapping>* mappings);

}  // namespace net

#endif  // NET_CERT_INTERNAL_CERTIFICATE_POLICIES_H_
