// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/infobar_badge_tab_helper.h"

#include "ios/chrome/browser/infobars/infobar.h"
#include "ios/chrome/browser/infobars/infobar_badge_model.h"
#include "ios/chrome/browser/infobars/infobar_badge_tab_helper_delegate.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/infobars/infobar_ui_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - Public

// static
void InfobarBadgeTabHelper::CreateForWebState(web::WebState* web_state) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(
        UserDataKey(), base::WrapUnique(new InfobarBadgeTabHelper(web_state)));
  }
}

void InfobarBadgeTabHelper::SetDelegate(
    id<InfobarBadgeTabHelperDelegate> delegate) {
  delegate_ = delegate;
}

void InfobarBadgeTabHelper::UpdateBadgeForInfobarAccepted(
    InfobarType infobar_type) {
  infobar_badge_models_[infobar_type].badgeState |=
      BadgeStateAccepted | BadgeStateRead;
  [delegate_ updateInfobarBadge:infobar_badge_models_[infobar_type]];
}

void InfobarBadgeTabHelper::UpdateBadgeForInfobarReverted(
    InfobarType infobar_type) {
  infobar_badge_models_[infobar_type].badgeState &= ~BadgeStateAccepted;
  [delegate_ updateInfobarBadge:infobar_badge_models_[infobar_type]];
}

void InfobarBadgeTabHelper::UpdateBadgeForInfobarBannerPresented(
    InfobarType infobar_type) {
  infobar_badge_models_[infobar_type].badgeState |= BadgeStatePresented;
  [delegate_ updateInfobarBadge:infobar_badge_models_[infobar_type]];
}

void InfobarBadgeTabHelper::UpdateBadgeForInfobarBannerDismissed(
    InfobarType infobar_type) {
  infobar_badge_models_[infobar_type].badgeState &= ~BadgeStatePresented;
  [delegate_ updateInfobarBadge:infobar_badge_models_[infobar_type]];
}

std::vector<id<BadgeItem>> InfobarBadgeTabHelper::GetInfobarBadgeItems() {
  // Return all infobar badge items.
  std::vector<id<BadgeItem>> infobar_badges_items;
  for (auto const& infobar_badge : infobar_badge_models_) {
    infobar_badges_items.push_back(infobar_badge.second);
  }
  return infobar_badges_items;
}

InfobarBadgeTabHelper::~InfobarBadgeTabHelper() = default;

#pragma mark - Private

InfobarBadgeTabHelper::InfobarBadgeTabHelper(web::WebState* web_state)
    : infobar_observer_(this) {
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(web_state);
  if (infoBarManager) {
    infobar_observer_.Add(infoBarManager);
  }
}

#pragma mark InfobarObserver

void InfobarBadgeTabHelper::OnInfoBarAdded(infobars::InfoBar* infobar) {
  this->UpdateBadgeForInfobar(infobar, true);
}

void InfobarBadgeTabHelper::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                             bool animate) {
  this->UpdateBadgeForInfobar(infobar, false);
}

void InfobarBadgeTabHelper::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  infobar_observer_.Remove(manager);
}

#pragma mark Helpers

void InfobarBadgeTabHelper::UpdateBadgeForInfobar(infobars::InfoBar* infobar,
                                                  bool display) {
  InfoBarIOS* infobar_ios = static_cast<InfoBarIOS*>(infobar);
  id<InfobarUIDelegate> controller_ = infobar_ios->InfobarUIDelegate();
  if (IsInfobarUIRebootEnabled() && controller_.hasBadge) {
    InfobarType infobar_type = controller_.infobarType;
    if (display) {
      InfobarBadgeModel* new_badge =
          [[InfobarBadgeModel alloc] initWithInfobarType:infobar_type];
      infobar_badge_models_[infobar_type] = new_badge;
      [delegate_ addInfobarBadge:new_badge];
    } else {
      InfobarBadgeModel* removed_badge = infobar_badge_models_[infobar_type];
      infobar_badge_models_.erase(infobar_type);
      [delegate_ removeInfobarBadge:removed_badge];
    }
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(InfobarBadgeTabHelper)
