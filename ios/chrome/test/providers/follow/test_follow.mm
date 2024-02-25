// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/follow/follow_api.h"

#import "base/functional/callback.h"
#import "base/observer_list.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/follow/model/follow_service_observer.h"

namespace ios {
namespace provider {
namespace {

// FollowService implementation used in tests. Uses an array to store followed
// sites.
class TestFollowService final : public FollowService {
 public:
  TestFollowService();

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

 private:
  // Returns the index of the given url, if it is currently followed.
  NSUInteger FindFollowedWebSite(NSURL* url);
  // Returns a FollowedWebSite with the URL from `web_page_urls`.
  FollowedWebSite* WebPageURLsToFollowedWebSite(WebPageURLs* web_page_urls);

  // Notify the observers of various events.
  void NotifyWebSiteFollowed(FollowedWebSite* web_site);
  void NotifyWebSiteUnfollowed(FollowedWebSite* web_site);
  void NotifyFollowedChannelLoaded();

  base::ObserverList<FollowServiceObserver> observer_list_;
  NSMutableArray<FollowedWebSite*>* following_;
};

TestFollowService::TestFollowService() {
  following_ = [[NSMutableArray alloc] init];
}

NSUInteger TestFollowService::FindFollowedWebSite(NSURL* url) {
  return [following_ indexOfObjectPassingTest:^BOOL(
                         FollowedWebSite* site, NSUInteger idx, BOOL* stop) {
    return [site.webPageURL isEqual:url];
  }];
}

bool TestFollowService::IsWebSiteFollowed(WebPageURLs* web_page_urls) {
  return FindFollowedWebSite(web_page_urls.URL) != NSNotFound;
}

NSURL* TestFollowService::GetRecommendedSiteURL(WebPageURLs* web_page_urls) {
  return nil;
}

NSArray<FollowedWebSite*>* TestFollowService::GetFollowedWebSites() {
  return [following_ copy];
}

void TestFollowService::LoadFollowedWebSites() {
  NotifyFollowedChannelLoaded();
}

FollowedWebSite* TestFollowService::WebPageURLsToFollowedWebSite(
    WebPageURLs* web_page_urls) {
  FollowedWebSite* followed_web_site = [[FollowedWebSite alloc] init];
  followed_web_site.title = @"";
  followed_web_site.webPageURL = web_page_urls.URL;
  followed_web_site.RSSURL = [web_page_urls.RSSURLs firstObject];
  followed_web_site.state = FollowedWebSiteStateStateUnknown;
  return followed_web_site;
}

void TestFollowService::FollowWebSite(WebPageURLs* web_page_urls,
                                      FollowSource source,
                                      ResultCallback callback) {
  FollowedWebSite* site = WebPageURLsToFollowedWebSite(web_page_urls);
  if (IsWebSiteFollowed(web_page_urls)) {
    return;
  }

  [following_ addObject:site];
  NotifyWebSiteFollowed(site);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), FollowResult::Success, site));
}

void TestFollowService::UnfollowWebSite(WebPageURLs* web_page_urls,
                                        FollowSource source,
                                        ResultCallback callback) {
  FollowedWebSite* site = WebPageURLsToFollowedWebSite(web_page_urls);
  NSUInteger index = FindFollowedWebSite(web_page_urls.URL);
  if (index == NSNotFound) {
    return;
  }

  [following_ removeObjectAtIndex:index];
  NotifyWebSiteUnfollowed(site);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), FollowResult::Success, site));
}

void TestFollowService::AddObserver(FollowServiceObserver* observer) {
  observer_list_.AddObserver(observer);
}

void TestFollowService::RemoveObserver(FollowServiceObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void TestFollowService::NotifyWebSiteFollowed(FollowedWebSite* web_site) {
  for (FollowServiceObserver& observer : observer_list_) {
    observer.OnWebSiteFollowed(web_site);
  }
}

void TestFollowService::NotifyWebSiteUnfollowed(FollowedWebSite* web_site) {
  for (FollowServiceObserver& observer : observer_list_) {
    observer.OnWebSiteUnfollowed(web_site);
  }
}

void TestFollowService::NotifyFollowedChannelLoaded() {
  for (FollowServiceObserver& observer : observer_list_) {
    observer.OnFollowedWebSitesLoaded();
  }
}

}  // anonymous namespace

std::unique_ptr<FollowService> CreateFollowService(
    FollowConfiguration* configuration) {
  return std::make_unique<TestFollowService>();
}

}  // namespace provider
}  // namespace ios
