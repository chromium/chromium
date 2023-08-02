// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/global_web_state_event_tracker.h"

#import <stddef.h>

#import "base/no_destructor.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {

GlobalWebStateEventTracker* GlobalWebStateEventTracker::GetInstance() {
  static base::NoDestructor<GlobalWebStateEventTracker> instance;
  return instance.get();
}

GlobalWebStateEventTracker::GlobalWebStateEventTracker() = default;
GlobalWebStateEventTracker::~GlobalWebStateEventTracker() = default;

void GlobalWebStateEventTracker::OnWebStateCreated(WebState* web_state) {
  scoped_observations_.AddObservation(web_state);
}

void GlobalWebStateEventTracker::AddObserver(GlobalWebStateObserver* observer) {
  observer_list_.AddObserver(observer);
}

void GlobalWebStateEventTracker::RemoveObserver(
    GlobalWebStateObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void GlobalWebStateEventTracker::DidStartNavigation(
    WebState* web_state,
    NavigationContext* navigation_context) {
  for (auto& observer : observer_list_)
    observer.WebStateDidStartNavigation(web_state, navigation_context);
}

void GlobalWebStateEventTracker::DidStartLoading(WebState* web_state) {
  for (auto& observer : observer_list_)
    observer.WebStateDidStartLoading(web_state);
}

void GlobalWebStateEventTracker::DidStopLoading(WebState* web_state) {
  for (auto& observer : observer_list_)
    observer.WebStateDidStopLoading(web_state);
}

void GlobalWebStateEventTracker::RenderProcessGone(WebState* web_state) {
  for (auto& observer : observer_list_)
    observer.RenderProcessGone(web_state);
}

void GlobalWebStateEventTracker::WebStateDestroyed(WebState* web_state) {
  for (auto& observer : observer_list_)
    observer.WebStateDestroyed(web_state);
  scoped_observations_.RemoveObservation(web_state);
}

}  // namespace web
