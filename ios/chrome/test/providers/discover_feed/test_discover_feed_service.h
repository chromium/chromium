// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_PROVIDERS_DISCOVER_FEED_TEST_DISCOVER_FEED_SERVICE_H_
#define IOS_CHROME_TEST_PROVIDERS_DISCOVER_FEED_TEST_DISCOVER_FEED_SERVICE_H_

#import "ios/chrome/browser/discover_feed/model/discover_feed_service.h"

// Dummy DiscoverFeedService implementation used for tests.
class TestDiscoverFeedService final : public DiscoverFeedService {
 public:
  // DiscoverFeedService implementation:
  void CreateFeedModels() final;
  void CreateFeedModel(FeedModelConfiguration* feed_model_config) final;
  void ClearFeedModels() final;
  void SetFollowingFeedSortType(FollowingFeedSortType sort_type) final;
  void SetIsShownOnStartSurface(bool shown_on_start_surface) final;
  FeedMetricsRecorder* GetFeedMetricsRecorder() final;
  UIViewController* NewDiscoverFeedViewControllerWithConfiguration(
      DiscoverFeedViewControllerConfiguration* configuration) final;
  UIViewController* NewFollowingFeedViewControllerWithConfiguration(
      DiscoverFeedViewControllerConfiguration* configuration) final;
  void RemoveFeedViewController(UIViewController* feed_view_controller) final;
  void UpdateTheme() final;
  BOOL GetFollowingFeedHasUnseenContent() final;
  void SetFollowingFeedContentSeen() final;
  // DiscoverFeedRefresher implementation:
  void RefreshFeed(FeedRefreshTrigger trigger) final;
  void PerformBackgroundRefreshes(void (^completion)(BOOL)) final;
  void HandleBackgroundRefreshTaskExpiration() final;
  NSDate* GetEarliestBackgroundRefreshBeginDate() final;
};

#endif  // IOS_CHROME_TEST_PROVIDERS_DISCOVER_FEED_TEST_DISCOVER_FEED_SERVICE_H_
