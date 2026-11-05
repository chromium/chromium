// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_tab_helper.h"

#import <optional>

#import "base/check.h"
#import "base/check_op.h"
#import "ios/chrome/browser/location_bar/badge/model/badge_type.h"
#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_tab_helper_observer.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/web_state.h"

LocationBarBadgeTabHelper::LocationBarBadgeTabHelper(web::WebState* web_state) {
  CHECK(web_state);
  web_state_observation_.Observe(web_state);
}
LocationBarBadgeTabHelper::~LocationBarBadgeTabHelper() {
  for (LocationBarBadgeTabHelperObserver& observer : observers_) {
    observer.LocationBarBadgeTabHelperDestroyed(this);
  }
}

void LocationBarBadgeTabHelper::AddObserver(
    LocationBarBadgeTabHelperObserver* observer) {
  observers_.AddObserver(observer);
}

void LocationBarBadgeTabHelper::RemoveObserver(
    LocationBarBadgeTabHelperObserver* observer) {
  observers_.RemoveObserver(observer);
}

void LocationBarBadgeTabHelper::SetBadge(LocationBarBadgeType type) {
  CHECK_NE(type, LocationBarBadgeType::kNone);
  if (!IsLocationBarBadgeMigrationEnabled()) {
    return;
  }
  const auto [badge_iterator, was_inserted] = active_badges_.insert(type);
  if (was_inserted) {
    NotifyDisplayedBadgeChangedIfNecessary();
  }
}

void LocationBarBadgeTabHelper::RemoveBadge(LocationBarBadgeType type) {
  CHECK_NE(type, LocationBarBadgeType::kNone);
  if (!IsLocationBarBadgeMigrationEnabled()) {
    return;
  }
  if (active_badges_.erase(type) > 0) {
    NotifyDisplayedBadgeChangedIfNecessary();
  }
}

bool LocationBarBadgeTabHelper::HasBadge(LocationBarBadgeType type) const {
  CHECK_NE(type, LocationBarBadgeType::kNone);
  if (!IsLocationBarBadgeMigrationEnabled()) {
    return false;
  }
  return active_badges_.contains(type);
}

std::optional<LocationBarBadgeType>
LocationBarBadgeTabHelper::GetDisplayedBadge() const {
  if (!IsLocationBarBadgeMigrationEnabled() || active_badges_.empty()) {
    return std::nullopt;
  }
  return *active_badges_.begin();
}

void LocationBarBadgeTabHelper::NotifyDisplayedBadgeChangedIfNecessary() {
  const std::optional<LocationBarBadgeType> current_badge = GetDisplayedBadge();
  if (current_badge == last_displayed_badge_) {
    return;
  }
  last_displayed_badge_ = current_badge;
  for (LocationBarBadgeTabHelperObserver& observer : observers_) {
    observer.OnDisplayedBadgeChanged(this);
  }
}

void LocationBarBadgeTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state_observation_.Reset();
}
