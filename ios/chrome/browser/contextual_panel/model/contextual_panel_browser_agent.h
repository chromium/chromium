// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_BROWSER_AGENT_H_

#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

class Browser;
class WebStateList;

// Browser agent that is responsible for observing the WebStateList to listen to
// the current contextual panel model for updates, and updating the entrypoint
// UI accordingly.
class ContextualPanelBrowserAgent
    : public ContextualPanelTabHelperObserver,
      public WebStateListObserver,
      public BrowserUserData<ContextualPanelBrowserAgent> {
 public:
  ContextualPanelBrowserAgent(const ContextualPanelBrowserAgent&) = delete;
  ContextualPanelBrowserAgent& operator=(const ContextualPanelBrowserAgent&) =
      delete;

  ~ContextualPanelBrowserAgent() override;

  // Whether the currently observed ContextualPanelTabHelper has an entrypoint
  // config available.
  bool IsEntrypointConfigurationAvailableForCurrentTab();

  // Gets the config that should be used currently to show the entrypoint, from
  // the currently observed ContextualPanelTabHelper.
  base::WeakPtr<ContextualPanelItemConfiguration>
  GetEntrypointConfigurationForCurrentTab();

  // Getter and setter for whether the large entrypoint of currently observed
  // ContextualPanelTabHelper has been shown.
  bool WasLargeEntrypointShownForCurrentTab();
  void SetLargeEntrypointShownForCurrentTab(bool shown);

 private:
  friend class BrowserUserData<ContextualPanelBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit ContextualPanelBrowserAgent(Browser* browser);

  // WebStateListObserver methods.
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  void WebStateListDestroyed(WebStateList* web_state_list) override;

  // ContextualPanelTabHelperObserver methods.
  void ContextualPanelHasNewData(
      ContextualPanelTabHelper* tab_helper,
      std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>
          item_configurations) override;

  void ContextualPanelTabHelperDestroyed(
      ContextualPanelTabHelper* tab_helper) override;

  // ScopedObservation for WebStateList.
  base::ScopedObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};

  // ScopeObservation for ContextualPanelTabHelper.
  base::ScopedObservation<ContextualPanelTabHelper,
                          ContextualPanelTabHelperObserver>
      contextual_panel_tab_helper_observation_{this};

  // The owning Browser.
  raw_ptr<Browser> browser_;
};

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_BROWSER_AGENT_H_
