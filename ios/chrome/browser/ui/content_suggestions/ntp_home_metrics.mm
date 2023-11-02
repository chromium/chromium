// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/ntp_home_metrics.h"

#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ntp_home {

void RecordNTPImpression(IOSNTPImpression impression_type) {
  UMA_HISTOGRAM_ENUMERATION("IOS.NTP.Impression", impression_type, COUNT);
}

}  // namespace ntp_home

@interface NTPHomeMetrics ()
@property(nonatomic, assign) ChromeBrowserState* browserState;
@end

@implementation NTPHomeMetrics

@synthesize browserState = _browserState;

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState {
  self = [super init];
  if (self) {
    _browserState = browserState;
  }
  return self;
}

- (void)recordAction:(new_tab_page_uma::ActionType)action {
  DCHECK(self.webState);
  new_tab_page_uma::RecordAction(self.browserState, self.webState, action);
}

- (void)recordContentSuggestionsActionForType:
    (IOSContentSuggestionsActionType)type {
  if (self.showingStartSurface) {
    UMA_HISTOGRAM_ENUMERATION("IOS.ContentSuggestions.ActionOnStartSurface",
                              type);
  } else {
    UMA_HISTOGRAM_ENUMERATION("IOS.ContentSuggestions.ActionOnNTP", type);
  }
}

@end
