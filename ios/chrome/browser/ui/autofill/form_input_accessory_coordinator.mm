// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/form_input_accessory_coordinator.h"

#include "base/mac/foundation_util.h"
#import "components/autofill/ios/browser/js_suggestion_manager.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_controller.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_accessory_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_coordinator.h"
#include "ios/chrome/browser/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FormInputAccessoryCoordinator ()<
    ManualFillAccessoryViewControllerDelegate,
    PasswordCoordinatorDelegate>

// The Mediator for the input accessory view controller.
@property(nonatomic, strong)
    FormInputAccessoryMediator* formInputAccessoryMediator;

// The View Controller for the input accessory view.
@property(nonatomic, strong)
    FormInputAccessoryViewController* formInputAccessoryViewController;

// The manual fill accessory to show above the keyboard.
@property(nonatomic, strong)
    ManualFillAccessoryViewController* manualFillAccessoryViewController;

// The object in charge of interacting with the web view. Used to fill the data
// in the forms.
@property(nonatomic, strong)
    ManualFillInjectionHandler* manualFillInjectionHandler;

// The WebStateList for this instance. Used to instantiate the child
// coordinators lazily.
@property(nonatomic, assign) WebStateList* webStateList;

@end

@implementation FormInputAccessoryCoordinator

@synthesize formInputAccessoryMediator = _formInputAccessoryMediator;
@synthesize formInputAccessoryViewController =
    _formInputAccessoryViewController;
@synthesize manualFillAccessoryViewController =
    _manualFillAccessoryViewController;
@synthesize webStateList = _webStateList;
@synthesize manualFillInjectionHandler = _manualFillInjectionHandler;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
                              webStateList:(WebStateList*)webStateList {
  DCHECK(browserState);
  DCHECK(webStateList);
  self = [super initWithBaseViewController:viewController
                              browserState:browserState];
  if (self) {
    _webStateList = webStateList;

    _manualFillInjectionHandler =
        [[ManualFillInjectionHandler alloc] initWithWebStateList:webStateList];

    _formInputAccessoryViewController =
        [[FormInputAccessoryViewController alloc] init];

    _manualFillAccessoryViewController =
        [[ManualFillAccessoryViewController alloc] initWithDelegate:self];

    _formInputAccessoryMediator = [[FormInputAccessoryMediator alloc]
        initWithConsumer:self.formInputAccessoryViewController
            webStateList:webStateList];
    _formInputAccessoryMediator.manualFillAccessoryViewController =
        _manualFillAccessoryViewController;
  }
  return self;
}

- (void)stop {
  [self stopChildren];
  [self.manualFillAccessoryViewController reset];
  [self.formInputAccessoryViewController restoreOriginalKeyboardView];
}

#pragma mark - Presenting Children

- (void)stopChildren {
  for (ChromeCoordinator* coordinator in self.childCoordinators) {
    [coordinator stop];
  }
  [self.childCoordinators removeAllObjects];
}

- (void)startPasswordsFromButton:(UIButton*)button {
  ManualFillPasswordCoordinator* passwordCoordinator =
      [[ManualFillPasswordCoordinator alloc]
          initWithBaseViewController:self.baseViewController
                        browserState:self.browserState
                        webStateList:self.webStateList
                    injectionHandler:self.manualFillInjectionHandler];
  passwordCoordinator.delegate = self;
  if (IsIPadIdiom()) {
    [passwordCoordinator presentFromButton:button];
  } else {
    [self.formInputAccessoryViewController
        presentView:passwordCoordinator.viewController.view];
  }

  [self.childCoordinators addObject:passwordCoordinator];
  [self.formInputAccessoryMediator disableSuggestions];
}

#pragma mark - ManualFillAccessoryViewControllerDelegate

- (void)keyboardButtonPressed {
  [self stopChildren];
  [self.formInputAccessoryMediator enableSuggestions];
}

- (void)accountButtonPressed {
  [self stopChildren];
  // TODO(crbug.com/845472): implement.
}

- (void)cardButtonPressed {
  [self stopChildren];
  // TODO(crbug.com/845472): implement.
}

- (void)passwordButtonPressed:(UIButton*)sender {
  [self stopChildren];
  [self startPasswordsFromButton:sender];
}

#pragma mark - PasswordCoordinatorDelegate

- (void)openPasswordSettings {
  [self.delegate openPasswordSettings];
}

- (void)resetAccessoryView {
  [self.manualFillAccessoryViewController reset];
}

@end
