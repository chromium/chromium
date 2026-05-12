// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/coordinator/level_up_coordinator.h"

#import "ios/chrome/browser/level_up/coordinator/level_up_mediator.h"
#import "ios/chrome/browser/level_up/ui/level_up_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/level_up_commands.h"

@interface LevelUpCoordinator ()

@property(nonatomic, strong) LevelUpMediator* mediator;
@property(nonatomic, strong) LevelUpViewController* viewController;
@property(nonatomic, strong) UINavigationController* navigationController;

@end

@implementation LevelUpCoordinator

- (void)start {
  [super start];

  self.viewController = [[LevelUpViewController alloc] init];
  self.viewController.handler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), LevelUpCommands);

  self.mediator = [[LevelUpMediator alloc] init];
  self.mediator.consumer = self.viewController;

  self.navigationController = [[UINavigationController alloc]
      initWithRootViewController:self.viewController];
  [self.navigationController
      setModalPresentationStyle:UIModalPresentationPageSheet];

  UISheetPresentationController* sheetPresentationController =
      self.navigationController.sheetPresentationController;
  sheetPresentationController.detents =
      @[ [UISheetPresentationControllerDetent largeDetent] ];

  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
  self.mediator = nil;
  self.navigationController = nil;

  [super stop];
}

@end
