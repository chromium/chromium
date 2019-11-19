// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_ACTIVE_WEB_STATE_OBSERVATION_FORWARDER_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_ACTIVE_WEB_STATE_OBSERVATION_FORWARDER_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/browser/web_state_list/web_state_list_observer.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"

// ActiveWebStateObservationForwarder forwards WebStateObserver methods for the
// active WebState in a WebStateList, handling cases where the active WebState
// changes.
class ActiveWebStateObservationForwarder : public WebStateListObserver {
 public:
  // Creates an object which forwards observation methods to |observer| and
  // tracks |web_state_list| to keep track of the currently-active WebState.
  // |web_state_list| and |observer| must both outlive this object.
  ActiveWebStateObservationForwarder(WebStateList* web_state_list,
                                     web::WebStateObserver* observer);
  ~ActiveWebStateObservationForwarder() override;

  // WebStateListObserver.
  void WebStateActivatedAt(WebStateList* web_state_list,
                           web::WebState* old_web_state,
                           web::WebState* new_web_state,
                           int active_index,
                           int reason) override;

 private:
  ScopedObserver<WebStateList, WebStateListObserver> web_state_list_observer_{
      this};
  ScopedObserver<web::WebState, web::WebStateObserver> web_state_observer_;

  DISALLOW_COPY_AND_ASSIGN(ActiveWebStateObservationForwarder);
};

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_ACTIVE_WEB_STATE_OBSERVATION_FORWARDER_H_
