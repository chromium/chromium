// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/privacy/privacy_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#import "ios/chrome/browser/settings/ui_bundled/privacy/handoff_table_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/privacy/incognito/incognito_lock_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/privacy/incognito/incognito_lock_coordinator_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/privacy/lockdown_mode/lockdown_mode_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/privacy/privacy_guide/privacy_guide_main_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/privacy/privacy_guide/privacy_guide_main_coordinator_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/privacy/privacy_navigation_commands.h"
#import "ios/chrome/browser/settings/ui_bundled/privacy/privacy_safe_browsing_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/privacy/privacy_table_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_navigation_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ui/base/device_form_factor.h"

@interface PrivacyCoordinator () <
    IncognitoLockCoordinatorDelegate,
    PrivacyGuideMainCoordinatorDelegate,
    PrivacyNavigationCommands,
    PrivacySafeBrowsingCoordinatorDelegate,
    PrivacyTableViewControllerPresentationDelegate,
    LockdownModeCoordinatorDelegate> {
}

@property(nonatomic, strong) PrivacyTableViewController* viewController;
// Coordinator for Privacy Safe Browsing settings.
@property(nonatomic, strong)
    PrivacySafeBrowsingCoordinator* safeBrowsingCoordinator;

// Coordinator for Incognito lock settings.
@property(nonatomic, strong) IncognitoLockCoordinator* incognitoLockCoordinator;

// Coordinator for Lockdown Mode settings.
@property(nonatomic, strong) LockdownModeCoordinator* lockdownModeCoordinator;

// Coordinator for the Privacy Guide screen.
@property(nonatomic, strong)
    PrivacyGuideMainCoordinator* privacyGuideMainCoordinator;
@end

@implementation PrivacyCoordinator {
  // Verifies that `stop` is always called before dealloc.
  BOOL _stopped;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  ReauthenticationModule* module = [[ReauthenticationModule alloc] init];
  PrivacyTableViewController* viewController =
      [[PrivacyTableViewController alloc] initWithBrowser:self.browser
                                   reauthenticationModule:module];
  self.viewController = viewController;

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  viewController.applicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  viewController.browserHandler =
      HandlerForProtocol(dispatcher, BrowserCommands);
  viewController.settingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);
  viewController.snackbarHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);

  DCHECK(self.baseNavigationController);
  viewController.handler = self;
  viewController.presentationDelegate = self;

  [self.baseNavigationController pushViewController:viewController
                                           animated:YES];
}

- (void)stop {
  _stopped = YES;
  [self stopLockdownModeCoordinator];
  [self stopSafeBrowsingCoordinator];
  [self stopIncognitoLockCoordinator];

  [self.viewController disconnect];
  self.viewController = nil;
}

- (void)dealloc {
  // TODO(crbug.com/427791272): If stop is always called before dealloc, then
  // do all C++ cleanup in stop.
  CHECK(_stopped, base::NotFatalUntil::M150);
}

#pragma mark - PrivacyTableViewControllerPresentationDelegate

- (void)privacyTableViewControllerWasRemoved:
    (PrivacyTableViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate privacyCoordinatorViewControllerWasRemoved:self];
}

#pragma mark - PrivacyNavigationCommands

- (void)showHandoff {
  HandoffTableViewController* viewController =
      [[HandoffTableViewController alloc] initWithProfile:self.profile];
  [self.viewController configureHandlersForRootViewController:viewController];
  [self.baseNavigationController pushViewController:viewController
                                           animated:YES];
}

- (void)showClearBrowsingData {
  base::RecordAction(base::UserMetricsAction("PrivacyPage_DeleteBrowsingData"));
  base::UmaHistogramEnumeration(
      browsing_data::kDeleteBrowsingDataDialogHistogram,
      browsing_data::DeleteBrowsingDataDialogAction::
          kPrivacyEntryPointSelected);

  id<QuickDeleteCommands> quickDeleteHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), QuickDeleteCommands);
  [quickDeleteHandler
      showQuickDeleteAndCanPerformRadialWipeAnimation:
          ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET];
}

- (void)showSafeBrowsing {
  [self.safeBrowsingCoordinator stop];
  self.safeBrowsingCoordinator = [[PrivacySafeBrowsingCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  self.safeBrowsingCoordinator.delegate = self;
  [self.safeBrowsingCoordinator start];
}

- (void)showIncognitoLock {
  DCHECK(!self.incognitoLockCoordinator);
  self.incognitoLockCoordinator = [[IncognitoLockCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  self.incognitoLockCoordinator.delegate = self;
  [self.incognitoLockCoordinator start];
}

- (void)showLockdownMode {
  DCHECK(!self.lockdownModeCoordinator);
  self.lockdownModeCoordinator = [[LockdownModeCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  self.lockdownModeCoordinator.delegate = self;
  [self.lockdownModeCoordinator start];
}

- (void)showPrivacyGuide {
  DCHECK(!self.privacyGuideMainCoordinator);
  self.privacyGuideMainCoordinator = [[PrivacyGuideMainCoordinator alloc]
      initWithBaseViewController:self.baseNavigationController
                         browser:self.browser];
  self.privacyGuideMainCoordinator.delegate = self;
  [self.privacyGuideMainCoordinator start];
}
#pragma mark - SafeBrowsingCoordinatorDelegate

- (void)privacySafeBrowsingCoordinatorDidRemove:
    (PrivacySafeBrowsingCoordinator*)coordinator {
  DCHECK_EQ(self.safeBrowsingCoordinator, coordinator);
  [self stopSafeBrowsingCoordinator];
}

#pragma mark - IncognitoLockCoordinatorDelegate

- (void)incognitoLockCoordinatorDidRemove:
    (IncognitoLockCoordinator*)coordinator {
  DCHECK_EQ(self.incognitoLockCoordinator, coordinator);
  [self stopIncognitoLockCoordinator];
}

#pragma mark - LockdownModeCoordinatorDelegate

- (void)lockdownModeCoordinatorDidRemove:(LockdownModeCoordinator*)coordinator {
  DCHECK_EQ(self.lockdownModeCoordinator, coordinator);
  [self stopLockdownModeCoordinator];
}

#pragma mark - PrivacyGuideMainCoordinatorDelegate

- (void)privacyGuideMainCoordinatorDidRemove:
    (PrivacyGuideMainCoordinator*)coordinator {
  DCHECK_EQ(self.privacyGuideMainCoordinator, coordinator);
  [self stopPrivacyGuideMainCoordinator];
}

#pragma mark - Private

- (void)stopLockdownModeCoordinator {
  [self.lockdownModeCoordinator stop];
  self.lockdownModeCoordinator.delegate = nil;
  self.lockdownModeCoordinator = nil;
}

- (void)stopIncognitoLockCoordinator {
  [self.incognitoLockCoordinator stop];
  self.incognitoLockCoordinator.delegate = nil;
  self.incognitoLockCoordinator = nil;
}

- (void)stopSafeBrowsingCoordinator {
  [self.safeBrowsingCoordinator stop];
  self.safeBrowsingCoordinator.delegate = nil;
  self.safeBrowsingCoordinator = nil;
}

- (void)stopPrivacyGuideMainCoordinator {
  [self.privacyGuideMainCoordinator stop];
  self.privacyGuideMainCoordinator.delegate = nil;
  self.privacyGuideMainCoordinator = nil;
}

@end
