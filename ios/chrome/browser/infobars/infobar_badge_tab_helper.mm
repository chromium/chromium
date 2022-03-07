// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/infobar_badge_tab_helper.h"

#include "ios/chrome/browser/infobars/infobar_badge_tab_helper_delegate.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns |infobar|'s InfobarType.
InfobarType GetInfobarType(infobars::InfoBar* infobar) {
  return static_cast<InfoBarIOS*>(infobar)->infobar_type();
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

void InfobarBadgeTabHelper::UpdateBadgeForInfobarRead(
    InfobarType infobar_type) {
  if (infobar_badge_states_.find(infobar_type) == infobar_badge_states_.end())
    return;
  infobar_badge_states_[infobar_type] |= BadgeStateRead;
}

void InfobarBadgeTabHelper::UpdateBadgeForInfobarBannerPresented(
    InfobarType infobar_type) {
  if (infobar_badge_states_.find(infobar_type) == infobar_badge_states_.end())
    return;
  infobar_badge_states_[infobar_type] |= BadgeStatePresented;
  [delegate_ updateBadgesShownForWebState:web_state_];
}

void InfobarBadgeTabHelper::UpdateBadgeForInfobarBannerDismissed(
    InfobarType infobar_type) {
  if (infobar_badge_states_.find(infobar_type) == infobar_badge_states_.end())
    return;
  infobar_badge_states_[infobar_type] &= ~BadgeStatePresented;
  [delegate_ updateBadgesShownForWebState:web_state_];
}

std::map<InfobarType, BadgeState> InfobarBadgeTabHelper::GetInfobarBadgeStates()
    const {
  return infobar_badge_states_;
}

#pragma mark Private

void InfobarBadgeTabHelper::ResetStateForAddedInfobar(
    InfobarType infobar_type) {
  infobar_badge_states_[infobar_type] = BadgeStateNone;
  [delegate_ updateBadgesShownForWebState:web_state_];
}

void InfobarBadgeTabHelper::ResetStateForRemovedInfobar(
    InfobarType infobar_type) {
  infobar_badge_states_.erase(infobar_type);
  [delegate_ updateBadgesShownForWebState:web_state_];
}

void InfobarBadgeTabHelper::OnInfobarAcceptanceStateChanged(
    InfobarType infobar_type,
    bool accepted) {
  if (infobar_badge_states_.find(infobar_type) == infobar_badge_states_.end())
    return;
  if (accepted) {
    infobar_badge_states_[infobar_type] |= BadgeStateAccepted | BadgeStateRead;
  } else {
    infobar_badge_states_[infobar_type] &= ~BadgeStateAccepted;
  }
  [delegate_ updateBadgesShownForWebState:web_state_];
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
  if ([tab_helper_->delegate_
          badgeSupportedForInfobarType:GetInfobarType(infobar)]) {
    tab_helper_->ResetStateForAddedInfobar(GetInfobarType(infobar));
    infobar_accept_observer_->scoped_observations().AddObservation(
        static_cast<InfoBarIOS*>(infobar));
  }
}

void InfobarBadgeTabHelper::InfobarManagerObserver::OnInfoBarRemoved(
    infobars::InfoBar* infobar,
    bool animate) {
  if ([tab_helper_->delegate_
          badgeSupportedForInfobarType:GetInfobarType(infobar)]) {
    tab_helper_->ResetStateForRemovedInfobar(GetInfobarType(infobar));
    infobar_accept_observer_->scoped_observations().RemoveObservation(
        static_cast<InfoBarIOS*>(infobar));
  }
}

void InfobarBadgeTabHelper::InfobarManagerObserver::OnInfoBarReplaced(
    infobars::InfoBar* old_infobar,
    infobars::InfoBar* new_infobar) {
  // New permission infobar in the same tab should keep preserve previous
  // states.
  if (GetInfobarType(old_infobar) == InfobarType::kInfobarTypePermissions &&
      GetInfobarType(new_infobar) == InfobarType::kInfobarTypePermissions) {
    return;
  }
  OnInfoBarRemoved(old_infobar, /*animate=*/false);
  OnInfoBarAdded(new_infobar);
}

void InfobarBadgeTabHelper::InfobarManagerObserver::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  DCHECK(scoped_observation_.IsObservingSource(manager));
  scoped_observation_.Reset();
}
