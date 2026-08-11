// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/coordinator/suggestions_from_gemini_help_improve_coordinator.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/coordinator/suggestions_from_gemini_help_improve_mediator.h"
#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_help_improve_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@implementation SuggestionsFromGeminiHelpImproveCoordinator {
  SuggestionsFromGeminiHelpImproveTableViewController* _viewController;
  SuggestionsFromGeminiHelpImproveMediator* _mediator;
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
  _viewController =
      [[SuggestionsFromGeminiHelpImproveTableViewController alloc] init];

  PrefService* prefService = self.browser->GetProfile()->GetPrefs();
  _mediator = [[SuggestionsFromGeminiHelpImproveMediator alloc]
      initWithPrefService:prefService];

  _mediator.consumer = _viewController;

  [_baseNavigationController pushViewController:_viewController animated:YES];
}

- (void)stop {
  [_mediator disconnect];
  _mediator.consumer = nil;
  _mediator = nil;
  _viewController = nil;
}

@end
