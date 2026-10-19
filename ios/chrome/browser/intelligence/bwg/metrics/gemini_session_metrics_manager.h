// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_METRICS_GEMINI_SESSION_METRICS_MANAGER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_METRICS_GEMINI_SESSION_METRICS_MANAGER_H_

#import <Foundation/Foundation.h>

// The instance responsible for tracking Gemini session state for metrics
// (timestamps, durations, counts) and recording UMA histograms.
@interface GeminiSessionMetricsManager : NSObject

#pragma mark - Session Lifecycle & Startup

// Called when the Gemini flow is about to start, initializing the startup
// timer.
- (void)geminiWillStart;

// Called when the Gemini flow has started, recording startup latency from the
// prior `geminiWillStart` call.
- (void)geminiStarted:(BOOL)isFirstSession;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_METRICS_GEMINI_SESSION_METRICS_MANAGER_H_
