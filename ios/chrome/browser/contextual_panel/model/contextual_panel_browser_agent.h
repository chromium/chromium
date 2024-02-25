// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_BROWSER_AGENT_H_

#import "base/scoped_observation.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

#import <UIKit/UIKit.h>

class Browser;
class WebStateList;

// Browser agent that is responsible for observing the WebStateList to listen to
// the current contextual panel model for updates, and updating the entrypoint
// UI accordingly.
class ContextualPanelBrowserAgent
    : public WebStateListObserver,
      public BrowserUserData<ContextualPanelBrowserAgent> {
 public:
  ContextualPanelBrowserAgent(const ContextualPanelBrowserAgent&) = delete;
  ContextualPanelBrowserAgent& operator=(const ContextualPanelBrowserAgent&) =
      delete;

  ~ContextualPanelBrowserAgent() override;

 private:
  friend class BrowserUserData<ContextualPanelBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit ContextualPanelBrowserAgent(Browser* browser);

  // WebStateListObserver methods.
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  void WebStateListDestroyed(WebStateList* web_state_list) override;

  // ScopedObservation for WebStateList.
  base::ScopedObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_BROWSER_AGENT_H_
