// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_CACHE_WEB_STATE_LIST_OBSERVER_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_CACHE_WEB_STATE_LIST_OBSERVER_H_

#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"

@class SnapshotCache;

// Updates the SnapshotCache when the active Tab changes.
class SnapshotCacheWebStateListObserver : public WebStateListObserver {
 public:
  explicit SnapshotCacheWebStateListObserver(SnapshotCache* snapshot_cache);

  SnapshotCacheWebStateListObserver(const SnapshotCacheWebStateListObserver&) =
      delete;
  SnapshotCacheWebStateListObserver& operator=(
      const SnapshotCacheWebStateListObserver&) = delete;

  ~SnapshotCacheWebStateListObserver() override;

 private:
  // WebStateListObserver implementation.
  void WebStateActivatedAt(WebStateList* web_state_list,
                           web::WebState* old_web_state,
                           web::WebState* new_web_state,
                           int active_index,
                           ActiveWebStateChangeReason reason) override;

  SnapshotCache* snapshot_cache_;
};

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_CACHE_WEB_STATE_LIST_OBSERVER_H_
