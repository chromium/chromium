// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"

#import "base/check.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ActiveWebStateObservationForwarder::ActiveWebStateObservationForwarder(
    WebStateList* web_state_list,
    web::WebStateObserver* observer)
    : web_state_observation_(observer) {
  DCHECK(observer);
  DCHECK(web_state_list);
  web_state_list_observation_.Observe(web_state_list);

  web::WebState* active_web_state = web_state_list->GetActiveWebState();
  if (active_web_state) {
    web_state_observation_.Observe(active_web_state);
  }
}

ActiveWebStateObservationForwarder::~ActiveWebStateObservationForwarder() {}

void ActiveWebStateObservationForwarder::WebStateActivatedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int active_index,
    ActiveWebStateChangeReason reason) {
  web_state_observation_.Reset();
  if (new_web_state) {
    web_state_observation_.Observe(new_web_state);
  }
}
