// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_session_metrics_manager.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/time/time.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"

using ios::provider::GeminiViewState;

@implementation GeminiSessionMetricsManager {
  // Timestamp when the Gemini flow started.
  base::TimeTicks _startTime;

  // Tracks the elapsed time a floaty is minimized until it is expanded.
  base::TimeTicks _elapsedMinimizedFloatyTime;

  // Whether the Gemini container is currently minimized.
  BOOL _isMinimized;

  // The number of times the active tab was switched during the current session.
  int _sessionTabSwitchCount;
}

#pragma mark - Session Lifecycle & Startup

- (void)geminiWillStart {
  _startTime = base::TimeTicks::Now();
}

- (void)geminiStarted:(BOOL)isFirstSession {
  if (_startTime.is_null()) {
    return;
  }
  base::TimeDelta startupTime = base::TimeTicks::Now() - _startTime;
  _startTime = base::TimeTicks();
  base::UmaHistogramLongTimes(isFirstSession ? kStartupTimeWithFirstRunHistogram
                                             : kStartupTimeNoFirstRunHistogram,
                              startupTime);
}

- (void)sessionDismissedWithViewState:(GeminiViewState)viewState {
  RecordFloatyDismissedState(viewState);
  [self sessionDismissed];
}

- (void)sessionDismissed {
  // TODO(crbug.com/573666470): Record the last detent, equivalent to
  // `RecordFloatyDismissedState`.
  RecordSessionTabSwitchCount(_sessionTabSwitchCount);
  _sessionTabSwitchCount = 0;
  _startTime = base::TimeTicks();
  _elapsedMinimizedFloatyTime = base::TimeTicks();
  _isMinimized = NO;
}

#pragma mark - View State

- (void)didChangeViewState:(GeminiViewState)viewState {
  if (viewState == GeminiViewState::kExpanded) {
    RecordFloatyCollapsedToExpanded();
    RecordFloatyMinimizedTime(_elapsedMinimizedFloatyTime);
    _elapsedMinimizedFloatyTime = base::TimeTicks();
  } else if (viewState == GeminiViewState::kCollapsed) {
    RecordFloatyExpandedToCollapsed();
    _elapsedMinimizedFloatyTime = base::TimeTicks::Now();
  }
}

- (void)didChangeDetent:(AssistantContainerDetent)detent {
  BOOL isMinimized = (detent == AssistantContainerDetent::kMinimized);
  if (isMinimized == _isMinimized) {
    return;
  }
  _isMinimized = isMinimized;
  [self didChangeViewState:isMinimized ? GeminiViewState::kCollapsed
                                       : GeminiViewState::kExpanded];
}

#pragma mark - Tab Switching

- (void)activeTabSwitched {
  _sessionTabSwitchCount++;
  base::RecordAction(base::UserMetricsAction("MobileGeminiFloatyTabSwitched"));
}

@end
