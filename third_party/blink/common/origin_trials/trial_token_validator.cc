// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"

#include <memory>
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "third_party/blink/public/common/origin_trials/origin_trial_policy.h"
#include "third_party/blink/public/common/origin_trials/trial_token.h"

namespace {

static base::RepeatingCallback<blink::OriginTrialPolicy*()>& PolicyGetter() {
  static base::NoDestructor<
      base::RepeatingCallback<blink::OriginTrialPolicy*()>>
      policy(base::BindRepeating(
          []() -> blink::OriginTrialPolicy* { return nullptr; }));
  return *policy;
}
}  // namespace

namespace blink {

TrialTokenResult::TrialTokenResult() = default;
TrialTokenResult::TrialTokenResult(OriginTrialTokenStatus status)
    : status(status) {}
TrialTokenResult::TrialTokenResult(OriginTrialTokenStatus status,
                                   std::string name,
                                   base::Time expiry,
                                   bool is_third_party)
    : status(status),
      feature_name(name),
      expiry_time(expiry),
      is_third_party(is_third_party) {}
TrialTokenResult::~TrialTokenResult() = default;

TrialTokenValidator::TrialTokenValidator() {}

TrialTokenValidator::~TrialTokenValidator() = default;

void TrialTokenValidator::SetOriginTrialPolicyGetter(
    base::RepeatingCallback<OriginTrialPolicy*()> policy_getter) {
  PolicyGetter() = policy_getter;
}

void TrialTokenValidator::ResetOriginTrialPolicyGetter() {
  SetOriginTrialPolicyGetter(base::BindRepeating(
      []() -> blink::OriginTrialPolicy* { return nullptr; }));
}

TrialTokenResult TrialTokenValidator::ValidateToken(
    base::StringPiece token,
    const url::Origin& origin,
    base::Time current_time) const {
  return ValidateToken(token, origin, nullptr, current_time);
}

TrialTokenResult TrialTokenValidator::ValidateToken(
    base::StringPiece token,
    const url::Origin& origin,
    const url::Origin* third_party_origin,
    base::Time current_time) const {
  OriginTrialPolicy* policy = PolicyGetter().Run();

  if (!policy || !policy->IsOriginTrialsSupported())
    return TrialTokenResult(OriginTrialTokenStatus::kNotSupported);

  std::vector<base::StringPiece> public_keys = policy->GetPublicKeys();
  if (public_keys.size() == 0)
    return TrialTokenResult(OriginTrialTokenStatus::kNotSupported);

  OriginTrialTokenStatus status;
  std::unique_ptr<TrialToken> trial_token;
  for (auto& key : public_keys) {
    trial_token = TrialToken::From(token, key, &status);
    if (status == OriginTrialTokenStatus::kSuccess)
      break;
  }

  if (status != OriginTrialTokenStatus::kSuccess)
    return TrialTokenResult(status);

  // If the third_party flag is set on the token, we match it against third
  // party origin if it exists. Otherwise match against document origin.
  if (trial_token->is_third_party()) {
    if (third_party_origin) {
      status = trial_token->IsValid(*third_party_origin, current_time);
    } else {
      status = OriginTrialTokenStatus::kWrongOrigin;
    }
  } else {
    status = trial_token->IsValid(origin, current_time);
  }

  if (status != OriginTrialTokenStatus::kSuccess)
    return TrialTokenResult(status);

  if (policy->IsFeatureDisabled(trial_token->feature_name()))
    return TrialTokenResult(OriginTrialTokenStatus::kFeatureDisabled);

  if (policy->IsTokenDisabled(trial_token->signature()))
    return TrialTokenResult(OriginTrialTokenStatus::kTokenDisabled);

  if (trial_token->usage_restriction() ==
          TrialToken::UsageRestriction::kSubset &&
      policy->IsFeatureDisabledForUser(trial_token->feature_name()))
    return TrialTokenResult(OriginTrialTokenStatus::kFeatureDisabledForUser);

  return TrialTokenResult(status, trial_token->feature_name(),
                          trial_token->expiry_time(),
                          trial_token->is_third_party());
}

bool TrialTokenValidator::RequestEnablesFeature(const net::URLRequest* request,
                                                base::StringPiece feature_name,
                                                base::Time current_time) const {
  // TODO(mek): Possibly cache the features that are availble for request in
  // UserData associated with the request.
  return RequestEnablesFeature(request->url(), request->response_headers(),
                               feature_name, current_time);
}

bool TrialTokenValidator::RequestEnablesFeature(
    const GURL& request_url,
    const net::HttpResponseHeaders* response_headers,
    base::StringPiece feature_name,
    base::Time current_time) const {
  if (!IsTrialPossibleOnOrigin(request_url))
    return false;

  url::Origin origin = url::Origin::Create(request_url);
  size_t iter = 0;
  std::string token;
  while (response_headers->EnumerateHeader(&iter, "Origin-Trial", &token)) {
    TrialTokenResult result = ValidateToken(token, origin, current_time);
    // TODO(mek): Log the validation errors to histograms?
    if (result.status == OriginTrialTokenStatus::kSuccess)
      if (result.feature_name == feature_name)
        return true;
  }
  return false;
}

std::unique_ptr<TrialTokenValidator::FeatureToTokensMap>
TrialTokenValidator::GetValidTokensFromHeaders(
    const url::Origin& origin,
    const net::HttpResponseHeaders* headers,
    base::Time current_time) const {
  std::unique_ptr<FeatureToTokensMap> tokens(
      std::make_unique<FeatureToTokensMap>());
  if (!IsTrialPossibleOnOrigin(origin.GetURL()))
    return tokens;

  size_t iter = 0;
  std::string token;
  while (headers->EnumerateHeader(&iter, "Origin-Trial", &token)) {
    TrialTokenResult result = ValidateToken(token, origin, current_time);
    if (result.status == OriginTrialTokenStatus::kSuccess) {
      (*tokens)[result.feature_name].push_back(token);
    }
  }
  return tokens;
}

std::unique_ptr<TrialTokenValidator::FeatureToTokensMap>
TrialTokenValidator::GetValidTokens(const url::Origin& origin,
                                    const FeatureToTokensMap& tokens,
                                    base::Time current_time) const {
  std::unique_ptr<FeatureToTokensMap> out_tokens(
      std::make_unique<FeatureToTokensMap>());
  if (!IsTrialPossibleOnOrigin(origin.GetURL()))
    return out_tokens;

  for (const auto& feature : tokens) {
    for (const std::string& token : feature.second) {
      TrialTokenResult result = ValidateToken(token, origin, current_time);
      if (result.status == OriginTrialTokenStatus::kSuccess) {
        DCHECK_EQ(result.feature_name, feature.first);
        (*out_tokens)[feature.first].push_back(token);
      }
    }
  }
  return out_tokens;
}

// static
bool TrialTokenValidator::IsTrialPossibleOnOrigin(const GURL& url) {
  OriginTrialPolicy* policy = PolicyGetter().Run();
  return policy && policy->IsOriginTrialsSupported() &&
         policy->IsOriginSecure(url);
}

}  // namespace blink
