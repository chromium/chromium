// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_api.h"

namespace ios {
namespace provider {
namespace {

// Dummy DiscoverFeedService implementation used for Chromium builds.
class ChromiumDiscoverFeedService final : public DiscoverFeedService {
 public:
  // DiscoverFeedService implementation:
  void CreateFeedModels() final {}
  void CreateFeedModel(FeedModelConfiguration* feed_model_config) final {}
  void ClearFeedModels() final {}
  void SetFollowingFeedSortType(FollowingFeedSortType sort_type) final {}
  FeedMetricsRecorder* GetFeedMetricsRecorder() final { return nil; }
  UIViewController* NewDiscoverFeedViewControllerWithConfiguration(
      DiscoverFeedViewControllerConfiguration* configuration) final {
    return nil;
  }
  UIViewController* NewFollowingFeedViewControllerWithConfiguration(
      DiscoverFeedViewControllerConfiguration* configuration) final {
    return nil;
  }
  void RemoveFeedViewController(UIViewController* feed_view_controller) final {}
  void UpdateTheme() final {}
  BOOL GetFollowingFeedHasUnseenContent() final { return NO; }
  void SetFollowingFeedContentSeen() final {}

  // DiscoverFeedRefresher implementation:
  void RefreshFeed(FeedRefreshTrigger trigger) final {}
  void PerformBackgroundRefreshes(void (^completion)(BOOL)) final {}
  void HandleBackgroundRefreshTaskExpiration() final {}
  NSDate* GetEarliestBackgroundRefreshBeginDate() final { return nil; }
};

}  // anonymous namespace

std::unique_ptr<DiscoverFeedService> CreateDiscoverFeedService(
    DiscoverFeedConfiguration* configuration) {
  return std::make_unique<ChromiumDiscoverFeedService>();
}

}  // namespace provider
}  // namespace ios
