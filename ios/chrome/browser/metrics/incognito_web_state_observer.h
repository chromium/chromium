// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_INCOGNITO_WEB_STATE_OBSERVER_H_
#define IOS_CHROME_BROWSER_METRICS_INCOGNITO_WEB_STATE_OBSERVER_H_

#include <set>

#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"

class AllWebStateListObservationRegistrar;

// Interface for getting notified when WebStates get added/removed to/from an
// incognito browser state.
class IncognitoWebStateObserver {
 public:
  IncognitoWebStateObserver();
  IncognitoWebStateObserver(const IncognitoWebStateObserver&) = delete;
  IncognitoWebStateObserver& operator=(const IncognitoWebStateObserver&) =
      delete;
  ~IncognitoWebStateObserver();

 protected:
  virtual void OnIncognitoWebStateAdded() = 0;
  virtual void OnIncognitoWebStateRemoved() = 0;

 private:
  // Observer implementation for each browser state.
  class Observer : public WebStateListObserver {
   public:
    explicit Observer(IncognitoWebStateObserver* incognito_tracker);
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() override;

   private:
    // WebStateListObserver:
    void WebStateInsertedAt(WebStateList* web_state_list,
                            web::WebState* web_state,
                            int index,
                            bool activating) override;
    void WebStateDetachedAt(WebStateList* web_state_list,
                            web::WebState* web_state,
                            int index) override;
    void WebStateReplacedAt(WebStateList* web_state_list,
                            web::WebState* old_web_state,
                            web::WebState* new_web_state,
                            int index) override;
    IncognitoWebStateObserver* incognito_tracker_;
  };

  // Observation registrars for each browser state; each one owns an instance
  // of IncognitoWebStateObserver::Observer.
  std::set<std::unique_ptr<AllWebStateListObservationRegistrar>> registrars_;
};

#endif  // IOS_CHROME_BROWSER_METRICS_INCOGNITO_WEB_STATE_OBSERVER_H_
