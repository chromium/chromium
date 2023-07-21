// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_ACTIVE_WEB_STATE_OBSERVATION_FORWARDER_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_ACTIVE_WEB_STATE_OBSERVATION_FORWARDER_H_

#include "base/scoped_observation.h"
#include "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#include "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"

// ActiveWebStateObservationForwarder forwards WebStateObserver methods for the
// active WebState in a WebStateList, handling cases where the active WebState
// changes.
class ActiveWebStateObservationForwarder : public WebStateListObserver {
 public:
  // Creates an object which forwards observation methods to `observer` and
  // tracks `web_state_list` to keep track of the currently-active WebState.
  // `web_state_list` and `observer` must both outlive this object.
  ActiveWebStateObservationForwarder(WebStateList* web_state_list,
                                     web::WebStateObserver* observer);

  ActiveWebStateObservationForwarder(
      const ActiveWebStateObservationForwarder&) = delete;
  ActiveWebStateObservationForwarder& operator=(
      const ActiveWebStateObservationForwarder&) = delete;

  ~ActiveWebStateObservationForwarder() override;

  // WebStateListObserver.
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

 private:
  base::ScopedObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_ACTIVE_WEB_STATE_OBSERVATION_FORWARDER_H_
