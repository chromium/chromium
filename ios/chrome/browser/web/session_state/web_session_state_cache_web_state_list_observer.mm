// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/session_state/web_session_state_cache_web_state_list_observer.h"

#import "base/logging.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web/session_state/web_session_state_cache.h"
#import "ios/chrome/browser/web/session_state/web_session_state_tab_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WebSessionStateCacheWebStateListObserver::
    WebSessionStateCacheWebStateListObserver(
        WebSessionStateCache* web_session_state_cache)
    : web_session_state_cache_(web_session_state_cache) {
  DCHECK(web_session_state_cache_);
}

WebSessionStateCacheWebStateListObserver::
    ~WebSessionStateCacheWebStateListObserver() = default;

#pragma mark - WebStateListObserver

void WebSessionStateCacheWebStateListObserver::WebStateListChanged(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateSelection& selection) {
  switch (change.type()) {
    case WebStateListChange::Type::kSelectionOnly:
      // Do nothing when a WebState is selected and its status is updated.
      break;
    case WebStateListChange::Type::kDetach:
      // Do nothing when a WebState is detached.
      break;
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      WebSessionStateTabHelper::FromWebState(
          replace_change.inserted_web_state())
          ->SaveSessionState();
      break;
    }
    case WebStateListChange::Type::kInsert:
      // Do nothing when a new WebState is inserted.
      break;
  }
}

void WebSessionStateCacheWebStateListObserver::WillCloseWebStateAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool user_action) {
  [web_session_state_cache_ removeSessionStateDataForWebState:web_state];
}

void WebSessionStateCacheWebStateListObserver::WillBeginBatchOperation(
    WebStateList* web_state_list) {
  [web_session_state_cache_ setDelayRemove:TRUE];
}

void WebSessionStateCacheWebStateListObserver::BatchOperationEnded(
    WebStateList* web_state_list) {
  [web_session_state_cache_ setDelayRemove:FALSE];
}
