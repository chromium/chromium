// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/infobar_badge_tab_helper.h"

#import <algorithm>
#import <optional>

#import "base/check.h"
#import "ios/chrome/browser/infobars/model/infobar_badge_tab_helper_delegate.h"
#import "ios/chrome/browser/infobars/model/infobar_badge_tab_helper_observer.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/location_bar/badge/model/badge_type.h"
#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_request.h"
#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_tab_helper.h"
#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"

namespace {
// Returns `infobar`'s InfobarType.
InfobarType GetInfobarType(infobars::InfoBar* infobar) {
  return static_cast<InfoBarIOS*>(infobar)->infobar_type();
}

// Builds a `LocationBarBadgeRequest` from a badge type and `BadgeState`
// bitmask.
LocationBarBadgeRequest RequestForBadgeState(LocationBarBadgeType badge_type,
                                             BadgeState badge_state) {
  LocationBarBadgeRequest request;
  request.badge_type = badge_type;
  request.priority = LocationBarBadgePriority::kDefault;
  request.has_unread_indicator = !(badge_state & BadgeStateRead);
  request.accepted = (badge_state & BadgeStateAccepted) != 0;
  request.is_highlighted = (badge_state & BadgeStatePresented) != 0;
  return request;
}
}  // namespace

#pragma mark - InfobarBadgeTabHelper

InfobarBadgeTabHelper::InfobarBadgeTabHelper(web::WebState* web_state)
    : infobar_accept_observer_(this),
      infobar_manager_observer_(this, web_state, &infobar_accept_observer_),
      web_state_(web_state) {
  if (IsLocationBarBadgeMigrationEnabled() && web_state_) {
    web_state_observation_.Observe(web_state_);
  }
}

InfobarBadgeTabHelper::~InfobarBadgeTabHelper() = default;

#pragma mark Public

void InfobarBadgeTabHelper::AddObserver(
    InfobarBadgeTabHelperObserver* observer) {
  badge_updates_observers_.AddObserver(observer);
}

void InfobarBadgeTabHelper::RemoveObserver(
    InfobarBadgeTabHelperObserver* observer) {
  badge_updates_observers_.RemoveObserver(observer);
}

void InfobarBadgeTabHelper::SetDelegate(
    id<InfobarBadgeTabHelperDelegate> delegate) {
  delegate_ = delegate;
  if (delegate_ == nil) {
    return;
  }
  // Prerendering complete; register infobars using delegate.
  for (size_t index = 0; index < infobars_added_when_prerendering_.size();
       ++index) {
    RegisterInfobar(infobars_added_when_prerendering_.at(index));
  }
  infobars_added_when_prerendering_.clear();
  UpdateBadgesShown();
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
  if (!infobar_badge_states_.contains(infobar_type)) {
    return;
  }
  infobar_badge_states_[infobar_type] |= BadgeStateRead;
  SyncLocationBarBadge(infobar_type);
}

void InfobarBadgeTabHelper::UpdateBadgeForInfobarBannerPresented(
    InfobarType infobar_type) {
  if (!infobar_badge_states_.contains(infobar_type)) {
    return;
  }
  infobar_badge_states_[infobar_type] |= BadgeStatePresented;
  SyncLocationBarBadge(infobar_type);
  UpdateBadgesShown();
}

void InfobarBadgeTabHelper::UpdateBadgeForInfobarBannerDismissed(
    InfobarType infobar_type) {
  if (!infobar_badge_states_.contains(infobar_type)) {
    return;
  }
  infobar_badge_states_[infobar_type] &= ~BadgeStatePresented;
  SyncLocationBarBadge(infobar_type);
  UpdateBadgesShown();
}

std::map<InfobarType, BadgeState> InfobarBadgeTabHelper::GetInfobarBadgeStates()
    const {
  return infobar_badge_states_;
}

size_t InfobarBadgeTabHelper::GetInfobarBadgesCount() {
  return infobar_badge_states_.size();
}

#pragma mark Private

LocationBarBadgeTabHelper* InfobarBadgeTabHelper::GetLocationBarBadgeTabHelper()
    const {
  if (!IsLocationBarBadgeMigrationEnabled() || !web_state_) {
    return nullptr;
  }
  return LocationBarBadgeTabHelper::FromWebState(web_state_);
}

