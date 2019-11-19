// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NTP_HOME_CONSTANT_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NTP_HOME_CONSTANT_H_

#import <UIKit/UIKit.h>

namespace ntp_home {

// Enum identifying the different panels displayed on the NTP.
enum PanelIdentifier {
  NONE,
  HOME_PANEL,
  BOOKMARKS_PANEL,
  RECENT_TABS_PANEL,
  INCOGNITO_PANEL,
};

// Type of content displayed when a NTP is opened, for UMA report. It should be
// treated as append-only.
// These match tools/metrics/histograms/enums.xml.
typedef NS_ENUM(NSUInteger, IOSNTPImpression) {
  // The NTP only displays the local suggestions.
  LOCAL_SUGGESTIONS = 0,
  // The NTP displays local and remote suggestions.
  REMOTE_SUGGESTIONS = 1,
  // The NTP displays local suggestions and remote suggestions are collapsed.
  REMOTE_COLLAPSED = 2,
  // Add new enum above COUNT.
  COUNT
};

// Returns the accessibility identifier used by the fake omnibox.
NSString* FakeOmniboxAccessibilityID();

// Distance between the Most Visited tiles and the suggestions on iPad.
extern const CGFloat kMostVisitedBottomMarginIPad;
// Distance between the Most Visited tiles and the suggestions on iPhone.
extern const CGFloat kMostVisitedBottomMarginIPhone;
// Height of the first suggestions peeking at the bottom of the screen.
extern const CGFloat kSuggestionPeekingHeight;

// Dimension of user's identity avatar as a square image.
extern const CGFloat kIdentityAvatarDimension;
// Margin around user's identity avatar.
extern const CGFloat kIdentityAvatarMargin;

// The background color of the NTP.
UIColor* kNTPBackgroundColor();

}  // namespace ntp_home

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NTP_HOME_CONSTANT_H_
