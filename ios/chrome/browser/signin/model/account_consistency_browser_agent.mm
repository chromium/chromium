// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_consistency_browser_agent.h"

#import <UIKit/UIKit.h>

#import "components/signin/core/browser/account_reconcilor.h"
#import "components/signin/ios/browser/account_consistency_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/account_consistency_service_factory.h"
#import "ios/chrome/browser/signin/model/account_reconcilor_factory.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web_state_list/model/web_state_dependency_installation_observer.h"

BROWSER_USER_DATA_KEY_IMPL(AccountConsistencyBrowserAgent)

AccountConsistencyBrowserAgent::AccountConsistencyBrowserAgent(
    Browser* browser,
    UIViewController* base_view_controller)
    : base_view_controller_(base_view_controller), browser_(browser) {
  installation_observer_ =
      std::make_unique<WebStateDependencyInstallationObserver>(
          browser->GetWebStateList(), this);
  browser_->AddObserver(this);
  application_handler_ =
      HandlerForProtocol(browser_->GetCommandDispatcher(), ApplicationCommands);
  settings_handler_ =
      HandlerForProtocol(browser_->GetCommandDispatcher(), SettingsCommands);
}

AccountConsistencyBrowserAgent::~AccountConsistencyBrowserAgent() {}

void AccountConsistencyBrowserAgent::InstallDependency(
    web::WebState* web_state) {
  if (AccountConsistencyService* accountConsistencyService =
          ios::AccountConsistencyServiceFactory::GetForProfile(
              browser_->GetProfile())) {
    accountConsistencyService->SetWebStateHandler(web_state, this);
  }
}

void AccountConsistencyBrowserAgent::UninstallDependency(
    web::WebState* web_state) {
  if (AccountConsistencyService* accountConsistencyService =
          ios::AccountConsistencyServiceFactory::GetForProfile(
              browser_->GetProfile())) {
    accountConsistencyService->RemoveWebStateHandler(web_state);
  }
}

void AccountConsistencyBrowserAgent::OnRestoreGaiaCookies() {
  signin_metrics::LogAccountReconcilorStateOnGaiaResponse(
      ios::AccountReconcilorFactory::GetForProfile(browser_->GetProfile())
          ->GetState());
  [application_handler_
      showSigninAccountNotificationFromViewController:base_view_controller_];
}

void AccountConsistencyBrowserAgent::OnManageAccounts() {
  signin_metrics::LogAccountReconcilorStateOnGaiaResponse(
      ios::AccountReconcilorFactory::GetForProfile(browser_->GetProfile())
          ->GetState());

  if (ShouldShowAccountMenu()) {
    ShowAccountMenu();
  } else {
    [settings_handler_
        showAccountsSettingsFromViewController:base_view_controller_
                          skipIfUINotAvailable:YES];
  }
}

void AccountConsistencyBrowserAgent::OnShowConsistencyPromo(
    const GURL& url,
    web::WebState* web_state) {
  signin_metrics::LogAccountReconcilorStateOnGaiaResponse(
      ios::AccountReconcilorFactory::GetForProfile(browser_->GetProfile())
          ->GetState());
  web::WebState* current_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  if (current_web_state == web_state) {
    [application_handler_
        showWebSigninPromoFromViewController:base_view_controller_
                                         URL:url];
  }
}

void AccountConsistencyBrowserAgent::OnAddAccount() {
  if ([base_view_controller_ presentedViewController]) {
    // If the base view controller is already presenting a view, the sign-in
    // should not appear on top of it.
    // See http://crbug.com/1399464.
    return;
  }

  if (ShouldShowAccountMenu()) {
    ShowAccountMenu();
  } else {
    ShowSigninCommand* command = [[ShowSigninCommand alloc]
        initWithOperation:AuthenticationOperation::kAddAccount
              accessPoint:signin_metrics::AccessPoint::
                              ACCESS_POINT_ACCOUNT_CONSISTENCY_SERVICE];
    command.skipIfUINotAvailable = YES;
    [application_handler_ showSignin:command
                  baseViewController:base_view_controller_];
  }
}

void AccountConsistencyBrowserAgent::OnGoIncognito(const GURL& url) {
  // The user taps on go incognito from the mobile U-turn webpage (the web
  // page that displays all users accounts available in the content area). As
  // the user chooses to go to incognito, the mobile U-turn page is no longer
  // neeeded. The current solution is to go back in history. This has the
  // advantage of keeping the current browsing session and give a good user
  // experience when the user comes back from incognito.
  WebNavigationBrowserAgent::FromBrowser(browser_)->GoBack();

  GURL url_to_open;
  if (url.is_valid()) {
    url_to_open = url;
  }
  OpenNewTabCommand* command = [[OpenNewTabCommand alloc]
       initWithURL:url_to_open
          referrer:web::Referrer()  // Strip referrer when switching modes.
       inIncognito:YES
      inBackground:NO
          appendTo:OpenPosition::kLastTab];
  [application_handler_ openURLInNewTab:command];
}

void AccountConsistencyBrowserAgent::BrowserDestroyed(Browser* browser) {
  installation_observer_.reset();
  browser_->RemoveObserver(this);
}

bool AccountConsistencyBrowserAgent::ShouldShowAccountMenu() const {
  if (!AreSeparateProfilesForManagedAccountsEnabled()) {
    return false;
  }
  size_t num_profiles = GetApplicationContext()
                            ->GetProfileManager()
                            ->GetProfileAttributesStorage()
                            ->GetNumberOfProfiles();
  // If there are any profiles beside the current one, it's likely the user
  // wanted to switch to another profile rather than add/manage accounts.
  return num_profiles > 1;
}

void AccountConsistencyBrowserAgent::ShowAccountMenu() {
  CHECK(AreSeparateProfilesForManagedAccountsEnabled());
  // TODO(crbug.com/375605412): Adjust the account menu shown here so that it
  // has "Manage accounts on this device" as a top-level button, and no overflow
  // menu.
  // TODO(crbug.com/375605412): If the user actually switches accounts/profiles
  // in this menu, show an IPH on the next NTP.
  [application_handler_ showAccountMenuWithAnchorView:nil
                                 skipIfUINotAvailable:YES
                                           completion:nil];
}
