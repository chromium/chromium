// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/web_session_state_cache_web_state_list_observer.h"

#import "base/logging.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/sessions/model/web_session_state_cache.h"
#import "ios/chrome/browser/sessions/model/web_session_state_tab_helper.h"
#import "ios/web/public/browser_state.h"

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

  web::WebState* web_state = detach_change.detached_web_state();
  [web_session_state_cache_
      removeSessionStateDataForWebStateID:web_state->GetUniqueIdentifier()
                                incognito:web_state->GetBrowserState()
                                              ->IsOffTheRecord()];
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
    case WebStateListChange::Type::kReplace:
      if (WebSessionStateTabHelper* tab_helper =
              WebSessionStateTabHelper::FromWebState(
                  change.As<WebStateListChangeReplace>()
                      .inserted_web_state())) {
        tab_helper->SaveSessionState();
      }
      break;
    case WebStateListChange::Type::kInsert:
      // Do nothing when a new WebState is inserted.
      break;
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

void WebSessionStateCacheWebStateListObserver::WillBeginBatchOperation(
    WebStateList* web_state_list) {
  [web_session_state_cache_ setDelayRemove:TRUE];
}

void WebSessionStateCacheWebStateListObserver::BatchOperationEnded(
    WebStateList* web_state_list) {
  [web_session_state_cache_ setDelayRemove:FALSE];
}
