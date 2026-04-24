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
  if (viewState == ios::provider::GeminiViewState::kExpanded) {
    _target->OnGeminiViewStateExpanded();
  }
  _target->SetLastShownViewState(viewState);
}

- (void)didUpdateProcessingStatus:
            (ios::provider::GeminiClientMode)processingStatus
                        sessionID:(NSString*)sessionID
                   conversationID:(NSString*)conversationID {
  // TODO(crbug.com/504758406): Handle processing status updates.
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

@end
