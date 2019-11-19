// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/alert/sc_alert_coordinator.h"

#import "ios/chrome/browser/ui/alert_view_controller/alert_action.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_view_controller.h"
#import "ios/chrome/browser/ui/elements/text_field_configuration.h"
#import "ios/chrome/browser/ui/presenters/non_modal_view_controller_presenter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCAlertCoordinator ()
@property(nonatomic, strong)
    UIViewController* presentationContextViewController;
@property(nonatomic, strong) UISwitch* blockAlertSwitch;
@property(nonatomic, strong) NonModalViewControllerPresenter* presenter;
@end

@implementation SCAlertCoordinator

@synthesize baseViewController = _baseViewController;

- (void)stop {
  self.baseViewController.toolbarHidden = YES;
  [self.baseViewController popViewControllerAnimated:YES];
}

- (void)start {
  self.baseViewController.toolbarHidden = NO;

  UIViewController* containerViewController = [[UIViewController alloc] init];
  containerViewController.extendedLayoutIncludesOpaqueBars = YES;
  UIBarButtonItem* dummyItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCamera
                           target:nil
                           action:nil];
  containerViewController.toolbarItems = @[ dummyItem ];
  UIBarButtonItem* backButton =
      [[UIBarButtonItem alloc] initWithTitle:@"Back"
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(stop)];
  containerViewController.navigationItem.leftBarButtonItem = backButton;

  self.presentationContextViewController = [[UIViewController alloc] init];
  self.presentationContextViewController.definesPresentationContext = YES;
  self.presentationContextViewController.title = @"Alert";
  self.presentationContextViewController.view.backgroundColor =
      [UIColor whiteColor];

  [containerViewController
      addChildViewController:self.presentationContextViewController];
  [containerViewController.view
      addSubview:self.presentationContextViewController.view];

  UIButton* alertButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [alertButton setTitle:@"alert()" forState:UIControlStateNormal];
  [alertButton addTarget:self
                  action:@selector(showAlert)
        forControlEvents:UIControlEventTouchUpInside];

  UIButton* promptButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [promptButton setTitle:@"prompt()" forState:UIControlStateNormal];
  [promptButton addTarget:self
                   action:@selector(showPrompt)
         forControlEvents:UIControlEventTouchUpInside];

  UIButton* confirmButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [confirmButton setTitle:@"confirm()" forState:UIControlStateNormal];
  [confirmButton addTarget:self
                    action:@selector(showConfirm)
          forControlEvents:UIControlEventTouchUpInside];

  UIButton* authButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [authButton setTitle:@"HTTP Auth" forState:UIControlStateNormal];
  [authButton addTarget:self
                 action:@selector(showHTTPAuth)
       forControlEvents:UIControlEventTouchUpInside];

  UIButton* longButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [longButton setTitle:@"Long Alert" forState:UIControlStateNormal];
  [longButton addTarget:self
                 action:@selector(showLongAlert)
       forControlEvents:UIControlEventTouchUpInside];

  UILabel* blockAlertsLabel = [[UILabel alloc] init];
  blockAlertsLabel.text = @"Show \"Block Alerts Button\"";

  self.blockAlertSwitch = [[UISwitch alloc] init];

  UIStackView* switchStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ self.blockAlertSwitch, blockAlertsLabel ]];
  switchStack.axis = UILayoutConstraintAxisHorizontal;
  switchStack.spacing = 16;
  switchStack.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView* verticalStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    alertButton, promptButton, confirmButton, authButton, longButton,
    switchStack
  ]];
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.spacing = 30;
  verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
  verticalStack.distribution = UIStackViewDistributionFillEqually;
  [self.presentationContextViewController.view addSubview:verticalStack];

  [NSLayoutConstraint activateConstraints:@[
    [verticalStack.centerXAnchor
        constraintEqualToAnchor:self.presentationContextViewController.view
                                    .centerXAnchor],
    [verticalStack.centerYAnchor
        constraintEqualToAnchor:self.presentationContextViewController.view
                                    .centerYAnchor],
  ]];
  [self.baseViewController pushViewController:containerViewController
                                     animated:YES];
}

- (void)showAlert {
  __weak __typeof__(self) weakSelf = self;
  AlertAction* action =
      [AlertAction actionWithTitle:@"OK"
                             style:UIAlertActionStyleDefault
                           handler:^(AlertAction* action) {
                             [weakSelf.presenter dismissAnimated:YES];
                           }];
  [self presentAlertWithTitle:@"chromium.org says"
                      message:@"This is an alert message from a website."
                      actions:@[ action ]
      textFieldConfigurations:nil];
}

- (void)showPrompt {
  TextFieldConfiguration* fieldConfiguration =
      [[TextFieldConfiguration alloc] initWithText:nil
                                       placeholder:@"placehorder"
                           accessibilityIdentifier:nil
                                   secureTextEntry:NO];
  __weak __typeof__(self) weakSelf = self;
  AlertAction* OKAction =
      [AlertAction actionWithTitle:@"OK"
                             style:UIAlertActionStyleDefault
                           handler:^(AlertAction* action) {
                             [weakSelf.presenter dismissAnimated:YES];
                           }];
  AlertAction* cancelAction =
      [AlertAction actionWithTitle:@"Cancel"
                             style:UIAlertActionStyleCancel
                           handler:^(AlertAction* action) {
                             [weakSelf.presenter dismissAnimated:YES];
                           }];
  [self presentAlertWithTitle:@"chromium.org says"
                      message:@"This is a promp message from a website."
                      actions:@[ OKAction, cancelAction ]
      textFieldConfigurations:@[ fieldConfiguration ]];
}

