// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_coordinator.h"

#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_configuration.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_confirmation/account_picker_confirmation_screen_coordinator.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_confirmation/account_picker_confirmation_screen_coordinator_delegate.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_coordinator_delegate.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_layout_delegate.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_logger.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_screen/account_picker_screen_navigation_controller.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_screen/account_picker_screen_presentation_controller.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_screen/account_picker_screen_slide_transition_animator.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_selection/account_picker_selection_screen_coordinator.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface AccountPickerCoordinator () <
    AccountPickerConfirmationScreenCoordinatorDelegate,
    AccountPickerLayoutDelegate,
    AccountPickerSelectionScreenCoordinatorDelegate,
    AccountPickerScreenPresentationControllerDelegate,
    UINavigationControllerDelegate,
    UIViewControllerTransitioningDelegate>

@end

@implementation AccountPickerCoordinator {
  // Navigation controller for the account picker.
  __strong AccountPickerScreenNavigationController* _navigationController;

  // Coordinator to display modal alerts to the user.
  __strong AlertCoordinator* _alertCoordinator;
  // Coordinator for the first screen.
  __strong AccountPickerConfirmationScreenCoordinator*
      _accountPickerConfirmationScreenCoordinator;
  // Coordinator to select another identity.
  __strong AccountPickerSelectionScreenCoordinator*
      _accountPickerSelectionScreenCoordinator;

  // The configuration for the account picker.
  __strong AccountPickerConfiguration* _configuration;
}

#pragma mark - Public

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                             configuration:
                                 (AccountPickerConfiguration*)configuration {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _configuration = configuration;
  }
  return self;
}

- (void)stopAnimated:(BOOL)animated {
  __weak __typeof(self) weakSelf = self;
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:animated
                         completion:^{
                           [weakSelf.delegate
                               accountPickerCoordinatorDidStop:weakSelf];
                         }];
  _navigationController.delegate = nil;
  _navigationController.transitioningDelegate = nil;
  _navigationController = nil;

  [_alertCoordinator stop];
  _alertCoordinator = nil;
  [_accountPickerSelectionScreenCoordinator stop];
  _accountPickerSelectionScreenCoordinator = nil;
  [_accountPickerConfirmationScreenCoordinator stop];
  _accountPickerConfirmationScreenCoordinator = nil;
}

#pragma mark - AccountPickerConsumer

- (void)startValidationSpinner {
  [_accountPickerConfirmationScreenCoordinator startValidationSpinner];
}

- (void)stopValidationSpinner {
  [_accountPickerConfirmationScreenCoordinator stopValidationSpinner];
}

- (void)setIdentityButtonHidden:(BOOL)hidden animated:(BOOL)animated {
  [_accountPickerConfirmationScreenCoordinator
      setIdentityButtonHidden:hidden
                     animated:animated];
}

#pragma mark - ChromeCoordinator

- (void)start {
  // Create AccountPickerConfirmationScreenCoordinator.
  _accountPickerConfirmationScreenCoordinator =
      [[AccountPickerConfirmationScreenCoordinator alloc]
          initWithBaseViewController:_navigationController
                             browser:self.browser
                       configuration:_configuration];
  _accountPickerConfirmationScreenCoordinator.delegate = self;
  _accountPickerConfirmationScreenCoordinator.layoutDelegate = self;
  _accountPickerConfirmationScreenCoordinator.childViewController =
      self.accountConfirmationChildViewController;
  [_accountPickerConfirmationScreenCoordinator start];

  // Create AccountPickerScreenNavigationController.
  _navigationController = [[AccountPickerScreenNavigationController alloc]
      initWithRootViewController:_accountPickerConfirmationScreenCoordinator
                                     .viewController];
  _navigationController.delegate = self;
  _navigationController.modalPresentationStyle = UIModalPresentationCustom;
  _navigationController.transitioningDelegate = self;
  // Present the view.
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self stopAnimated:NO];
}

#pragma mark - Properties

