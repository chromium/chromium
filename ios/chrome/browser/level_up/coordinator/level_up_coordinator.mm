// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/coordinator/level_up_coordinator.h"

#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/level_up/coordinator/level_up_mediator.h"
#import "ios/chrome/browser/level_up/model/level_up_service.h"
#import "ios/chrome/browser/level_up/model/level_up_service_factory.h"
#import "ios/chrome/browser/level_up/model/task_info.h"
#import "ios/chrome/browser/level_up/ui/level_up_all_tasks_view_controller.h"
#import "ios/chrome/browser/level_up/ui/level_up_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/level_up_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message_action.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Spacing before icon in opt out confirmation sheet.
constexpr CGFloat kOptOutSheetSpacingAboveIcon = 24;

// Spacing between content in opt out confirmation sheet.
constexpr CGFloat kOptOutSheetContentSpacing = 16;

// Corner radius for the icon container in opt out confirmation sheet.
constexpr CGFloat kOptOutSheetIconContainerCornerRadius = 12;

// Point size for the icon symbol image in opt out confirmation sheet.
constexpr CGFloat kOptOutSheetIconPointSize = 24;

// Width and height of the icon container in opt out confirmation sheet.
constexpr CGFloat kOptOutSheetIconContainerSize = 56;

// Maximum height detent ratio for the opt out confirmation sheet.
constexpr double kOptOutSheetMaxDetentRatio = 0.75;

void RunPendingAction(TaskInfo::NavigationAction pending_action,
                      base::WeakPtr<Browser> weak_browser) {
  if (weak_browser && !pending_action.is_null()) {
    pending_action.Run(weak_browser->GetCommandDispatcher(),
                       weak_browser.get());
  }
}

}  // namespace

@interface LevelUpCoordinator () <ConfirmationAlertActionHandler,
                                  LevelUpAllTasksViewControllerDelegate,
                                  LevelUpMediatorDelegate,
                                  LevelUpViewControllerDelegate>

@property(nonatomic, strong) LevelUpMediator* mediator;
@property(nonatomic, strong) LevelUpViewController* viewController;
@property(nonatomic, strong) UINavigationController* navigationController;

@end

@implementation LevelUpCoordinator {
  TaskInfo::NavigationAction _pendingNavigationAction;
  ConfirmationAlertViewController* _optOutConfirmationViewController;
}

- (void)start {
  [super start];

  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(self.browser->GetProfile());
  if (!authService->HasPrimaryIdentity()) {
    [self showSignedOutSnackbarAndDismiss];
    return;
  }

  PrefService* prefService = self.browser->GetProfile()->GetPrefs();
  if (!prefService->GetBoolean(prefs::kLevelUpOptIn)) {
    // TODO(crbug.com/546095156): Show the promo when the user didn't opt in to
    // Level Up.
    prefService->SetBoolean(prefs::kLevelUpOptIn, true);
  }

  self.viewController = [[LevelUpViewController alloc] init];
  self.viewController.handler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), LevelUpCommands);
  [self.viewController setDelegate:self];

  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(self.browser->GetProfile());
  LevelUpService* levelUpService =
      LevelUpServiceFactory::GetForProfile(self.browser->GetProfile());
  self.mediator =
      [[LevelUpMediator alloc] initWithAuthenticationService:authService
                                             identityManager:identityManager
                                              levelUpService:levelUpService
                                                 prefService:prefService];

  self.mediator.delegate = self;
  self.mediator.profileConsumer = self.viewController;
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
  if (_optOutConfirmationViewController) {
    [_optOutConfirmationViewController dismissViewControllerAnimated:NO
                                                          completion:nil];
    _optOutConfirmationViewController = nil;
  }

  TaskInfo::NavigationAction pendingAction = _pendingNavigationAction;
  base::WeakPtr<Browser> weakBrowser =
      self.browser ? self.browser->AsWeakPtr() : nullptr;

  [self.navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           RunPendingAction(pendingAction, weakBrowser);
                         }];
  self.viewController = nil;
  self.mediator.delegate = nil;
  self.mediator.profileConsumer = nil;
  self.mediator.consumer = nil;
  [self.mediator disconnect];
  self.mediator = nil;
  self.navigationController = nil;

  [super stop];
}

