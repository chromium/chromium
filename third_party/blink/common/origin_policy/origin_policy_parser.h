// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_COMMON_ORIGIN_POLICY_ORIGIN_POLICY_PARSER_H_
#define THIRD_PARTY_BLINK_COMMON_ORIGIN_POLICY_ORIGIN_POLICY_PARSER_H_

#include "third_party/blink/public/common/origin_policy/origin_policy.h"

#include <string>
#include "base/macros.h"

namespace base {
class Value;
}  // namespace base

namespace blink {

class OriginPolicyParser {
 public:
  // Parse the given origin policy.
  // Returns an empty unique_ptr if parsing fails.
  static std::unique_ptr<OriginPolicy> Parse(base::StringPiece);

 private:
  OriginPolicyParser();
  ~OriginPolicyParser();

  bool DoParse(base::StringPiece);
  bool ParseContentSecurityPolicies(const base::Value&);
  bool ParseContentSecurityPolicy(const base::Value&);
  bool ParseFeaturePolicies(const base::Value&);
  bool ParseFeaturePolicy(const base::Value&);
  bool ParseFirstPartySet(const base::Value&);
  bool ParseFirstPartyOrigin(const base::Value&);

  std::unique_ptr<OriginPolicy> policy_;

  DISALLOW_COPY_AND_ASSIGN(OriginPolicyParser);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_COMMON_ORIGIN_POLICY_ORIGIN_POLICY_PARSER_H_
