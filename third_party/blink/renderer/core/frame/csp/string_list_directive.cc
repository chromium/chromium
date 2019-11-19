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
    : CSPDirective(name, value, policy), allow_any_(false) {
  // Turn whitespace-y characters into ' ' and then split on ' ' into list_.
  value.SimplifyWhiteSpace().Split(' ', false, list_);

  // A single entry "*" means all values are allowed.
  if (list_.size() == 1 && list_.at(0) == "*") {
    allow_any_ = true;
    list_.clear();
  }

  // There appears to be no wtf::Vector equivalent to STLs erase(from, to)
  // method, so we can't do the canonical .erase(remove_if(..), end) and have
  // to emulate this:
  list_.Shrink(
      std::remove_if(list_.begin(), list_.end(), &IsInvalidStringValue) -
      list_.begin());
}

// TODO(vogelheim): If StringListDirective will be used in contexts other than
//                  TrustedTypes, this needs to be made configurable or
//                  over-rideable.
bool StringListDirective::IsInvalidStringValue(const String& str) {
  // TODO(vogelheim): Update this as the Trusted Type spec evolves.

  // Currently, Trusted Type demands that quoted strings are treated as
  // placeholders (and thus cannot be policy names). We'll just disallow any
  // string with quote marks in them.
  return str.Contains('\'') || str.Contains('"');
}

bool StringListDirective::Allows(const String& string_piece,
                                 bool is_duplicate) {
  if (string_piece == "default" && is_duplicate)
    return false;
  if (allow_any_)
    return true;
  return list_.Contains(string_piece) && !is_duplicate;
}

void StringListDirective::Trace(blink::Visitor* visitor) {
  CSPDirective::Trace(visitor);
}

}  // namespace blink
