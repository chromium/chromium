// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/string_list_directive.h"

#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

StringListDirective::StringListDirective(const String& name,
                                         const String& value,
                                         ContentSecurityPolicy* policy)
    : CSPDirective(name, value, policy),
      allow_any_(false),
      allow_duplicates_(false) {
  // Turn whitespace-y characters into ' ' and then split on ' ' into list_.
  value.SimplifyWhiteSpace().Split(' ', false, list_);

  auto drop_fn = [this](const String& value) -> bool {
    return !AllowOrProcessValue(value);
  };

  // There appears to be no wtf::Vector equivalent to STLs erase(from, to)
  // method, so we can't do the canonical .erase(remove_if(..), end) and have
  // to emulate this:
  list_.Shrink(std::remove_if(list_.begin(), list_.end(), drop_fn) -
               list_.begin());
}

bool StringListDirective::IsPolicyName(const String& name) {
  // This implements tt-policy-name from
  // https://w3c.github.io/webappsec-trusted-types/dist/spec/#trusted-types-csp-directive/
  return name.Find(&IsNotPolicyNameChar) == kNotFound;
}

bool StringListDirective::IsNotPolicyNameChar(UChar c) {
  // This implements the negation of one char of tt-policy-name from
  // https://w3c.github.io/webappsec-trusted-types/dist/spec/#trusted-types-csp-directive/
  bool is_name_char = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                      (c >= 'A' && c <= 'Z') || c == '-' || c == '#' ||
                      c == '=' || c == '_' || c == '/' || c == '@' ||
                      c == '.' || c == '%';
  return !is_name_char;
}

bool StringListDirective::AllowOrProcessValue(const String& src) {
  DCHECK_EQ(src, src.StripWhiteSpace());
  // Handle keywords and special tokens first:
  if (src == "'allow-duplicates'") {
    allow_duplicates_ = true;
    return false;
  }
  if (src == "*") {
    allow_any_ = true;
    return false;
  }
  if (src == "'none'") {
    if (list_.size() > 1) {
      Policy()->ReportInvalidSourceExpression(GetName(), src);
    }
    return false;
  }
  return IsPolicyName(src);
}

bool StringListDirective::Allows(
    const String& value,
    bool is_duplicate,
    ContentSecurityPolicy::AllowTrustedTypePolicyDetails& violation_details) {
  if (is_duplicate && !allow_duplicates_) {
    violation_details = ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
        kDisallowedDuplicateName;
  } else if (is_duplicate && value == "default") {
    violation_details = ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
        kDisallowedDuplicateName;
  } else if (!IsPolicyName(value)) {
    violation_details =
        ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kDisallowedName;
  } else if (!(allow_any_ || list_.Contains(value))) {
    violation_details =
        ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kDisallowedName;
  } else {
    violation_details =
        ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed;
  }
  return violation_details ==
         ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed;
}

void StringListDirective::Trace(Visitor* visitor) const {
  CSPDirective::Trace(visitor);
}

}  // namespace blink
