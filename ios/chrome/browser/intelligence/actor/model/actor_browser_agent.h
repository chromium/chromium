// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_BROWSER_AGENT_H_

#import "base/scoped_observation.h"
#import "components/sessions/core/session_id.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

class ActorTabHelper;
class Browser;
class WebStateList;

// Observes the `WebStateList` and the active tab's `ActorTabHelper` to dispatch
// commands to show/hide the actuation UI when a tab undergoes actuation.
class ActorBrowserAgent : public BrowserUserData<ActorBrowserAgent>,
                          public WebStateListObserver,
                          public ActorTabHelperObserver {
 public:
  ActorBrowserAgent(const ActorBrowserAgent&) = delete;
  ActorBrowserAgent& operator=(const ActorBrowserAgent&) = delete;

  ~ActorBrowserAgent() override;

  // Returns the unique session ID of the `Browser*` that `this` is tied to.
  SessionID browser_id() const;

 private:
  friend class BrowserUserData<ActorBrowserAgent>;

  explicit ActorBrowserAgent(Browser* browser);

  // WebStateListObserver:
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;
  void WebStateListDestroyed(WebStateList* web_state_list) override;

  // ActorTabHelperObserver:
  void OnActuationStateChanged(ActorTabHelper* tab_helper,
                               web::WebState* web_state,
                               bool actuating) override;

  // Updates the observed tab helper based on the active `WebState` change from
  // `old_web_state` to `new_web_state`.
  void UpdateActiveWebState(web::WebState* old_web_state,
                            web::WebState* new_web_state);

  // The active `WebState`'s tab helper observation.
  base::ScopedObservation<ActorTabHelper, ActorTabHelperObserver>
      tab_helper_observation_{this};

  // The `WebStateList` observation.
  base::ScopedObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};

  // A unique session ID of the `Browser*` that `this` is tied to.
  const SessionID browser_id_;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_BROWSER_AGENT_H_
