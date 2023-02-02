// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_safe_browsing_coordinator.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/safe_browsing/core/common/safe_browsing_settings_metrics.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_safe_browsing_mediator.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_safe_browsing_navigation_commands.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_safe_browsing_view_controller.h"
#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_enhanced_protection_coordinator.h"
#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_standard_protection_coordinator.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Currently takes into account a view controller delegate and not a command
// handler to communicate with the mediator since there's currently no needed
// functionality that requires this.
@interface PrivacySafeBrowsingCoordinator () <
    PrivacySafeBrowsingNavigationCommands,
    PrivacySafeBrowsingViewControllerPresentationDelegate,
    SafeBrowsingEnhancedProtectionCoordinatorDelegate,
    SafeBrowsingStandardProtectionCoordinatorDelegate>

// View controller presented by this coordinator.
@property(nonatomic, strong) PrivacySafeBrowsingViewController* viewController;
// Safe Browsing settings mediator.
@property(nonatomic, strong) PrivacySafeBrowsingMediator* mediator;
// Coordinator for No Protection Safe Browsing Pop Up.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;
// Coordinator for Privacy Safe Browsing Enhanced Protection settings.
@property(nonatomic, strong) SafeBrowsingEnhancedProtectionCoordinator*
    safeBrowsingEnhancedProtectionCoordinator;
// Coordinator for Privacy Safe Browsing Standard Protection settings.
@property(nonatomic, strong) SafeBrowsingStandardProtectionCoordinator*
    safeBrowsingStandardProtectionCoordinator;

@end

@implementation PrivacySafeBrowsingCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  if ([super initWithBaseViewController:navigationController browser:browser]) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  self.viewController = [[PrivacySafeBrowsingViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  self.viewController.styler.cellHighlightColor =
      [UIColor colorNamed:kTextfieldHighlightBackgroundColor];
  self.viewController.presentationDelegate = self;
  self.mediator = [[PrivacySafeBrowsingMediator alloc]
      initWithUserPrefService:self.browser->GetBrowserState()->GetPrefs()];
  self.mediator.consumer = self.viewController;
  self.mediator.handler = self;
  self.viewController.modelDelegate = self.mediator;
  DCHECK(self.baseNavigationController);
  safe_browsing::LogShowEnhancedProtectionAction();
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

#pragma mark - SafeBrowsingViewControllerPresentationDelegate

- (void)privacySafeBrowsingViewControllerDidRemove:
    (PrivacySafeBrowsingViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate privacySafeBrowsingCoordinatorDidRemove:self];
}

#pragma mark - PrivacySafeBrowsingNavigationCommands

- (void)showSafeBrowsingEnhancedProtection {
  DCHECK(!self.safeBrowsingEnhancedProtectionCoordinator);
  self.safeBrowsingEnhancedProtectionCoordinator =
      [[SafeBrowsingEnhancedProtectionCoordinator alloc]
          initWithBaseNavigationController:self.baseNavigationController
                                   browser:self.browser];
  self.safeBrowsingEnhancedProtectionCoordinator.delegate = self;
  [self.safeBrowsingEnhancedProtectionCoordinator start];
}

- (void)showSafeBrowsingStandardProtection {
  DCHECK(!self.safeBrowsingStandardProtectionCoordinator);
  self.safeBrowsingStandardProtectionCoordinator =
      [[SafeBrowsingStandardProtectionCoordinator alloc]
          initWithBaseNavigationController:self.baseNavigationController
                                   browser:self.browser];
  self.safeBrowsingStandardProtectionCoordinator.delegate = self;
  [self.safeBrowsingStandardProtectionCoordinator start];
}

- (void)showSafeBrowsingNoProtectionPopUp:(TableViewItem*)item {
  DCHECK(!self.alertCoordinator);
  self.alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                           title:
                               l10n_util::GetNSString(
                                   IDS_IOS_SAFE_BROWSING_NO_PROTECTION_CONFIRMATION_DIALOG_TITLE)
                         message:
                             l10n_util::GetNSString(
                                 IDS_IOS_SAFE_BROWSING_NO_PROTECTION_CONFIRMATION_DIALOG_MESSAGE)];

  __weak __typeof__(self) weakSelf = self;
  NSString* actionTitle = l10n_util::GetNSString(
      IDS_IOS_SAFE_BROWSING_NO_PROTECTION_CONFIRMATION_DIALOG_CONFIRM);
  [self.alertCoordinator addItemWithTitle:actionTitle
                                   action:^{
                                     base::RecordAction(base::UserMetricsAction(
                                         "SafeBrowsing.Settings."
                                         "DisableSafeBrowsingDialogConfirmed"));
                                     [weakSelf.mediator selectSettingItem:item];
                                     [weakSelf.alertCoordinator stop];
                                     weakSelf.alertCoordinator = nil;
                                   }
                                    style:UIAlertActionStyleDefault];

  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                action:^{
                  base::RecordAction(base::UserMetricsAction(
                      "SafeBrowsing.Settings.DisableSafeBrowsingDialogDenied"));
                  [weakSelf.mediator selectSettingItem:nil];
                  [weakSelf.alertCoordinator stop];
                  weakSelf.alertCoordinator = nil;
                }
                 style:UIAlertActionStyleCancel];

  [self.alertCoordinator start];
}

#pragma mark - SafeBrowsingEnhancedProtectionCoordinatorDelegate

- (void)safeBrowsingEnhancedProtectionCoordinatorDidRemove:
    (SafeBrowsingEnhancedProtectionCoordinator*)coordinator {
  DCHECK_EQ(self.safeBrowsingEnhancedProtectionCoordinator, coordinator);
  [self.safeBrowsingEnhancedProtectionCoordinator stop];
  self.safeBrowsingEnhancedProtectionCoordinator.delegate = nil;
  self.safeBrowsingEnhancedProtectionCoordinator = nil;
}

#pragma mark - SafeBrowsingStandardProtectionCoordinatorDelegate

- (void)safeBrowsingStandardProtectionCoordinatorDidRemove:
    (SafeBrowsingStandardProtectionCoordinator*)coordinator {
  DCHECK_EQ(self.safeBrowsingStandardProtectionCoordinator, coordinator);
  [self.safeBrowsingStandardProtectionCoordinator stop];
  self.safeBrowsingStandardProtectionCoordinator.delegate = nil;
  self.safeBrowsingStandardProtectionCoordinator = nil;
}

@end
