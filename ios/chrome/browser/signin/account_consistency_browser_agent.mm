// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/account_consistency_browser_agent.h"

#import <UIKit/UIKit.h>

#import "components/signin/core/browser/account_reconcilor.h"
#import "components/signin/ios/browser/account_consistency_service.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/account_consistency_service_factory.h"
#import "ios/chrome/browser/signin/account_reconcilor_factory.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_dependency_installation_observer.h"

BROWSER_USER_DATA_KEY_IMPL(AccountConsistencyBrowserAgent)

AccountConsistencyBrowserAgent::AccountConsistencyBrowserAgent(
    Browser* browser,
    UIViewController* base_view_controller,
    id<ApplicationCommands> handler)
    : base_view_controller_(base_view_controller),
      handler_(handler),
      browser_(browser) {
  installation_observer_ =
      std::make_unique<WebStateDependencyInstallationObserver>(
          browser->GetWebStateList(), this);
  browser_->AddObserver(this);
}

AccountConsistencyBrowserAgent::~AccountConsistencyBrowserAgent() {}

void AccountConsistencyBrowserAgent::InstallDependency(
    web::WebState* web_state) {
  if (AccountConsistencyService* accountConsistencyService =
          ios::AccountConsistencyServiceFactory::GetForBrowserState(
              browser_->GetBrowserState())) {
    accountConsistencyService->SetWebStateHandler(web_state, this);
  }
}

void AccountConsistencyBrowserAgent::UninstallDependency(
    web::WebState* web_state) {
  if (AccountConsistencyService* accountConsistencyService =
          ios::AccountConsistencyServiceFactory::GetForBrowserState(
              browser_->GetBrowserState())) {
    accountConsistencyService->RemoveWebStateHandler(web_state);
  }
}

void AccountConsistencyBrowserAgent::OnRestoreGaiaCookies() {
  signin_metrics::LogAccountReconcilorStateOnGaiaResponse(
      ios::AccountReconcilorFactory::GetForBrowserState(
          browser_->GetBrowserState())
          ->GetState());
  [handler_
      showSigninAccountNotificationFromViewController:base_view_controller_];
}

void AccountConsistencyBrowserAgent::OnManageAccounts() {
  signin_metrics::LogAccountReconcilorStateOnGaiaResponse(
      ios::AccountReconcilorFactory::GetForBrowserState(
          browser_->GetBrowserState())
          ->GetState());
  [handler_ showAccountsSettingsFromViewController:base_view_controller_];
}

void AccountConsistencyBrowserAgent::OnShowConsistencyPromo(
    const GURL& url,
    web::WebState* web_state) {
  signin_metrics::LogAccountReconcilorStateOnGaiaResponse(
      ios::AccountReconcilorFactory::GetForBrowserState(
          browser_->GetBrowserState())
          ->GetState());
  web::WebState* current_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  if (current_web_state == web_state) {
    [handler_ showWebSigninPromoFromViewController:base_view_controller_
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
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kAddAccount
            accessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_ACCOUNT_CONSISTENCY_SERVICE];
  [handler_ showSignin:command baseViewController:base_view_controller_];
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
  [handler_ openURLInNewTab:command];
}

void AccountConsistencyBrowserAgent::BrowserDestroyed(Browser* browser) {
  installation_observer_.reset();
  browser_->RemoveObserver(this);
}
