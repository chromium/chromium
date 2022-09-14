// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NTP_HOME_METRICS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NTP_HOME_METRICS_H_

#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"

#import "ios/chrome/browser/metrics/new_tab_page_uma.h"

class ChromeBrowserState;

namespace web {
class WebState;
}

namespace ntp_home {

// Records an NTP impression of type `impression_type`.
void RecordNTPImpression(ntp_home::IOSNTPImpression impression_type);

}  // namespace ntp_home

// These values are persisted to IOS.ContentSuggestions.ActionOn* histograms.
// Entries should not be renumbered and numeric values should never be reused.
enum class IOSContentSuggestionsActionType {
  kMostVisitedTile = 0,
  kShortcuts = 1,
  kReturnToRecentTab = 2,
  kFeedCard = 3,
  kFakebox = 4,
  kTrendingQuery = 5,
  kMaxValue = kTrendingQuery,
};

// Metrics recorder for the action used to potentially leave the NTP.
@interface NTPHomeMetrics : NSObject

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Currently active WebState with an active NTP.
@property(nonatomic, assign) web::WebState* webState;

// Whether `webState` is showing the Start Surface.
@property(nonatomic, assign) BOOL showingStartSurface;

- (void)recordAction:(new_tab_page_uma::ActionType)action;

// Records a user action on a ContentSuggestions module `type`.
- (void)recordContentSuggestionsActionForType:
    (IOSContentSuggestionsActionType)type;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NTP_HOME_METRICS_H_
