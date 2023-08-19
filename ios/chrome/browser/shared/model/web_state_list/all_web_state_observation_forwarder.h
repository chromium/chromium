// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_ALL_WEB_STATE_OBSERVATION_FORWARDER_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_ALL_WEB_STATE_OBSERVATION_FORWARDER_H_

#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#include "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"

// AllWebStateObservationForwarder forwards WebStateObserver methods for all
// WebStates in a WebStateList, handling cases where WebStates are added,
// removed, or replaced.
class AllWebStateObservationForwarder : public WebStateListObserver {
 public:
  // Creates an object which forwards observation methods to `observer` and
  // tracks the set of WebStates in `web_state_list`. `web_state_list` and
  // `observer` must both outlive this object.
  AllWebStateObservationForwarder(WebStateList* web_state_list,
                                  web::WebStateObserver* observer);

  AllWebStateObservationForwarder(const AllWebStateObservationForwarder&) =
      delete;
  AllWebStateObservationForwarder& operator=(
      const AllWebStateObservationForwarder&) = delete;

  ~AllWebStateObservationForwarder() override;

  // WebStateListObserver.
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

 private:
  base::ScopedObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};
  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      web_state_observations_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_ALL_WEB_STATE_OBSERVATION_FORWARDER_H_
