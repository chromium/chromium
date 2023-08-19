// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/follow/follow_api.h"

#import "base/functional/callback.h"

namespace ios {
namespace provider {
namespace {

// FollowService implementation used in tests.
class TestFollowService final : public FollowService {
 public:
  // FollowService implementation.
  bool IsWebSiteFollowed(WebPageURLs* web_page_urls) final;
  NSURL* GetRecommendedSiteURL(WebPageURLs* web_page_urls) final;
  NSArray<FollowedWebSite*>* GetFollowedWebSites() final;
  void LoadFollowedWebSites() final;
  void FollowWebSite(WebPageURLs* web_page_urls,
                     FollowSource source,
                     ResultCallback callback) final;
  void UnfollowWebSite(WebPageURLs* web_page_urls,
                       FollowSource source,
                       ResultCallback callback) final;
  void AddObserver(FollowServiceObserver* observer) final;
  void RemoveObserver(FollowServiceObserver* observer) final;
};

bool TestFollowService::IsWebSiteFollowed(WebPageURLs* web_page_urls) {
  return false;
}

NSURL* TestFollowService::GetRecommendedSiteURL(WebPageURLs* web_page_urls) {
  return nil;
}

NSArray<FollowedWebSite*>* TestFollowService::GetFollowedWebSites() {
  return @[];
}

void TestFollowService::LoadFollowedWebSites() {
  // Do nothing.
}

void TestFollowService::FollowWebSite(WebPageURLs* web_page_urls,
                                      FollowSource source,
                                      ResultCallback callback) {
  // Do nothing.
}

void TestFollowService::UnfollowWebSite(WebPageURLs* web_page_urls,
                                        FollowSource source,
                                        ResultCallback callback) {
  // Do nothing.
}

void TestFollowService::AddObserver(FollowServiceObserver* observer) {
  // Do nothing.
}

void TestFollowService::RemoveObserver(FollowServiceObserver* observer) {
  // Do nothing.
}

}  // anonymous namespace

std::unique_ptr<FollowService> CreateFollowService(
    FollowConfiguration* configuration) {
  return std::make_unique<TestFollowService>();
}

}  // namespace provider
}  // namespace ios
