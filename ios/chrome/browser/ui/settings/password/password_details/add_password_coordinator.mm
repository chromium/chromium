// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/add_password_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/memory/scoped_refptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/model/metrics/ios_password_manager_metrics.h"
#import "ios/chrome/browser/passwords/model/metrics/ios_password_manager_visits_recorder.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_coordinator.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

@interface AddPasswordCoordinator () <AddPasswordMediatorDelegate,
                                      ReauthenticationCoordinatorDelegate,
                                      UIAdaptivePresentationControllerDelegate>

// Main view controller for this coordinator.
@property(nonatomic, strong) AddPasswordViewController* viewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) AddPasswordMediator* mediator;

// Dispatcher.
@property(nonatomic, weak) id<ApplicationCommands, BrowserCommands> dispatcher;

// Used for requiring authentication after the browser comes from the background
// with Add Password open.
@property(nonatomic, strong) ReauthenticationCoordinator* reauthCoordinator;

@end

@implementation AddPasswordCoordinator {
  // For recording visits to the page.
  IOSPasswordManagerVisitsRecorder* _visitsRecorder;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    DCHECK(viewController);
    _dispatcher = static_cast<id<BrowserCommands, ApplicationCommands>>(
        browser->GetCommandDispatcher());
  }
  return self;
}

- (void)start {
  ProfileIOS* profile = self.browser->GetProfile();
  self.viewController = [[AddPasswordViewController alloc] init];
  self.viewController.presentationController.delegate = self;

  self.mediator = [[AddPasswordMediator alloc]
          initWithDelegate:self
      passwordCheckManager:IOSChromePasswordCheckManagerFactory::GetForProfile(
                               profile)
                               .get()
               prefService:profile->GetPrefs()
               syncService:SyncServiceFactory::GetForProfile(profile)];
  self.mediator.consumer = self.viewController;
  self.viewController.delegate = self.mediator;

  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:self.viewController];
  _baseNavigationController = navigationController;
  navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  navigationController.presentationController.delegate = self;

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];

  _visitsRecorder = [[IOSPasswordManagerVisitsRecorder alloc]
      initWithPasswordManagerSurface:password_manager::PasswordManagerSurface::
                                         kAddPassword];
  [_visitsRecorder maybeRecordVisitMetric];

  [self startReauthCoordinator];
}

- (void)stop {
  [self stopWithUIDismissal:YES];
}

#pragma mark - AddPasswordCoordinator

- (void)stopWithUIDismissal:(BOOL)shouldDismissUI {
  // When the coordinator is stopped due to failed authentication, the whole
  // Password Manager UI is dismissed via command. Not dismissing the top
  // coordinator UI before everything else prevents the Password Manager UI
  // from being visible without local authentication.
  if (shouldDismissUI) {
    UINavigationController* navigationController =
        _viewController.navigationController;
    [navigationController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
  }

  self.mediator = nil;
  self.viewController = nil;

  [self stopReauthCoordinator];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate passwordDetailsTableViewControllerDidFinish:self];
}

#pragma mark - AddPasswordMediatorDelegate

- (void)dismissAddPasswordTableViewController {
  [self.delegate passwordDetailsTableViewControllerDidFinish:self];
}

- (void)setUpdatedPassword:
    (const password_manager::CredentialUIEntry&)credential {
  [self.delegate setMostRecentlyUpdatedPasswordDetails:credential];
}

- (void)showPasswordDetailsControllerWithCredential:
    (const password_manager::CredentialUIEntry&)credential {
  [self.delegate dismissAddViewControllerAndShowPasswordDetails:credential
                                                    coordinator:self];
}

#pragma mark - ReauthenticationCoordinatorDelegate

- (void)successfulReauthenticationWithCoordinator:
    (ReauthenticationCoordinator*)coordinator {
  // No-op.
}

- (void)dismissUIAfterFailedReauthenticationWithCoordinator:
    (ReauthenticationCoordinator*)coordinator {
  CHECK_EQ(_reauthCoordinator, coordinator);
  [_delegate dismissPasswordManagerAfterFailedReauthentication];
}

- (void)willPushReauthenticationViewController {
  // No-op as the surface does not present other surfaces.
}

#pragma mark - Private

// Starts reauthCoordinator.
// Local authentication is required every time the current
// scene is backgrounded and foregrounded until reauthCoordinator is stopped.
- (void)startReauthCoordinator {
  _reauthCoordinator = [[ReauthenticationCoordinator alloc]
      initWithBaseNavigationController:_baseNavigationController
                               browser:self.browser
                reauthenticationModule:nil
                           authOnStart:NO];

  _reauthCoordinator.delegate = self;

  [_reauthCoordinator start];
}

- (void)stopReauthCoordinator {
  [_reauthCoordinator stop];
  _reauthCoordinator.delegate = nil;
  _reauthCoordinator = nil;
}

@end