- (id<SystemIdentity>)selectedIdentity {
  return _accountPickerConfirmationScreenCoordinator.selectedIdentity;
}

- (void)setSelectedIdentity:(id<SystemIdentity>)selectedIdentity {
  _accountPickerConfirmationScreenCoordinator.selectedIdentity =
      selectedIdentity;
}

- (UIViewController*)viewController {
  return _navigationController;
}

#pragma mark - Private

// Called on completion of the AddAccountSigninCoordinator view.
- (void)addAccountCompletionWithIdentity:(id<SystemIdentity>)identity {
  if (!identity) {
    return;
  }
  _accountPickerConfirmationScreenCoordinator.selectedIdentity = identity;
  [self.logger logAccountPickerAddAccountCompleted];
}

// Opens an AddAccountSigninCoordinator to add an account to the device.
- (void)openAddAccountCoordinator {
  __weak __typeof(self) weakSelf = self;
  [self.delegate accountPickerCoordinator:self
             openAddAccountWithCompletion:^(id<SystemIdentity> identity) {
               [weakSelf addAccountCompletionWithIdentity:identity];
             }];
  [self.logger logAccountPickerAddAccountScreenOpened];
}

// Starts the validation flow.
- (void)startValidation {
  [self.delegate
      accountPickerCoordinator:self
             didSelectIdentity:self.selectedIdentity
                  askEveryTime:_accountPickerConfirmationScreenCoordinator
                                   .askEveryTime];
}

#pragma mark - AccountPickerLayoutDelegate

- (AccountPickerSheetDisplayStyle)displayStyle {
  return _navigationController.displayStyle;
}

#pragma mark - UINavigationControllerDelegate

- (id<UIViewControllerAnimatedTransitioning>)
               navigationController:
                   (UINavigationController*)navigationController
    animationControllerForOperation:(UINavigationControllerOperation)operation
                 fromViewController:(UIViewController*)fromVC
                   toViewController:(UIViewController*)toVC {
  switch (operation) {
    case UINavigationControllerOperationNone:
      return nil;
    case UINavigationControllerOperationPush:
      return [[AccountPickerScreenSlideTransitionAnimator alloc]
             initWithAnimation:kAccountPickerScreenSlideAnimationPushing
          navigationController:_navigationController];
    case UINavigationControllerOperationPop:
      return [[AccountPickerScreenSlideTransitionAnimator alloc]
             initWithAnimation:kAccountPickerScreenSlideAnimationPopping
          navigationController:_navigationController];
  }
}

- (id<UIViewControllerInteractiveTransitioning>)
                           navigationController:
                               (UINavigationController*)navigationController
    interactionControllerForAnimationController:
        (id<UIViewControllerAnimatedTransitioning>)animationController {
  return _navigationController.interactionTransition;
}

- (void)navigationController:(UINavigationController*)navigationController
       didShowViewController:(UIViewController*)viewController
                    animated:(BOOL)animated {
  DCHECK(navigationController == _navigationController)
      << base::SysNSStringToUTF8([self description]);
  DCHECK(navigationController.viewControllers.count > 0)
      << base::SysNSStringToUTF8([self description]);
  DCHECK(navigationController.viewControllers[0] ==
         _accountPickerConfirmationScreenCoordinator.viewController)
      << base::SysNSStringToUTF8([self description]);
  if (_navigationController.viewControllers.count == 1 &&
      _accountPickerSelectionScreenCoordinator) {
    // AccountChooserCoordinator has been removed by "Back" button.
    [_accountPickerSelectionScreenCoordinator stop];
    _accountPickerSelectionScreenCoordinator = nil;
    [self.logger logAccountPickerSelectionScreenClosed];
  }
}

#pragma mark - AccountPickerSelectionScreenCoordinatorDelegate

