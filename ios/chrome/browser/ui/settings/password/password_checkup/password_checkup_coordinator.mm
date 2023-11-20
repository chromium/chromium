// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_coordinator.h"

#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/model/metrics/ios_password_manager_metrics.h"
#import "ios/chrome/browser/passwords/model/metrics/ios_password_manager_visits_recorder.h"
#import "ios/chrome/browser/passwords/model/password_checkup_metrics.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_commands.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_ui_features.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_coordinator.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"

using password_manager::PasswordCheckReferrer;
using password_manager::features::IsAuthOnEntryV2Enabled;

@interface PasswordCheckupCoordinator () <PasswordCheckupCommands,
                                          PasswordIssuesCoordinatorDelegate,
                                          ReauthenticationCoordinatorDelegate>

@end

@implementation PasswordCheckupCoordinator {
  // Main view controller for this coordinator.
  PasswordCheckupViewController* _viewController;

  // Main mediator for this coordinator.
  PasswordCheckupMediator* _mediator;

  // Coordinator for password issues.
  PasswordIssuesCoordinator* _passwordIssuesCoordinator;

  // Reauthentication module used by password issues coordinator.
  id<ReauthenticationProtocol> _reauthModule;

  // Coordinator for blocking Password Checkup until Local Authentication is
  // passed. Used for requiring authentication when opening Password Checkup
  // from outside the Password Manager and when the app is
  // backgrounded/foregrounded with Password Checkup opened.
  ReauthenticationCoordinator* _reauthCoordinator;

  // Location in the app from which Password Checkup was opened.
  PasswordCheckReferrer _referrer;

  // For recording visits after successful authentication.
  IOSPasswordManagerVisitsRecorder* _visitsRecorder;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                        reauthModule:(id<ReauthenticationProtocol>)reauthModule
                            referrer:(PasswordCheckReferrer)referrer {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _reauthModule = reauthModule;
    _dispatcher = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                     ApplicationCommands);
    _referrer = referrer;
    password_manager::LogPasswordCheckReferrer(referrer);
  }
  return self;
}

