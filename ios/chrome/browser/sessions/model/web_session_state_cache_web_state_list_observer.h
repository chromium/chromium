// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_WEB_SESSION_STATE_CACHE_WEB_STATE_LIST_OBSERVER_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_WEB_SESSION_STATE_CACHE_WEB_STATE_LIST_OBSERVER_H_

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

@class WebSessionStateCache;

// Updates the WebSessionStateCache when the active tab changes or when
// batch operations occur.
class WebSessionStateCacheWebStateListObserver : public WebStateListObserver {
 public:
  explicit WebSessionStateCacheWebStateListObserver(
      WebSessionStateCache* web_session_state_cache);

  WebSessionStateCacheWebStateListObserver(
      const WebSessionStateCacheWebStateListObserver&) = delete;
  WebSessionStateCacheWebStateListObserver& operator=(
      const WebSessionStateCacheWebStateListObserver&) = delete;

  ~WebSessionStateCacheWebStateListObserver() override;

 private:
  // WebStateListObserver implementation.
  void WebStateListWillChange(WebStateList* web_state_list,
                              const WebStateListChangeDetach& detach_change,
                              const WebStateListStatus& status) override;
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;
  void WillBeginBatchOperation(WebStateList* web_state_list) override;
  void BatchOperationEnded(WebStateList* web_state_list) override;
  WebSessionStateCache* web_session_state_cache_;
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_WEB_SESSION_STATE_CACHE_WEB_STATE_LIST_OBSERVER_H_
