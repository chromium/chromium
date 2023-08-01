// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_cache_web_state_list_observer.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

SnapshotCacheWebStateListObserver::SnapshotCacheWebStateListObserver(
    SnapshotCache* snapshot_cache)
    : snapshot_cache_(snapshot_cache) {
  DCHECK(snapshot_cache_);
}

SnapshotCacheWebStateListObserver::~SnapshotCacheWebStateListObserver() =
    default;

#pragma mark - WebStateListObserver

void SnapshotCacheWebStateListObserver::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  if (!status.active_web_state_change() ||
      change.type() == WebStateListChange::Type::kReplace) {
    return;
  }

  NSMutableSet<NSString*>* set = [NSMutableSet set];
  if (status.active_index > 0) {
    web::WebState* web_state =
        web_state_list->GetWebStateAt(status.active_index - 1);
    [set addObject:SnapshotTabHelper::FromWebState(web_state)->GetSnapshotID()];
  }

  if (status.active_index + 1 < web_state_list->count()) {
    web::WebState* web_state =
        web_state_list->GetWebStateAt(status.active_index + 1);
    [set addObject:SnapshotTabHelper::FromWebState(web_state)->GetSnapshotID()];
  }

  snapshot_cache_.pinnedSnapshotIDs = [set copy];
}
