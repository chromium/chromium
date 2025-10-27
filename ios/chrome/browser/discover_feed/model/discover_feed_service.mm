// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/discover_feed/model/discover_feed_service.h"

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

void DiscoverFeedService::BrowsingHistoryCleared() {}
