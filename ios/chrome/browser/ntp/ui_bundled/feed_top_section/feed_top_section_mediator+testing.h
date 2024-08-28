// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_TOP_SECTION_FEED_TOP_SECTION_MEDIATOR_TESTING_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_TOP_SECTION_FEED_TOP_SECTION_MEDIATOR_TESTING_H_

#import "ios/chrome/browser/ntp/ui_bundled/feed_top_section/feed_top_section_mediator.h"

@interface FeedTopSectionMediator (Testing)

// Exposing this method to return the user sign in status.
- (BOOL)isUserSignedIn;

// Exposing this method to return if the notifications promo should be shown.
- (BOOL)shouldShowNotificationsPromo;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_TOP_SECTION_FEED_TOP_SECTION_MEDIATOR_TESTING_H_
