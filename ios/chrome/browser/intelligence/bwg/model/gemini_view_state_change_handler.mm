// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_view_state_change_handler.h"

#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent.h"

@implementation GeminiViewStateChangeHandler {
  base::WeakPtr<GeminiBrowserAgent> _agent;
}

- (instancetype)initWithBrowserAgent:(base::WeakPtr<GeminiBrowserAgent>)agent {
  if ((self = [super init])) {
    _agent = agent;
  }
  return self;
}

#pragma mark - GeminiViewStateDelegate

- (void)didSwitchToViewState:(ios::provider::GeminiViewState)viewState {
  if (_agent && viewState == ios::provider::GeminiViewState::kExpanded) {
    _agent->OnGeminiViewStateExpanded();
  }
  _agent->SetLastShownViewState(viewState);
}

- (void)switchToViewState:(ios::provider::GeminiViewState)viewState {
  if (!_agent) {
    return;
  }

  if (viewState == ios::provider::GeminiViewState::kCollapsed) {
    _agent->CollapseFloatyIfInvoked();
  }
}

@end
