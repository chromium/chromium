// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sad_tab/sad_tab_coordinator.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/browser/ui/sad_tab/sad_tab_view_controller.h"
#import "ios/chrome/browser/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/web/sad_tab_tab_helper.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SadTabCoordinator ()<SadTabViewControllerDelegate> {
  SadTabViewController* _viewController;
}
@end

@implementation SadTabCoordinator

- (void)start {
  if (_viewController)
    return;

  _viewController = [[SadTabViewController alloc] init];
  _viewController.delegate = self;
  _viewController.overscrollDelegate = self.overscrollDelegate;
  _viewController.offTheRecord = self.browserState->IsOffTheRecord();
  _viewController.repeatedFailure = _repeatedFailure;

  [self.baseViewController addChildViewController:_viewController];
  [self.baseViewController.view addSubview:_viewController.view];
  [_viewController didMoveToParentViewController:self.baseViewController];

  _viewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints([NamedGuide guideWithName:kContentAreaGuide
                                          view:self.baseViewController.view],
                     _viewController.view);
}

- (void)stop {
  if (!_viewController)
    return;

  [_viewController willMoveToParentViewController:nil];
  [_viewController.view removeFromSuperview];
  [_viewController removeFromParentViewController];
  _viewController = nil;
}

- (void)setOverscrollDelegate:
    (id<OverscrollActionsControllerDelegate>)delegate {
  _viewController.overscrollDelegate = delegate;
  _overscrollDelegate = delegate;
}

#pragma mark - SadTabViewDelegate

- (void)sadTabViewControllerShowReportAnIssue:
    (SadTabViewController*)sadTabViewController {
  [self.dispatcher showReportAnIssueFromViewController:self.baseViewController];
}

- (void)sadTabViewController:(SadTabViewController*)sadTabViewController
    showSuggestionsPageWithURL:(const GURL&)URL {
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [self.dispatcher openURLInNewTab:command];
}

- (void)sadTabViewControllerReload:(SadTabViewController*)sadTabViewController {
  [self.dispatcher reload];
}

#pragma mark - SadTabTabHelperDelegate

- (void)sadTabTabHelper:(SadTabTabHelper*)tabHelper
    presentSadTabForWebState:(web::WebState*)webState
             repeatedFailure:(BOOL)repeatedFailure {
  if (!webState->IsVisible())
    return;

  _repeatedFailure = repeatedFailure;
  [self start];
}

- (void)sadTabTabHelperDismissSadTab:(SadTabTabHelper*)tabHelper {
  [self stop];
}

- (void)sadTabTabHelper:(SadTabTabHelper*)tabHelper
    didShowForRepeatedFailure:(BOOL)repeatedFailure {
  _repeatedFailure = repeatedFailure;
  [self start];
}

- (void)sadTabTabHelperDidHide:(SadTabTabHelper*)tabHelper {
  [self stop];
}

@end