#pragma mark - LevelUpViewControllerDelegate

- (void)didTapSeeAllTasks:(LevelUpViewController*)controller {
  LevelUpAllTasksViewController* allTasksVC =
      [[LevelUpAllTasksViewController alloc] init];
  allTasksVC.delegate = self;
  [self.navigationController pushViewController:allTasksVC animated:YES];
  [self.mediator configureAllTasksConsumer:allTasksVC];
}

- (void)didTapToggleProgressUpdates:(LevelUpViewController*)controller {
  [self.mediator toggleProgressUpdates];
}

- (void)didTapTurnOffLevelUp:(LevelUpViewController*)controller {
  ButtonStackConfiguration* config = [[ButtonStackConfiguration alloc] init];
  config.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_LEVEL_UP_TURN_OFF_LEVEL_UP);
  config.primaryButtonStyle = ChromeButtonStylePrimaryDestructive;
  config.secondaryActionString = l10n_util::GetNSString(IDS_CANCEL);

  ConfirmationAlertViewController* confirmationAlert =
      [[ConfirmationAlertViewController alloc] initWithConfiguration:config];
  confirmationAlert.titleString =
      l10n_util::GetNSString(IDS_IOS_LEVEL_UP_TURN_OFF_LEVEL_UP);
  confirmationAlert.subtitleString =
      l10n_util::GetNSString(IDS_IOS_LEVEL_UP_TURN_OFF_CONFIRMATION_SUBTITLE);
  confirmationAlert.actionHandler = self;
  confirmationAlert.topAlignedLayout = YES;
  confirmationAlert.customSpacingBeforeImage = kOptOutSheetSpacingAboveIcon;
  confirmationAlert.customSpacing = kOptOutSheetContentSpacing;
  confirmationAlert.addsContentViewBottomInset = NO;

  // Use a full-width wrapper view so the icon container is centered
  // horizontally without being stretched by the stack view's fill alignment.
  UIView* iconWrapper = [[UIView alloc] init];
  iconWrapper.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* iconContainer = [[UIView alloc] init];
  iconContainer.translatesAutoresizingMaskIntoConstraints = NO;
  iconContainer.backgroundColor = [UIColor colorNamed:kRed100Color];
  iconContainer.layer.cornerRadius = kOptOutSheetIconContainerCornerRadius;

  UIImageView* iconImageView = [[UIImageView alloc]
      initWithImage:SymbolTemplateWithPointSize(SymbolArrowshapeUp,
                                                kOptOutSheetIconPointSize)];
  iconImageView.translatesAutoresizingMaskIntoConstraints = NO;
  iconImageView.tintColor = [UIColor colorNamed:kRed500Color];
  [iconContainer addSubview:iconImageView];
  [iconWrapper addSubview:iconContainer];

  [NSLayoutConstraint activateConstraints:@[
    [iconContainer.widthAnchor
        constraintEqualToConstant:kOptOutSheetIconContainerSize],
    [iconContainer.heightAnchor
        constraintEqualToConstant:kOptOutSheetIconContainerSize],
    [iconContainer.topAnchor constraintEqualToAnchor:iconWrapper.topAnchor],
    [iconContainer.bottomAnchor
        constraintEqualToAnchor:iconWrapper.bottomAnchor],
    [iconContainer.centerXAnchor
        constraintEqualToAnchor:iconWrapper.centerXAnchor],

    [iconImageView.centerXAnchor
        constraintEqualToAnchor:iconContainer.centerXAnchor],
    [iconImageView.centerYAnchor
        constraintEqualToAnchor:iconContainer.centerYAnchor],
  ]];

  confirmationAlert.aboveTitleView = iconWrapper;
  confirmationAlert.modalPresentationStyle = UIModalPresentationPageSheet;

  UISheetPresentationController* sheet =
      confirmationAlert.sheetPresentationController;
  sheet.prefersGrabberVisible = NO;

  __weak ConfirmationAlertViewController* weakAlert = confirmationAlert;

  auto preferredHeightForSheetContent = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    CGFloat height = [weakAlert preferredHeightForContent];
    // Make sure detent is not too large, but also make
    // sure it looks like a sheet, not a full screen card.
    return MIN(height, kOptOutSheetMaxDetentRatio * context.maximumDetentValue);
  };
  sheet.detents = @[ [UISheetPresentationControllerDetent
      customDetentWithIdentifier:nil
                        resolver:preferredHeightForSheetContent] ];

  _optOutConfirmationViewController = confirmationAlert;
  [self.navigationController presentViewController:confirmationAlert
                                          animated:YES
                                        completion:nil];
}

