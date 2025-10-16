// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/discover_feed/model/discover_feed_service.h"

DiscoverFeedService::DiscoverFeedService() = default;

DiscoverFeedService::~DiscoverFeedService() = default;

// TODO(crbug.com/448683013): Remove this when downstream implementation is
// removed.
void DiscoverFeedService::SetFollowingFeedSortType(
    FollowingFeedSortType sort_type) {}
UIViewController*
DiscoverFeedService::NewFollowingFeedViewControllerWithConfiguration(
    DiscoverFeedViewControllerConfiguration* configuration) {
  return nil;
}
BOOL DiscoverFeedService::GetFollowingFeedHasUnseenContent() {
  return NO;
}
void DiscoverFeedService::SetFollowingFeedContentSeen() {}
void DiscoverFeedService::CreateFeedModels() {}
void DiscoverFeedService::CreateFeedModel(
    FeedModelConfiguration* feed_model_config) {}
void DiscoverFeedService::ClearFeedModels() {}

void DiscoverFeedService::AddObserver(DiscoverFeedObserver* observer) {
  observer_list_.AddObserver(observer);
}

void DiscoverFeedService::RemoveObserver(DiscoverFeedObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void DiscoverFeedService::CreateFeedModel() {}

void DiscoverFeedService::NotifyDiscoverFeedModelRecreated() {
  for (auto& observer : observer_list_) {
    observer.OnDiscoverFeedModelRecreated();
  }
}

void DiscoverFeedService::BrowsingHistoryCleared() {}
