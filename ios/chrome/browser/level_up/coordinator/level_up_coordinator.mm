// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/coordinator/level_up_coordinator.h"

#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/level_up/coordinator/level_up_mediator.h"
#import "ios/chrome/browser/level_up/model/level_up_service.h"
#import "ios/chrome/browser/level_up/model/level_up_service_factory.h"
#import "ios/chrome/browser/level_up/model/task_info.h"
#import "ios/chrome/browser/level_up/ui/level_up_all_tasks_view_controller.h"
#import "ios/chrome/browser/level_up/ui/level_up_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/level_up_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message_action.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

void RunPendingAction(TaskInfo::NavigationAction pending_action,
                      base::WeakPtr<Browser> weak_browser) {
  if (weak_browser && !pending_action.is_null()) {
    pending_action.Run(weak_browser->GetCommandDispatcher(),
                       weak_browser.get());
  }
}

}  // namespace

@interface LevelUpCoordinator () <LevelUpAllTasksViewControllerDelegate,
                                  LevelUpMediatorDelegate,
                                  LevelUpViewControllerDelegate>

@property(nonatomic, strong) LevelUpMediator* mediator;
@property(nonatomic, strong) LevelUpViewController* viewController;
@property(nonatomic, strong) UINavigationController* navigationController;

@end

@implementation LevelUpCoordinator {
  TaskInfo::NavigationAction _pendingNavigationAction;
}

- (void)start {
  [super start];

  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(self.browser->GetProfile());
  if (!authService->HasPrimaryIdentity()) {
    [self showSignedOutSnackbarAndDismiss];
    return;
  }

  self.viewController = [[LevelUpViewController alloc] init];
  self.viewController.handler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), LevelUpCommands);
  [self.viewController setDelegate:self];

  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(self.browser->GetProfile());
  LevelUpService* levelUpService =
      LevelUpServiceFactory::GetForProfile(self.browser->GetProfile());
  PrefService* prefService = self.browser->GetProfile()->GetPrefs();
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
  [self.mediator turnOffLevelUp];
}

- (void)levelUpViewController:(LevelUpViewController*)controller
                   didTapTask:(LevelUpTask*)task {
  [self didTapTask:task];
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
