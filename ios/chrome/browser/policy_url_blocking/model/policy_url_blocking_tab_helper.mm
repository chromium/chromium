// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy_url_blocking/model/policy_url_blocking_tab_helper.h"

#import <memory>
#import <utility>

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/memory/weak_ptr.h"
#import "components/policy/core/browser/url_list/policy_blocklist_service.h"
#import "components/policy/core/browser/url_list/url_blocklist_manager.h"
#import "components/policy/core/browser/url_list/url_blocklist_policy_handler.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/url_matcher/url_util.h"
#import "ios/chrome/browser/policy_url_blocking/model/features.h"
#import "ios/chrome/browser/policy_url_blocking/model/policy_url_blocking_service_factory.h"
#import "ios/chrome/browser/policy_url_blocking/model/policy_url_blocking_util.h"
#import "ios/chrome/browser/policy_url_blocking/model/safe_search_service.h"
#import "ios/chrome/browser/policy_url_blocking/model/safe_search_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"
#import "url/url_constants.h"

namespace {

// Helper class that wraps a
// `web::WebStatePolicyDecider::PolicyDecisionCallback` and ensures it is run
// exactly once. If destroyed before being explicitly invoked (for example, if
// the asynchronous check is cancelled or the underlying service is destroyed),
// it invokes the callback with `PolicyDecision::Cancel()` to fail closed.
// Precedent: `web::PolicyDecisionStateTracker` and `base::ScopedClosureRunner`.
class ScopedPolicyDecisionCallback {
 public:
  explicit ScopedPolicyDecisionCallback(
      web::WebStatePolicyDecider::PolicyDecisionCallback callback)
      : callback_(std::move(callback)) {}
  ~ScopedPolicyDecisionCallback() {
    if (!callback_.is_null()) {
      std::move(callback_).Run(
          web::WebStatePolicyDecider::PolicyDecision::Cancel());
    }
  }

  ScopedPolicyDecisionCallback(const ScopedPolicyDecisionCallback&) = delete;
  ScopedPolicyDecisionCallback& operator=(const ScopedPolicyDecisionCallback&) =
      delete;

  ScopedPolicyDecisionCallback(ScopedPolicyDecisionCallback&&) = default;
  ScopedPolicyDecisionCallback& operator=(ScopedPolicyDecisionCallback&&) =
      default;

  void Run(web::WebStatePolicyDecider::PolicyDecision decision) {
    if (!callback_.is_null()) {
      std::move(callback_).Run(decision);
    }
  }

 private:
  web::WebStatePolicyDecider::PolicyDecisionCallback callback_;
};

void OnSafeSearchCheckDone(
    base::WeakPtr<web::WebState> weak_web_state,
    std::unique_ptr<ScopedPolicyDecisionCallback> scoped_callback,
    bool is_safe) {
  if (!weak_web_state) {
    scoped_callback->Run(web::WebStatePolicyDecider::PolicyDecision::Cancel());
    return;
  }
  if (is_safe) {
    scoped_callback->Run(web::WebStatePolicyDecider::PolicyDecision::Allow());
  } else {
    scoped_callback->Run(
        web::WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(
            policy_url_blocking_util::CreateBlockedUrlError(
                PolicyBlocklistService::PolicyBlocklistState::URL_POLICY)));
  }
}

}  // namespace

PolicyUrlBlockingTabHelper::~PolicyUrlBlockingTabHelper() = default;

PolicyUrlBlockingTabHelper::PolicyUrlBlockingTabHelper(web::WebState* web_state)
    : web::WebStatePolicyDecider(web_state) {}

void PolicyUrlBlockingTabHelper::ShouldAllowRequest(
    NSURLRequest* request,
    web::WebStatePolicyDecider::RequestInfo request_info,
    web::WebStatePolicyDecider::PolicyDecisionCallback callback) {
  GURL gurl = net::GURLWithNSURL(request.URL);
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state()->GetBrowserState());
  PolicyBlocklistService* service =
      PolicyBlocklistServiceFactory::GetForProfile(profile);
  PolicyBlocklistService::PolicyBlocklistState blocklist_state =
      service->GetURLBlocklistStateWithPolicySource(gurl);
  if (blocklist_state.url_blocklist_state ==
      policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST) {
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(
            policy_url_blocking_util::CreateBlockedUrlError(
                blocklist_state.policy_source)));
    return;
  }

  // URLAllowlist takes precedence over SafeSites filter.
  if (blocklist_state.url_blocklist_state ==
      policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST) {
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }

  PrefService* prefs = profile->GetPrefs();
  auto filter_behavior = static_cast<policy::SafeSitesFilterBehavior>(
      prefs->GetInteger(policy::policy_prefs::kSafeSitesFilterBehavior));
  if (filter_behavior ==
          policy::SafeSitesFilterBehavior::kSafeSitesFilterDisabled ||
      !base::FeatureList::IsEnabled(kSafeSitesFilterBehaviorIOS)) {
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }

  if (gurl.SchemeIs(url::kBlobScheme) || !gurl.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }

  SafeSearchService* safe_search_service =
      SafeSearchServiceFactory::GetForProfile(profile);
  if (!safe_search_service) {
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }

  GURL effective_url = url_matcher::util::GetEmbeddedURL(gurl);
  if (!effective_url.is_valid()) {
    effective_url = gurl;
  }

  safe_search_service->CheckSafeSearchURL(
      effective_url,
      base::BindOnce(
          &OnSafeSearchCheckDone, web_state()->GetWeakPtr(),
          std::make_unique<ScopedPolicyDecisionCallback>(std::move(callback))));
}
