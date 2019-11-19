// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/active_web_state_observation_forwarder.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ActiveWebStateObservationForwarder::ActiveWebStateObservationForwarder(
    WebStateList* web_state_list,
    web::WebStateObserver* observer)
    : web_state_observer_(observer) {
  DCHECK(observer);
  DCHECK(web_state_list);
  web_state_list_observer_.Add(web_state_list);

  web::WebState* active_web_state = web_state_list->GetActiveWebState();
  if (active_web_state) {
    web_state_observer_.Add(active_web_state);
  }
}

ActiveWebStateObservationForwarder::~ActiveWebStateObservationForwarder() {}

void ActiveWebStateObservationForwarder::WebStateActivatedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int active_index,
    int reason) {
  if (old_web_state) {
    web_state_observer_.Remove(old_web_state);
  }

  if (new_web_state) {
    web_state_observer_.Add(new_web_state);
  }
}
