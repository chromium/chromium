// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_FAKE_LOCATION_BAR_BADGE_TAB_HELPER_OBSERVER_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_FAKE_LOCATION_BAR_BADGE_TAB_HELPER_OBSERVER_H_

#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_tab_helper_observer.h"

class LocationBarBadgeTabHelper;

// Fake observer for `LocationBarBadgeTabHelper` to record notifications.
class FakeLocationBarBadgeTabHelperObserver
    : public LocationBarBadgeTabHelperObserver {
 public:
  FakeLocationBarBadgeTabHelperObserver();
  ~FakeLocationBarBadgeTabHelperObserver() override;

  FakeLocationBarBadgeTabHelperObserver(
      const FakeLocationBarBadgeTabHelperObserver&) = delete;
  FakeLocationBarBadgeTabHelperObserver& operator=(
      const FakeLocationBarBadgeTabHelperObserver&) = delete;

  // `LocationBarBadgeTabHelperObserver`:
  void OnDisplayedBadgeChanged(LocationBarBadgeTabHelper* tab_helper) override;
  void LocationBarBadgeTabHelperDestroyed(
      LocationBarBadgeTabHelper* tab_helper) override;

  // Returns the number of times `OnDisplayedBadgeChanged` was invoked.
  int call_count() const { return call_count_; }

  // Returns whether `LocationBarBadgeTabHelperDestroyed` was invoked.
  bool tab_helper_destroyed() const { return tab_helper_destroyed_; }

 private:
  int call_count_ = 0;
  bool tab_helper_destroyed_ = false;
};

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_FAKE_LOCATION_BAR_BADGE_TAB_HELPER_OBSERVER_H_
