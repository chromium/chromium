// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_browser_agent.h"

#import "base/check.h"

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
  // TODO(crbug.com/568445140): Show the primary account re-authentication flow
  // and reload `web_state` once the user signed in again.
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
