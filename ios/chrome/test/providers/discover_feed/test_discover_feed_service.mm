// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/providers/discover_feed/test_discover_feed_service.h"

void TestDiscoverFeedService::CreateFeedModels() {}

void TestDiscoverFeedService::CreateFeedModel(
    FeedModelConfiguration* feed_model_config) {}

void TestDiscoverFeedService::ClearFeedModels() {}

void TestDiscoverFeedService::SetFollowingFeedSortType(
    FollowingFeedSortType sort_type) {}

void TestDiscoverFeedService::SetIsShownOnStartSurface(
    bool shown_on_start_surface) {}

FeedMetricsRecorder* TestDiscoverFeedService::GetFeedMetricsRecorder() {
  return nil;
}

UIViewController*
TestDiscoverFeedService::NewDiscoverFeedViewControllerWithConfiguration(
    DiscoverFeedViewControllerConfiguration* configuration) {
  return nil;
}

UIViewController*
TestDiscoverFeedService::NewFollowingFeedViewControllerWithConfiguration(
    DiscoverFeedViewControllerConfiguration* configuration) {
  return nil;
}

void TestDiscoverFeedService::RemoveFeedViewController(
    UIViewController* feed_view_controller) {}

void TestDiscoverFeedService::UpdateTheme() {}

BOOL TestDiscoverFeedService::GetFollowingFeedHasUnseenContent() {
  return NO;
}

void TestDiscoverFeedService::SetFollowingFeedContentSeen() {}

void TestDiscoverFeedService::UpdateFeedViewVisibilityState(
    UICollectionView* collection_view,
    BrowserViewVisibilityState current_state,
    BrowserViewVisibilityState previous_state) {
  collection_view_ = collection_view;
  visibility_state_ = current_state;
}

void TestDiscoverFeedService::RefreshFeed(FeedRefreshTrigger trigger) {}

void TestDiscoverFeedService::PerformBackgroundRefreshes(
    void (^completion)(BOOL)) {}

void TestDiscoverFeedService::HandleBackgroundRefreshTaskExpiration() {}

NSDate* TestDiscoverFeedService::GetEarliestBackgroundRefreshBeginDate() {
  return nil;
}

void TestDiscoverFeedService::set_eligibility_handler(
    FakeDiscoverFeedEligibilityHandler* handler) {
  eligibility_handler_ = handler;
}

FakeDiscoverFeedEligibilityHandler*
TestDiscoverFeedService::get_eligibility_handler() {
  return eligibility_handler_;
}

UICollectionView* TestDiscoverFeedService::collection_view() {
  return collection_view_;
}

BrowserViewVisibilityState TestDiscoverFeedService::visibility_state() {
  return visibility_state_;
}
