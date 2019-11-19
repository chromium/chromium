// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/password_breach_coordinator.h"

#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/password_breach_commands.h"
#import "ios/chrome/browser/ui/passwords/password_breach_mediator.h"
#import "ios/chrome/browser/ui/passwords/password_breach_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordBreachCoordinator () <PasswordBreachCommands,
                                         PasswordBreachPresenter>

// The main view controller for this coordinator.
@property(nonatomic, strong) PasswordBreachViewController* viewController;

// The main mediator for this coordinator.
@property(nonatomic, strong) PasswordBreachMediator* mediator;

@end

@implementation PasswordBreachCoordinator

- (void)start {
  [super start];
  // To start, a mediator and view controller should be ready.
  DCHECK(self.viewController);
  DCHECK(self.mediator);
  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.mediator disconnect];
  self.mediator = nil;
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
  [super stop];
}

#pragma mark - Setters

- (void)setDispatcher:(CommandDispatcher*)dispatcher {
  if (_dispatcher == dispatcher) {
    return;
  }
  [_dispatcher stopDispatchingToTarget:self];
  [dispatcher startDispatchingToTarget:self
                           forProtocol:@protocol(PasswordBreachCommands)];
  _dispatcher = dispatcher;
}

#pragma mark - PasswordBreachCommands

- (void)showPasswordBreachForLeakType:(CredentialLeakType)leakType
                                  URL:(const GURL&)URL {
  self.viewController = [[PasswordBreachViewController alloc] init];
  self.viewController.modalPresentationStyle = UIModalPresentationFormSheet;
  if (@available(iOS 13, *)) {
    self.viewController.modalInPresentation = YES;
  }
  id<ApplicationCommands> dispatcher =
      static_cast<id<ApplicationCommands>>(self.dispatcher);
  self.mediator =
      [[PasswordBreachMediator alloc] initWithConsumer:self.viewController
                                             presenter:self
                                            dispatcher:dispatcher
                                                   URL:URL
                                              leakType:leakType];
  self.viewController.actionHandler = self.mediator;
  [self start];
}

@end
