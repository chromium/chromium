// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/proxy/model/proxy_service_controller.h"

#import <Foundation/Foundation.h>

#import <cstddef>
#import <optional>
#import <utility>
#import <vector>

#import "base/check.h"
#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/memory/scoped_refptr.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/numerics/safe_conversions.h"
#import "base/sequence_checker.h"
#import "base/strings/sys_string_conversions.h"
#import "components/enterprise/net/core/enterprise_proxy_error_service.h"
#import "components/enterprise/net/core/features.h"
#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_error.h"
#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_tab_helper.h"
#import "ios/chrome/browser/enterprise/proxy/model/proxy_auth_challenge_util.h"
#import "ios/web/public/proxy/proxy_config.h"
#import "ios/web/public/proxy/proxy_configuration_provider.h"
#import "ios/web/public/proxy/pvd_route_translator.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/auth.h"
#import "net/http/http_response_headers.h"
#import "net/proxy_resolution/proxy_config.h"
#import "url/gurl.h"

namespace {

// Histograms of the routing rules given to WebKit.
constexpr char kRoutingRuleCountHistogram[] =
    "Enterprise.SecureGateway.Routing.RuleCount";
constexpr char kRoutingMatchDomainCountHistogram[] =
    "Enterprise.SecureGateway.Routing.MatchDomainCount";
constexpr char kRoutingUnsupportedRuleCountHistogram[] =
    "Enterprise.SecureGateway.Routing.UnsupportedRuleCount";
constexpr char kRoutingApplyTimeHistogram[] =
    "Enterprise.SecureGateway.Routing.ApplyTime";
constexpr char kRoutingNativeConfigCountHistogram[] =
    "Enterprise.SecureGateway.Routing.NativeConfigCount";

// Navigation ID reported for challenges that are not attributed to a
// navigation. `EnterpriseProxyErrorService` records no error for it.
constexpr int64_t kUnknownNavigationId = 0;

// Code of the `kEnterpriseProxyErrorDomain` error that fails a navigation. The
// error carries no details: they are in the disguised error that
// `EnterpriseProxyErrorService` records for the navigation.
constexpr NSInteger kEnterpriseProxyErrorCode = 1;

// Returns whether `navigation_id` is the pending main-frame navigation of
// `web_state`.
bool IsPendingNavigation(web::WebState* web_state, int64_t navigation_id) {
  if (!web_state) {
    return false;
  }
  EnterpriseProxyTabHelper* tab_helper =
      EnterpriseProxyTabHelper::FromWebState(web_state);
  return tab_helper && tab_helper->pending_navigation_id() == navigation_id;
}

// Cancels the challenge answered by `callback` without failing the pending
// navigation. Answering without credentials or error would instead reject the
// protection space, which makes CFNetwork fall back to the system "Proxy
// Authentication Required" prompt for the Secure Gateway proxy.
void CancelProxyAuthChallenge(
    web::WebStateDelegate::ProxyAuthCallback callback) {
  std::move(callback).Run(/*username=*/nil, /*password=*/nil,
                          [NSError errorWithDomain:NSURLErrorDomain
                                              code:NSURLErrorCancelled
                                          userInfo:nil]);
}

// Returns the ID of the pending main-frame navigation of `web_state` if the
// challenge for `destination_url` belongs to it, or `kUnknownNavigationId`.
//
// WebKit doesn't tell which request a challenge is for, so it is attributed to
// the navigation only while the navigation is pending and only if the
// challenged URL is the navigation's URL. Challenges for subresources and for
// requests made after commit are left unattributed, as on desktop where only
// main-frame requests get auth UI. The fragment is ignored because it is never
// sent with the request. An invalid URL, e.g. from a challenge without a
// failure response, is never attributed.
int64_t GetNavigationIdForChallenge(web::WebState* web_state,
                                    const GURL& destination_url) {
  EnterpriseProxyTabHelper* tab_helper =
      EnterpriseProxyTabHelper::FromWebState(web_state);
  if (!tab_helper || !tab_helper->pending_navigation_id().has_value() ||
      !destination_url.is_valid() ||
      !destination_url.EqualsIgnoringRef(
          tab_helper->pending_navigation_url())) {
    return kUnknownNavigationId;
  }
  return *tab_helper->pending_navigation_id();
}

// Adapts the credentials resolved by `enterprise_proxy_error_service` into the
// `(username, password, error)` triple that
// `web::WebStateDelegate::ProxyAuthCallback` expects. Without credentials, the
// navigation `navigation_id` of `web_state` is failed with the enterprise proxy
// error if it is still pending and enterprise proxy error handling is enabled.
// If that navigation is no longer pending, the disguised error recorded for it
// is dropped instead. Challenges that are not attributed to a pending
// navigation, e.g. for subresources, are cancelled.
void RunProxyAuthCallback(
    enterprise_net::EnterpriseProxyErrorService* enterprise_proxy_error_service,
    base::WeakPtr<web::WebState> web_state,
    int64_t navigation_id,
    web::WebStateDelegate::ProxyAuthCallback callback,
    const std::optional<net::AuthCredentials>& credentials) {
  if (credentials.has_value()) {
    std::move(callback).Run(base::SysUTF16ToNSString(credentials->username()),
                            base::SysUTF16ToNSString(credentials->password()),
                            /*error=*/nil);
    return;
  }

  if (navigation_id == kUnknownNavigationId) {
    CancelProxyAuthChallenge(std::move(callback));
    return;
  }

  // The web layer fails whichever navigation is pending when the callback
  // runs. If the attributed navigation is gone (e.g. replaced while the access
  // token was fetched), the error would fail an unrelated navigation, so it is
  // dropped instead, along with the disguised error just recorded for it.
  if (!IsPendingNavigation(web_state.get(), navigation_id)) {
    enterprise_proxy_error_service->RemoveDisguisedError(navigation_id);
    CancelProxyAuthChallenge(std::move(callback));
    return;
  }

  // Without the Secure Gateway error page, the request continues without
  // credentials, and `EnterpriseProxyTabHelper` drops the disguised error when
  // the navigation finishes.
  if (!enterprise_net::IsEnterpriseProxyErrorHandlingEnabled()) {
    CancelProxyAuthChallenge(std::move(callback));
    return;
  }

  std::move(callback).Run(
      /*username=*/nil, /*password=*/nil,
      [NSError errorWithDomain:kEnterpriseProxyErrorDomain
                          code:kEnterpriseProxyErrorCode
                      userInfo:nil]);
}

// Records the routing `rules` given to WebKit, and the number of rules left out
// of them because WebKit supports none of their proxies.
void RecordRoutingRules(const std::vector<web::ProxyRule>& rules,
                        size_t unsupported_rule_count) {
  size_t match_domain_count = 0;
  for (const web::ProxyRule& rule : rules) {
    match_domain_count += rule.match_domains.size();
  }
  base::UmaHistogramCounts10000(kRoutingRuleCountHistogram,
                                base::saturated_cast<int>(rules.size()));
  base::UmaHistogramCounts100000(kRoutingMatchDomainCountHistogram,
                                 base::saturated_cast<int>(match_domain_count));
  base::UmaHistogramCounts1000(
      kRoutingUnsupportedRuleCountHistogram,
      base::saturated_cast<int>(unsupported_rule_count));
}

}  // namespace

