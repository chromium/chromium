// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_MODEL_COBROWSE_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_COBROWSE_MODEL_COBROWSE_BROWSER_AGENT_H_

#import "base/callback_list.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"

@class CobrowseContext;

// Browser agent that manages the CobrowseTabHelper delegate.
class CobrowseBrowserAgent : public BrowserUserData<CobrowseBrowserAgent>,
                             public CobrowseTabHelper::Delegate,
                             public TabsDependencyInstaller {
 public:
  // Interface to be implemented by the UI layer to provide state information.
  class UIStateProvider {
   public:
    virtual ~UIStateProvider() = default;

    virtual bool IsTabGridVisible() = 0;
  };

  ~CobrowseBrowserAgent() override;

  // Sets the provider for UI state information.
  void SetUIStateProvider(UIStateProvider* provider);

  // Sets the context for the Cobrowse flow.
  void SetCobrowseContext(CobrowseContext* context);

  // Returns the current Cobrowse context.
  CobrowseContext* GetCobrowseContext();

  // CobrowseTabHelper::Delegate:
  bool CanShowAssistantForWebState(web::WebState* web_state) override;
  void ConfigureAssistantContextForWebState(web::WebState* web_state) override;
  bool IsSessionActive() override;
  void SetSessionActive(bool active) override;

  // TabsDependencyInstaller:
  void OnWebStateInserted(web::WebState* web_state) override;
  void OnWebStateRemoved(web::WebState* web_state) override;
  void OnWebStateDeleted(web::WebState* web_state) override;
  void OnActiveWebStateChanged(web::WebState* old_active,
                               web::WebState* new_active) override;

 private:
  friend class BrowserUserData<CobrowseBrowserAgent>;

  explicit CobrowseBrowserAgent(Browser* browser);

  // The provider for UI state information.
  raw_ptr<UIStateProvider> ui_state_provider_ = nullptr;

  // The context for the Cobrowse flow.
  __strong CobrowseContext* context_ = nil;

  // Whether a cobrowse session is currently active for this browser.
  bool is_session_active_ = false;

  // Subscription for eligibility changes.
  base::CallbackListSubscription eligibility_subscription_;

  // Called when eligibility changes.
  void OnEligibilityChanged();
};

#endif  // IOS_CHROME_BROWSER_COBROWSE_MODEL_COBROWSE_BROWSER_AGENT_H_