std::optional<LocationBarBadgeType> InfobarBadgeTabHelper::MigratedBadgeType(
    InfobarType infobar_type) const {
  if (infobar_type == InfobarType::kInfobarTypePermissions) {
    // TODO(crbug.com/466407913): Support dynamic microphone vs. camera
    // resolution for permissions infobars.
    return std::nullopt;
  }
  return LocationBarBadgeTypeForInfobarType(infobar_type);
}

bool InfobarBadgeTabHelper::IsMigratedBadgeSupported(
    InfobarType infobar_type) const {
  return IsLocationBarBadgeMigrationEnabled() &&
         GetLocationBarBadgeTabHelper() != nullptr &&
         MigratedBadgeType(infobar_type).has_value();
}

void InfobarBadgeTabHelper::SyncLocationBarBadge(InfobarType infobar_type) {
  LocationBarBadgeTabHelper* location_bar_badge_tab_helper =
      GetLocationBarBadgeTabHelper();
  if (!location_bar_badge_tab_helper) {
    return;
  }
  const std::optional<LocationBarBadgeType> badge_type =
      MigratedBadgeType(infobar_type);
  if (!badge_type.has_value()) {
    return;
  }
  const auto badge_state_iterator = infobar_badge_states_.find(infobar_type);
  if (badge_state_iterator == infobar_badge_states_.end()) {
    location_bar_badge_tab_helper->RemoveBadge(*badge_type);
    return;
  }
  location_bar_badge_tab_helper->SetBadge(
      RequestForBadgeState(*badge_type, badge_state_iterator->second));
}

void InfobarBadgeTabHelper::RegisterInfobar(infobars::InfoBar* infobar) {
  CHECK(infobar);
  const InfobarType infobar_type = GetInfobarType(infobar);
  const bool migrated_badge_supported = IsMigratedBadgeSupported(infobar_type);
  if (!delegate_) {
    // Defer legacy registration until SetDelegate is called.
    infobars_added_when_prerendering_.push_back(infobar);
    if (!migrated_badge_supported) {
      return;
    }
  }
  if (migrated_badge_supported ||
      [delegate_ badgeSupportedForInfobarType:infobar_type]) {
    InfoBarIOS* infobar_ios = static_cast<InfoBarIOS*>(infobar);
    const BadgeState initial_badge_state =
        infobar_ios->accepted() ? (BadgeStateAccepted | BadgeStateRead)
                                : BadgeStateNone;
    infobar_badge_states_.try_emplace(infobar_type, initial_badge_state);
    auto& accept_observations = infobar_accept_observer_.scoped_observations();
    if (!accept_observations.IsObservingSource(infobar_ios)) {
      accept_observations.AddObservation(infobar_ios);
    }
    SyncLocationBarBadge(infobar_type);
  }
}

void InfobarBadgeTabHelper::UnregisterInfobar(infobars::InfoBar* infobar) {
  CHECK(infobar);
  const InfobarType infobar_type = GetInfobarType(infobar);
  const bool migrated_badge_supported = IsMigratedBadgeSupported(infobar_type);
  if (!delegate_) {
    auto position =
        std::ranges::find(infobars_added_when_prerendering_, infobar);
    if (position != infobars_added_when_prerendering_.end()) {
      infobars_added_when_prerendering_.erase(position);
    }
    if (!migrated_badge_supported) {
      return;
    }
  }
  if (migrated_badge_supported ||
      [delegate_ badgeSupportedForInfobarType:infobar_type]) {
    infobar_badge_states_.erase(infobar_type);
    auto& accept_observations = infobar_accept_observer_.scoped_observations();
    InfoBarIOS* infobar_ios = static_cast<InfoBarIOS*>(infobar);
    if (accept_observations.IsObservingSource(infobar_ios)) {
      accept_observations.RemoveObservation(infobar_ios);
    } else {
      DUMP_WILL_BE_NOTREACHED()
          << "cannot find observed infobar with type: "
          << static_cast<int>(infobar_ios->infobar_type());
    }
    SyncLocationBarBadge(infobar_type);
  }
}