ProxyServiceController::ProxyServiceController(
    enterprise_net::EnterpriseProxyService* enterprise_proxy_service,
    enterprise_net::EnterpriseProxyErrorService* enterprise_proxy_error_service,
    web::ProxyConfigurationProvider* proxy_configuration_provider)
    : enterprise_proxy_service_(enterprise_proxy_service),
      enterprise_proxy_error_service_(enterprise_proxy_error_service),
      proxy_configuration_provider_(proxy_configuration_provider) {
  CHECK(enterprise_proxy_service_);
  CHECK(enterprise_proxy_error_service_);
  CHECK(proxy_configuration_provider_);
  scoped_observation_.Observe(enterprise_proxy_service_);
  // `base::Unretained()` is safe: the subscription, owned by `this`, removes
  // the callback when destroyed.
  proxy_configurations_applied_subscription_ =
      proxy_configuration_provider_->RegisterProxyConfigurationsAppliedCallback(
          base::BindRepeating(
              &ProxyServiceController::OnProxyConfigurationsApplied,
              base::Unretained(this)));
  OnDynamicProxyConfigsStatusChanged();
}

ProxyServiceController::~ProxyServiceController() = default;

bool ProxyServiceController::MaybeHandleProxyAuthChallenge(
    web::WebState* source,
    NSURLProtectionSpace* protection_space,
    NSURLCredential* proposed_credential,
    NSURLResponse* failure_response,
    web::WebStateDelegate::ProxyAuthCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(source);

  // Null once `Shutdown()` has run.
  if (!enterprise_proxy_error_service_) {
    return false;
  }

  scoped_refptr<net::HttpResponseHeaders> response_headers =
      CreateProxyAuthHeadersFromNSURLResponse(failure_response);
  std::optional<net::AuthChallengeInfo> auth_info =
      CreateAuthChallengeInfoFromProtectionSpace(protection_space,
                                                 response_headers);
  if (!auth_info.has_value()) {
    return false;
  }

  const GURL destination_url = net::GURLWithNSURL(failure_response.URL);
  const int64_t navigation_id =
      GetNavigationIdForChallenge(source, destination_url);
  std::optional<
      enterprise_net::EnterpriseProxyErrorService::PendingInterception>
      interception =
          enterprise_proxy_error_service_->EvaluateProxyAuthChallenge(
              *auth_info, destination_url, response_headers, navigation_id);
  if (!interception.has_value()) {
    return false;
  }

  // `base::Unretained()` is safe because `enterprise_proxy_error_service_` runs
  // the callback itself, synchronously or from a continuation weakly bound to
  // itself, and must never post it. The callback can't be weakly bound instead:
  // it must always run, as WebKit requires its completion handler to be called.
  enterprise_proxy_error_service_->ResolveProxyAuthChallenge(
      *std::move(interception),
      base::BindOnce(&RunProxyAuthCallback,
                     base::Unretained(enterprise_proxy_error_service_.get()),
                     source->GetWeakPtr(), navigation_id, std::move(callback)));
  return true;
}

