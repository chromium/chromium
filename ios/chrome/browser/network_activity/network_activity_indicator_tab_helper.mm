// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/network_activity/network_activity_indicator_tab_helper.h"

#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/network_activity/network_activity_indicator_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
void NetworkActivityIndicatorTabHelper::CreateForWebState(
    web::WebState* web_state,
    NSString* tab_id) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(
        UserDataKey(), base::WrapUnique(new NetworkActivityIndicatorTabHelper(
                           web_state, tab_id)));
  }
}

NetworkActivityIndicatorTabHelper::NetworkActivityIndicatorTabHelper(
    web::WebState* web_state,
    NSString* tab_id)
    : network_activity_key_([tab_id copy]) {
  web_state->AddObserver(this);
}

NetworkActivityIndicatorTabHelper::~NetworkActivityIndicatorTabHelper() {
  Stop();
}

void NetworkActivityIndicatorTabHelper::DidStartLoading(
    web::WebState* web_state) {
  NetworkActivityIndicatorManager* shared_manager =
      [NetworkActivityIndicatorManager sharedInstance];
  // Verifies that there are not any network tasks associated with this instance
  // before starting another task, so that this method is idempotent.
  if (![shared_manager numNetworkTasksForGroup:network_activity_key_])
    [shared_manager startNetworkTaskForGroup:network_activity_key_];
}

void NetworkActivityIndicatorTabHelper::DidStopLoading(
    web::WebState* web_state) {
  Stop();
}

void NetworkActivityIndicatorTabHelper::WebStateDestroyed(
    web::WebState* web_state) {
  web_state->RemoveObserver(this);
}

void NetworkActivityIndicatorTabHelper::Stop() {
  NetworkActivityIndicatorManager* shared_manager =
      [NetworkActivityIndicatorManager sharedInstance];
  // Verifies that there is a network task associated with this instance
  // before stopping a task, so that this method is idempotent.
  if ([shared_manager numNetworkTasksForGroup:network_activity_key_])
    [shared_manager stopNetworkTaskForGroup:network_activity_key_];
}

WEB_STATE_USER_DATA_KEY_IMPL(NetworkActivityIndicatorTabHelper)
