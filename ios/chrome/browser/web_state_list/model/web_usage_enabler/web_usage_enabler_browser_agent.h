// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_MODEL_WEB_USAGE_ENABLER_WEB_USAGE_ENABLER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_MODEL_WEB_USAGE_ENABLER_WEB_USAGE_ENABLER_BROWSER_AGENT_H_

#import "base/memory/raw_ptr.h"
#import "base/observer_list.h"
#import "base/scoped_multi_source_observation.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"

class WebUsageEnablerBrowserAgentObserver;

// An agent that observes the browser's WebStateList and enables or disables web
// usage for WebStates that are added or removed.  This can be used to easily
// enable or disable web usage for all the WebStates in a WebStateList.
class WebUsageEnablerBrowserAgent
    : public BrowserUserData<WebUsageEnablerBrowserAgent>,
      public BrowserObserver,
      public web::WebStateObserver,
      public WebStateListObserver {
 public:
  // Not copyable or moveable
  WebUsageEnablerBrowserAgent(const WebUsageEnablerBrowserAgent&) = delete;
  WebUsageEnablerBrowserAgent& operator=(const WebUsageEnablerBrowserAgent&) =
      delete;

  ~WebUsageEnablerBrowserAgent() override;

  // Sets the WebUsageEnabled property for all WebStates in the list.  When new
  // WebStates are added to the list, their web usage will be set to the
  // `web_usage_enabled` as well. Initially `false`.
  void SetWebUsageEnabled(bool web_usage_enabled);

  // The current value set with `SetWebUsageEnabled`.
  bool IsWebUsageEnabled() const;

  // Adds and removes observers.
  void AddObserver(WebUsageEnablerBrowserAgentObserver* observer);
  void RemoveObserver(WebUsageEnablerBrowserAgentObserver* observer);

 private:
  friend class BrowserUserData<WebUsageEnablerBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit WebUsageEnablerBrowserAgent(Browser* browser);

  // Updates the web usage enabled status of all WebStates in `browser_`'s web
  // state list to `web_usage_enabled_`.
  void UpdateWebUsageForAllWebStates();
  // Updates the web usage enabled status of `web_state`, triggering the initial
  // load if `triggers_initial_load` is true.
  void UpdateWebUsageForAddedWebState(web::WebState* web_state,
                                      bool triggers_initial_load);

  // BrowserObserver:
  void BrowserDestroyed(Browser* browser) override;

  // WebStateListObserver:
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  // web::WebStateObserver:
  void WebStateRealized(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // The browser whose WebStates' web usage is being managed.
  raw_ptr<Browser> browser_ = nullptr;

  // Whether web usage is enabled for the WebState in `web_state_list_`.
  bool web_usage_enabled_ = false;

  // Scoped observations of Browser, WebStateList and WebStates.
  base::ScopedObservation<Browser, BrowserObserver> browser_observation_{this};

  base::ScopedObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};

  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      web_state_observations_{this};

  // The list of observers.
  base::ObserverList<WebUsageEnablerBrowserAgentObserver, /*check_empty=*/true>
      observers_;
};

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_MODEL_WEB_USAGE_ENABLER_WEB_USAGE_ENABLER_BROWSER_AGENT_H_
