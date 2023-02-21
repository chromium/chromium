// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/safety_check/safety_check_coordinator.h"

#import "base/mac/foundation_util.h"
#import "base/memory/scoped_refptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/safe_browsing/core/common/features.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_issues_coordinator.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_safe_browsing_coordinator.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_constants.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_mediator.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_navigation_commands.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_ui_swift.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "net/base/mac/url_conversions.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SafetyCheckCoordinator () <
    PasswordIssuesCoordinatorDelegate,
    PopoverLabelViewControllerDelegate,
    PrivacySafeBrowsingCoordinatorDelegate,
    SafetyCheckNavigationCommands,
    SafetyCheckTableViewControllerPresentationDelegate>

// Safety check mediator.
@property(nonatomic, strong) SafetyCheckMediator* mediator;

// The container view controller.
@property(nonatomic, strong) SafetyCheckTableViewController* viewController;

// Coordinator for passwords issues screen.
@property(nonatomic, strong)
    PasswordIssuesCoordinator* passwordIssuesCoordinator;

// Dispatcher which can handle changing passwords on sites.
@property(nonatomic, strong) id<ApplicationCommands> handler;

// Coordinator for the Privacy and Security screen (SafeBrowsing toggle
// location).
@property(nonatomic, strong)
    PrivacySafeBrowsingCoordinator* privacySafeBrowsingCoordinator;

// Popover view controller with error information.
@property(nonatomic, strong)
    PopoverLabelViewController* errorInfoPopoverViewController;

@end

@implementation SafetyCheckCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _handler = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                  ApplicationCommands);
  }
  return self;
}

- (void)startCheckIfNotRunning {
  [self.mediator startCheckIfNotRunning];
}

#pragma mark - ChromeCoordinator

- (void)start {
  SafetyCheckTableViewController* viewController =
      [[SafetyCheckTableViewController alloc]
          initWithStyle:ChromeTableViewStyle()];
  self.viewController = viewController;

  scoped_refptr<IOSChromePasswordCheckManager> passwordCheckManager =
      IOSChromePasswordCheckManagerFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  self.mediator = [[SafetyCheckMediator alloc]
      initWithUserPrefService:self.browser->GetBrowserState()->GetPrefs()
         passwordCheckManager:passwordCheckManager
                  authService:AuthenticationServiceFactory::GetForBrowserState(
                                  self.browser->GetBrowserState())
                  syncService:SyncSetupServiceFactory::GetForBrowserState(
                                  self.browser->GetBrowserState())];

  self.mediator.consumer = self.viewController;
  self.mediator.handler = self;
  self.viewController.serviceDelegate = self.mediator;
  self.viewController.presentationDelegate = self;

  DCHECK(self.baseNavigationController);
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

- (void)stop {
  // If the Safe Browsing Settings page was accessed through the Safe
  // Browsing row of the safety check, we need to explicity stop the
  // privacySafeBrowsingCoordinator before closing the settings window.
  [self.privacySafeBrowsingCoordinator stop];
  self.privacySafeBrowsingCoordinator.delegate = nil;
  self.privacySafeBrowsingCoordinator = nil;
}

#pragma mark - SafetyCheckTableViewControllerPresentationDelegate

- (void)safetyCheckTableViewControllerDidRemove:
    (SafetyCheckTableViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate safetyCheckCoordinatorDidRemove:self];
}

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  GURL convertedURL = net::GURLWithNSURL(URL);
  const GURL safeBrowsingURL(
      base::SysNSStringToUTF8(kSafeBrowsingSafetyCheckStringURL));

  // Take the user to Sync and Google Services page in Bling instead of desktop
  // settings.
  if (convertedURL == safeBrowsingURL) {
    [self.errorInfoPopoverViewController
        dismissViewControllerAnimated:YES
                           completion:^{
                             [self showSafeBrowsingPreferencePage];
                           }];
    return;
  }

  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:convertedURL];
  [self.handler closeSettingsUIAndOpenURL:command];
}