void InfobarBadgeTabHelper::OnInfobarAcceptanceStateChanged(
    InfobarType infobar_type,
    bool accepted) {
  if (!infobar_badge_states_.contains(infobar_type)) {
    return;
  }
  if (accepted) {
    infobar_badge_states_[infobar_type] |= BadgeStateAccepted | BadgeStateRead;
  } else {
    infobar_badge_states_[infobar_type] &= ~BadgeStateAccepted;
  }
  SyncLocationBarBadge(infobar_type);
  UpdateBadgesShown();
}

void InfobarBadgeTabHelper::UpdateBadgesShown() {
  [delegate_ updateBadgesShownForWebState:web_state_];

  // Notify all badge update observers.
  for (auto& observer : badge_updates_observers_) {
    observer.InfobarBadgesUpdated(this);
  }
}

void InfobarBadgeTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  CHECK(navigation_context);
  if (!IsLocationBarBadgeMigrationEnabled() ||
      navigation_context->IsSameDocument()) {
    return;
  }
  // `LocationBarBadgeTabHelper::DidStartNavigation` clears transient badges on
  // navigation start, whereas `InfoBarManagerImpl` only expires infobars when a
  // navigation commits (or retains them on non-committed navigations / client
  // redirects). Re-sync surviving infobar badges after any cross-document
  // navigation finishes.
  for (const auto& [infobar_type, badge_state] : infobar_badge_states_) {
    SyncLocationBarBadge(infobar_type);
  }
}

void InfobarBadgeTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state_observation_.Reset();
  web_state_ = nullptr;
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
  InfoBarIOS* infobar_ios = static_cast<InfoBarIOS*>(infobar);
  // Defensively ensure the infobar is currently observed before removal.
  if (scoped_observations_.IsObservingSource(infobar_ios)) {
    scoped_observations_.RemoveObservation(infobar_ios);
  } else {
    DUMP_WILL_BE_NOTREACHED() << "cannot find observed infobar with type: "
                              << static_cast<int>(infobar_ios->infobar_type());
  }
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
  tab_helper_->RegisterInfobar(infobar);
  // Ensure that any changes to the acceptance state prior to registering the
  // infobar are captured.
  tab_helper_->OnInfobarAcceptanceStateChanged(
      GetInfobarType(infobar), static_cast<InfoBarIOS*>(infobar)->accepted());
  tab_helper_->UpdateBadgesShown();
}

void InfobarBadgeTabHelper::InfobarManagerObserver::OnInfoBarRemoved(
    infobars::InfoBar* infobar,
    bool animate) {
  tab_helper_->UnregisterInfobar(infobar);
  tab_helper_->UpdateBadgesShown();
}

void InfobarBadgeTabHelper::InfobarManagerObserver::OnInfoBarReplaced(
    infobars::InfoBar* old_infobar,
    infobars::InfoBar* new_infobar) {
  // New permission infobar in the same tab should keep preserving previous
  // states.
  if (tab_helper_->delegate_ &&
      GetInfobarType(old_infobar) == InfobarType::kInfobarTypePermissions &&
      GetInfobarType(new_infobar) == InfobarType::kInfobarTypePermissions) {
    base::ScopedMultiSourceObservation<InfoBarIOS, InfoBarIOS::Observer>&
        infobar_accept_observations =
            infobar_accept_observer_->scoped_observations();
    InfoBarIOS* old_infobar_ios = static_cast<InfoBarIOS*>(old_infobar);
    // Defensively ensure the infobar is currently observed before removal.
    if (infobar_accept_observations.IsObservingSource(old_infobar_ios)) {
      infobar_accept_observations.RemoveObservation(old_infobar_ios);
    } else {
      DUMP_WILL_BE_NOTREACHED();
    }
    infobar_accept_observations.AddObservation(
        static_cast<InfoBarIOS*>(new_infobar));
    return;
  }
  OnInfoBarRemoved(old_infobar, /*animate=*/false);
  OnInfoBarAdded(new_infobar);
}

void InfobarBadgeTabHelper::InfobarManagerObserver::OnManagerWillBeDestroyed(
    infobars::InfoBarManager* manager) {
  DCHECK(scoped_observation_.IsObservingSource(manager));
  scoped_observation_.Reset();
}