- (void)start {
  [super start];

  password_manager::LogOpenPasswordCheckupHomePage();
  _viewController = [[PasswordCheckupViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _viewController.handler = self;
  _mediator = [[PasswordCheckupMediator alloc]
      initWithPasswordCheckManager:IOSChromePasswordCheckManagerFactory::
                                       GetForBrowserState(
                                           self.browser->GetBrowserState())];
  _viewController.delegate = _mediator;
  _mediator.consumer = _viewController;

  BOOL requireAuthOnStart = [self shouldRequireAuthOnStart];

  // Disable animation when content will be blocked for reauth to prevent
  // flickering in navigation bar.
  [self.baseNavigationController pushViewController:_viewController
                                           animated:!requireAuthOnStart];

  _visitsRecorder = [[IOSPasswordManagerVisitsRecorder alloc]
      initWithPasswordManagerSurface:password_manager::PasswordManagerSurface::
                                         kPasswordCheckup];

  // Only record visit if no auth is required, otherwise wait for successful
  // auth.
  if (!requireAuthOnStart) {
    [_visitsRecorder maybeRecordVisitMetric];
  }

  if (IsAuthOnEntryV2Enabled()) {
    [self startReauthCoordinatorWithAuthOnStart:requireAuthOnStart];
  }
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  _viewController.handler = nil;
  _viewController = nil;

  [self stopPasswordIssuesCoordinator];
  [self stopReauthenticationCoordinator];
}

#pragma mark - PasswordCheckupCommands

- (void)dismissPasswordCheckupViewController {
  [self.delegate passwordCheckupCoordinatorDidRemove:self];
}

// Opens the Password Issues list displaying compromised, weak or reused
// credentials for `warningType`.
- (void)showPasswordIssuesWithWarningType:
    (password_manager::WarningType)warningType {
  DUMP_WILL_BE_CHECK(!_passwordIssuesCoordinator);

  [self stopReauthCoordinatorBeforeStartingChildCoordinator];

  password_manager::LogOpenPasswordIssuesList(warningType);

  _passwordIssuesCoordinator = [[PasswordIssuesCoordinator alloc]
            initForWarningType:warningType
      baseNavigationController:self.baseNavigationController
                       browser:self.browser];
  // No need to authenticate the user before showing password issues as the user
  // was already authenticated when opening the password manager.
  _passwordIssuesCoordinator.skipAuthenticationOnStart = YES;
  _passwordIssuesCoordinator.delegate = self;
  _passwordIssuesCoordinator.reauthModule = _reauthModule;
  [_passwordIssuesCoordinator start];
}

- (void)dismissAndOpenURL:(CrURL*)URL {
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:URL.gurl];
  [self.dispatcher closeSettingsUIAndOpenURL:command];
}

- (void)dismissAfterAllPasswordsGone {
  NSArray<UIViewController*>* viewControllers =
      self.baseNavigationController.viewControllers;
  NSInteger viewControllerIndex =
      [viewControllers indexOfObject:_viewController];

  // Nothing to do if the view controller was already removed from the
  // navigation stack.
  if (viewControllerIndex == NSNotFound) {
    return;
  }

  // Dismiss the whole navigation stack if checkup is the root view controller.
  if (viewControllerIndex == 0) {
    UIViewController* presentingViewController =
        _baseNavigationController.presentingViewController;
    [presentingViewController dismissViewControllerAnimated:YES completion:nil];
    return;
  }

  // Go to the previous view controller in the navigation stack.
  [self.baseNavigationController
      popToViewController:viewControllers[viewControllerIndex - 1]
                 animated:YES];
}

#pragma mark - PasswordIssuesCoordinatorDelegate

- (void)passwordIssuesCoordinatorDidRemove:
    (PasswordIssuesCoordinator*)coordinator {
  CHECK_EQ(_passwordIssuesCoordinator, coordinator);
  [self stopPasswordIssuesCoordinator];
  [self restartReauthCoordinator];
}

#pragma mark - PasswordManagerReauthenticationDelegate

- (void)dismissPasswordManagerAfterFailedReauthentication {
  [_delegate dismissPasswordManagerAfterFailedReauthentication];
}

#pragma mark - ReauthenticationCoordinatorDelegate

- (void)successfulReauthenticationWithCoordinator:
    (ReauthenticationCoordinator*)coordinator {
  [_visitsRecorder maybeRecordVisitMetric];
}

- (void)dismissUIAfterFailedReauthenticationWithCoordinator:
    (ReauthenticationCoordinator*)coordinator {
  CHECK_EQ(_reauthCoordinator, coordinator);

  [_delegate dismissPasswordManagerAfterFailedReauthentication];
}

- (void)willPushReauthenticationViewController {
  // No-op.
}

#pragma mark - Private

- (void)stopPasswordIssuesCoordinator {
  [_passwordIssuesCoordinator stop];
  _passwordIssuesCoordinator.delegate = nil;
  _passwordIssuesCoordinator = nil;
}

- (void)stopReauthenticationCoordinator {
  [_reauthCoordinator stop];
  _reauthCoordinator.delegate = nil;
  _reauthCoordinator = nil;
}

// Starts reauthCoordinator.
// - authOnStart: Pass `YES` to cover Password Checkup with an empty view
// controller until successful Local Authentication when reauthCoordinator
// starts.
//
// Local authentication is required every time the current
// scene is backgrounded and foregrounded until reauthCoordinator is stopped.
- (void)startReauthCoordinatorWithAuthOnStart:(BOOL)authOnStart {
  DCHECK(!_reauthCoordinator);

  _reauthCoordinator = [[ReauthenticationCoordinator alloc]
      initWithBaseNavigationController:_baseNavigationController
                               browser:self.browser
                reauthenticationModule:_reauthModule
                           authOnStart:authOnStart];

  _reauthCoordinator.delegate = self;

  [_reauthCoordinator start];
}

// Stop reauth coordinator when a child coordinator will be started.
//
// Needed so reauth coordinator doesn't block for reauth if the scene state
// changes while the child coordinator is presenting its content. The child
// coordinator will add its own reauth coordinator to block its content for
// reauth.
- (void)stopReauthCoordinatorBeforeStartingChildCoordinator {
  // Popping the view controller in case Local Authentication was triggered
  // outside reauthCoordinator before starting the child coordinator. Local
  // Authentication changes the scene state which triggers the presentation of
  // the ReauthenticationViewController by reauthCoordinator. Ideally
  // reauthCoordinator would be stopped when Local Authentication is triggered
  // outside of it but still defending against that scenario to avoid leaving an
  // unintended view controller in the navigation stack.
  [_reauthCoordinator stopAndPopViewController];
  _reauthCoordinator.delegate = nil;
  _reauthCoordinator = nil;
}

// Starts reauthCoordinator after a child coordinator content was dismissed.
- (void)restartReauthCoordinator {
  // Restart reauth coordinator so it monitors scene state changes and requests
  // local authentication after the scene goes to the background.
  if (password_manager::features::IsAuthOnEntryV2Enabled()) {
    [self startReauthCoordinatorWithAuthOnStart:NO];
  }
}

// Whether Local Authentication sould be required when the coordinator is
// started.
- (BOOL)shouldRequireAuthOnStart {
  if (!IsAuthOnEntryV2Enabled()) {
    return NO;
  }

  // Request auth when opened from outside Password Manager.
  switch (_referrer) {
    case PasswordCheckReferrer::kSafetyCheck:
    case PasswordCheckReferrer::kPhishGuardDialog:
    case PasswordCheckReferrer::kPasswordBreachDialog:
    case PasswordCheckReferrer::kMoreToFixBubble:
    case PasswordCheckReferrer::kSafetyCheckMagicStack:
      return YES;
    case PasswordCheckReferrer::kPasswordSettings:
      return NO;
  }
}

@end
