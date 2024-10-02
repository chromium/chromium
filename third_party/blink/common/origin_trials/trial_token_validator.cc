// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"

#include <memory>
#include <string_view>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/origin_trials/origin_trial_policy.h"
#include "third_party/blink/public/common/origin_trials/origin_trials.h"
#include "third_party/blink/public/common/origin_trials/trial_token.h"
#include "third_party/blink/public/common/origin_trials/trial_token_result.h"
#include "third_party/blink/public/mojom/origin_trial_feature/origin_trial_feature.mojom-shared.h"

namespace blink {
namespace {

static base::RepeatingCallback<OriginTrialPolicy*()>& PolicyGetter() {
  static base::NoDestructor<base::RepeatingCallback<OriginTrialPolicy*()>>
      policy(
          base::BindRepeating([]() -> OriginTrialPolicy* { return nullptr; }));
  return *policy;
}

bool IsDeprecationTrialPossible() {
  OriginTrialPolicy* policy = PolicyGetter().Run();
  return policy && policy->IsOriginTrialsSupported();
}

// Validates the provided trial_token. If provided, the third_party_origins is
// only used for validating third-party tokens.
OriginTrialTokenStatus IsTokenValid(
    const TrialToken& trial_token,
    const url::Origin& origin,
    base::span<const url::Origin> third_party_origins,
    base::Time current_time) {
  OriginTrialTokenStatus status;
  if (trial_token.is_third_party()) {
    if (!third_party_origins.empty()) {
      for (const auto& third_party_origin : third_party_origins) {
        status = trial_token.IsValid(third_party_origin, current_time);
        if (status == OriginTrialTokenStatus::kSuccess)
          break;
      }
    } else {
      status = OriginTrialTokenStatus::kWrongOrigin;
    }
  } else {
    status = trial_token.IsValid(origin, current_time);
  }
  return status;
}

// Determine if the the |token_expiry_time| should be considered as expired
// at |current_time| given the |trial_name|.
// Manual completion trials add an expiry grace period, which has to be taken
// into account to answer this question.
bool IsTokenExpired(std::string_view trial_name,
                    const base::Time token_expiry_time,
                    const base::Time current_time) {
  // Check token expiry.
  bool token_expired = token_expiry_time <= current_time;
  if (token_expired) {
    if (origin_trials::IsTrialValid(trial_name)) {
      // Manual completion trials have an expiry grace period. For these trials
      // the token expiry time is valid if:
      // token_expiry_time + kExpiryGracePeriod > current_time
      for (mojom::OriginTrialFeature feature :
           origin_trials::FeaturesForTrial(trial_name)) {
        if (origin_trials::FeatureHasExpiryGracePeriod(feature)) {
          if (token_expiry_time + kExpiryGracePeriod > current_time) {
            token_expired = false;  // consider the token non-expired
            break;
          }
        }
      }
    }
  }
  return token_expired;
}

// Validate that the passed token has not yet expired and that the trial or
// token has not been disabled.
OriginTrialTokenStatus ValidateTokenEnabled(
    const OriginTrialPolicy& policy,
    std::string_view trial_name,
    const base::Time token_expiry_time,
    const TrialToken::UsageRestriction usage_restriction,
    std::string_view token_signature,
    const base::Time current_time) {
  if (IsTokenExpired(trial_name, token_expiry_time, current_time))
    return OriginTrialTokenStatus::kExpired;

  if (policy.IsFeatureDisabled(trial_name))
    return OriginTrialTokenStatus::kFeatureDisabled;

  if (policy.IsTokenDisabled(token_signature))
    return OriginTrialTokenStatus::kTokenDisabled;

  if (usage_restriction == TrialToken::UsageRestriction::kSubset &&
      policy.IsFeatureDisabledForUser(trial_name)) {
    return OriginTrialTokenStatus::kFeatureDisabledForUser;
  }

  // All checks passed, return success
  return OriginTrialTokenStatus::kSuccess;
}

}  // namespace

TrialTokenValidator::OriginInfo::OriginInfo(const url::Origin& wrapped_origin)
    : origin(wrapped_origin) {
  is_secure = network::IsOriginPotentiallyTrustworthy(origin);
}

TrialTokenValidator::OriginInfo::OriginInfo(const url::Origin& wrapped_origin,
                                            bool origin_is_secure)
    : origin(wrapped_origin), is_secure(origin_is_secure) {}

TrialTokenValidator::TrialTokenValidator() = default;

TrialTokenValidator::~TrialTokenValidator() = default;

void TrialTokenValidator::SetOriginTrialPolicyGetter(
    base::RepeatingCallback<OriginTrialPolicy*()> policy_getter) {
  PolicyGetter() = policy_getter;
}

void TrialTokenValidator::ResetOriginTrialPolicyGetter() {
  SetOriginTrialPolicyGetter(
      base::BindRepeating([]() -> OriginTrialPolicy* { return nullptr; }));
}

TrialTokenResult TrialTokenValidator::ValidateTokenAndTrial(
    std::string_view token,
    const url::Origin& origin,
    base::Time current_time) const {
  return ValidateTokenAndTrialWithOriginInfo(
      token, OriginInfo(origin), base::span<const OriginInfo>{}, current_time);
}

TrialTokenResult TrialTokenValidator::ValidateTokenAndTrial(
    std::string_view token,
    const url::Origin& origin,
    base::span<const url::Origin> third_party_origins,
    base::Time current_time) const {
  std::vector<OriginInfo> third_party_origin_info;
  for (const url::Origin& third_party_origin : third_party_origins) {
    third_party_origin_info.emplace_back(third_party_origin);
  }
  return ValidateTokenAndTrialWithOriginInfo(
      token, OriginInfo(origin), third_party_origin_info, current_time);
}

TrialTokenResult TrialTokenValidator::ValidateTokenAndTrialWithOriginInfo(
    std::string_view token,
    const OriginInfo& origin,
    base::span<const OriginInfo> third_party_origin_info,
    base::Time current_time) const {
  std::vector<url::Origin> third_party_origins;
  for (const OriginInfo& info : third_party_origin_info) {
    third_party_origins.push_back(info.origin);
  }
  TrialTokenResult token_result =
      ValidateToken(token, origin.origin, third_party_origins, current_time);

  if (token_result.Status() != OriginTrialTokenStatus::kSuccess)
    return token_result;

  const TrialToken& parsed_token = *token_result.ParsedToken();

  if (!origin_trials::IsTrialValid(parsed_token.feature_name())) {
    token_result.SetStatus(OriginTrialTokenStatus::kUnknownTrial);
    return token_result;
  }

  if (parsed_token.is_third_party() &&
      !origin_trials::IsTrialEnabledForThirdPartyOrigins(
          parsed_token.feature_name())) {
    DVLOG(1) << "ValidateTokenAndTrial: feature disabled for third party trial";
    token_result.SetStatus(OriginTrialTokenStatus::kFeatureDisabled);
    return token_result;
  }

  // Origin trials are only enabled for secure origins. The only exception
  // is for deprecation trials. For those, the secure origin check can be
  // skipped.
  if (origin_trials::IsTrialEnabledForInsecureContext(
          parsed_token.feature_name())) {
    return token_result;
  }

  bool is_secure = origin.is_secure;
  if (parsed_token.is_third_party()) {
    // For third-party tokens, both the current origin and the script origin
    // must be secure. Due to subdomain matching, the token origin might not
    // be an exact match for one of the provided script origins, and the result
    // doesn't indicate which specific origin was matched. This means it's not
    // a direct lookup to find the appropriate script origin. To avoid re-doing
    // all the origin comparisons, there are shortcuts that depend on how many
    // script origins were provided. There must be at least one, or the third
    // party token would not be validated successfully.
    DCHECK(!third_party_origin_info.empty());
    if (third_party_origin_info.size() == 1) {
      // Only one script origin, it must be the origin used for validation.
      is_secure &= third_party_origin_info[0].is_secure;
    } else {
      // Match the origin in the token to one of the multiple script origins, if
      // necessary. If all the provided origins are secure, then it doesn't
      // matter which one matched. Only insecure origins need to be matched.
      bool is_script_origin_secure = true;
      for (const OriginInfo& script_origin_info : third_party_origin_info) {
        if (script_origin_info.is_secure) {
          continue;
        }
        // Re-use the IsValid() check, as it contains the subdomain matching
        // logic. The token validation takes the first valid match, so can
        // assume that success means it was the origin used.
        if (parsed_token.IsValid(script_origin_info.origin, current_time) ==
            OriginTrialTokenStatus::kSuccess) {
          is_script_origin_secure = false;
          break;
        }
      }
      is_secure &= is_script_origin_secure;
    }
  }

  if (!is_secure) {
    DVLOG(1) << "ValidateTokenAndTrial: not secure";
    token_result.SetStatus(OriginTrialTokenStatus::kInsecure);
  }

  return token_result;
}

TrialTokenResult TrialTokenValidator::ValidateToken(
    std::string_view token,
    const url::Origin& origin,
    base::Time current_time) const {
  return ValidateToken(token, origin, base::span<const url::Origin>{},
                       current_time);
}

TrialTokenResult TrialTokenValidator::ValidateToken(
    std::string_view token,
    const url::Origin& origin,
    base::span<const url::Origin> third_party_origins,
    base::Time current_time) const {
  OriginTrialPolicy* policy = PolicyGetter().Run();

  if (!policy || !policy->IsOriginTrialsSupported())
    return TrialTokenResult(OriginTrialTokenStatus::kNotSupported);

  std::vector<OriginTrialPublicKey> public_keys = policy->GetPublicKeys();
  if (public_keys.size() == 0)
    return TrialTokenResult(OriginTrialTokenStatus::kNotSupported);

  OriginTrialTokenStatus status;
  std::unique_ptr<TrialToken> trial_token;
  for (OriginTrialPublicKey& key : public_keys) {
    trial_token = TrialToken::From(token, key, &status);
    if (status == OriginTrialTokenStatus::kSuccess)
      break;
  }

  // Not attaching trial_token to result when token is unable to parse.
  if (status != OriginTrialTokenStatus::kSuccess)
    return TrialTokenResult(status);

  status =
      IsTokenValid(*trial_token, origin, third_party_origins, current_time);

  if (status == OriginTrialTokenStatus::kSuccess ||
      status == OriginTrialTokenStatus::kExpired) {
    // Since manual completion trials have a grace period, we need to check
    // expired tokens in addition to valid tokens.
    status = ValidateTokenEnabled(*policy, trial_token->feature_name(),
                                  trial_token->expiry_time(),
                                  trial_token->usage_restriction(),
                                  trial_token->signature(), current_time);
  }
  return TrialTokenResult(status, std::move(trial_token));
}

bool TrialTokenValidator::RevalidateTokenAndTrial(
    std::string_view trial_name,
    const base::Time token_expiry_time,
    const TrialToken::UsageRestriction usage_restriction,
    std::string_view token_signature,
    const base::Time current_time) const {
  OriginTrialPolicy* policy = PolicyGetter().Run();

  if (!policy || !policy->IsOriginTrialsSupported())
    return false;

  if (!origin_trials::IsTrialValid(trial_name))
    return false;

  OriginTrialTokenStatus status =
      ValidateTokenEnabled(*policy, trial_name, token_expiry_time,
                           usage_restriction, token_signature, current_time);
  return status == OriginTrialTokenStatus::kSuccess;
}

std::vector<mojom::OriginTrialFeature>
TrialTokenValidator::FeaturesEnabledByTrial(std::string_view trial_name) {
  std::vector<mojom::OriginTrialFeature> enabled_features;
  base::span<const mojom::OriginTrialFeature> features =
      origin_trials::FeaturesForTrial(trial_name);
  for (const mojom::OriginTrialFeature feature : features) {
    if (origin_trials::FeatureEnabledForOS(feature)) {
      enabled_features.push_back(feature);
      // Also add implied features
      for (const mojom::OriginTrialFeature implied_feature :
           origin_trials::GetImpliedFeatures(feature)) {
        enabled_features.push_back(implied_feature);
      }
    }
  }
  return enabled_features;
}

bool TrialTokenValidator::TrialEnablesFeaturesForOS(
    std::string_view trial_name) {
  return !FeaturesEnabledByTrial(trial_name).empty();
}

bool TrialTokenValidator::RequestEnablesFeature(const net::URLRequest* request,
                                                std::string_view feature_name,
                                                base::Time current_time) const {
  // TODO(mek): Possibly cache the features that are availble for request in
  // UserData associated with the request.
  return RequestEnablesFeature(request->url(), request->response_headers(),
                               feature_name, current_time);
}

bool TrialTokenValidator::RequestEnablesFeature(
    const GURL& request_url,
    const net::HttpResponseHeaders* response_headers,
    std::string_view feature_name,
    base::Time current_time) const {
  return IsTrialPossibleOnOrigin(request_url) &&
         ResponseBearsValidTokenForFeature(request_url, *response_headers,
                                           feature_name, current_time);
}

bool TrialTokenValidator::RequestEnablesDeprecatedFeature(
    const GURL& request_url,
    const net::HttpResponseHeaders* response_headers,
    std::string_view feature_name,
    base::Time current_time) const {
  return IsDeprecationTrialPossible() &&
         ResponseBearsValidTokenForFeature(request_url, *response_headers,
                                           feature_name, current_time);
}

bool TrialTokenValidator::ResponseBearsValidTokenForFeature(
    const GURL& request_url,
    const net::HttpResponseHeaders& response_headers,
    std::string_view feature_name,
    base::Time current_time) const {
  url::Origin origin = url::Origin::Create(request_url);
  size_t iter = 0;
  std::string token;
  while (response_headers.EnumerateHeader(&iter, "Origin-Trial", &token)) {
    TrialTokenResult result =
        ValidateTokenAndTrial(token, origin, current_time);
    // TODO(mek): Log the validation errors to histograms?
    if (result.Status() == OriginTrialTokenStatus::kSuccess)
      if (result.ParsedToken()->feature_name() == feature_name)
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
    if (result.Status() == OriginTrialTokenStatus::kSuccess) {
      (*tokens)[result.ParsedToken()->feature_name()].push_back(token);
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
      if (result.Status() == OriginTrialTokenStatus::kSuccess) {
        DCHECK_EQ(result.ParsedToken()->feature_name(), feature.first);
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
