// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/whats_new_detail_coordinator.h"

#import "base/check_op.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_detail_view_action_handler.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_detail_view_controller.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_detail_view_delegate.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface WhatsNewDetailCoordinator ()

// The view controller used to display the What's New features and chrome tips.
@property(nonatomic, strong) WhatsNewDetailViewController* viewController;

@end

@implementation WhatsNewDetailCoordinator

@synthesize baseNavigationController = _baseNavigationController;
@synthesize browser = _browser;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                            item:(WhatsNewItem*)item
                                   actionHandler:
                                       (id<WhatsNewDetailViewActionHandler>)
                                           actionHandler {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _browser = browser;
    _baseNavigationController = navigationController;
    self.viewController = [[WhatsNewDetailViewController alloc]
            initWithParams:item.bannerImage
                     title:item.title
                  subtitle:item.subtitle
        primaryActionTitle:item.primaryActionTitle
          instructionSteps:item.instructionSteps
          hasPrimaryAction:item.hasPrimaryAction
                      type:item.type
              learnMoreURL:item.learnMoreURL
        hasLearnMoreAction:item.learnMoreURL.is_valid()];
    self.viewController.actionHandler = actionHandler;
    self.viewController.delegate = self;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
  [super start];
}

- (void)stop {
  // Pop the detail view controller if it is at the top of the navigation stack.
  if (self.baseNavigationController.topViewController == self.viewController) {
    [self.baseNavigationController popViewControllerAnimated:NO];
    self.viewController = nil;
  }

  [super stop];
}

#pragma mark - WhatsNewDetailViewDelegate

- (void)dismissWhatsNewDetailView:
    (WhatsNewDetailViewController*)whatsNewDetailViewController {
  DCHECK_EQ(self.viewController, whatsNewDetailViewController);

  id<BrowserCoordinatorCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  DCHECK(handler);

  [handler dismissWhatsNew];
}

@end
