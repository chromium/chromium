// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/infobar_badge_tab_helper.h"

#include "ios/chrome/browser/infobars/infobar_badge_model.h"
#include "ios/chrome/browser/infobars/infobar_badge_tab_helper_delegate.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#include "ios/chrome/browser/ui/badges/badge_type_util.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns |infobar|'s InfobarType.
InfobarType GetInfobarType(infobars::InfoBar* infobar) {
  return static_cast<InfoBarIOS*>(infobar)->infobar_type();
}
// Returns whether |infobar| supports badges.
bool SupportsBadges(infobars::InfoBar* infobar) {
  return BadgeTypeForInfobarType(GetInfobarType(infobar)) !=
         BadgeType::kBadgeTypeNone;
}
}  // namespace

#pragma mark - InfobarBadgeTabHelper

WEB_STATE_USER_DATA_KEY_IMPL(InfobarBadgeTabHelper)

InfobarBadgeTabHelper::InfobarBadgeTabHelper(web::WebState* web_state)
    : infobar_accept_observer_(this),
      infobar_manager_observer_(this, web_state, &infobar_accept_observer_),
      web_state_(web_state) {}

InfobarBadgeTabHelper::~InfobarBadgeTabHelper() = default;

#pragma mark Public

void InfobarBadgeTabHelper::SetDelegate(
    id<InfobarBadgeTabHelperDelegate> delegate) {
  delegate_ = delegate;
}

void InfobarBadgeTabHelper::UpdateBadgeForInfobarAccepted(
    InfobarType infobar_type) {
  OnInfobarAcceptanceStateChanged(infobar_type, /*accepted=*/true);
}

void InfobarBadgeTabHelper::UpdateBadgeForInfobarReverted(
    InfobarType infobar_type) {
  OnInfobarAcceptanceStateChanged(infobar_type, /*accepted=*/false);
}

void InfobarBadgeTabHelper::UpdateBadgeForInfobarBannerPresented(
    InfobarType infobar_type) {
  infobar_badge_models_[infobar_type].badgeState |= BadgeStatePresented;
  [delegate_ updateInfobarBadge:infobar_badge_models_[infobar_type]
                    forWebState:web_state_];
}

void InfobarBadgeTabHelper::UpdateBadgeForInfobarBannerDismissed(
    InfobarType infobar_type) {
  infobar_badge_models_[infobar_type].badgeState &= ~BadgeStatePresented;
  [delegate_ updateInfobarBadge:infobar_badge_models_[infobar_type]
                    forWebState:web_state_];
}

NSArray<id<BadgeItem>>* InfobarBadgeTabHelper::GetInfobarBadgeItems() {
  NSMutableArray* badge_items = [NSMutableArray array];
  for (auto& infobar_type_badge_model_pair : infobar_badge_models_) {
    id<BadgeItem> badge = infobar_type_badge_model_pair.second;
    if (badge)
      [badge_items addObject:badge];
  }
  return badge_items;
}

#pragma mark Private

void InfobarBadgeTabHelper::ResetStateForAddedInfobar(
    InfobarType infobar_type) {
  InfobarBadgeModel* new_badge =
      [[InfobarBadgeModel alloc] initWithInfobarType:infobar_type];
  infobar_badge_models_[infobar_type] = new_badge;
  [delegate_ addInfobarBadge:new_badge forWebState:web_state_];
}

void InfobarBadgeTabHelper::ResetStateForRemovedInfobar(
    InfobarType infobar_type) {
  InfobarBadgeModel* removed_badge = infobar_badge_models_[infobar_type];
  infobar_badge_models_[infobar_type] = nil;
  [delegate_ removeInfobarBadge:removed_badge forWebState:web_state_];
}

void InfobarBadgeTabHelper::OnInfobarAcceptanceStateChanged(
    InfobarType infobar_type,
    bool accepted) {
  id<BadgeItem> item = infobar_badge_models_[infobar_type];
  if (accepted) {
    item.badgeState |= BadgeStateAccepted | BadgeStateRead;
  } else {
    item.badgeState &= ~BadgeStateAccepted;
  }
  [delegate_ updateInfobarBadge:item forWebState:web_state_];
}

#pragma mark - InfobarBadgeTabHelper::InfobarAcceptanceObserver

InfobarBadgeTabHelper::InfobarAcceptanceObserver::InfobarAcceptanceObserver(
    InfobarBadgeTabHelper* tab_helper)
    : tab_helper_(tab_helper) {
  DCHECK(tab_helper_);
}

InfobarBadgeTabHelper::InfobarAcceptanceObserver::~InfobarAcceptanceObserver() =
    default;

void InfobarBadgeTabHelper::InfobarAcceptanceObserver::DidUpdateAcceptedState(
    InfoBarIOS* infobar) {
  tab_helper_->OnInfobarAcceptanceStateChanged(GetInfobarType(infobar),
                                               infobar->accepted());
}

void InfobarBadgeTabHelper::InfobarAcceptanceObserver::InfobarDestroyed(
    InfoBarIOS* infobar) {
  scoped_observations_.RemoveObservation(infobar);
}

#pragma mark - InfobarBadgeTabHelper::InfobarManagerObserver

InfobarBadgeTabHelper::InfobarManagerObserver::InfobarManagerObserver(
    InfobarBadgeTabHelper* tab_helper,
    web::WebState* web_state,
    InfobarAcceptanceObserver* infobar_accept_observer)
    : tab_helper_(tab_helper),
      infobar_accept_observer_(infobar_accept_observer) {
  DCHECK(tab_helper_);
  DCHECK(infobar_accept_observer_);
  scoped_observation_.Observe(InfoBarManagerImpl::FromWebState(web_state));
}

InfobarBadgeTabHelper::InfobarManagerObserver::~InfobarManagerObserver() =
    default;

void InfobarBadgeTabHelper::InfobarManagerObserver::OnInfoBarAdded(
    infobars::InfoBar* infobar) {
  if (SupportsBadges(infobar)) {
    tab_helper_->ResetStateForAddedInfobar(GetInfobarType(infobar));
    infobar_accept_observer_->scoped_observations().AddObservation(
        static_cast<InfoBarIOS*>(infobar));
  }
}

void InfobarBadgeTabHelper::InfobarManagerObserver::OnInfoBarRemoved(
    infobars::InfoBar* infobar,
    bool animate) {
  if (SupportsBadges(infobar)) {
    tab_helper_->ResetStateForRemovedInfobar(GetInfobarType(infobar));
    infobar_accept_observer_->scoped_observations().RemoveObservation(
        static_cast<InfoBarIOS*>(infobar));
  }
}

void InfobarBadgeTabHelper::InfobarManagerObserver::OnInfoBarReplaced(
    infobars::InfoBar* old_infobar,
    infobars::InfoBar* new_infobar) {
  OnInfoBarRemoved(old_infobar, /*animate=*/false);
  OnInfoBarAdded(new_infobar);
}

void InfobarBadgeTabHelper::InfobarManagerObserver::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  DCHECK(scoped_observation_.IsObservingSource(manager));
  scoped_observation_.Reset();
}
