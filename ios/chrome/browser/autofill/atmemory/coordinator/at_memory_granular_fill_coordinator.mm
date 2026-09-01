// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_granular_fill_coordinator.h"

#import "base/check.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_granular_fill_mediator.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"

using autofill::Suggestion;

@implementation AtMemoryGranularFillCoordinator {
  // View controller for the AtMemory granular fill screen.
  AtMemoryGranularFillViewController* _atMemoryGranularFillViewController;
  // Mediator for the AtMemory granular fill coordinator.
  AtMemoryGranularFillMediator* _mediator;
  // Suggestion containing attributes to display.
  std::optional<Suggestion> _suggestion;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                      suggestion:(const Suggestion&)suggestion {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _suggestion = suggestion;
  }
  return self;
}

- (void)start {
  _atMemoryGranularFillViewController =
      [[AtMemoryGranularFillViewController alloc]
          initWithStyle:ChromeTableViewStyle()];

  CHECK(_suggestion);
  _mediator = [[AtMemoryGranularFillMediator alloc]
      initWithSuggestion:std::move(*_suggestion)];
  _suggestion.reset();
  id<AtMemoryCommands> atMemoryHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), AtMemoryCommands);
  _mediator.fillHandler = self.fillHandler;
  _mediator.atMemoryHandler = atMemoryHandler;
  _atMemoryGranularFillViewController.atMemoryHandler = atMemoryHandler;
  _atMemoryGranularFillViewController.mutator = _mediator;
  _mediator.consumer = _atMemoryGranularFillViewController;

  [self.baseNavigationController
      pushViewController:_atMemoryGranularFillViewController
                animated:YES];
}

- (void)stop {
  if (self.baseNavigationController.topViewController ==
      _atMemoryGranularFillViewController) {
    [self.baseNavigationController popViewControllerAnimated:YES];
  }
  _atMemoryGranularFillViewController = nil;
  _mediator = nil;
  _suggestion.reset();
}

@end
