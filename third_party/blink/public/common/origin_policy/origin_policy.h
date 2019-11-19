// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_ORIGIN_POLICY_ORIGIN_POLICY_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_ORIGIN_POLICY_ORIGIN_POLICY_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "third_party/blink/public/common/common_export.h"
#include "url/origin.h"

namespace blink {

class OriginPolicyParser;

class BLINK_COMMON_EXPORT OriginPolicy {
 public:
  ~OriginPolicy();

  // Create & parse the manifest.
  static std::unique_ptr<OriginPolicy> From(base::StringPiece);

  struct CSP {
    std::string policy;
    bool report_only;
  };
  const std::vector<CSP>& GetContentSecurityPolicies() const { return csp_; }

  const std::vector<std::string>& GetFeaturePolicies() const {
    return features_;
  }

  const std::set<url::Origin>& GetFirstPartySet() const {
    return first_party_set_;
  }

 private:
  friend class OriginPolicyParser;

  OriginPolicy();

  std::vector<CSP> csp_;
  std::vector<std::string> features_;
  std::set<url::Origin> first_party_set_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_ORIGIN_POLICY_ORIGIN_POLICY_H_
