// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_METRICS_GEMINI_SESSION_METRICS_MANAGER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_METRICS_GEMINI_SESSION_METRICS_MANAGER_H_

#import <Foundation/Foundation.h>

enum class AssistantContainerDetent : NSInteger;

namespace ios::provider {
enum class GeminiViewState;
}  // namespace ios::provider

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

// Called when the Gemini Floaty session is dismissed in `viewState`. Records
// end-of-session metrics and resets session state.
- (void)sessionDismissedWithViewState:(ios::provider::GeminiViewState)viewState;

// Called when the Gemini container session is dismissed. Records end-of-session
// metrics and resets session state.
- (void)sessionDismissed;

#pragma mark - View State

// Called when the Gemini view state changes to `viewState`. Tracks minimized
// duration and records expansion/collapse transitions.
- (void)didChangeViewState:(ios::provider::GeminiViewState)viewState;

// Called when the Gemini container detent changes to `detent`. Tracks
// minimized duration and records expansion/collapse transitions.
- (void)didChangeDetent:(AssistantContainerDetent)detent;

#pragma mark - Tab Switching

// Called when the active tab is switched during an active session.
- (void)activeTabSwitched;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_METRICS_GEMINI_SESSION_METRICS_MANAGER_H_
