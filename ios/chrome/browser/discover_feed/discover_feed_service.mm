// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/discover_feed/discover_feed_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DiscoverFeedService::DiscoverFeedService() = default;

DiscoverFeedService::~DiscoverFeedService() = default;

void DiscoverFeedService::AddObserver(DiscoverFeedObserver* observer) {
  observer_list_.AddObserver(observer);
}

void DiscoverFeedService::RemoveObserver(DiscoverFeedObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void DiscoverFeedService::NotifyDiscoverFeedModelRecreated() {
  for (auto& observer : observer_list_) {
    observer.OnDiscoverFeedModelRecreated();
  }
}

// TODO(crbug.com/1277974): Remove this when downstream has landed.
void DiscoverFeedService::CreateFeedModel(
    FeedModelConfiguration* feed_model_config) {}
void DiscoverFeedService::SetFollowingFeedSortType(
    FollowingFeedSortType sort_type) {}
