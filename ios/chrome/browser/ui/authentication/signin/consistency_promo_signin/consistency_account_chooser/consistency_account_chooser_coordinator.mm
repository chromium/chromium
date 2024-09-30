// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_coordinator.h"

#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_mediator.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_table_view_controller_action_delegate.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_view_controller.h"

@interface ConsistencyAccountChooserCoordinator () <
    ConsistencyAccountChooserTableViewControllerActionDelegate>

@property(nonatomic, strong)
    ConsistencyAccountChooserViewController* accountChooserViewController;

@property(nonatomic, strong) ConsistencyAccountChooserMediator* mediator;

@end

@implementation ConsistencyAccountChooserCoordinator

- (void)startWithSelectedIdentity:(id<SystemIdentity>)selectedIdentity {
  [super start];
  base::RecordAction(
      base::UserMetricsAction("Signin_BottomSheet_IdentityChooser_Opened"));
  self.mediator = [[ConsistencyAccountChooserMediator alloc]
      initWithSelectedIdentity:selectedIdentity
         accountManagerService:ChromeAccountManagerServiceFactory::
                                   GetForProfile(self.browser->GetProfile())];

  self.accountChooserViewController =
      [[ConsistencyAccountChooserViewController alloc] init];
  self.accountChooserViewController.modelDelegate = self.mediator;
  self.mediator.consumer = self.accountChooserViewController.consumer;
  self.accountChooserViewController.actionDelegate = self;
  self.accountChooserViewController.layoutDelegate = self.layoutDelegate;
  [self.accountChooserViewController view];
}

- (void)stop {
  [super stop];
  [self.mediator disconnect];
  self.mediator = nil;
  self.accountChooserViewController = nil;
  base::RecordAction(
      base::UserMetricsAction("Signin_BottomSheet_IdentityChooser_Closed"));
}

#pragma mark - Properties

- (UIViewController*)viewController {
  return self.accountChooserViewController;
}

- (id<SystemIdentity>)selectedIdentity {
  return self.mediator.selectedIdentity;
}

#pragma mark - ConsistencyAccountChooserTableViewControllerActionDelegate

- (void)consistencyAccountChooserTableViewController:
            (ConsistencyAccountChooserTableViewController*)viewController
                         didSelectIdentityWithGaiaID:(NSString*)gaiaID {
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(
          self.browser->GetProfile());

  id<SystemIdentity> identity = accountManagerService->GetIdentityWithGaiaID(
      base::SysNSStringToUTF8(gaiaID));
  DCHECK(identity);
  self.mediator.selectedIdentity = identity;
  [self.delegate consistencyAccountChooserCoordinatorIdentitySelected:self];
}

- (void)consistencyAccountChooserTableViewControllerDidTapOnAddAccount:
    (ConsistencyAccountChooserTableViewController*)viewController {
  [self.delegate consistencyAccountChooserCoordinatorOpenAddAccount:self];
}

- (void)showManagementHelpPage {
  OpenNewTabCommand* command = [OpenNewTabCommand
      commandWithURLFromChrome:GURL(kManagementLearnMoreURL)];
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [handler closeSettingsUIAndOpenURL:command];
}

@end
