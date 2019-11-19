// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_ALL_WEB_STATE_OBSERVATION_FORWARDER_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_ALL_WEB_STATE_OBSERVATION_FORWARDER_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/browser/web_state_list/web_state_list_observer.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"

// AllWebStateObservationForwarder forwards WebStateObserver methods for all
// WebStates in a WebStateList, handling cases where WebStates are added,
// removed, or replaced.
class AllWebStateObservationForwarder : public WebStateListObserver {
 public:
  // Creates an object which forwards observation methods to |observer| and
  // tracks the set of WebStates in |web_state_list|. |web_state_list| and
  // |observer| must both outlive this object.
  AllWebStateObservationForwarder(WebStateList* web_state_list,
                                  web::WebStateObserver* observer);
  ~AllWebStateObservationForwarder() override;

  // WebStateListObserver.
  void WebStateInsertedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index,
                          bool activating) override;
  void WebStateReplacedAt(WebStateList* web_state_list,
                          web::WebState* old_web_state,
                          web::WebState* new_web_state,
                          int index) override;
  void WebStateDetachedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index) override;

 private:
  ScopedObserver<WebStateList, WebStateListObserver> web_state_list_observer_;
  ScopedObserver<web::WebState, web::WebStateObserver> web_state_observer_;

  DISALLOW_COPY_AND_ASSIGN(AllWebStateObservationForwarder);
};

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_ALL_WEB_STATE_OBSERVATION_FORWARDER_H_
