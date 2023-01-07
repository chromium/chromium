// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_METRICS_FEED_SESSION_RECORDER_H_
#define IOS_CHROME_BROWSER_UI_NTP_METRICS_FEED_SESSION_RECORDER_H_

#import <Foundation/Foundation.h>

// Records metrics related to user sessions in the feed.
@interface FeedSessionRecorder : NSObject

// Records metrics related to user sessions in the feed. This must be called at
// every user interaction or scrolling event. Failure to do so will result in
// inaccurate reporting.
- (void)recordUserInteractionOrScrolling;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_METRICS_FEED_SESSION_RECORDER_H_
