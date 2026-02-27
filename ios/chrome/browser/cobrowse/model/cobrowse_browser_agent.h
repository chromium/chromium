// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_MODEL_COBROWSE_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_COBROWSE_MODEL_COBROWSE_BROWSER_AGENT_H_

#import "ios/chrome/browser/cobrowse/model/cobrowse_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"

// Browser agent that manages the CobrowseTabHelper delegate.
class CobrowseBrowserAgent : public BrowserUserData<CobrowseBrowserAgent>,
                             public CobrowseTabHelper::Delegate,
                             public TabsDependencyInstaller {
 public:
  ~CobrowseBrowserAgent() override;

  // CobrowseTabHelper::Delegate:
  bool CanShowAssistantForWebState(web::WebState* web_state) override;

  // TabsDependencyInstaller:
  void OnWebStateInserted(web::WebState* web_state) override;
  void OnWebStateRemoved(web::WebState* web_state) override;
  void OnWebStateDeleted(web::WebState* web_state) override;
  void OnActiveWebStateChanged(web::WebState* old_active,
                               web::WebState* new_active) override;

 private:
  friend class BrowserUserData<CobrowseBrowserAgent>;

  explicit CobrowseBrowserAgent(Browser* browser);
};

#endif  // IOS_CHROME_BROWSER_COBROWSE_MODEL_COBROWSE_BROWSER_AGENT_H_