- (void)showConfirm {
  __weak __typeof__(self) weakSelf = self;
  AlertAction* OKAction =
      [AlertAction actionWithTitle:@"OK"
                             style:UIAlertActionStyleDefault
                           handler:^(AlertAction* action) {
                             [weakSelf.presenter dismissAnimated:YES];
                           }];
  AlertAction* cancelAction =
      [AlertAction actionWithTitle:@"Cancel"
                             style:UIAlertActionStyleCancel
                           handler:^(AlertAction* action) {
                             [weakSelf.presenter dismissAnimated:YES];
                           }];
  [self presentAlertWithTitle:@"chromium.org says"
                      message:@"This is a confirm message from a website."
                      actions:@[ OKAction, cancelAction ]
      textFieldConfigurations:nil];
}

- (void)showHTTPAuth {
  TextFieldConfiguration* usernameOptions =
      [[TextFieldConfiguration alloc] initWithText:nil
                                       placeholder:@"Username"
                           accessibilityIdentifier:nil
                                   secureTextEntry:NO];
  TextFieldConfiguration* passwordOptions =
      [[TextFieldConfiguration alloc] initWithText:nil
                                       placeholder:@"Password"
                           accessibilityIdentifier:nil
                                   secureTextEntry:YES];

  __weak __typeof__(self) weakSelf = self;
  AlertAction* OKAction =
      [AlertAction actionWithTitle:@"Sign In"
                             style:UIAlertActionStyleDefault
                           handler:^(AlertAction* action) {
                             [weakSelf.presenter dismissAnimated:YES];
                           }];
  AlertAction* cancelAction =
      [AlertAction actionWithTitle:@"Cancel"
                             style:UIAlertActionStyleCancel
                           handler:^(AlertAction* action) {
                             [weakSelf.presenter dismissAnimated:YES];
                           }];
  [self presentAlertWithTitle:@"Sign In"
                      message:@"https://www.chromium.org requires a "
                              @"username and a password."
                      actions:@[ OKAction, cancelAction ]
      textFieldConfigurations:@[ usernameOptions, passwordOptions ]];
}

- (void)showLongAlert {
  TextFieldConfiguration* usernameOptions =
      [[TextFieldConfiguration alloc] initWithText:nil
                                       placeholder:@"Username"
                           accessibilityIdentifier:nil
                                   secureTextEntry:NO];
  TextFieldConfiguration* passwordOptions =
      [[TextFieldConfiguration alloc] initWithText:nil
                                       placeholder:@"Password"
                           accessibilityIdentifier:nil
                                   secureTextEntry:YES];
  __weak __typeof__(self) weakSelf = self;
  AlertAction* OKAction =
      [AlertAction actionWithTitle:@"Sign In"
                             style:UIAlertActionStyleDefault
                           handler:^(AlertAction* action) {
                             [weakSelf.presenter dismissAnimated:YES];
                           }];
  AlertAction* cancelAction =
      [AlertAction actionWithTitle:@"Cancel"
                             style:UIAlertActionStyleCancel
                           handler:^(AlertAction* action) {
                             [weakSelf.presenter dismissAnimated:YES];
                           }];
  NSString* message =
      @"It was the best of times, it was the worst of times, it was the age of "
      @"wisdom, it was the age of foolishness, it was the epoch of belief, it "
      @"was the epoch of incredulity, it was the season of Light, it was the "
      @"season of Darkness, it was the spring of hope, it was the winter of "
      @"despair, we had everything before us, we had nothing before us, we "
      @"were all going direct to Heaven, we were all going direct the other "
      @"way. It was the best of times, it was the worst of times, it was the "
      @"age of wisdom, it was the age of foolishness, it was the epoch of "
      @"belief, it was the epoch of incredulity, it was the season of Light, "
      @"it was the season of Darkness, it was the spring of hope, it was the "
      @"winter of despair, we had everything before us, we had nothing before "
      @"us, we were all going direct to Heaven, we were all going direct the "
      @"other way.";
  [self presentAlertWithTitle:@"Long Alert"
                      message:message
                      actions:@[ OKAction, cancelAction ]
      textFieldConfigurations:@[ usernameOptions, passwordOptions ]];
}

- (void)presentAlertWithTitle:(NSString*)title
                      message:(NSString*)message
                      actions:(NSArray<AlertAction*>*)actions
      textFieldConfigurations:
          (NSArray<TextFieldConfiguration*>*)textFieldConfigurations {
  AlertViewController* alert = [[AlertViewController alloc] init];
  [alert setTitle:title];
  [alert setMessage:message];
  [alert setTextFieldConfigurations:textFieldConfigurations];

  if (self.blockAlertSwitch.isOn) {
    __weak __typeof__(self) weakSelf = self;
    AlertAction* blockAction =
        [AlertAction actionWithTitle:@"Block Dialogs"
                               style:UIAlertActionStyleDestructive
                             handler:^(AlertAction* action) {
                               [weakSelf.presenter dismissAnimated:YES];
                             }];
    NSArray* newActions = [actions arrayByAddingObject:blockAction];
    [alert setActions:newActions];
  } else {
    [alert setActions:actions];
  }

  self.presenter = [[NonModalViewControllerPresenter alloc] init];
  self.presenter.baseViewController = self.presentationContextViewController;
  self.presenter.presentedViewController = alert;
  [self.presenter prepareForPresentation];
  [self.presenter presentAnimated:YES];
}

@end
