// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/find_bar/find_bar_coordinator.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/find_in_page/find_tab_helper.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/find_in_page_commands.h"
#import "ios/chrome/browser/ui/find_bar/find_bar_controller_ios.h"
#import "ios/chrome/browser/ui/find_bar/find_bar_mediator.h"
#import "ios/chrome/browser/ui/find_bar/find_bar_view_controller.h"
#include "ios/chrome/browser/ui/presenters/contained_presenter_delegate.h"
#import "ios/chrome/browser/ui/toolbar/accessory/toolbar_accessory_coordinator_delegate.h"
#import "ios/chrome/browser/ui/toolbar/accessory/toolbar_accessory_presenter.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FindBarCoordinator () <ContainedPresenterDelegate>

@property(nonatomic, strong) FindBarMediator* mediator;

// Allows simplified access to the FindInPageCommands handler.
@property(nonatomic, readonly) id<FindInPageCommands> findInPageCommandHandler;

@end

@implementation FindBarCoordinator

- (void)start {
  if (!self.findBarController) {
    self.findBarController = [[FindBarControllerIOS alloc]
        initWithIncognito:self.browser->GetBrowserState()->IsOffTheRecord()];

    self.findBarController.commandHandler = self.findInPageCommandHandler;
  }
  self.presenter.delegate = self;

  self.mediator =
      [[FindBarMediator alloc] initWithWebState:self.currentWebState
                                 commandHandler:self.findInPageCommandHandler];
  self.mediator.consumer = self.findBarController;

  DCHECK(self.currentWebState);
  FindTabHelper* helper = FindTabHelper::FromWebState(self.currentWebState);
  helper->SetResponseDelegate(self.mediator);
  // If the FindUI is already active, just reshow it.
  if (helper->IsFindUIActive()) {
    [self showAnimated:NO shouldFocus:[self.findBarController isFocused]];
  } else {
    helper->SetFindUIActive(true);
    [self showAnimated:YES shouldFocus:YES];
  }
}

- (void)stop {
  if (![self.presenter isPresentingViewController:self.findBarController
                                                      .findBarViewController]) {
    return;
  }

  // If the FindUI is still active, the dismiss should be unanimated, because
  // the UI will be brought back later.
  BOOL animated;
  if (self.currentWebState) {
    FindTabHelper* helper = FindTabHelper::FromWebState(self.currentWebState);
    animated = helper && !helper->IsFindUIActive();
  } else {
    animated = true;
  }
  [self.findBarController findBarViewWillHide];
  [self.presenter dismissAnimated:animated];
  self.mediator = nil;
}

- (void)showAnimated:(BOOL)animated shouldFocus:(BOOL)shouldFocus {
  self.presenter.presentedViewController =
      self.findBarController.findBarViewController;

  [self.presenter prepareForPresentation];
  [self.presenter presentAnimated:animated];

  // TODO(crbug.com/731045): This early return temporarily replaces a DCHECK.
  // For unknown reasons, this DCHECK sometimes was hit in the wild, resulting
  // in a crash.
  if (!self.currentWebState) {
    return;
  }
  FindTabHelper* helper = FindTabHelper::FromWebState(self.currentWebState);
  DCHECK(helper && helper->IsFindUIActive());
  if (!self.browser->GetBrowserState()->IsOffTheRecord()) {
    helper->RestoreSearchTerm();
  }
  [self.presentationDelegate setHeadersForFindBarCoordinator:self];
  [self.findBarController updateView:helper->GetFindResult()
                       initialUpdate:YES
                      focusTextfield:shouldFocus];
}

- (void)defocusFindBar {
  FindTabHelper* helper = FindTabHelper::FromWebState(self.currentWebState);
  if (helper && helper->IsFindUIActive()) {
    [self.findBarController updateView:helper->GetFindResult()
                         initialUpdate:NO
                        focusTextfield:NO];
  }
}

#pragma mark - ContainedPresenterDelegate

- (void)containedPresenterDidPresent:(id<ContainedPresenter>)presenter {
  [self.findBarController selectAllText];
}

- (void)containedPresenterDidDismiss:(id<ContainedPresenter>)presenter {
  [self.findBarController findBarViewDidHide];
  [self.delegate toolbarAccessoryCoordinatorDidDismissUI:self];
}

#pragma mark - Private

- (web::WebState*)currentWebState {
  return self.browser->GetWebStateList()
             ? self.browser->GetWebStateList()->GetActiveWebState()
             : nullptr;
}

- (id<FindInPageCommands>)findInPageCommandHandler {
  return HandlerForProtocol(self.browser->GetCommandDispatcher(),
                            FindInPageCommands);
}

@end
