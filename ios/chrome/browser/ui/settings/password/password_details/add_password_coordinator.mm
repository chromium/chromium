// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/add_password_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/memory/scoped_refptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_ui_features.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_coordinator.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

@interface AddPasswordCoordinator () <AddPasswordHandler,
                                      AddPasswordMediatorDelegate,
                                      ReauthenticationCoordinatorDelegate,
                                      UIAdaptivePresentationControllerDelegate>

// Main view controller for this coordinator.
@property(nonatomic, strong) AddPasswordViewController* viewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) AddPasswordMediator* mediator;

// Module containing the reauthentication mechanism for editing existing
// passwords.
@property(nonatomic, weak) id<ReauthenticationProtocol> reauthenticationModule;

// Modal alert for interactions with password.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

// Dispatcher.
@property(nonatomic, weak) id<ApplicationCommands, BrowserCommands> dispatcher;

// Used for requiring authentication after the browser comes from the background
// with Add Password open.
@property(nonatomic, strong) ReauthenticationCoordinator* reauthCoordinator;

@end

@implementation AddPasswordCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              reauthModule:
                                  (id<ReauthenticationProtocol>)reauthModule {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    DCHECK(viewController);
    DCHECK(reauthModule);
    _reauthenticationModule = reauthModule;
    _dispatcher = static_cast<id<BrowserCommands, ApplicationCommands>>(
        browser->GetCommandDispatcher());
  }
  return self;
}

- (void)start {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  self.viewController = [[AddPasswordViewController alloc] init];
  self.viewController.presentationController.delegate = self;

  self.mediator = [[AddPasswordMediator alloc]
          initWithDelegate:self
      passwordCheckManager:IOSChromePasswordCheckManagerFactory::
                               GetForBrowserState(browserState)
                                   .get()
               prefService:browserState->GetPrefs()
               syncService:SyncServiceFactory::GetForBrowserState(
                               browserState)];
  self.mediator.consumer = self.viewController;
  self.viewController.delegate = self.mediator;
  self.viewController.addPasswordHandler = self;
  self.viewController.reauthModule = self.reauthenticationModule;

  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:self.viewController];
  _baseNavigationController = navigationController;
  navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  navigationController.presentationController.delegate = self;

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];

  if (password_manager::features::IsAuthOnEntryV2Enabled()) {
    [self startReauthCoordinator];
  }
}

- (void)stop {
  [self.viewController.navigationController dismissViewControllerAnimated:YES
                                                               completion:nil];
  [self dismissAlertCoordinator];
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

#pragma mark - AddPasswordHandler

- (void)showPasscodeDialog {
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_TITLE);
  NSString* message =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_CONTENT);
  self.alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:self.viewController
                                                   browser:self.browser
                                                     title:title
                                                   message:message];

  __weak __typeof(self) weakSelf = self;
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:GURL(kPasscodeArticleURL)];

  [self.alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_OK)
                                   action:^{
                                     [weakSelf dismissAlertCoordinator];
                                   }
                                    style:UIAlertActionStyleCancel];

  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_LEARN_HOW)
                action:^{
                  [weakSelf.dispatcher closeSettingsUIAndOpenURL:command];
                  [weakSelf dismissAlertCoordinator];
                }
                 style:UIAlertActionStyleDefault];

  [self.alertCoordinator start];
}

#pragma mark - ReauthenticationCoordinatorDelegate

- (void)successfulReauthenticationWithCoordinator:
    (ReauthenticationCoordinator*)coordinator {
  // No-op.
}

- (void)willPushReauthenticationViewController {
  [self dismissAlertCoordinator];
}

#pragma mark - Private

- (void)dismissAlertCoordinator {
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;
}

// Starts reauthCoordinator.
// Local authentication is required every time the current
// scene is backgrounded and foregrounded until reauthCoordinator is stopped.
- (void)startReauthCoordinator {
  _reauthCoordinator = [[ReauthenticationCoordinator alloc]
      initWithBaseNavigationController:_baseNavigationController
                               browser:self.browser
                reauthenticationModule:_reauthenticationModule
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
