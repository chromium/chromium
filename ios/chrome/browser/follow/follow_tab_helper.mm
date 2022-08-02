// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/follow/follow_tab_helper.h"

#import "base/callback.h"
#import "base/memory/ptr_util.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/cancelable_task_tracker.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/history/core/browser/history_service.h"
#import "components/history/core/browser/history_types.h"
#import "components/keyed_service/core/service_access_type.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/chrome_url_util.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/follow/follow_action_state.h"
#import "ios/chrome/browser/follow/follow_iph_presenter.h"
#import "ios/chrome/browser/follow/follow_java_script_feature.h"
#import "ios/chrome/browser/follow/follow_menu_updater.h"
#import "ios/chrome/browser/follow/follow_util.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/follow/follow_provider.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The prefix of domain name that can be removed. It is used when generating the
// follow item text.
const char kRemovablePrefix[] = "www.";

// Number of visits before the follow IPH is presented.
const int kDefaultDailyVisitMin = 3;
const int kDefaultNumVisitMin = 3;

// Constants used to query for previous visit to the page.
constexpr base::TimeDelta kVisitHostoryExclusiveDuration = base::Hours(1);
constexpr base::TimeDelta kVisitHostoryDuration = base::Days(14);

// Delay before displaying the IPH.
constexpr base::TimeDelta kShowFollowIPHAfterLoaded = base::Seconds(3);

}  // namespace.

FollowTabHelper::~FollowTabHelper() {
  DCHECK(!web_state_);
}

// static
void FollowTabHelper::CreateForWebState(web::WebState* web_state) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(UserDataKey(),
                           base::WrapUnique(new FollowTabHelper(web_state)));
  }
}

FollowTabHelper::FollowTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state_);
  web_state_observation_.Observe(web_state_);
}

void FollowTabHelper::SetFollowMenuUpdater(
    id<FollowMenuUpdater> follow_menu_updater) {
  DCHECK(web_state_);
  follow_menu_updater_ = follow_menu_updater;
  if (should_update_follow_item_ && !web_state_->IsLoading()) {
    // If the page has finished loading check if the Follow menu item should be
    // updated, if not it will be updated once the page finishes loading.
    FollowJavaScriptFeature::GetInstance()->GetFollowWebPageURLs(
        web_state_, base::BindOnce(&FollowTabHelper::UpdateFollowMenuItem,
                                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void FollowTabHelper::RemoveFollowMenuUpdater() {
  follow_menu_updater_ = nil;
  should_update_follow_item_ = true;
}

void FollowTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  should_update_follow_item_ = true;
}
void FollowTabHelper::DidRedirectNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  should_update_follow_item_ = true;
}

void FollowTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  // TODO(crbug.com/1340154): move the checking to `follow_iph_presenter_`
  // (FollowIPHCoordinator), so this class won't need to access browser_state
  // anymore, which brings convinience to testing.

  // Record when the page was successfully loaded. Computing whether the
  // IPH needs to be displayed is done asynchronously and the time used
  // to compute this will be removed from the delay before the IPH is
  // displayed.
  const base::Time page_load_time = base::Time::Now();

  // Do not update follow menu option and do not show IPH when browsing in
  // incognito.
  if (web_state->GetBrowserState()->IsOffTheRecord()) {
    return;
  }

  // Do not update follow menu option and do not show IPH when browsing non
  // http,https URLs and Chrome URLs, such as NTP, flags, version, sad tab, etc.
  const GURL& url = web_state->GetVisibleURL();
  if (UrlHasChromeScheme(url) || !url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  switch (load_completion_status) {
    case web::PageLoadCompletionStatus::FAILURE:
      break;
    case web::PageLoadCompletionStatus::SUCCESS:
      FollowJavaScriptFeature::GetInstance()->GetFollowWebPageURLs(
          web_state,
          base::BindOnce(&FollowTabHelper::OnSuccessfulPageLoad,
                         weak_ptr_factory_.GetWeakPtr(), url, page_load_time));
  }
}

void FollowTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  DCHECK(web_state_observation_.IsObservingSource(web_state));
  web_state_observation_.Reset();
  web_state_ = nullptr;
}

void FollowTabHelper::OnSuccessfulPageLoad(const GURL& url,
                                           base::Time page_load_time,
                                           FollowWebPageURLs* web_page_urls) {
  // Update follow menu option if needed.
  if (follow_menu_updater_ && should_update_follow_item_) {
    UpdateFollowMenuItem(web_page_urls);
  }

  // Show follow in-product help (IPH) if eligible.

  // Don't show follow in-product help (IPH) if there's no presenter. Ex.
  // follow_iph_presenter_ is nil when link preview page is loaded.
  if (!follow_iph_presenter_) {
    return;
  }

  feature_engagement::Tracker* feature_engagement_tracker =
      feature_engagement::TrackerFactory::GetForBrowserState(
          ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState()));
  // Do not show follow IPH if the feature engagement conditions are
  // not fulfilled. Ex. Do not show more than 5 Follow IPHs per week.
  if (!feature_engagement_tracker->WouldTriggerHelpUI(
          feature_engagement::kIPHFollowWhileBrowsingFeature)) {
    return;
  }

  recommended_url_ = ios::GetChromeBrowserProvider()
                         .GetFollowProvider()
                         ->GetRecommendedSiteURL(web_page_urls);

  // Do not show follow IPH if:
  // 1. The site is not recommended;
  // 2. The recommended url is empty (it happens if there's an error when
  // fetching);
  // 3. The IPH was shown too recently.
  if (!recommended_url_ || recommended_url_.absoluteString.length == 0 ||
      !IsFollowIPHShownFrequencyEligible(recommended_url_.host)) {
    return;
  }

  // Check if the site has enough visit count.
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForBrowserState(
          ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState()),
          ServiceAccessType::EXPLICIT_ACCESS);

  // Ignore any visits within the last hour so that we do not count
  // the current visit to the page.
  const base::Time end_time = page_load_time - kVisitHostoryExclusiveDuration;
  const base::Time begin_time = page_load_time - kVisitHostoryDuration;

  // Get daily visit count for `url` from the history service.
  history_service->GetDailyVisitsToHost(
      url, begin_time, end_time,
      base::BindOnce(&FollowTabHelper::OnDailyVisitQueryResult,
                     weak_ptr_factory_.GetWeakPtr(), page_load_time),
      &history_task_tracker_);
}

