// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_UTIL_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_UTIL_H_

#import <UIKit/UIKit.h>

@class FirstRunConfiguration;
@protocol SyncPresenter;

namespace base {
class TimeTicks;
}

namespace ios {
class ChromeBrowserState;
}
namespace web {
class WebState;
}

// Notification sent when the first run ends, right before dimissing the Terms
// of Service modal view.
extern NSString* const kChromeFirstRunUIWillFinishNotification;

// Notification sent when the first run has finished and has dismissed the Terms
// of Service modal view.
extern NSString* const kChromeFirstRunUIDidFinishNotification;

// Checks if the last line of the label only contains one word and if so, insert
// a newline character before the second to last word so that there are two
// words on the last line. Should only be called on labels that span multiple
// lines. Returns YES if a newline was added.
BOOL FixOrphanWord(UILabel* label);

// Creates the First Run sentinel file so that the user will not be shown First
// Run on subsequent cold starts. The user is considered done with First Run
// only after a successful sign-in or explicitly skipping signing in. First Run
// metrics are recorded iff the sentinel file didn't previous exist and was
// successfully created.
void WriteFirstRunSentinelAndRecordMetrics(
    ios::ChromeBrowserState* browserState,
    BOOL sign_in_attempted,
    BOOL has_sso_account);

// Methods for writing sentinel and recording metrics and posting notifications
void FinishFirstRun(ios::ChromeBrowserState* browserState,
                    web::WebState* web_state,
                    FirstRunConfiguration* config,
                    id<SyncPresenter> presenter);

// Records Product tour timing metrics using histogram.
void RecordProductTourTimingMetrics(NSString* timer_name,
                                    base::TimeTicks start_time);

// Posts a notification that First Run did finish.
void FirstRunDismissed();

// Enables or disables the data reduction proxy and also sets a key indicating
// application is using Data Reduction Proxy.
void SetDataReductionProxyEnabled(ios::ChromeBrowserState* browserState,
                                  BOOL enabled,
                                  BOOL toggled_switch);

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_UTIL_H_