#pragma mark - SafetyCheckNavigationCommands

- (void)showPasswordIssuesPage {
  IOSChromePasswordCheckManager* passwordCheckManager =
      IOSChromePasswordCheckManagerFactory::GetForBrowserState(
          self.browser->GetBrowserState())
          .get();
  self.passwordIssuesCoordinator = [[PasswordIssuesCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser
                  passwordCheckManager:passwordCheckManager];
  self.passwordIssuesCoordinator.delegate = self;
  self.passwordIssuesCoordinator.reauthModule = nil;
  [self.passwordIssuesCoordinator start];
}

- (void)showErrorInfoFrom:(UIButton*)buttonView
                 withText:(NSAttributedString*)text {
  self.errorInfoPopoverViewController =
      [[PopoverLabelViewController alloc] initWithPrimaryAttributedString:text
                                                secondaryAttributedString:nil];

  self.errorInfoPopoverViewController.delegate = self;

  self.errorInfoPopoverViewController.popoverPresentationController.sourceView =
      buttonView;
  self.errorInfoPopoverViewController.popoverPresentationController.sourceRect =
      buttonView.bounds;
  self.errorInfoPopoverViewController.popoverPresentationController
      .permittedArrowDirections = UIPopoverArrowDirectionAny;
  [self.viewController presentViewController:self.errorInfoPopoverViewController
                                    animated:YES
                                  completion:nil];
}

- (void)showUpdateAtLocation:(NSString*)location {
  if (!location) {
    NOTREACHED();
    return;
  }
  const GURL url(base::SysNSStringToUTF8(location));
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:url];
  [self.handler closeSettingsUIAndOpenURL:command];
}

- (void)showSafeBrowsingPreferencePage {
  DCHECK(!self.privacySafeBrowsingCoordinator);
  base::RecordAction(
      base::UserMetricsAction("Settings.SafetyCheck.ManageSafeBrowsing"));
  base::UmaHistogramEnumeration("Settings.SafetyCheck.Interactions",
                                SafetyCheckInteractions::kSafeBrowsingManage);
  self.privacySafeBrowsingCoordinator = [[PrivacySafeBrowsingCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  self.privacySafeBrowsingCoordinator.delegate = self;
  [self.privacySafeBrowsingCoordinator start];
}

- (void)showManagedInfoFrom:(UIButton*)buttonView {
  EnterpriseInfoPopoverViewController* bubbleViewController =
      [[EnterpriseInfoPopoverViewController alloc] initWithEnterpriseName:nil];
  [self.viewController presentViewController:bubbleViewController
                                    animated:YES
                                  completion:nil];

  // Disable the button when showing the bubble.
  // The button will be enabled when close the bubble in
  // (void)popoverPresentationControllerDidDismissPopover: of
  // EnterpriseInfoPopoverViewController.
  buttonView.enabled = NO;

  // Set the anchor and arrow direction of the bubble.
  bubbleViewController.popoverPresentationController.sourceView = buttonView;
  bubbleViewController.popoverPresentationController.sourceRect =
      buttonView.bounds;
  bubbleViewController.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;
}

#pragma mark - PasswordIssuesCoordinatorDelegate

- (void)passwordIssuesCoordinatorDidRemove:
    (PasswordIssuesCoordinator*)coordinator {
  DCHECK_EQ(self.passwordIssuesCoordinator, coordinator);
  [self.passwordIssuesCoordinator stop];
  self.passwordIssuesCoordinator.delegate = nil;
  self.passwordIssuesCoordinator = nil;
}

#pragma mark - PrivacySafeBrowsingCoordinatorDelegate

- (void)privacySafeBrowsingCoordinatorDidRemove:
    (PrivacySafeBrowsingCoordinator*)coordinator {
  DCHECK_EQ(_privacySafeBrowsingCoordinator, coordinator);
  [self.privacySafeBrowsingCoordinator stop];
  self.privacySafeBrowsingCoordinator.delegate = nil;
  self.privacySafeBrowsingCoordinator = nil;
}

@end
