// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_cache_web_state_list_observer.h"

#include "base/check.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

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

void SnapshotCacheWebStateListObserver::WebStateActivatedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int active_index,
    ActiveWebStateChangeReason reason) {
  if (reason == ActiveWebStateChangeReason::Replaced)
    return;

  NSMutableSet<NSString*>* set = [NSMutableSet set];
  if (active_index > 0) {
    web::WebState* web_state = web_state_list->GetWebStateAt(active_index - 1);
    [set addObject:TabIdTabHelper::FromWebState(web_state)->tab_id()];
  }

  if (active_index + 1 < web_state_list->count()) {
    web::WebState* web_state = web_state_list->GetWebStateAt(active_index + 1);
    [set addObject:TabIdTabHelper::FromWebState(web_state)->tab_id()];
  }

  snapshot_cache_.pinnedIDs = [set copy];
}
