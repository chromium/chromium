// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_PROVIDERS_DISCOVER_FEED_TEST_DISCOVER_FEED_SERVICE_H_
#define IOS_CHROME_TEST_PROVIDERS_DISCOVER_FEED_TEST_DISCOVER_FEED_SERVICE_H_

#import "ios/chrome/browser/discover_feed/model/discover_feed_service.h"

@class FakeDiscoverFeedEligibilityHandler;

// Dummy DiscoverFeedService implementation used for tests.
class TestDiscoverFeedService final : public DiscoverFeedService {
 public:
  // DiscoverFeedService implementation:
  void CreateFeedModel() final;
  void SetIsShownOnStartSurface(bool shown_on_start_surface) final;
  FeedMetricsRecorder* GetFeedMetricsRecorder() final;
  UIViewController* NewDiscoverFeedViewControllerWithConfiguration(
      DiscoverFeedViewControllerConfiguration* configuration) final;
  void RemoveFeedViewController(UIViewController* feed_view_controller) final;
  void UpdateTheme() final;
  void UpdateFeedViewVisibilityState(
      UICollectionView* collection_view,
      BrowserViewVisibilityState current_state,
      BrowserViewVisibilityState previous_state) final;

  // DiscoverFeedRefresher implementation:
  void RefreshFeed(FeedRefreshTrigger trigger) final;
  void PerformBackgroundRefreshes(void (^completion)(BOOL)) final;
  void HandleBackgroundRefreshTaskExpiration() final;
  NSDate* GetEarliestBackgroundRefreshBeginDate() final;

  // TestDiscoverFeedService methods:
  void set_eligibility_handler(FakeDiscoverFeedEligibilityHandler* handler);
  FakeDiscoverFeedEligibilityHandler* get_eligibility_handler();
  UICollectionView* collection_view();
  BrowserViewVisibilityState visibility_state();

 private:
  UICollectionView* collection_view_;
  BrowserViewVisibilityState visibility_state_;
  FakeDiscoverFeedEligibilityHandler* eligibility_handler_;
};

#endif  // IOS_CHROME_TEST_PROVIDERS_DISCOVER_FEED_TEST_DISCOVER_FEED_SERVICE_H_
