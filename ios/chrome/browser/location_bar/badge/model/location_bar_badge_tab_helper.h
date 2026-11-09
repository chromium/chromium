// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_LOCATION_BAR_BADGE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_LOCATION_BAR_BADGE_TAB_HELPER_H_

#import <cstdint>
#import <map>
#import <optional>

#import "base/observer_list.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/location_bar/badge/model/badge_type.h"
#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_request.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

class LocationBarBadgeTabHelperObserver;

namespace web {
class NavigationContext;
class WebState;
}  // namespace web

// Tracks active location bar badges and arbitrates priority for a
// `web::WebState`.
class LocationBarBadgeTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<LocationBarBadgeTabHelper> {
 public:
  LocationBarBadgeTabHelper(const LocationBarBadgeTabHelper&) = delete;
  LocationBarBadgeTabHelper& operator=(const LocationBarBadgeTabHelper&) =
      delete;

  ~LocationBarBadgeTabHelper() override;

  // Registers `observer` to be notified when the displayed badge changes.
  void AddObserver(LocationBarBadgeTabHelperObserver* observer);

  // Unregisters `observer`.
  void RemoveObserver(LocationBarBadgeTabHelperObserver* observer);

  // Registers or updates an active badge request. Preserves arrival order for
  // attribute-only updates at the same priority; assigns a new arrival sequence
  // when `request.priority` changes. `request.badge_type` must not be
  // `LocationBarBadgeType::kNone`.
  void SetBadge(LocationBarBadgeRequest request);

  // Removes an active badge of `type`, if present. `type` must not be
  // `LocationBarBadgeType::kNone`.
  void RemoveBadge(LocationBarBadgeType type);

  // Returns whether a badge of `type` is currently active. `type` must not be
  // `LocationBarBadgeType::kNone`.
  bool HasBadge(LocationBarBadgeType type) const;

  // Returns the currently displayed badge request, or `std::nullopt` if no
  // badges are active.
  std::optional<LocationBarBadgeRequest> GetDisplayedBadgeRequest() const;

  // Internal record tracking an active badge's request and arrival
  // sequence.
  struct LocationBarBadgeRecord {
    LocationBarBadgeRequest request;
    uint64_t sequence_number = 0;
  };

 private:
  friend class web::WebStateUserData<LocationBarBadgeTabHelper>;

  // `web_state` must be non-null and must outlive this object.
  explicit LocationBarBadgeTabHelper(web::WebState* web_state);

  // Re-evaluates the currently displayed badge and notifies `observers_` if
  // the displayed badge changed.
  void NotifyDisplayedBadgeChangedIfNecessary();

  // web::WebStateObserver:
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Scoped observation for the associated `web::WebState`.
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  // Currently active badges for this tab, mapped to their request and arrival
  // order.
  std::map<LocationBarBadgeType, LocationBarBadgeRecord> active_badges_;

  // Monotonically increasing sequence number for deterministic badge
  // tie-breaking.
  uint64_t next_sequence_number_ = 0;

  // Cached badge request that was last broadcast to observers.
  std::optional<LocationBarBadgeRequest> last_displayed_badge_request_;

  // List of registered observers. `/*check_empty=*/true` verifies that all
  // observers unregister before destruction.
  base::ObserverList<LocationBarBadgeTabHelperObserver, /*check_empty=*/true>
      observers_;
};

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_LOCATION_BAR_BADGE_TAB_HELPER_H_
