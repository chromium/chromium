// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_ENTERPRISE_PROXY_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_ENTERPRISE_PROXY_BROWSER_AGENT_H_

#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"

namespace web {
class WebState;
}  // namespace web

// Browser agent that acts as the `EnterpriseProxyTabHelper::Delegate` for the
// tabs of a `Browser` and their Reader mode content, handling the re-sign-in
// requests of the Secure Gateway authentication interstitial. Other WebStates
// that aren't tabs, like the Lens overlay and AI Mode ones, are handled
// separately (crbug.com/578096267).
class EnterpriseProxyBrowserAgent
    : public BrowserUserData<EnterpriseProxyBrowserAgent>,
      public EnterpriseProxyTabHelper::Delegate,
      public TabsDependencyInstaller {
 public:
  EnterpriseProxyBrowserAgent(const EnterpriseProxyBrowserAgent&) = delete;
  EnterpriseProxyBrowserAgent& operator=(const EnterpriseProxyBrowserAgent&) =
      delete;

  ~EnterpriseProxyBrowserAgent() override;

  // `EnterpriseProxyTabHelper::Delegate`:
  void SignIn(web::WebState* web_state) override;

  // `TabsDependencyInstaller`:
  void OnWebStateInserted(web::WebState* web_state) override;
  void OnWebStateRemoved(web::WebState* web_state) override;
  void OnWebStateDeleted(web::WebState* web_state) override;
  void OnActiveWebStateChanged(web::WebState* old_active,
                               web::WebState* new_active) override;

 private:
  friend class BrowserUserData<EnterpriseProxyBrowserAgent>;

  explicit EnterpriseProxyBrowserAgent(Browser* browser);
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_ENTERPRISE_PROXY_BROWSER_AGENT_H_