- (void)levelUpViewController:(LevelUpViewController*)controller
                   didTapTask:(LevelUpTask*)task {
  [self didTapTask:task];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  __weak __typeof(self) weakSelf = self;
  [_optOutConfirmationViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [weakSelf.mediator turnOffLevelUp];
                         }];
  _optOutConfirmationViewController = nil;
}

- (void)confirmationAlertSecondaryAction {
  [_optOutConfirmationViewController dismissViewControllerAnimated:YES
                                                        completion:nil];
  _optOutConfirmationViewController = nil;
}

#pragma mark - LevelUpMediatorDelegate

- (void)levelUpMediatorWantsToBeDismissed:(LevelUpMediator*)mediator {
  [HandlerForProtocol(self.browser->GetCommandDispatcher(), LevelUpCommands)
      dismissLevelUp];
}

#pragma mark - LevelUpAllTasksViewControllerDelegate

- (void)levelUpAllTasksViewController:(LevelUpAllTasksViewController*)controller
                           didTapTask:(LevelUpTask*)task {
  [self didTapTask:task];
}

#pragma mark - Private

// Handles a task tap by closing the level up screen and preparing to navigate
// to the beginning of the tapped task.
- (void)didTapTask:(LevelUpTask*)task {
  // Save the task so it can be executed once the level up view is closed.
  _pendingNavigationAction = task.taskInfo->GetNavigationAction();

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  id<LevelUpCommands> levelUpHandler =
      HandlerForProtocol(dispatcher, LevelUpCommands);
  [levelUpHandler dismissLevelUp];
}

// Shows a snackbar prompting the user to sign in and dismisses the Level Up
// view.
- (void)showSignedOutSnackbarAndDismiss {
  id<SnackbarCommands> snackbarHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SnackbarCommands);
  NSString* messageText =
      l10n_util::GetNSString(IDS_IOS_LEVEL_UP_SIGNED_OUT_SNACKBAR);
  NSString* buttonText =
      l10n_util::GetNSString(IDS_IOS_LEVEL_UP_SIGNED_OUT_SNACKBAR_ACTION);
  SnackbarMessage* message =
      [[SnackbarMessage alloc] initWithTitle:messageText];
  SnackbarMessageAction* action = [[SnackbarMessageAction alloc] init];
  action.title = buttonText;
  __weak id<SceneCommands> weakSceneHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), SceneCommands);
  __weak UIViewController* weakBaseViewController = self.baseViewController;
  action.handler = ^{
    ShowSigninCommand* command = [[ShowSigninCommand alloc]
        initWithOperation:AuthenticationOperation::kSigninOnly
                 identity:nil
              accessPoint:signin_metrics::AccessPoint::kLevelUp
              promoAction:signin_metrics::PromoAction::
                              PROMO_ACTION_NO_SIGNIN_PROMO
               completion:nil];
    [weakSceneHandler showSignin:command
              baseViewController:weakBaseViewController];
  };
  message.action = action;
  [snackbarHandler showSnackbarMessage:message];

  id<LevelUpCommands> handler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), LevelUpCommands);
  [handler dismissLevelUp];
}

@end
