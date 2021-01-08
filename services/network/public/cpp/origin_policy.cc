// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/origin_policy.h"

namespace network {
bool operator==(const OriginPolicyContentsPtr& a,
                const OriginPolicyContentsPtr& b) {
  return (a.get() == b.get()) || (a && b && *a == *b);
}

bool operator!=(const OriginPolicyContentsPtr& a,
                const OriginPolicyContentsPtr& b) {
  return !(a == b);
}

OriginPolicyContents::OriginPolicyContents() = default;

OriginPolicyContents::~OriginPolicyContents() = default;

OriginPolicyContents::OriginPolicyContents(const OriginPolicyContents& other) =
    default;

OriginPolicyContents::OriginPolicyContents(
    const std::vector<std::string>& ids,
    const base::Optional<std::string>& feature_policy,
    const std::vector<std::string>& content_security_policies,
    const std::vector<std::string>& content_security_policies_report_only)
    : ids(ids),
      feature_policy(feature_policy),
      content_security_policies(content_security_policies),
      content_security_policies_report_only(
          content_security_policies_report_only) {}

OriginPolicyContents& OriginPolicyContents::operator=(
    const OriginPolicyContents& other) = default;

bool OriginPolicyContents::operator==(const OriginPolicyContents& other) const {
  return ids == other.ids && feature_policy == other.feature_policy &&
         content_security_policies == other.content_security_policies &&
         content_security_policies_report_only ==
             other.content_security_policies_report_only;
}

OriginPolicyContentsPtr OriginPolicyContents::ClonePtr() {
  // Uses the copy constructor to create a new pointer to a cloned object of
  // this object.
  return std::make_unique<OriginPolicyContents>(*this);
}

OriginPolicy::OriginPolicy() = default;

OriginPolicy::~OriginPolicy() = default;

OriginPolicy::OriginPolicy(const OriginPolicy& other)
    : state(other.state),
      policy_url(other.policy_url),
      contents(other.contents ? other.contents->ClonePtr() : nullptr) {}

OriginPolicy& OriginPolicy::operator=(const OriginPolicy& other) {
  state = other.state;
  policy_url = other.policy_url;
  if (other.contents) {
    contents = other.contents->ClonePtr();
  }

  return *this;
}

}  // namespace network
