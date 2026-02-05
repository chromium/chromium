// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_CONSISTENCY_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_CONSISTENCY_BROWSER_AGENT_H_

#import "base/memory/raw_ptr.h"
#import "components/signin/ios/browser/manage_accounts_delegate.h"
#import "components/signin/ios/browser/signin_enabled_datasource.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"

class Browser;
@class ManageAccountsDelegateBridge;
@protocol SceneCommands;
@class SceneState;
@protocol SettingsCommands;
@class SigninCoordinator;
@protocol SystemIdentity;
@class UIViewController;

// A browser agent that tracks the addition and removal of webstates, registers
// them with the AccountConsistencyService, and handles events triggered from
// them.
class AccountConsistencyBrowserAgent
    : public BrowserUserData<AccountConsistencyBrowserAgent>,
      public TabsDependencyInstaller,
      public ManageAccountsDelegate {
 public:
  ~AccountConsistencyBrowserAgent() override;

  // TabsDependencyInstaller
  void OnWebStateInserted(web::WebState* web_state) override;
  void OnWebStateRemoved(web::WebState* web_state) override;
  void OnWebStateDeleted(web::WebState* web_state) override;
  void OnActiveWebStateChanged(web::WebState* old_active,
                               web::WebState* new_active) override;

  // ManageAccountsDelegate
  void OnRestoreGaiaCookies() override;
  void OnManageAccounts(const GURL& url) override;
  void OnAddAccount(const GURL& url,
                    const std::string& prefilled_email) override;
  void OnShowConsistencyPromo(const GURL& url,
                              web::WebState* webState) override;
  void OnGoIncognito(const GURL& url) override;
  bool SigninEnabled() const override;

 private:
  friend class BrowserUserData<AccountConsistencyBrowserAgent>;

  // Opens the account menu if the prefilled account is on the device.
  // Otherwise, open the Add Account view.
  void OnAddPrefilledAccount(const GURL& url,
                             const std::string& prefilled_email);
  // Opens the account menu if there is at least another profile. Otherwise open
  // the Add Account View.
  void OnAddUnkwownAccount(const GURL& url);

  void StopSigninCoordinator(SigninCoordinatorResult result,
                             id<SystemIdentity> identity);

  // `base_view_controller` is the view controller which UI will be presented
  // from.
  AccountConsistencyBrowserAgent(
      Browser* browser,
      UIViewController* base_view_controller,
      signin::SigninEnabledDataSource* signin_enabled_data_source);

  // Returns whether it is is possible to show the browser's account menu.
  bool CanShowAccountMenu() const;

  // Opens the account menu, offering to switch to a different account (even one
  // that's in a different profile).
  void ShowAccountMenu(const GURL& url);

  UIViewController* base_view_controller_;
  id<SceneCommands> application_handler_;
  id<SettingsCommands> settings_handler_;
  SigninCoordinator* add_account_coordinator_;

  raw_ptr<signin::SigninEnabledDataSource> signin_enabled_data_source_;

  // Bridge object to act as the delegate.
  ManageAccountsDelegateBridge* bridge_;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_CONSISTENCY_BROWSER_AGENT_H_
