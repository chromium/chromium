// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/discover_feed/discover_feed_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DiscoverFeedService::DiscoverFeedService() = default;

DiscoverFeedService::~DiscoverFeedService() = default;

// TODO(crbug.com/1314418): Remove this when downstream implementation is
// landed.
void DiscoverFeedService::SetIsShownOnStartSurface(
    bool shown_on_start_surface) {}

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

void DiscoverFeedService::BrowsingHistoryCleared() {}
