// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/follow/model/follow_tab_helper.h"

#import "base/functional/callback.h"
#import "base/memory/ptr_util.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/cancelable_task_tracker.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/single_thread_task_runner.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/history/core/browser/history_service.h"
#import "components/history/core/browser/history_types.h"
#import "components/keyed_service/core/service_access_type.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/follow/model/follow_action_state.h"
#import "ios/chrome/browser/follow/model/follow_features.h"
#import "ios/chrome/browser/follow/model/follow_java_script_feature.h"
#import "ios/chrome/browser/follow/model/follow_menu_updater.h"
#import "ios/chrome/browser/follow/model/follow_service.h"
#import "ios/chrome/browser/follow/model/follow_service_factory.h"
#import "ios/chrome/browser/follow/model/follow_util.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace {

// The prefix of domain name that can be removed. It is used when generating the
// follow item text.
const char kRemovablePrefix[] = "www.";

}  // namespace.

FollowTabHelper::~FollowTabHelper() {
  DCHECK(!web_state_);
}

FollowTabHelper::FollowTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  // Ensure that follow is not enabled for incognito.
  DCHECK(web_state_);
  DCHECK(!web_state_->GetBrowserState()->IsOffTheRecord());
  web_state_observation_.Observe(web_state_.get());
}

void FollowTabHelper::set_help_handler(id<HelpCommands> help_handler) {
  if (!IsWebChannelsEnabled()) {
    return;
  }
  help_handler_ = help_handler;
}

void FollowTabHelper::SetFollowMenuUpdater(
    id<FollowMenuUpdater> follow_menu_updater) {
  if (!IsWebChannelsEnabled()) {
    return;
  }
  DCHECK(web_state_);
  follow_menu_updater_ = follow_menu_updater;
}

