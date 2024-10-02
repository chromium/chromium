// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/browsing_data/core/browsing_data_utils.h"
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
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_coordinator.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/features.h"
#import "ios/chrome/browser/ui/settings/privacy/handoff_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/privacy/lockdown_mode/lockdown_mode_coordinator.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_main_coordinator.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_main_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_navigation_commands.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_safe_browsing_coordinator.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ui/base/device_form_factor.h"

@interface PrivacyCoordinator () <
    ClearBrowsingDataCoordinatorDelegate,
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

// TODO(crbug.com/335387869): Delete this coordinator when Quick Delete is fully
// launched. The coordinator for the clear browsing data screen.
@property(nonatomic, strong)
    ClearBrowsingDataCoordinator* clearBrowsingDataCoordinator;

// Coordinator for Lockdown Mode settings.
@property(nonatomic, strong) LockdownModeCoordinator* lockdownModeCoordinator;

// Coordinator for the Privacy Guide screen.
@property(nonatomic, strong)
    PrivacyGuideMainCoordinator* privacyGuideMainCoordinator;

@end

@implementation PrivacyCoordinator

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
  [self.clearBrowsingDataCoordinator stop];
  self.clearBrowsingDataCoordinator = nil;
  [self stopLockdownModeCoordinator];
  [self stopSafeBrowsingCoordinator];

  self.viewController = nil;
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
      [[HandoffTableViewController alloc]
          initWithProfile:self.browser->GetProfile()];
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

  if (IsIosQuickDeleteEnabled()) {
    id<QuickDeleteCommands> quickDeleteHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), QuickDeleteCommands);
    [quickDeleteHandler
        showQuickDeleteAndCanPerformTabsClosureAnimation:
            ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET];
  } else {
    self.clearBrowsingDataCoordinator = [[ClearBrowsingDataCoordinator alloc]
        initWithBaseNavigationController:self.baseNavigationController
                                 browser:self.browser];
    self.clearBrowsingDataCoordinator.delegate = self;
    [self.clearBrowsingDataCoordinator start];
  }
}

- (void)showSafeBrowsing {
  DCHECK(!self.safeBrowsingCoordinator);
  self.safeBrowsingCoordinator = [[PrivacySafeBrowsingCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  self.safeBrowsingCoordinator.delegate = self;
  [self.safeBrowsingCoordinator start];
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

#pragma mark - ClearBrowsingDataCoordinatorDelegate

- (void)clearBrowsingDataCoordinatorViewControllerWasRemoved:
    (ClearBrowsingDataCoordinator*)coordinator {
  DCHECK_EQ(self.clearBrowsingDataCoordinator, coordinator);
  [self.clearBrowsingDataCoordinator stop];
  self.clearBrowsingDataCoordinator = nil;
}

#pragma mark - SafeBrowsingCoordinatorDelegate

- (void)privacySafeBrowsingCoordinatorDidRemove:
    (PrivacySafeBrowsingCoordinator*)coordinator {
  DCHECK_EQ(self.safeBrowsingCoordinator, coordinator);
  [self stopSafeBrowsingCoordinator];
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
