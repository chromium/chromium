// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/follow/follow_tab_helper.h"

#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/chrome_url_util.h"
#include "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/follow/follow_action_state.h"
#import "ios/chrome/browser/follow/follow_iph_presenter.h"
#import "ios/chrome/browser/follow/follow_java_script_feature.h"
#import "ios/chrome/browser/follow/follow_menu_updater.h"
#import "ios/chrome/browser/follow/follow_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/follow/follow_provider.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/web_state.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The prefix of domain name that can be removed. It is used when generating the
// follow item text.
const std::string kRemovablePrefix = "www.";

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
    : web_state_(web_state), weak_ptr_factory_(this) {
  DCHECK(web_state_);
  web_state_observation_.Observe(web_state_);
}

void FollowTabHelper::set_follow_menu_updater(
    id<FollowMenuUpdater> follow_menu_updater) {
  DCHECK(web_state_);
  follow_menu_updater_ = follow_menu_updater;
  if (should_update_follow_item_ && !web_state_->IsLoading()) {
    // If the page has finished loading check if the Follow menu item should be
    // updated, if not it will be updated once the page finishes loading.
    FollowJavaScriptFeature::GetInstance()->GetFollowWebPageURLs(
        web_state_, base::BindOnce(^(FollowWebPageURLs* web_page_urls) {
          UpdateFollowMenuItem(web_page_urls);
        }));
  }
}

void FollowTabHelper::remove_follow_menu_updater() {
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
  // Do not show follow IPH when browsing Chrome URLs, such as NTP, flags,
  // version, sad tab, etc.
  if (UrlHasChromeScheme(web_state->GetVisibleURL())) {
    return;
  }

  switch (load_completion_status) {
    case web::PageLoadCompletionStatus::FAILURE:
      break;
    case web::PageLoadCompletionStatus::SUCCESS:
      FollowJavaScriptFeature::GetInstance()->GetFollowWebPageURLs(
          web_state, base::BindOnce(^(FollowWebPageURLs* web_page_urls) {
            if (follow_menu_updater_ && should_update_follow_item_) {
              UpdateFollowMenuItem(web_page_urls);
            }
            BOOL channel_recommended =
                ios::GetChromeBrowserProvider()
                    .GetFollowProvider()
                    ->GetRecommendedStatus(web_page_urls);
            feature_engagement::Tracker* tracker =
                feature_engagement::TrackerFactory::GetForBrowserState(
                    ChromeBrowserState::FromBrowserState(
                        web_state->GetBrowserState()));
            if (channel_recommended &&
                tracker->ShouldTriggerHelpUI(
                    feature_engagement::kIPHFollowWhileBrowsingFeature)) {
              DCHECK(follow_iph_presenter_);
              [follow_iph_presenter_ presentFollowWhileBrowsingIPH];
            }
          }));
  }
}

void FollowTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  DCHECK(web_state_observation_.IsObservingSource(web_state));
  web_state_observation_.Reset();
  web_state_ = nullptr;
}

void FollowTabHelper::UpdateFollowMenuItem(FollowWebPageURLs* web_page_urls) {
  BOOL status =
      ios::GetChromeBrowserProvider().GetFollowProvider()->GetFollowStatus(
          web_page_urls);

  std::string domainName =
      web::GetMainFrame(web_state_)->GetSecurityOrigin().host();
  if (domainName.substr(0, kRemovablePrefix.length()) == kRemovablePrefix) {
    domainName =
        domainName.substr(kRemovablePrefix.length(), domainName.length());
  }

  bool enabled = GetFollowActionState(web_state_) == FollowActionStateEnabled;

  [follow_menu_updater_
      updateFollowMenuItemWithFollowWebPageURLs:web_page_urls
                                         status:status
                                     domainName:base::SysUTF8ToNSString(
                                                    domainName)
                                        enabled:enabled];
  should_update_follow_item_ = false;
}

WEB_STATE_USER_DATA_KEY_IMPL(FollowTabHelper)