void ProxyServiceController::Shutdown() {
  scoped_observation_.Reset();
  proxy_configurations_applied_subscription_ = {};
  enterprise_proxy_service_ = nullptr;
  enterprise_proxy_error_service_ = nullptr;
  proxy_configuration_provider_ = nullptr;
}

void ProxyServiceController::OnDynamicProxyConfigsStatusChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(enterprise_proxy_service_);
  CHECK(proxy_configuration_provider_);
  // WebKit has a bug prior to iOS 18.1 where it does not deliver proxy
  // authentication challenges, so dynamic proxy routing configurations are not
  // applied on older OS versions.
  if (!@available(iOS 18.1, *)) {
    return;
  }
  const base::TimeTicks update_time = base::TimeTicks::Now();
  const net::ProxyConfig::DynamicRoutingConfig routing_config =
      enterprise_proxy_service_->GetDynamicRoutingConfig();
  std::vector<web::ProxyRule> rules =
      web::TranslateProvisioningDomainRoutingConfig(routing_config);
  // The translation only leaves out the rules without a proxy that WebKit
  // supports.
  DCHECK_GE(routing_config.routing_rules.size(), rules.size());
  const size_t unsupported_rule_count =
      routing_config.routing_rules.size() - rules.size();
  // Both start empty, so nothing is recorded for a profile without routes. The
  // rules left out are compared too, so that a config where WebKit supports
  // none of the rules is recorded.
  const bool rules_changed = rules != rules_;
  if (rules_changed) {
    rules_change_time_ = update_time;
  }
  if (rules_changed || unsupported_rule_count != unsupported_rule_count_) {
    RecordRoutingRules(rules, unsupported_rule_count);
    rules_ = rules;
    unsupported_rule_count_ = unsupported_rule_count;
  }
  last_request_id_ =
      proxy_configuration_provider_->UpdateProxyConfiguration(std::move(rules));
}

void ProxyServiceController::OnProxyConfigurationsApplied(
    web::ProxyConfigurationProvider::RequestId request_id,
    size_t config_count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Configurations set for another caller don't reflect `rules_`.
  // `rules_change_time_` is unset when the rules are set again without having
  // changed.
  if (request_id != last_request_id_ || !rules_change_time_.has_value()) {
    return;
  }
  base::UmaHistogramTimes(kRoutingApplyTimeHistogram,
                          base::TimeTicks::Now() - *rules_change_time_);
  base::UmaHistogramCounts10000(kRoutingNativeConfigCountHistogram,
                                base::saturated_cast<int>(config_count));
  rules_change_time_.reset();
}
