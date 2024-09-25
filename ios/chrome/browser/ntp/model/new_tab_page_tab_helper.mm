// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/ptr_util.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/discover_feed/model/feed_constants.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_state.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper_delegate.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/common/features.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

NewTabPageTabHelper::~NewTabPageTabHelper() = default;

NewTabPageTabHelper::NewTabPageTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state->AddObserver(this);
  active_ = IsUrlNtp(web_state_->GetVisibleURL());
  ntp_state_ = [[NewTabPageState alloc] init];

  // Assign sort type to NTP state from prefs.
  PrefService* pref_service =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState())->GetPrefs();
  ntp_state_.followingFeedSortType =
      (FollowingFeedSortType)pref_service->GetInteger(
          prefs::kNTPFollowingFeedSortType);
}

#pragma mark - Static

void NewTabPageTabHelper::UpdateItem(web::NavigationItem* item) {
  if (item && item->GetURL() == GURL(kChromeUIAboutNewTabURL)) {
    item->SetVirtualURL(GURL(kChromeUINewTabURL));
    item->SetTitle(l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));
  }
}

#pragma mark - Public

void NewTabPageTabHelper::SetDelegate(
    id<NewTabPageTabHelperDelegate> delegate) {
  delegate_ = delegate;
  if (delegate_) {
    active_ = IsUrlNtp(web_state_->GetVisibleURL());
    if (active_) {
      UpdateItem(web_state_->GetNavigationManager()->GetPendingItem());
    }
  }
}

bool NewTabPageTabHelper::ShouldShowStartSurface() const {
  return show_start_surface_;
}

void NewTabPageTabHelper::SetShowStartSurface(bool show_start_surface) {
  show_start_surface_ = show_start_surface;
}

bool NewTabPageTabHelper::IsActive() const {
  return active_;
}

void NewTabPageTabHelper::SetNTPState(NewTabPageState* ntpState) {
  ntp_state_.scrollPosition = ntpState.scrollPosition;
  ntp_state_.selectedFeed = ntpState.selectedFeed;
}

NewTabPageState* NewTabPageTabHelper::GetNTPState() {
  return ntp_state_;
}

#pragma mark - WebStateObserver

void NewTabPageTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
  SetActive(false);
}

void NewTabPageTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (IsUrlNtp(navigation_context->GetUrl())) {
    UpdateItem(web_state_->GetNavigationManager()->GetPendingItem());
  } else {
    SetActive(false);
  }
}

void NewTabPageTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  web::NavigationItem* item =
      web_state_->GetNavigationManager()->GetLastCommittedItem();
  if (navigation_context->IsSameDocument() || !item) {
    return;
  }

  UpdateItem(web_state_->GetNavigationManager()->GetLastCommittedItem());
}

void NewTabPageTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS) {
    if (IsUrlNtp(web_state->GetVisibleURL())) {
      SetActive(true);
    }
  }
}

#pragma mark - Private

void NewTabPageTabHelper::SetActive(bool active) {
  if (active_ == active) {
    return;
  }
  active_ = active;

  [delegate_ newTabPageHelperDidChangeVisibility:this forWebState:web_state_];
}

WEB_STATE_USER_DATA_KEY_IMPL(NewTabPageTabHelper)
