// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/suspicious_site_tab_helper.h"

#import <algorithm>
#import <string>
#import <string_view>
#import <utility>

#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/security_interstitials/core/unsafe_resource.h"
#import "ios/chrome/browser/safe_browsing/model/suspicious_site_tab_helper_delegate.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_tab_helper.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

namespace {

constexpr std::string_view kWarningOutcomeHistogram =
    "SafeBrowsing.SuspiciousSiteWarning.WarningOutcome";
constexpr std::string_view kUserInteractionHistogram =
    "SafeBrowsing.SuspiciousSiteWarning.UserInteraction";

// Records `interaction` to
// `SafeBrowsing.SuspiciousSiteWarning.UserInteraction`.
void RecordUserInteraction(
    safe_browsing::SuspiciousSiteWarningUserInteraction interaction) {
  base::UmaHistogramEnumeration(kUserInteractionHistogram, interaction);
}

// Records `outcome` to `SafeBrowsing.SuspiciousSiteWarning.WarningOutcome`.
void RecordWarningOutcome(safe_browsing::SuspiciousSiteWarningOutcome outcome) {
  base::UmaHistogramEnumeration(kWarningOutcomeHistogram, outcome);
}

}  // namespace

SuspiciousSiteTabHelper::SuspiciousSiteTabHelper(
    web::WebState* web_state,
    scoped_refptr<HostContentSettingsMap> host_content_settings_map)
    : web_state_(web_state), allowlist_(std::move(host_content_settings_map)) {
  CHECK(web_state_);
  web_state_observation_.Observe(web_state_);
  if (SafeBrowsingQueryManager* query_manager =
          SafeBrowsingQueryManager::FromWebState(web_state_)) {
    query_manager_observation_.Observe(query_manager);
  }
}

SuspiciousSiteTabHelper::~SuspiciousSiteTabHelper() = default;

#pragma mark - Public

void SuspiciousSiteTabHelper::SetDelegate(
    id<SuspiciousSiteTabHelperDelegate> delegate) {
  delegate_ = delegate;
  if (delegate_) {
    MaybeShowWarning();
  }
}

void SuspiciousSiteTabHelper::OnSuspiciousSiteWarningTriggered(
    const security_interstitials::UnsafeResource& resource) {
  if (!resource.url.is_valid()) {
    // Ignore malformed resources with an invalid URL.
    return;
  }

  if (has_shown_warning_) {
    RecordUserInteraction(
        safe_browsing::SuspiciousSiteWarningUserInteraction::kManualNavigation);
    RecordWarningOutcome(safe_browsing::SuspiciousSiteWarningOutcome::kAdhered);
    if (is_warning_visible_) {
      [delegate_ dismissSuspiciousSiteWarning];
    }
    ResetWarningState();
  }

  pending_suspicious_url_ = resource.url;
  MaybeShowWarning();
}

void SuspiciousSiteTabHelper::OnBackToSafetyClicked() {
  if (!has_shown_warning_) {
    // Ignore duplicate or late callbacks after the warning has already been
    // resolved.
    return;
  }

  RecordUserInteraction(
      safe_browsing::SuspiciousSiteWarningUserInteraction::kBackToSafetyButton);
  RecordWarningOutcome(safe_browsing::SuspiciousSiteWarningOutcome::kAdhered);
  ResetWarningState();

  web::NavigationManager* navigation_manager =
      web_state_->GetNavigationManager();
  if (!navigation_manager) {
    // `WebState` may not have a `NavigationManager` in minimal test setups.
    return;
  }

  const std::string suspicious_host(web_state_->GetLastCommittedURL().host());
  const int last_committed_index =
      navigation_manager->GetLastCommittedItemIndex();
  for (int i = last_committed_index - 1; i >= 0; --i) {
    web::NavigationItem* item = navigation_manager->GetItemAtIndex(i);
    if (item && item->GetURL().host() != suspicious_host) {
      navigation_manager->GoToIndex(i);
      return;
    }
  }

  navigation_manager->LoadURLWithParams(
      web::NavigationManager::WebLoadParams(GURL(kChromeUINewTabURL)));
}

void SuspiciousSiteTabHelper::OnMarkAsSafeClicked() {
  if (!has_shown_warning_) {
    // Ignore duplicate or late callbacks after the warning has already been
    // resolved.
    return;
  }

  const GURL& committed_url = web_state_->GetLastCommittedURL();
  if (committed_url.is_valid() && committed_url.has_host()) {
    allowlist_.AllowSiteForHost(std::string(committed_url.host()));
  }

  RecordUserInteraction(
      safe_browsing::SuspiciousSiteWarningUserInteraction::kMarkAsSafe);
  RecordWarningOutcome(safe_browsing::SuspiciousSiteWarningOutcome::kBypassed);
  ResetWarningState();
}

