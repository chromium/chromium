// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_consistency_browser_agent.h"

#import <UIKit/UIKit.h>

#import "base/functional/callback_helpers.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/core/browser/account_reconcilor.h"
#import "components/signin/ios/browser/account_consistency_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/model/account_consistency_service_factory.h"
#import "ios/chrome/browser/signin/model/account_reconcilor_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/web/public/navigation/referrer.h"

AccountConsistencyBrowserAgent::AccountConsistencyBrowserAgent(
    Browser* browser,
    UIViewController* base_view_controller)
    : BrowserUserData(browser), base_view_controller_(base_view_controller) {
  StartObserving(browser, Policy::kOnlyRealized);
  application_handler_ =
      HandlerForProtocol(browser_->GetCommandDispatcher(), ApplicationCommands);
  settings_handler_ =
      HandlerForProtocol(browser_->GetCommandDispatcher(), SettingsCommands);
}

AccountConsistencyBrowserAgent::~AccountConsistencyBrowserAgent() {
  StopSigninCoordinator(SigninCoordinatorResultInterrupted, nil);
  StopObserving();
}

void AccountConsistencyBrowserAgent::StopSigninCoordinator(
    SigninCoordinatorResult result,
    id<SystemIdentity> identity) {
  [add_account_coordinator_ stop];
  add_account_coordinator_ = nil;
}

void AccountConsistencyBrowserAgent::OnWebStateInserted(
    web::WebState* web_state) {
  if (AccountConsistencyService* accountConsistencyService =
          ios::AccountConsistencyServiceFactory::GetForProfile(
              browser_->GetProfile())) {
    accountConsistencyService->SetWebStateHandler(web_state, this);
  }
}

void AccountConsistencyBrowserAgent::OnWebStateRemoved(
    web::WebState* web_state) {
  if (AccountConsistencyService* accountConsistencyService =
          ios::AccountConsistencyServiceFactory::GetForProfile(
              browser_->GetProfile())) {
    accountConsistencyService->RemoveWebStateHandler(web_state);
  }
}

void AccountConsistencyBrowserAgent::OnWebStateDeleted(
    web::WebState* web_state) {
  // Nothing to do.
}

void AccountConsistencyBrowserAgent::OnActiveWebStateChanged(
    web::WebState* old_active,
    web::WebState* new_active) {
  // Nothing to do.
}

void AccountConsistencyBrowserAgent::OnRestoreGaiaCookies() {
  signin_metrics::LogAccountReconcilorStateOnGaiaResponse(
      ios::AccountReconcilorFactory::GetForProfile(browser_->GetProfile())
          ->GetState());
  [application_handler_
      showSigninAccountNotificationFromViewController:base_view_controller_];
}

void AccountConsistencyBrowserAgent::OnManageAccounts(const GURL& url) {
  Browser::Type browser_type = browser_->type();
  base::UmaHistogramEnumeration("Signin.ShowManageAccountFromGaia.BrowserType",
                                browser_type);
  if (browser_type != Browser::Type::kRegular) {
    return;
  }
  signin_metrics::LogAccountReconcilorStateOnGaiaResponse(
      ios::AccountReconcilorFactory::GetForProfile(browser_->GetProfile())
          ->GetState());

  if (ShouldShowAccountMenu()) {
    ShowAccountMenu(url);
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

void AccountConsistencyBrowserAgent::OnAddAccount(
    const GURL& url,
    const std::string& prefilled_email) {
  if ([base_view_controller_ presentedViewController]) {
    // If the base view controller is already presenting a view, the sign-in
    // should not appear on top of it.
    // See http://crbug.com/1399464.
    return;
  }

  if (ShouldShowAccountMenu()) {
    ShowAccountMenu(url);
  } else {
    id<BrowserCoordinatorCommands> browser_coordinator_handler =
        HandlerForProtocol(browser_->GetCommandDispatcher(),
                           BrowserCoordinatorCommands);
    signin_metrics::AccessPoint access_point =
        signin_metrics::AccessPoint::kAccountConsistencyService;
    [browser_coordinator_handler
        showAddAccountWithAccessPoint:access_point
                       prefilledEmail:base::SysUTF8ToNSString(prefilled_email)];
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

bool AccountConsistencyBrowserAgent::ShouldShowAccountMenu() const {
  if (!AreSeparateProfilesForManagedAccountsEnabled()) {
    return false;
  }
  ProfileIOS* profile = browser_->GetProfile()->GetOriginalProfile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    // The account menu requires a primary identity.
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

void AccountConsistencyBrowserAgent::ShowAccountMenu(const GURL& url) {
  CHECK(AreSeparateProfilesForManagedAccountsEnabled());
  [application_handler_ showAccountMenuFromWebWithURL:url];
}
