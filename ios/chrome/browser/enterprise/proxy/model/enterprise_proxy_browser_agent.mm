// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_browser_agent.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/weak_ptr.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/primary_account_reauth_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/reload_type.h"
#import "ios/web/public/web_state.h"

namespace {

// Returns the `EnterpriseProxyTabHelper` of `web_state`. It should always
// exist: this agent only receives WebStates that went through
// `AttachTabHelpers()` with a filter that keeps the tab helper, i.e. the tabs
// of its `Browser` and their Reader mode content, which
// `TabsDependencyInstallerManager` hands to it. The `CHECK` is not fatal until
// M166, to report a change of the tab helper conditions during the rollout
// without crashing the browser, so callers must still handle a missing tab
// helper.
EnterpriseProxyTabHelper* GetTabHelper(web::WebState* web_state) {
  EnterpriseProxyTabHelper* tab_helper =
      EnterpriseProxyTabHelper::FromWebState(web_state);
  CHECK(tab_helper, base::NotFatalUntil::M166);
  return tab_helper;
}

}  // namespace

EnterpriseProxyBrowserAgent::EnterpriseProxyBrowserAgent(Browser* browser)
    : BrowserUserData<EnterpriseProxyBrowserAgent>(browser) {
  StartObserving(browser_);
}

EnterpriseProxyBrowserAgent::~EnterpriseProxyBrowserAgent() {
  StopObserving();
}

#pragma mark - EnterpriseProxyTabHelper::Delegate

void EnterpriseProxyBrowserAgent::SignIn(web::WebState* web_state) {
  CHECK(web_state);
  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForProfile(browser_->GetProfile());

  // TODO(crbug.com/579867931): Handle the edge case where there is no primary
  // account signed in.
  if (!auth_service || !auth_service->HasPrimaryIdentity()) {
    // Only users with an active primary account can reauthenticate (e.g. not
    // signed in or in incognito mode where `auth_service` is null).
    return;
  }

  id<PrimaryAccountReauthCommands> handler = HandlerForProtocol(
      browser_->GetCommandDispatcher(), PrimaryAccountReauthCommands);
  [handler
      showPrimaryAccountReauthWithAccessPoint:signin_metrics::AccessPoint::
                                                  kEnterpriseProxy
                            reauthAccessPoint:signin_metrics::
                                                  ReauthAccessPoint::
                                                      kEnterpriseProxy
                          dismissalCompletion:
                              base::CallbackToBlock(base::BindOnce(
                                  [](base::WeakPtr<web::WebState>
                                         target_web_state) {
                                    // Tries to reload the `WebState` after the
                                    // reauth UI is dismissed.
                                    if (target_web_state) {
                                      target_web_state->GetNavigationManager()
                                          ->Reload(web::ReloadType::NORMAL,
                                                   /*check_for_repost=*/true);
                                    }
                                  },
                                  web_state->GetWeakPtr()))];
}

#pragma mark - TabsDependencyInstaller

void EnterpriseProxyBrowserAgent::OnWebStateInserted(web::WebState* web_state) {
  if (EnterpriseProxyTabHelper* tab_helper = GetTabHelper(web_state)) {
    tab_helper->SetDelegate(this);
  }
}

void EnterpriseProxyBrowserAgent::OnWebStateRemoved(web::WebState* web_state) {
  if (EnterpriseProxyTabHelper* tab_helper = GetTabHelper(web_state)) {
    tab_helper->SetDelegate(nullptr);
  }
}

void EnterpriseProxyBrowserAgent::OnWebStateDeleted(web::WebState* web_state) {
  // Nothing to do.
}

void EnterpriseProxyBrowserAgent::OnActiveWebStateChanged(
    web::WebState* old_active,
    web::WebState* new_active) {
  // Nothing to do.
}
