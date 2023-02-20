// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace provider {
namespace {

// Dummy DiscoverFeedService implementation used for tests.
class TestDiscoverFeedService final : public DiscoverFeedService {
 public:
  // DiscoverFeedService implementation:
  void CreateFeedModels() final {}
  void CreateFeedModel(FeedModelConfiguration* feed_model_config) final {}
  void ClearFeedModels() final {}
  void SetFollowingFeedSortType(FollowingFeedSortType sort_type) final {}
  void SetIsShownOnStartSurface(bool shown_on_start_surface) final {}
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
  void RefreshFeedIfNeeded() final {}
  void RefreshFeed(bool feed_visible) final {}
  void PerformBackgroundRefreshes(void (^completion)(BOOL)) final {}
  void HandleBackgroundRefreshTaskExpiration() final {}
  NSDate* GetEarliestBackgroundRefreshBeginDate() final { return nil; }
  BOOL GetFollowingFeedHasUnseenContent() final { return NO; }
  void SetFollowingFeedContentSeen() final {}
};

}  // anonymous namespace

std::unique_ptr<DiscoverFeedService> CreateDiscoverFeedService(
    DiscoverFeedConfiguration* configuration) {
  return std::make_unique<TestDiscoverFeedService>();
}

}  // namespace provider
}  // namespace ios
