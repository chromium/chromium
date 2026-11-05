// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_tab_helper.h"

#import <algorithm>
#import <map>
#import <optional>

#import "base/check.h"
#import "base/check_op.h"
#import "ios/chrome/browser/location_bar/badge/model/badge_type.h"
#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_tab_helper_observer.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"

namespace {

// Comparator for `std::max_element`: returns true if `candidate_badge_entry`
// has lower precedence than `competitor_badge_entry` (higher priority wins,
// earlier arrival with smaller sequence number breaks ties).
bool CompareBadgePrecedence(
    const std::pair<const LocationBarBadgeType,
                    LocationBarBadgeTabHelper::LocationBarBadgeRecord>&
        candidate_badge_entry,
    const std::pair<const LocationBarBadgeType,
                    LocationBarBadgeTabHelper::LocationBarBadgeRecord>&
        competitor_badge_entry) {
  const auto& candidate_badge_record = candidate_badge_entry.second;
  const auto& competitor_badge_record = competitor_badge_entry.second;
  if (candidate_badge_record.priority != competitor_badge_record.priority) {
    return candidate_badge_record.priority < competitor_badge_record.priority;
  }
  return candidate_badge_record.sequence_number >
         competitor_badge_record.sequence_number;
}

}  // namespace

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

void LocationBarBadgeTabHelper::SetBadge(LocationBarBadgeType type,
                                         LocationBarBadgePriority priority) {
  CHECK_NE(type, LocationBarBadgeType::kNone);
  if (!IsLocationBarBadgeMigrationEnabled()) {
    return;
  }
  const auto active_badge_iterator = active_badges_.find(type);
  if (active_badge_iterator != active_badges_.end() &&
      active_badge_iterator->second.priority == priority) {
    return;
  }
  const LocationBarBadgeRecord badge_record = {
      .priority = priority,
      .sequence_number = ++next_sequence_number_,
  };
  active_badges_.insert_or_assign(type, badge_record);
  NotifyDisplayedBadgeChangedIfNecessary();
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
  const auto highest_priority_badge_iterator = std::max_element(
      active_badges_.begin(), active_badges_.end(), &CompareBadgePrecedence);
  return highest_priority_badge_iterator->first;
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

void LocationBarBadgeTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  CHECK(navigation_context);
  if (!IsLocationBarBadgeMigrationEnabled() ||
      navigation_context->IsSameDocument()) {
    return;
  }
  // Keep persistent session badges (e.g. Incognito) and clear transient badges.
  const size_t erased_count =
      std::erase_if(active_badges_, [](const auto& badge_entry) {
        return badge_entry.first != LocationBarBadgeType::kIncognito;
      });
  if (erased_count > 0) {
    NotifyDisplayedBadgeChangedIfNecessary();
  }
}

void LocationBarBadgeTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state_observation_.Reset();
}
