// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/ptr_util.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper_delegate.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/web/common/features.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Internally the NTP URL is about://newtab/.  However, with
// `url::kAboutScheme`, there's no host value, only a path.  Use this value for
// matching the NTP.
const char kAboutNewTabPath[] = "//newtab/";

}  // namespace

NewTabPageTabHelper::~NewTabPageTabHelper() = default;

NewTabPageTabHelper::NewTabPageTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state->AddObserver(this);
  active_ = IsNTPURL(web_state_->GetVisibleURL());
  next_ntp_feed_type_ = DefaultFeedType();
}

#pragma mark - Static

void NewTabPageTabHelper::UpdateItem(web::NavigationItem* item) {
  if (item && item->GetURL() == GURL(kChromeUIAboutNewTabURL)) {
    item->SetVirtualURL(GURL(kChromeUINewTabURL));
    item->SetTitle(l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));
  }
}

FeedType NewTabPageTabHelper::DefaultFeedType() {
  return FeedTypeDiscover;
}

#pragma mark - Public

void NewTabPageTabHelper::SetDelegate(
    id<NewTabPageTabHelperDelegate> delegate) {
  delegate_ = delegate;
  active_ = IsNTPURL(web_state_->GetVisibleURL());
  if (active_) {
    UpdateItem(web_state_->GetNavigationManager()->GetPendingItem());
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

bool NewTabPageTabHelper::IsNTPURL(const GURL& url) {
  // `url` can be chrome://newtab/ or about://newtab/ depending on where `url`
  // comes from (the VisibleURL chrome:// from a navigation item or the actual
  // webView url about://).  If the url is about://newtab/, there is no origin
  // to match, so instead check the scheme and the path.
  return url.DeprecatedGetOriginAsURL() == kChromeUINewTabURL ||
         (url.SchemeIs(url::kAboutScheme) && url.path() == kAboutNewTabPath);
}

FeedType NewTabPageTabHelper::GetNextNTPFeedType() {
  return next_ntp_feed_type_;
}

void NewTabPageTabHelper::SetNextNTPFeedType(FeedType feed_type) {
  next_ntp_feed_type_ = feed_type;
}

bool NewTabPageTabHelper::GetNextNTPScrolledToFeed() {
  bool scrolled_ = next_ntp_scrolled_to_feed_;
  // Resets next_ntp_scrolled_to_feed_ in case it was overriden by
  // SetNextNTPScrolledToFeed.
  next_ntp_scrolled_to_feed_ = NO;
  return scrolled_;
}

void NewTabPageTabHelper::SetNextNTPScrolledToFeed(bool scrolled_to_feed) {
  next_ntp_scrolled_to_feed_ = scrolled_to_feed;
}

void NewTabPageTabHelper::SaveNTPState(CGFloat scroll_position,
                                       FeedType feed_type) {
  saved_scroll_position_ = scroll_position;
  SetNextNTPFeedType(feed_type);
}

CGFloat NewTabPageTabHelper::ScrollPositionFromSavedState() {
  return saved_scroll_position_;
}

#pragma mark - WebStateObserver

void NewTabPageTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
  SetActive(false);
}

void NewTabPageTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (IsNTPURL(navigation_context->GetUrl())) {
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
    if (IsNTPURL(web_state->GetVisibleURL())) {
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