void FollowTabHelper::UpdateFollowMenuItem() {
  if (!IsWebChannelsEnabled()) {
    return;
  }
  if (should_update_follow_item_) {
    FollowJavaScriptFeature::GetInstance()->GetWebPageURLs(
        web_state_,
        base::BindOnce(&FollowTabHelper::UpdateFollowMenuItemWithURL,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void FollowTabHelper::RemoveFollowMenuUpdater() {
  if (!IsWebChannelsEnabled()) {
    return;
  }
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
  // Do not show follow IPH if the user is not signed in.
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);
  if (!authenticationService || !authenticationService->GetPrimaryIdentity(
                                    signin::ConsentLevel::kSignin)) {
    return;
  }

  if (!IsWebChannelsEnabled()) {
    return;
  }

  // Do not show Follow IPH if it is disabled.
  if (!base::FeatureList::IsEnabled(
          feature_engagement::kIPHFollowWhileBrowsingFeature)) {
    return;
  }

  // Record when the page was successfully loaded. Computing whether the
  // IPH needs to be displayed is done asynchronously and the time used
  // to compute this will be removed from the delay before the IPH is
  // displayed.
  const base::Time page_load_time = base::Time::Now();

  // Do not show IPH when browsing non http, https URLs and Chrome URLs, such as
  // NTP, flags, version, sad tab, etc.
  const GURL& url = web_state->GetVisibleURL();
  if (UrlHasChromeScheme(url) || !url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  switch (load_completion_status) {
    case web::PageLoadCompletionStatus::FAILURE:
      break;
    case web::PageLoadCompletionStatus::SUCCESS:
      FollowJavaScriptFeature::GetInstance()->GetWebPageURLs(
          web_state,
          base::BindOnce(&FollowTabHelper::OnSuccessfulPageLoad,
                         weak_ptr_factory_.GetWeakPtr(), url, page_load_time));
      break;
  }
}

void FollowTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  DCHECK(web_state_observation_.IsObservingSource(web_state));
  weak_ptr_factory_.InvalidateWeakPtrs();
  web_state_observation_.Reset();
  web_state_ = nullptr;
}

void FollowTabHelper::OnSuccessfulPageLoad(const GURL& url,
                                           base::Time page_load_time,
                                           WebPageURLs* web_page_urls) {
  DCHECK(web_state_);

  // Don't show follow in-product help (IPH) if there's no help handler. Ex.
  // help_handler_ is nil when link preview page is loaded.
  if (!help_handler_) {
    return;
  }

  // Always show IPH for eligible website if experimental setting is enabled.
  if (experimental_flags::ShouldAlwaysShowFollowIPH()) {
    // A non-nil URL is required to display the IPH (as PresentFollowIPH
    // crash when trying to store a nil URL). Use the -URL property of
    // `web_page_urls`.
    PresentFollowIPH(web_page_urls.URL);
    return;
  }

  feature_engagement::Tracker* feature_engagement_tracker =
      feature_engagement::TrackerFactory::GetForProfile(
          ProfileIOS::FromBrowserState(web_state_->GetBrowserState()));
  // Do not show follow IPH if the feature engagement conditions are
  // not fulfilled. Ex. Do not show more than 5 Follow IPHs per week.
  if (!feature_engagement_tracker->WouldTriggerHelpUI(
          feature_engagement::kIPHFollowWhileBrowsingFeature)) {
    return;
  }

  NSURL* recommended_url =
      FollowServiceFactory::GetForProfile(
          ProfileIOS::FromBrowserState(web_state_->GetBrowserState()))
          ->GetRecommendedSiteURL(web_page_urls);

  // Do not show follow IPH if:
  // 1. The site is not recommended;
  // 2. The recommended url is empty (it happens if there's an error when
  // fetching);
  // 3. The IPH was shown too recently.
  if (!recommended_url || recommended_url.absoluteString.length == 0 ||
      !IsFollowIPHShownFrequencyEligible(recommended_url.host)) {
    return;
  }

  // Check if the site has enough visit count.
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForProfile(
          ProfileIOS::FromBrowserState(web_state_->GetBrowserState()),
          ServiceAccessType::EXPLICIT_ACCESS);

  // Ignore any visits within the last hour so that we do not count
  // the current visit to the page.
  const base::Time end_time =
      page_load_time - GetVisitHistoryExclusiveDuration();
  const base::Time begin_time = page_load_time - GetVisitHistoryDuration();

  // Get daily visit count for `url` from the history service.
  history_service->GetDailyVisitsToOrigin(
      url::Origin::Create(url), begin_time, end_time,
      base::BindOnce(&FollowTabHelper::OnDailyVisitQueryResult,
                     weak_ptr_factory_.GetWeakPtr(), page_load_time,
                     recommended_url),
      &history_task_tracker_);
}

void FollowTabHelper::OnDailyVisitQueryResult(
    base::Time page_load_time,
    NSURL* recommended_url,
    history::DailyVisitsResult result) {
  // Do not display the IPH if there are not enough visits.
  if (result.total_visits < GetNumVisitMin() ||
      result.days_with_visits < GetDailyVisitMin()) {
    return;
  }

  // Check how much time remains before the IPH needs to be displayed.
  const base::TimeDelta elapsed_time = base::Time::Now() - page_load_time;
  if (elapsed_time >= GetShowFollowIPHAfterLoaded()) {
    PresentFollowIPH(recommended_url);
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FollowTabHelper::PresentFollowIPH,
                       weak_ptr_factory_.GetWeakPtr(), recommended_url),
        GetShowFollowIPHAfterLoaded() - elapsed_time);
  }
}

void FollowTabHelper::UpdateFollowMenuItemWithURL(WebPageURLs* web_page_urls) {
  DCHECK(web_state_);

  web::WebFrame* web_frame = FollowJavaScriptFeature::GetInstance()
                                 ->GetWebFramesManager(web_state_)
                                 ->GetMainWebFrame();
  // Only update the follow menu item when web_page_urls is not null and when
  // webFrame can be retrieved. Otherwise, leave the option disabled.
  if (web_page_urls && web_frame) {
    const bool followed =
        FollowServiceFactory::GetForProfile(
            ProfileIOS::FromBrowserState(web_state_->GetBrowserState()))
            ->IsWebSiteFollowed(web_page_urls);

    std::string domain_name = web_frame->GetSecurityOrigin().host();
    if (base::StartsWith(domain_name, kRemovablePrefix)) {
      domain_name = domain_name.substr(strlen(kRemovablePrefix));
    }

    const bool enabled =
        GetFollowActionState(web_state_) == FollowActionStateEnabled;

    [follow_menu_updater_
        updateFollowMenuItemWithWebPage:web_page_urls
                               followed:followed
                             domainName:base::SysUTF8ToNSString(domain_name)
                                enabled:enabled];
  }

  should_update_follow_item_ = false;
}

void FollowTabHelper::PresentFollowIPH(NSURL* recommended_url) {
  DCHECK(help_handler_);
  [help_handler_
      presentInProductHelpWithType:InProductHelpType::kFollowWhileBrowsing];
  StoreFollowIPHDisplayEvent(recommended_url.host);
  if (experimental_flags::ShouldAlwaysShowFollowIPH()) {
    // Remove the follow IPH display event that just added because it's
    // triggered by experimental settings.
    RemoveLastFollowIPHDisplayEvent();
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(FollowTabHelper)
