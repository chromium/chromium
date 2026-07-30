// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/coordinator/suggestions_from_gemini_coordinator.h"

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/coordinator/suggestions_from_gemini_mediator.h"
#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/web/public/navigation/referrer.h"
#import "url/gurl.h"

@interface SuggestionsFromGeminiCoordinator () <
    SuggestionsFromGeminiMediatorDelegate>

@end

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
  _mediator.delegate = self;

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

#pragma mark - SuggestionsFromGeminiMediatorDelegate

- (void)suggestionsFromGeminiMediatorOpenConnectedApps:
    (SuggestionsFromGeminiMediator*)mediator {
  OpenNewTabCommand* command =
      [[OpenNewTabCommand alloc] initWithURL:GURL(kGeminiExtensionsURL)
                                    referrer:web::Referrer()
                                 inIncognito:NO
                                inBackground:NO
                                    appendTo:OpenPosition::kLastTab];
  id<SceneCommands> sceneHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), SceneCommands);
  [sceneHandler closePresentedViewsAndOpenURL:command];
}

@end
