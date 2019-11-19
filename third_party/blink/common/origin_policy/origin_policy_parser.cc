// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/common/origin_policy/origin_policy_parser.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

std::unique_ptr<OriginPolicy> OriginPolicyParser::Parse(
    base::StringPiece text) {
  OriginPolicyParser parser;
  if (!parser.DoParse(text))
    return nullptr;
  return std::move(parser.policy_);
}

OriginPolicyParser::OriginPolicyParser() {}
OriginPolicyParser::~OriginPolicyParser() {}

bool OriginPolicyParser::DoParse(base::StringPiece policy_text) {
  policy_ = base::WrapUnique(new OriginPolicy);

  if (policy_text.empty())
    return false;

  std::unique_ptr<base::Value> json =
      base::JSONReader::ReadDeprecated(policy_text);
  if (!json || !json->is_dict())
    return false;

  base::Value* csp = json->FindKey("content-security-policy");
  bool csp_ok = !csp || ParseContentSecurityPolicies(*csp);

  base::Value* features = json->FindKey("feature-policy");
  bool features_ok = !features || ParseFeaturePolicies(*features);

  base::Value* first_party_set = json->FindKey("first-party-set");
  bool first_party_set_ok =
      !first_party_set || ParseFirstPartySet(*first_party_set);

  return csp_ok && features_ok && first_party_set_ok;
}

bool OriginPolicyParser::ParseContentSecurityPolicies(
    const base::Value& policies) {
  if (!policies.is_list())
    return false;

  bool ok = true;
  for (const auto& csp : policies.GetList()) {
    ok &= ParseContentSecurityPolicy(csp);
  }
  return ok;
}

bool OriginPolicyParser::ParseContentSecurityPolicy(const base::Value& csp) {
  if (!csp.is_dict())
    return false;

  const auto* policy = csp.FindKeyOfType("policy", base::Value::Type::STRING);
  if (!policy)
    return false;

  const auto* report_only =
      csp.FindKeyOfType("report-only", base::Value::Type::BOOLEAN);
  policy_->csp_.push_back(
      {policy->GetString(), report_only && report_only->GetBool()});
  return true;
}

bool OriginPolicyParser::ParseFeaturePolicies(const base::Value& policies) {
  if (!policies.is_list())
    return false;

  bool ok = true;
  for (const auto& feature : policies.GetList()) {
    ok &= ParseFeaturePolicy(feature);
  }
  return ok;
}

bool OriginPolicyParser::ParseFeaturePolicy(const base::Value& policy) {
  if (!policy.is_string())
    return false;

  policy_->features_.push_back(policy.GetString());
  return true;
}

// This will not fail policy parsing, even if the first party set field can't
// be parsed. Therefore this function always returns true.
bool OriginPolicyParser::ParseFirstPartySet(
    const base::Value& first_party_set) {
  if (!first_party_set.is_list())
    return true;

  for (const auto& first_party : first_party_set.GetList()) {
    if (!ParseFirstPartyOrigin(first_party)) {
      policy_->first_party_set_.clear();
      return true;
    }
  }

  return true;
}

bool OriginPolicyParser::ParseFirstPartyOrigin(const base::Value& first_party) {
  if (!first_party.is_string())
    return false;

  GURL first_party_url(first_party.GetString());

  if (!first_party_url.is_valid() || first_party_url.is_empty())
    return false;

  policy_->first_party_set_.insert(url::Origin::Create(first_party_url));
  return true;
}

}  // namespace blink