void FollowTabHelper::OnDailyVisitQueryResult(
    base::Time page_load_time,
    history::DailyVisitsResult result) {
  // Do not display the IPH if there are not enough visits.
  if (result.total_visits < kDefaultNumVisitMin ||
      result.days_with_visits < kDefaultDailyVisitMin) {
    return;
  }

  // Check how much time remains before the IPH needs to be displayed.
  const base::TimeDelta elapsed_time = base::Time::Now() - page_load_time;
  if (elapsed_time >= kShowFollowIPHAfterLoaded) {
    PresentFollowIPH();
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FollowTabHelper::PresentFollowIPH,
                       weak_ptr_factory_.GetWeakPtr()),
        kShowFollowIPHAfterLoaded - elapsed_time);
  }
}

void FollowTabHelper::UpdateFollowMenuItem(FollowWebPageURLs* web_page_urls) {
  DCHECK(web_state_);

  web::WebFrame* web_frame = web::GetMainFrame(web_state_);
  // Only update the follow menu item when web_page_urls is not null and when
  // webFrame can be retrieved. Otherwise, leave the option disabled.
  if (web_page_urls && web_frame) {
    BOOL status =
        ios::GetChromeBrowserProvider().GetFollowProvider()->GetFollowStatus(
            web_page_urls);

    std::string domain_name = web_frame->GetSecurityOrigin().host();
    if (base::StartsWith(domain_name, kRemovablePrefix)) {
      domain_name = domain_name.substr(strlen(kRemovablePrefix));
    }

    bool enabled = GetFollowActionState(web_state_) == FollowActionStateEnabled;

    [follow_menu_updater_
        updateFollowMenuItemWithFollowWebPageURLs:web_page_urls
                                           status:status
                                       domainName:base::SysUTF8ToNSString(
                                                      domain_name)
                                          enabled:enabled];
  }

  should_update_follow_item_ = false;
}

void FollowTabHelper::PresentFollowIPH() {
  DCHECK(follow_iph_presenter_);
  [follow_iph_presenter_ presentFollowWhileBrowsingIPH];
  StoreFollowIPHDisplayEvent(recommended_url_.host);
}

WEB_STATE_USER_DATA_KEY_IMPL(FollowTabHelper)
