// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/lookalikes/lookalike_url_tab_helper.h"

#import "components/lookalikes/core/lookalike_url_ui_util.h"
#import "components/lookalikes/core/lookalike_url_util.h"
#import "components/lookalikes/core/safety_tips_config.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "components/url_formatter/spoof_checks/top_domains/top_domain_util.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_container.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_error.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_tab_allow_list.h"
#import "ios/net/protocol_handler_util.h"
#import "net/base/apple/url_conversions.h"

namespace {
// Creates a PolicyDecision that allows the navigation.
web::WebStatePolicyDecider::PolicyDecision CreateAllowDecision() {
  return web::WebStatePolicyDecider::PolicyDecision::Allow();
}
}  // namespace

LookalikeUrlTabHelper::~LookalikeUrlTabHelper() = default;

LookalikeUrlTabHelper::LookalikeUrlTabHelper(web::WebState* web_state)
    : web::WebStatePolicyDecider(web_state) {}

void LookalikeUrlTabHelper::ShouldAllowResponse(
    NSURLResponse* response,
    web::WebStatePolicyDecider::ResponseInfo response_info,
    web::WebStatePolicyDecider::PolicyDecisionCallback callback) {
  // Ignore subframe navigations.
  if (!response_info.for_main_frame) {
    std::move(callback).Run(CreateAllowDecision());
    return;
  }
  // TODO(crbug.com/40755149): Only detect lookalike navigations if they're the
  // first or last URL in the redirect chain. Other URLs are invisible to the
  // user. Then, ensure UKM is set correctly to record which URL triggered.

  // TODO(crbug.com/40705072): Create container and ReleaseInterstitialParams.
  // Get stored interstitial parameters early. Doing so ensures that a
  // navigation to an irrelevant (for this interstitial's purposes) URL such as
  // chrome://settings while the lookalike interstitial is being shown clears
  // the stored state:
  // 1. User navigates to lookalike.tld which redirects to site.tld.
  // 2. Interstitial shown.
  // 3. User navigates to chrome://settings.
  // If, after this, the user somehow ends up on site.tld with a reload (e.g.
  // with ReloadType::ORIGINAL_REQUEST_URL), this will correctly not show an
  // interstitial.

  GURL response_url = net::GURLWithNSURL(response.URL);

  // If the URL is not an HTTP or HTTPS page, don't show any warning.
  if (!response_url.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(CreateAllowDecision());
    return;
  }

  // If the URL is in the allowlist, don't show any warning.
  LookalikeUrlTabAllowList* allow_list =
      LookalikeUrlTabAllowList::FromWebState(web_state());
  if (allow_list->IsDomainAllowed(response_url.host())) {
    std::move(callback).Run(CreateAllowDecision());
    return;
  }

  // Fetch the component allowlist.
  const auto* proto = lookalikes::GetSafetyTipsRemoteConfigProto();
  // When there's no proto (like at browser start), fail-safe and don't block.
  if (!proto) {
    std::move(callback).Run(CreateAllowDecision());
    return;
  }

  // TODO(crbug.com/40705072): If this is a reload and if the current
  // URL is the last URL of the stored redirect chain, the interstitial
  // was probably reloaded. Stop the reload and navigate back to the
  // original lookalike URL so that the full checks are exercised again.

  const lookalikes::DomainInfo navigated_domain =
      lookalikes::GetDomainInfo(response_url);
  // Empty domain_and_registry happens on private domains.
  if (navigated_domain.domain_and_registry.empty() ||
      IsTopDomain(navigated_domain)) {
    std::move(callback).Run(CreateAllowDecision());
    return;
  }

  // TODO(crbug.com/40705070): After site engagement has been componentized,
  // fetch and set `engaged_sites` here so that an interstitial won't be
  // shown on engaged sites, and so that the interstitial will be shown on
  // lookalikes of engaged sites.
  std::vector<lookalikes::DomainInfo> engaged_sites;
  std::string matched_domain;
  lookalikes::LookalikeUrlMatchType match_type =
      lookalikes::LookalikeUrlMatchType::kNone;
  // Target allowlist is not currently used in ios.
  const lookalikes::LookalikeTargetAllowlistChecker in_target_allowlist =
      base::BindRepeating(^(const std::string& hostname) {
        return false;
      });
  if (!GetMatchingDomain(navigated_domain, engaged_sites, in_target_allowlist,
                         proto, &matched_domain, &match_type)) {
    // If the URL fails a spoof check, and isn't in the component allowlist,
    // then show a spoof check interstitial.
    if (ShouldBlockBySpoofCheckResult(navigated_domain) &&
        !lookalikes::IsUrlAllowlistedBySafetyTipsComponent(
            proto, response_url.GetWithEmptyPath(),
            response_url.GetWithEmptyPath())) {
      match_type = lookalikes::LookalikeUrlMatchType::kFailedSpoofChecks;
      RecordUMAFromMatchType(match_type);
      LookalikeUrlContainer* lookalike_container =
          LookalikeUrlContainer::FromWebState(web_state());
      lookalike_container->SetLookalikeUrlInfo(/*suggested_url=*/GURL(),
                                               response_url, match_type);
      std::move(callback).Run(CreateLookalikeErrorDecision());
      return;
    }

    std::move(callback).Run(CreateAllowDecision());
    return;
  }
  DCHECK(!matched_domain.empty());

  RecordUMAFromMatchType(match_type);

  const std::string suggested_domain =
      lookalikes::GetETLDPlusOne(matched_domain);
  DCHECK(!suggested_domain.empty());
  GURL::Replacements replace_host;
  replace_host.SetHostStr(suggested_domain);
  const GURL suggested_url =
      response_url.ReplaceComponents(replace_host).GetWithEmptyPath();

  // If the URL is in the component updater allowlist, don't show any warning.
  if (lookalikes::IsUrlAllowlistedBySafetyTipsComponent(
          proto, response_url.GetWithEmptyPath(),
          suggested_url.GetWithEmptyPath())) {
    std::move(callback).Run(CreateAllowDecision());
    return;
  }

  // GetActionForMatchType checks gradual rollout which currently only controls
  // Safety Tips. Since Safety Tips aren't implemented on iOS, ignore gradual
  // rollout checks by passing null proto and unknown channel.
  if (GetActionForMatchType(nullptr, version_info::Channel::UNKNOWN,
                            navigated_domain.domain_and_registry, match_type) ==
      lookalikes::LookalikeActionType::kRecordMetrics) {
    // Interstitial normally records UKM, but still record when it's not shown.
    RecordUkmForLookalikeUrlBlockingPage(
        ukm::GetSourceIdForWebStateDocument(web_state()), match_type,
        lookalikes::LookalikeUrlBlockingPageUserAction::kInterstitialNotShown,
        /*triggered_by_initial_url=*/false);

    std::move(callback).Run(CreateAllowDecision());
    return;
  }

  LookalikeUrlContainer* lookalike_container =
      LookalikeUrlContainer::FromWebState(web_state());
  lookalike_container->SetLookalikeUrlInfo(suggested_url, response_url,
                                           match_type);
  std::move(callback).Run(CreateLookalikeErrorDecision());
}

WEB_STATE_USER_DATA_KEY_IMPL(LookalikeUrlTabHelper)
