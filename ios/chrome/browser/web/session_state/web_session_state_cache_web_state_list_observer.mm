// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/session_state/web_session_state_cache_web_state_list_observer.h"

#import "base/logging.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web/session_state/web_session_state_cache.h"
#import "ios/chrome/browser/web/session_state/web_session_state_tab_helper.h"

WebSessionStateCacheWebStateListObserver::
    WebSessionStateCacheWebStateListObserver(
        WebSessionStateCache* web_session_state_cache)
    : web_session_state_cache_(web_session_state_cache) {
  DCHECK(web_session_state_cache_);
}

WebSessionStateCacheWebStateListObserver::
    ~WebSessionStateCacheWebStateListObserver() = default;

#pragma mark - WebStateListObserver

void WebSessionStateCacheWebStateListObserver::WebStateListWillChange(
    WebStateList* web_state_list,
    const WebStateListChangeDetach& detach_change,
    const WebStateListStatus& status) {
  if (!detach_change.is_closing()) {
    return;
  }

  [web_session_state_cache_
      removeSessionStateDataForWebState:detach_change.detached_web_state()];
}

void WebSessionStateCacheWebStateListObserver::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
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

void WebSessionStateCacheWebStateListObserver::WillBeginBatchOperation(
    WebStateList* web_state_list) {
  [web_session_state_cache_ setDelayRemove:TRUE];
}

void WebSessionStateCacheWebStateListObserver::BatchOperationEnded(
    WebStateList* web_state_list) {
  [web_session_state_cache_ setDelayRemove:FALSE];
}