- (void)accountPickerSelectionScreenCoordinatorIdentitySelected:
    (AccountPickerSelectionScreenCoordinator*)coordinator {
  if (_accountPickerSelectionScreenCoordinator.selectedIdentity !=
      _accountPickerConfirmationScreenCoordinator.selectedIdentity) {
    [self.logger logAccountPickerNewIdentitySelected];
  }
  _accountPickerConfirmationScreenCoordinator.selectedIdentity =
      _accountPickerSelectionScreenCoordinator.selectedIdentity;
  [_accountPickerSelectionScreenCoordinator stop];
  _accountPickerSelectionScreenCoordinator = nil;
  [_navigationController popViewControllerAnimated:YES];
}

- (void)accountPickerSelectionScreenCoordinatorOpenAddAccount:
    (AccountPickerSelectionScreenCoordinator*)coordinator {
  [self openAddAccountCoordinator];
}

#pragma mark - AccountPickerConfirmationScreenCoordinatorDelegate

- (void)accountPickerConfirmationScreenCoordinatorCancel:
    (AccountPickerConfirmationScreenCoordinator*)coordinator {
  [self.delegate accountPickerCoordinatorCancel:self];
}

- (void)
    accountPickerConfirmationScreenCoordinatorOpenAccountPickerSelectionScreen:
        (AccountPickerConfirmationScreenCoordinator*)coordinator {
  if (_accountPickerSelectionScreenCoordinator) {
    // If the user taps the identity button several times in the confirmation
    // screen before the transition to the selection screen, this method might
    // be invoked more than once. Hence an early return if the selection screen
    // coordinator has already been created.
    return;
  }

  _accountPickerSelectionScreenCoordinator =
      [[AccountPickerSelectionScreenCoordinator alloc]
          initWithBaseViewController:_navigationController
                             browser:self.browser];
  _accountPickerSelectionScreenCoordinator.delegate = self;
  _accountPickerSelectionScreenCoordinator.layoutDelegate = self;
  [_accountPickerSelectionScreenCoordinator
      startWithSelectedIdentity:_accountPickerConfirmationScreenCoordinator
                                    .selectedIdentity];
  [_navigationController
      pushViewController:_accountPickerSelectionScreenCoordinator.viewController
                animated:YES];
  [self.logger logAccountPickerSelectionScreenOpened];
}

- (void)accountPickerConfirmationScreenCoordinatorSubmit:
    (AccountPickerConfirmationScreenCoordinator*)coordinator {
  [self startValidation];
}

- (void)accountPickerConfirmationScreenCoordinatorOpenAddAccount:
    (AccountPickerConfirmationScreenCoordinator*)coordinator {
  [self openAddAccountCoordinator];
}

#pragma mark - UIViewControllerTransitioningDelegate

- (UIPresentationController*)
    presentationControllerForPresentedViewController:
        (UIViewController*)presentedViewController
                            presentingViewController:
                                (UIViewController*)presentingViewController
                                sourceViewController:(UIViewController*)source {
  DCHECK_EQ(_navigationController, presentedViewController)
      << base::SysNSStringToUTF8([self description]);
  AccountPickerScreenPresentationController* controller =
      [[AccountPickerScreenPresentationController alloc]
          initWithAccountPickerScreenNavigationController:_navigationController
                                 presentingViewController:
                                     presentingViewController];
  controller.actionDelegate = self;
  return controller;
}

#pragma mark - AccountPickerScreenPresentationControllerDelegate

- (void)accountPickerScreenPresentationControllerBackgroundTapped:
    (AccountPickerScreenPresentationController*)controller {
  if (_configuration.dismissOnBackgroundTap) {
    [self.delegate accountPickerCoordinatorCancel:self];
  }
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString
      stringWithFormat:@"<%@: %p, accountPickerConfirmationScreenCoordinator: "
                       @"%p, alertCoordinator: %p, "
                       @"accountPickerSelectionScreenCoordinator %p>",
                       self.class.description, self,
                       _accountPickerConfirmationScreenCoordinator,
                       _alertCoordinator,
                       _accountPickerSelectionScreenCoordinator];
}

@end
