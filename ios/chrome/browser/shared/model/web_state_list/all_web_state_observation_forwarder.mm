// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/all_web_state_observation_forwarder.h"

#import "base/check.h"

AllWebStateObservationForwarder::AllWebStateObservationForwarder(
    WebStateList* web_state_list,
    web::WebStateObserver* observer)
    : web_state_observations_(observer) {
  DCHECK(observer);
  DCHECK(web_state_list);
  web_state_list_observation_.Observe(web_state_list);

  for (int ii = 0; ii < web_state_list->count(); ++ii) {
    web::WebState* web_state = web_state_list->GetWebStateAt(ii);
    web_state_observations_.AddObservation(web_state);
  }
}

AllWebStateObservationForwarder::~AllWebStateObservationForwarder() {}

#pragma mark - WebStateListObserver

void AllWebStateObservationForwarder::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // Do nothing when a WebState is selected and its status is updated.
      break;
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detach_change =
          change.As<WebStateListChangeDetach>();
      web_state_observations_.RemoveObservation(
          detach_change.detached_web_state());
      break;
    }
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      web_state_observations_.RemoveObservation(
          replace_change.replaced_web_state());
      web_state_observations_.AddObservation(
          replace_change.inserted_web_state());
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();
      web_state_observations_.AddObservation(
          insert_change.inserted_web_state());
      break;
    }
    case WebStateListChange::Type::kGroupCreate:
      // Do nothing when a group is created.
      break;
    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // Do nothing when a tab group's visual data are updated.
      break;
    case WebStateListChange::Type::kGroupMove:
      // Do nothing when a tab group is moved.
      break;
    case WebStateListChange::Type::kGroupDelete:
      // Do nothing when a group is deleted.
      break;
  }
}
