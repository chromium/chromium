// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/model/fake_location_bar_badge_tab_helper_observer.h"

#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_tab_helper.h"

FakeLocationBarBadgeTabHelperObserver::FakeLocationBarBadgeTabHelperObserver() =
    default;

FakeLocationBarBadgeTabHelperObserver::
    ~FakeLocationBarBadgeTabHelperObserver() = default;

void FakeLocationBarBadgeTabHelperObserver::OnDisplayedBadgeChanged(
    LocationBarBadgeTabHelper* tab_helper) {
  call_count_++;
}

void FakeLocationBarBadgeTabHelperObserver::LocationBarBadgeTabHelperDestroyed(
    LocationBarBadgeTabHelper* tab_helper) {
  tab_helper_destroyed_ = true;
  tab_helper->RemoveObserver(this);
}