void SuspiciousSiteTabHelper::OnWarningDismissedBySystem() {
  if (!has_shown_warning_ || !is_warning_visible_) {
    // Ignore dismissal callbacks triggered by programmatic suspension (e.g.
    // tab switch in `WasHidden`) or after the warning was already resolved.
    return;
  }

  RecordWarningOutcome(
      safe_browsing::SuspiciousSiteWarningOutcome::kDismissedBySystem);
  ResetWarningState();
}

#pragma mark - web::WebStateObserver

void SuspiciousSiteTabHelper::WasShown(web::WebState* web_state) {
  MaybeShowWarning();
}

void SuspiciousSiteTabHelper::WasHidden(web::WebState* web_state) {
  if (is_warning_visible_) {
    is_warning_visible_ = false;
    [delegate_ dismissSuspiciousSiteWarning];
  }
}

void SuspiciousSiteTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!navigation_context->IsSameDocument()) {
    has_committed_navigation_ = false;
  }
}

void SuspiciousSiteTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument()) {
    return;
  }

  if (!navigation_context->HasCommitted()) {
    if (has_shown_warning_) {
      // Restore `has_committed_navigation_` so that an aborted navigation (such
      // as a download or external scheme launch) does not prevent re-presenting
      // the active warning for the committed page when `WasShown` is called.
      has_committed_navigation_ = true;
    }
    MaybeShowWarning();
    return;
  }

  if (has_shown_warning_) {
    RecordUserInteraction(
        safe_browsing::SuspiciousSiteWarningUserInteraction::kManualNavigation);
    RecordWarningOutcome(safe_browsing::SuspiciousSiteWarningOutcome::kAdhered);
    if (is_warning_visible_) {
      [delegate_ dismissSuspiciousSiteWarning];
    }
    ResetWarningState();
  }

  if (navigation_context->GetError() != nil) {
    has_committed_navigation_ = false;
    ResetWarningState();
    return;
  }

  has_committed_navigation_ = true;
  MaybeShowWarning();
}

void SuspiciousSiteTabHelper::WebStateDestroyed(web::WebState* web_state) {
  if (has_shown_warning_) {
    RecordUserInteraction(
        safe_browsing::SuspiciousSiteWarningUserInteraction::kCloseTab);
    RecordWarningOutcome(safe_browsing::SuspiciousSiteWarningOutcome::kAdhered);
    ResetWarningState();
  }
  web_state_observation_.Reset();
  query_manager_observation_.Reset();
  web_state_ = nullptr;
}

#pragma mark - SafeBrowsingQueryManager::Observer

void SuspiciousSiteTabHelper::SafeBrowsingSyncQueryFinished(
    const SafeBrowsingQueryManager::QueryData& query_data) {
  MaybeShowWarning();
}

void SuspiciousSiteTabHelper::SafeBrowsingAsyncQueryFinished(
    const SafeBrowsingQueryManager::QueryData& query_data) {
  MaybeShowWarning();
}

void SuspiciousSiteTabHelper::SafeBrowsingQueryManagerDestroyed(
    SafeBrowsingQueryManager* manager) {
  query_manager_observation_.Reset();
}

#pragma mark - Private

void SuspiciousSiteTabHelper::MaybeShowWarning() {
  if (!pending_suspicious_url_.is_valid() || is_warning_visible_) {
    return;
  }

  SafeBrowsingTabHelper* sb_tab_helper =
      SafeBrowsingTabHelper::FromWebState(web_state_);
  if (!has_shown_warning_ && sb_tab_helper) {
    if (!sb_tab_helper->IsUrlInCurrentOrPendingNavigation(
            pending_suspicious_url_)) {
      // The navigation that triggered the warning was cancelled (for example,
      // by a blocking Safe Browsing interstitial) or superseded by another
      // navigation before the warning could be shown.
      ResetWarningState();
      return;
    }
    if (sb_tab_helper->HasPendingMainFrameChecks()) {
      // Wait until all sync and async checks in the redirect chain complete so
      // that blocking interstitials take precedence over suspicious site
      // warnings.
      return;
    }
  }

  if (!has_committed_navigation_) {
    return;
  }

  if (sb_tab_helper &&
      !std::ranges::contains(sb_tab_helper->GetRedirectChain(),
                             pending_suspicious_url_.GetWithoutRef())) {
    // The redirect chain containing `pending_suspicious_url_` has not committed
    // yet.
    return;
  }

  const GURL& committed_url = web_state_->GetLastCommittedURL();
  if (!committed_url.is_valid() || !committed_url.has_host()) {
    return;
  }

  if (allowlist_.IsSiteAllowedForHost(std::string(committed_url.host()))) {
    ResetWarningState();
    return;
  }

  if (!web_state_->IsVisible() || !delegate_) {
    return;
  }

  is_warning_visible_ = true;
  if (!has_shown_warning_) {
    has_shown_warning_ = true;
    RecordUserInteraction(
        safe_browsing::SuspiciousSiteWarningUserInteraction::kShown);
  }
  [delegate_ showSuspiciousSiteWarning];
}

void SuspiciousSiteTabHelper::ResetWarningState() {
  pending_suspicious_url_ = GURL();
  has_shown_warning_ = false;
  is_warning_visible_ = false;
}
