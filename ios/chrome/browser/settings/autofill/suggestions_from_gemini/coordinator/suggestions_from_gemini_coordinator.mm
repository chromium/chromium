// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/coordinator/suggestions_from_gemini_coordinator.h"

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/coordinator/suggestions_from_gemini_mediator.h"
#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@implementation SuggestionsFromGeminiCoordinator {
  SuggestionsFromGeminiTableViewController* _viewController;
  SuggestionsFromGeminiMediator* _mediator;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  _viewController = [[SuggestionsFromGeminiTableViewController alloc] init];
  _mediator = [[SuggestionsFromGeminiMediator alloc]
      initWithPrefService:self.browser->GetProfile()->GetPrefs()];

  _viewController.mutator = _mediator;
  _mediator.consumer = _viewController;

  [_baseNavigationController pushViewController:_viewController animated:YES];
}

- (void)stop {
  [super stop];
  _mediator.consumer = nil;
  [_mediator disconnect];
  _mediator = nil;
  _viewController.mutator = nil;
  _viewController = nil;
}

@end
