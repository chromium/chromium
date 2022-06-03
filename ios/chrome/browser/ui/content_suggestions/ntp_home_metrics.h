// Copyright 2017 The Chromium Authors. All rights reserved.
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

// Records an NTP impression of type |impression_type|.
void RecordNTPImpression(ntp_home::IOSNTPImpression impression_type);

}  // namespace ntp_home

// Metrics recorder for the action used to potentially leave the NTP.
@interface NTPHomeMetrics : NSObject

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
                            webState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (void)recordAction:(new_tab_page_uma::ActionType)action;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NTP_HOME_METRICS_H_
