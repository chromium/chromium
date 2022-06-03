// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_USAGE_ENABLER_WEB_USAGE_ENABLER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_USAGE_ENABLER_WEB_USAGE_ENABLER_BROWSER_AGENT_H_

#import "ios/chrome/browser/main/browser_observer.h"
#include "ios/chrome/browser/main/browser_user_data.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"

// An agent that observes the browser's WebStateList and enables or disables web
// usage for WebStates that are added or removed.  This can be used to easily
// enable or disable web usage for all the WebStates in a WebStateList.
class WebUsageEnablerBrowserAgent
    : public BrowserUserData<WebUsageEnablerBrowserAgent>,
      BrowserObserver,
      WebStateListObserver {
 public:
  // Not copyable or moveable
  WebUsageEnablerBrowserAgent(const WebUsageEnablerBrowserAgent&) = delete;
  WebUsageEnablerBrowserAgent& operator=(const WebUsageEnablerBrowserAgent&) =
      delete;
  ~WebUsageEnablerBrowserAgent() override;

  // Sets the WebUsageEnabled property for all WebStates in the list.  When new
  // WebStates are added to the list, their web usage will be set to the
  // |web_usage_enabled| as well. Initially |false|.
  void SetWebUsageEnabled(bool web_usage_enabled);

  // The current value set with |SetWebUsageEnabled|.
  bool IsWebUsageEnabled() const;

  // When TriggersInitialLoad() is |true|, the enabler will kick off the initial
  // load for WebStates added to the list while |IsWebUsageEnabled| is |true|.
  // Initially |true|.
  bool TriggersInitialLoad() const;

  // Sets the value for |TriggersInitialLoad|.
  void SetTriggersInitialLoad(bool triggers_initial_load);

 private:
  // Updates the web usage enabled status of all WebStates in |browser_|'s web
  // state list to |web_usage_enabled_|.
  void UpdateWebUsageForAllWebStates();
  // Updates the web usage enabled status of |web_state|, triggering the initial
  // load if that is enabled.
  void UpdateWebUsageForAddedWebState(web::WebState* web_state);

  // BrowserObserver:
  void BrowserDestroyed(Browser* browser) override;

  // WebStateListObserver:
  void WebStateInsertedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index,
                          bool activating) override;
  void WebStateReplacedAt(WebStateList* web_state_list,
                          web::WebState* old_web_state,
                          web::WebState* new_web_state,
                          int index) override;

  explicit WebUsageEnablerBrowserAgent(Browser* browser);
  friend class BrowserUserData<WebUsageEnablerBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  // The browser whose WebStates' web usage is being managed.
  Browser* browser_;
  // Whether web usage is enabled for the WebState in |web_state_list_|.
  bool web_usage_enabled_ = false;
  // Whether the initial load for a WebState added to |web_state_list_| should
  // be triggered if |web_usage_enabled_| is true.
  bool triggers_initial_load_ = true;
};

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_USAGE_ENABLER_WEB_USAGE_ENABLER_BROWSER_AGENT_H_
