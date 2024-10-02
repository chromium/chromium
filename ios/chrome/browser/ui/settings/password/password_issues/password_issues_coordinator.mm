// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/debug/dump_without_crashing.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/scoped_refptr.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/model/metrics/ios_password_manager_metrics.h"
#import "ios/chrome/browser/passwords/model/metrics/ios_password_manager_visits_recorder.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issue.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_presenter.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_coordinator.h"
#import "ios/chrome/browser/ui/settings/utils/password_utils.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ui/base/l10n/l10n_util.h"

using password_manager::WarningType;

namespace {

// Returns a DetailsContext based on the given WarningType.
DetailsContext ComputeDetailsContextFromWarningType(WarningType warning_type) {
  switch (warning_type) {
    case WarningType::kCompromisedPasswordsWarning:
      return DetailsContext::kCompromisedIssues;
    case WarningType::kReusedPasswordsWarning:
      return DetailsContext::kReusedIssues;
    case WarningType::kWeakPasswordsWarning:
      return DetailsContext::kWeakIssues;
    case WarningType::kDismissedWarningsWarning:
      return DetailsContext::kDismissedWarnings;
    case WarningType::kNoInsecurePasswordsWarning:
      return DetailsContext::kPasswordSettings;
  }
}

}  // namespace

@interface PasswordIssuesCoordinator () <PasswordDetailsCoordinatorDelegate,
                                         PasswordIssuesCoordinatorDelegate,
                                         PasswordIssuesPresenter,
                                         ReauthenticationCoordinatorDelegate>

@end

@implementation PasswordIssuesCoordinator {
  // Main view controller for this coordinator.
  PasswordIssuesTableViewController* _viewController;

  // Main mediator for this coordinator.
  PasswordIssuesMediator* _mediator;

  // Coordinator for password details.
  PasswordDetailsCoordinator* _passwordDetails;

  // Password check manager to power mediator.
  raw_ptr<IOSChromePasswordCheckManager> _manager;

  // Type of insecure credentials issues to display.
  password_manager::WarningType _warningType;

  // Coordinator for password issues displaying dismissed compromised
  // credentials.
  PasswordIssuesCoordinator* _dismissedPasswordIssuesCoordinator;

  // Flag indicating if the coordinator should dismiss its view controller after
  // the view controller of a child coordinator is removed from the stack. When
  // the issues and dismissed warnings are removed by the user, the coordinator
  // should dismiss its view controller and go back to the previous screen. If
  // there are child coordinators, this flag is used to dismiss the view
  // controller after the children are dismissed.
  BOOL _shouldDismissAfterChildCoordinatorRemoved;

  // Coordinator for blocking Password Issues until Local Authentication is
  // passed. Used for requiring authentication when opening Password Issues
  // from outside the Password Manager and when the app is
  // backgrounded/foregrounded with Password Issues opened.
  ReauthenticationCoordinator* _reauthCoordinator;

  // For recording visits after successful authentication.
  IOSPasswordManagerVisitsRecorder* _visitsRecorder;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initForWarningType:(password_manager::WarningType)warningType
          baseNavigationController:(UINavigationController*)navigationController
                           browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _warningType = warningType;
    _baseNavigationController = navigationController;
    _dispatcher = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                     ApplicationCommands);
    _skipAuthenticationOnStart = NO;
  }
  return self;
}

- (void)start {
  [super start];

  ProfileIOS* profile = self.browser->GetProfile();
  _mediator = [[PasswordIssuesMediator alloc]
        initForWarningType:_warningType
      passwordCheckManager:IOSChromePasswordCheckManagerFactory::GetForProfile(
                               profile)
                               .get()
             faviconLoader:IOSChromeFaviconLoaderFactory::GetForProfile(profile)
               syncService:SyncServiceFactory::GetForProfile(profile)];

  PasswordIssuesTableViewController* passwordIssuesTableViewController =
      [[PasswordIssuesTableViewController alloc]
          initWithWarningType:_warningType];
  passwordIssuesTableViewController.imageDataSource = _mediator;
  _viewController = passwordIssuesTableViewController;

  // If reauthentication module was not provided, coordinator will create its
  // own.
  if (!self.reauthModule) {
    self.reauthModule = password_manager::BuildReauthenticationModule(
        /*successfulReauthTimeAccessor=*/_mediator);
  }

  _mediator.consumer = _viewController;
  _viewController.presenter = self;

  _visitsRecorder = [[IOSPasswordManagerVisitsRecorder alloc]
      initWithPasswordManagerSurface:password_manager::PasswordManagerSurface::
                                         kPasswordIssues];

  // Only record visit if no auth is required, otherwise wait for successful
  // auth.
  if (_skipAuthenticationOnStart) {
    [_visitsRecorder maybeRecordVisitMetric];
  }

  // Disable animation when content will be blocked for reauth to prevent
  // flickering in navigation bar.
  [self.baseNavigationController pushViewController:_viewController
                                           animated:_skipAuthenticationOnStart];

  [self startReauthCoordinatorWithAuthOnStart:!_skipAuthenticationOnStart];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  _viewController = nil;

  [_passwordDetails stop];
  _passwordDetails.delegate = nil;
  _passwordDetails = nil;

  [self stopDismissedPasswordIssuesCoordinator];
  [self stopReauthenticationCoordinator];
}

