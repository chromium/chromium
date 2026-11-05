// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_LOCATION_BAR_BADGE_TAB_HELPER_OBSERVER_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_LOCATION_BAR_BADGE_TAB_HELPER_OBSERVER_H_

#import "base/observer_list_types.h"

class LocationBarBadgeTabHelper;

// Observes changes to the displayed location bar badge.
class LocationBarBadgeTabHelperObserver : public base::CheckedObserver {
 public:
  LocationBarBadgeTabHelperObserver() = default;

  LocationBarBadgeTabHelperObserver(const LocationBarBadgeTabHelperObserver&) =
      delete;
  LocationBarBadgeTabHelperObserver& operator=(
      const LocationBarBadgeTabHelperObserver&) = delete;

  ~LocationBarBadgeTabHelperObserver() override = default;

  // Invoked when the currently displayed location bar badge changes for
  // `tab_helper`.
  virtual void OnDisplayedBadgeChanged(
      LocationBarBadgeTabHelper* tab_helper) = 0;

  // Invoked when `tab_helper` is being destroyed. Observers should reset any
  // scoped observations or references to `tab_helper`.
  virtual void LocationBarBadgeTabHelperDestroyed(
      LocationBarBadgeTabHelper* tab_helper) {}
};

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_LOCATION_BAR_BADGE_TAB_HELPER_OBSERVER_H_
