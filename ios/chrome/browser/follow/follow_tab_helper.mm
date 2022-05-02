// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/follow/follow_tab_helper.h"

#include "base/memory/ptr_util.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/follow/follow_iph_presenter.h"
#import "ios/chrome/browser/follow/follow_java_script_feature.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/follow/follow_provider.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

void FollowTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  switch (load_completion_status) {
    case web::PageLoadCompletionStatus::FAILURE:
      break;
    case web::PageLoadCompletionStatus::SUCCESS:
      FollowJavaScriptFeature::GetInstance()->GetFollowWebPageURLs(
          web_state, base::BindOnce(^(FollowWebPageURLs* web_page_urls) {
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

WEB_STATE_USER_DATA_KEY_IMPL(FollowTabHelper)
