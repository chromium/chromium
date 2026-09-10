// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_ui_state_manager.h"

#import "base/notreached.h"
#import "base/time/time.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_session_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"

using ios::provider::GeminiClientMode;
using ios::provider::GeminiViewMode;

#pragma mark - GeminiContainerUIState

// static
GeminiContainerUIState GeminiContainerUIState::ZeroState(
    AssistantContainerDetent detent) {
  return {
      .detent = detent,
      .hasGrabber = YES,
      .zeroStateVisible = YES,
  };
}

// static
GeminiContainerUIState GeminiContainerUIState::ExpandedResponse() {
  return {
      .detent = AssistantContainerDetent::kMedium,
      .hasGrabber = YES,
      .zeroStateVisible = NO,
  };
}

// static
GeminiContainerUIState GeminiContainerUIState::Minimized(BOOL has_grabber) {
  return {
      .detent = AssistantContainerDetent::kMinimized,
      .hasGrabber = has_grabber,
      .zeroStateVisible = NO,
  };
}

#pragma mark - GeminiContainerUIStateManager

@interface GeminiContainerUIStateManager ()

// Properties declared here (instead of private ivars) to synthesize accessors
// used by `gemini_container_ui_state_manager_unittest.mm` via its `(Testing)`
// category.

// Current processing status of the Gemini client.
@property(nonatomic, readonly) ios::provider::GeminiClientMode processingStatus;

// Current view mode of the Gemini UI (e.g. floaty or live).
@property(nonatomic, readonly) ios::provider::GeminiViewMode viewMode;

// Whether there is currently an active conversation in the container.
@property(nonatomic, assign) BOOL hasConversation;

// Current UI state of the container.
@property(nonatomic, assign) GeminiContainerUIState currentUIState;

@end

@implementation GeminiContainerUIStateManager {
  // Start time of the thinking state.
  base::TimeTicks _thinkingStartTime;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _viewMode = GeminiViewMode::kUnknown;
    _processingStatus = GeminiClientMode::kUnknown;
  }
  return self;
}

- (void)setupInitialUIState {
  // Default values for mode and processing status. Actual values driven by SDK.
  // TODO(crbug.com/546065071): Pass the default mode with init UI
  // config/startup config.
  _viewMode = GeminiViewMode::kFloaty;
  _processingStatus = GeminiClientMode::kDormant;
  [self resetToZeroStateWithDetent:AssistantContainerDetent::kMedium];
}

- (void)transitionToMode:(GeminiViewMode)mode {
  if (_viewMode == mode) {
    return;
  }

  _viewMode = mode;
  // Any mode change resets the thinking timer.
  _thinkingStartTime = base::TimeTicks();

  GeminiContainerUIState state;
  switch (_viewMode) {
    case GeminiViewMode::kLive:
      state = GeminiContainerUIState::Minimized(/*has_grabber=*/NO);
      break;
    case GeminiViewMode::kFloaty:
      state = _hasConversation ? GeminiContainerUIState::ExpandedResponse()
                               : GeminiContainerUIState::ZeroState();
      break;
    case GeminiViewMode::kUnknown:
      NOTREACHED();
  }

  _currentUIState = state;
  [self.delegate didChangeUIState:_currentUIState];
}

- (void)transitionToProcessingStatus:(GeminiClientMode)processingStatus {
  if (_processingStatus == processingStatus) {
    return;
  }

  // Any status change after thinking that is not responding resets the thinking
  // timer.
  if (!_thinkingStartTime.is_null() &&
      processingStatus != GeminiClientMode::kResponding) {
    _thinkingStartTime = base::TimeTicks();
  }

  _processingStatus = processingStatus;

  switch (_viewMode) {
    case GeminiViewMode::kLive:
      [self updateProcessingStatusForLiveMode];
      return;
    case GeminiViewMode::kFloaty:
      [self updateProcessingStatusForFloatyMode];
      return;
    case GeminiViewMode::kUnknown:
      // Ignore in case mode is still unknown while processing status is being
      // set.
      return;
  }
}

- (void)handleNewChat {
  [self resetToZeroStateWithDetent:_currentUIState.detent];
}

- (void)handleResponseCancellationWithReason:(GeminiCancelType)reason {
  if (reason != GeminiCancelTypeStopButtonTapped) {
    return;
  }

  _thinkingStartTime = base::TimeTicks();
  _currentUIState = GeminiContainerUIState::ExpandedResponse();
  [self.delegate didChangeUIState:_currentUIState];
}

- (void)updateDetent:(AssistantContainerDetent)detent {
  _currentUIState.detent = detent;
}

- (BOOL)shouldBeDismissed {
  return _viewMode == GeminiViewMode::kFloaty &&
         _currentUIState.detent == AssistantContainerDetent::kMinimized &&
         !_hasConversation && IsChromeNextIaEnabled();
}

- (void)reset {
  _thinkingStartTime = base::TimeTicks();
  _hasConversation = NO;
  _viewMode = GeminiViewMode::kUnknown;
  _processingStatus = GeminiClientMode::kUnknown;
  _currentUIState = {};
}

#pragma mark - Private

- (void)resetToZeroStateWithDetent:(AssistantContainerDetent)detent {
  _hasConversation = NO;
  _thinkingStartTime = base::TimeTicks();
  _currentUIState = GeminiContainerUIState::ZeroState(detent);
  [self.delegate didChangeUIState:_currentUIState];
}

// Handles processing status updates while in Live mode.
- (void)updateProcessingStatusForLiveMode {
  // While in Live mode, update conversation state without pushing to the
  // consumer if the status changes to responding.
  if (_processingStatus == GeminiClientMode::kResponding) {
    _hasConversation = YES;
  }
}

- (void)updateProcessingStatusForFloatyMode {
  GeminiContainerUIState state;
  switch (_processingStatus) {
    case GeminiClientMode::kThinking:
      _hasConversation = YES;
      _thinkingStartTime = base::TimeTicks::Now();
      state = GeminiContainerUIState::Minimized(/*has_grabber=*/NO);
      break;
    case GeminiClientMode::kResponding: {
      BOOL shouldExpand = [self shouldExpandOnResponding];
      state = shouldExpand
                  ? GeminiContainerUIState::ExpandedResponse()
                  : GeminiContainerUIState::Minimized(/*has_grabber=*/YES);
      _thinkingStartTime = base::TimeTicks();
      break;
    }
    case GeminiClientMode::kPreviousConversationLoading:
      _hasConversation = YES;
      state = GeminiContainerUIState::ExpandedResponse();
      break;
    case GeminiClientMode::kDormant:
      return;
    case GeminiClientMode::kListening:
    case GeminiClientMode::kTranscribing:
    case GeminiClientMode::kUnknown:
      NOTREACHED();
  }

  _currentUIState = state;
  [self.delegate didChangeUIState:_currentUIState];
}

- (BOOL)shouldExpandOnResponding {
  if (_thinkingStartTime.is_null()) {
    return NO;
  }

  base::TimeDelta thinkingDuration =
      base::TimeTicks::Now() - _thinkingStartTime;
  return thinkingDuration <= base::Seconds(GetGeminiResponseReadyInterval());
}

@end
