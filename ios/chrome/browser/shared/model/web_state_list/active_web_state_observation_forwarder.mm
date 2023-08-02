// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"

#import "base/check.h"

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

#pragma mark - WebStateListObserver

void ActiveWebStateObservationForwarder::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  if (!status.active_web_state_change()) {
    return;
  }

  web_state_observation_.Reset();
  if (status.new_active_web_state) {
    web_state_observation_.Observe(status.new_active_web_state);
  }
}
