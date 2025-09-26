// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PERSIST_TAB_CONTEXT_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PERSIST_TAB_CONTEXT_BROWSER_AGENT_H_

#include "base/scoped_observation.h"
#include "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#include "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#include "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"
#include "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"

@class PersistTabContextStateObserver;

// PersistTabContextBrowserAgent allows saving and retrieving saved page
// contexts. Page contexts are retrieved and saved to storage when a tab is
// backgrounded (either switched tab or closed app). The page contexts that are
// stored contain information on tab content such as the APC and the inner_text,
// along with the page title and url. Once a tab is closed, its page context is
// deleted from storage.
class PersistTabContextBrowserAgent
    : public BrowserUserData<PersistTabContextBrowserAgent>,
      public web::WebStateObserver,
      public TabsDependencyInstaller {
 public:
  PersistTabContextBrowserAgent(const PersistTabContextBrowserAgent&) = delete;
  PersistTabContextBrowserAgent& operator=(
      const PersistTabContextBrowserAgent&) = delete;

  ~PersistTabContextBrowserAgent() override;

  // TabsDependencyInstaller
  void OnWebStateInserted(web::WebState* web_state) override;
  void OnWebStateRemoved(web::WebState* web_state) override;
  void OnWebStateDeleted(web::WebState* web_state) override;
  void OnActiveWebStateChanged(web::WebState* old_active,
                               web::WebState* new_active) override;

  // WebStateObserver:
  void WasHidden(web::WebState* web_state) override;

 private:
  friend class BrowserUserData<PersistTabContextBrowserAgent>;

  explicit PersistTabContextBrowserAgent(Browser* browser);

  // Private callback for PageContextWrapper.
  void OnPageContextExtracted(const std::string& webstate_unique_id,
                              PageContextWrapperCallbackResponse response);

  // The service's PageContext wrapper.
  __strong PageContextWrapper* page_context_wrapper_;

  // Manages this object as an observer of `web_state_`.
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  base::WeakPtrFactory<PersistTabContextBrowserAgent> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PERSIST_TAB_CONTEXT_BROWSER_AGENT_H_
