// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_GLOBAL_WEB_STATE_EVENT_TRACKER_H_
#define IOS_WEB_WEB_STATE_GLOBAL_WEB_STATE_EVENT_TRACKER_H_

#include <stddef.h>

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/scoped_multi_source_observation.h"
#include "ios/web/public/deprecated/global_web_state_observer.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"

namespace web {

// This singleton serves as the mechanism via which GlobalWebStateObservers get
// informed of relevant events from all WebState instances.
class GlobalWebStateEventTracker : public WebStateObserver {
 public:
  // Returns the instance of GlobalWebStateEventTracker.
  static GlobalWebStateEventTracker* GetInstance();

  GlobalWebStateEventTracker(const GlobalWebStateEventTracker&) = delete;
  GlobalWebStateEventTracker& operator=(const GlobalWebStateEventTracker&) =
      delete;

  // Adds/removes observers.
  void AddObserver(GlobalWebStateObserver* observer);
  void RemoveObserver(GlobalWebStateObserver* observer);

 private:
  friend class base::NoDestructor<GlobalWebStateEventTracker>;
  friend class WebStateEventForwarder;
  friend class WebStateImpl;

  // Should be called whenever a WebState instance is created.
  void OnWebStateCreated(WebState* web_state);

  // WebStateObserver implementation.
  void DidStartNavigation(WebState* web_state,
                          NavigationContext* navigation_context) override;
  void DidStartLoading(WebState* web_state) override;
  void DidStopLoading(WebState* web_state) override;
  void RenderProcessGone(WebState* web_state) override;
  void WebStateDestroyed(WebState* web_state) override;

  GlobalWebStateEventTracker();
  ~GlobalWebStateEventTracker() override;

  // ScopedMultiSourceObservation used to track registration with WebState.
  base::ScopedMultiSourceObservation<WebState, WebStateObserver>
      scoped_observations_{this};

  // List of observers currently registered with the tracker.
  base::ObserverList<GlobalWebStateObserver, true> observer_list_;
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_GLOBAL_WEB_STATE_EVENT_TRACKER_H_
