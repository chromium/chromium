// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/model/fake_location_bar_badge_observer.h"

@implementation FakeLocationBarBadgeObserver

#pragma mark - LocationBarBadgeTabHelperObserving

- (void)locationBarBadgeTabHelperDidUpdateDisplayedBadge:
    (LocationBarBadgeTabHelper*)tabHelper {
  _callCount++;
}

@end
