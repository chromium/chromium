// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_view_state_change_handler.h"

#import "base/memory/raw_ptr.h"

@implementation GeminiViewStateChangeHandler {
  raw_ptr<GeminiViewStateChangeHandlerTarget> _target;
}

- (instancetype)initWithTarget:(GeminiViewStateChangeHandlerTarget*)target {
  if ((self = [super init])) {
    _target = target;
  }
  return self;
}

- (void)disconnect {
  _target = nullptr;
}

#pragma mark - GeminiViewStateDelegate

- (void)didSwitchToViewState:(ios::provider::GeminiViewState)viewState {
  if (!_target) {
    return;
  }
  _target->OnViewStateChanged(viewState);
  _target->SetLastShownViewState(viewState);
}

- (void)didUpdateProcessingStatus:
            (ios::provider::GeminiClientMode)processingStatus
                        sessionID:(NSString*)sessionID
                   conversationID:(NSString*)conversationID {
  if (!_target) {
    return;
  }
  _target->OnProcessingStatusChanged(
      processingStatus, ios::provider::GeminiDormantReason::kUnknown);
}

- (void)
    didUpdateProcessingStatus:(ios::provider::GeminiClientMode)processingStatus
                dormantReason:(ios::provider::GeminiDormantReason)dormantReason
                    sessionID:(NSString*)sessionID
               conversationID:(NSString*)conversationID {
  if (!_target) {
    return;
  }
  _target->OnProcessingStatusChanged(processingStatus, dormantReason);
}

- (void)switchToViewState:(ios::provider::GeminiViewState)viewState {
  if (!_target) {
    return;
  }

  // Only handle collapsed state for now.
  if (viewState == ios::provider::GeminiViewState::kCollapsed) {
    _target->CollapseFloatyIfInvoked();
  }
}

- (void)geminiLiveUserDidTapLiveButton {
  if (!_target) {
    return;
  }
  _target->OnLiveButtonTapped();
}

- (void)geminiLiveUserDidPressStopButton {
  if (!_target) {
    return;
  }
  _target->OnGeminiLiveUserDidPressStopButton();
}

- (void)geminiLiveUserDidBargeIn {
  if (!_target) {
    return;
  }
  _target->OnGeminiLiveUserDidBargeIn();
}

- (void)didSwitchToMode:(ios::provider::GeminiViewMode)mode {
  if (!_target) {
    return;
  }
  _target->OnModeChanged(mode);
}

- (void)geminiUIDidAppear {
  if (!_target) {
    return;
  }
  _target->OnGeminiUIDidAppear();
}

@end