#pragma mark - PasswordIssuesPresenter

- (void)dismissPasswordIssuesTableViewController {
  [self.delegate passwordIssuesCoordinatorDidRemove:self];
}

- (void)dismissAndOpenURL:(CrURL*)URL {
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:URL.gurl];
  [self.dispatcher closeSettingsUIAndOpenURL:command];
}

- (void)presentPasswordIssueDetails:(PasswordIssue*)password {
  DCHECK(!_passwordDetails);

  [self stopReauthCoordinatorBeforeStartingChildCoordinator];

  _passwordDetails = [[PasswordDetailsCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser
                            credential:password.credential
                          reauthModule:self.reauthModule
                               context:ComputeDetailsContextFromWarningType(
                                           _warningType)];
  _passwordDetails.delegate = self;
  [_passwordDetails start];
}

- (void)presentDismissedCompromisedCredentials {
  if (_dismissedPasswordIssuesCoordinator &&
      self.baseNavigationController.topViewController != _viewController) {
    base::debug::DumpWithoutCrashing();
  }

  [self stopReauthCoordinatorBeforeStartingChildCoordinator];

  _dismissedPasswordIssuesCoordinator = [[PasswordIssuesCoordinator alloc]
            initForWarningType:password_manager::WarningType::
                                   kDismissedWarningsWarning
      baseNavigationController:self.baseNavigationController
                       browser:self.browser];
  _dismissedPasswordIssuesCoordinator.skipAuthenticationOnStart = YES;
  _dismissedPasswordIssuesCoordinator.reauthModule = self.reauthModule;
  _dismissedPasswordIssuesCoordinator.delegate = self;
  [_dismissedPasswordIssuesCoordinator start];
}

- (void)dismissAfterAllIssuesGone {
  if (self.baseNavigationController.topViewController == _viewController) {
    [self.baseNavigationController popViewControllerAnimated:NO];
  } else {
    _shouldDismissAfterChildCoordinatorRemoved = YES;
  }
}

#pragma mark - PasswordDetailsCoordinatorDelegate

- (void)passwordDetailsCoordinatorDidRemove:
    (PasswordDetailsCoordinator*)coordinator {
  DCHECK_EQ(_passwordDetails, coordinator);
  [_passwordDetails stop];
  _passwordDetails.delegate = nil;
  _passwordDetails = nil;

  [self onChildCoordinatorDidRemove];
}

#pragma mark - PasswordIssuesCoordinatorDelegate

- (void)passwordIssuesCoordinatorDidRemove:
    (PasswordIssuesCoordinator*)coordinator {
  CHECK_EQ(_dismissedPasswordIssuesCoordinator, coordinator);
  [self stopDismissedPasswordIssuesCoordinator];

  [self onChildCoordinatorDidRemove];
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

- (void)stopDismissedPasswordIssuesCoordinator {
  [_dismissedPasswordIssuesCoordinator stop];
  _dismissedPasswordIssuesCoordinator.reauthModule = nil;
  _dismissedPasswordIssuesCoordinator.delegate = nil;
  _dismissedPasswordIssuesCoordinator = nil;
}

- (void)stopReauthenticationCoordinator {
  [_reauthCoordinator stop];
  _reauthCoordinator.delegate = nil;
  _reauthCoordinator = nil;
}

// Called after the view controller of a child coordinator of `self` was removed
// from the navigation stack.
- (void)onChildCoordinatorDidRemove {
  // If the content of the view controller was gone while a child coordinator
  // was presenting content, dismiss the view controller now that the child
  // coordinator's vc was removed.
  if (_shouldDismissAfterChildCoordinatorRemoved) {
    CHECK_EQ(self.baseNavigationController.topViewController, _viewController);
    _shouldDismissAfterChildCoordinatorRemoved = NO;
    dispatch_async(dispatch_get_main_queue(), ^{
      [self.baseNavigationController popViewControllerAnimated:NO];
    });
  } else {
    // Otherwise restart scene monitoring so authentication is required when the
    // scene is backgrounded/foregrounded.
    [self restartReauthCoordinator];
  }
}

// Starts reauthCoordinator.
// - authOnStart: Pass `YES` to cover Password Issues with an empty view
// controller until successful Local Authentication when reauthCoordinator
// starts.
//
// Local authentication is required every time the current
// scene is backgrounded and foregrounded until reauthCoordinator is stopped.
- (void)startReauthCoordinatorWithAuthOnStart:(BOOL)authOnStart {
  if (_reauthCoordinator &&
      self.baseNavigationController.topViewController != _viewController) {
    base::debug::DumpWithoutCrashing();
  }

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
  // See PasswordsCoordinator
  // stopReauthCoordinatorBeforeStartingChildCoordinator.
  [_reauthCoordinator stopAndPopViewController];
  _reauthCoordinator.delegate = nil;
  _reauthCoordinator = nil;
}

// Starts reauthCoordinator after a child coordinator content was dismissed.
- (void)restartReauthCoordinator {
  // Restart reauth coordinator so it monitors scene state changes and requests
  // local authentication after the scene goes to the background.
  [self startReauthCoordinatorWithAuthOnStart:NO];
}

@end
