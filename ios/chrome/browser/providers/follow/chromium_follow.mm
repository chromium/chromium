// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/follow/follow_api.h"

#import "base/task/sequenced_task_runner.h"

namespace ios {
namespace provider {
namespace {

// Null FollowService used in Chromium.
class ChromiumFollowService final : public FollowService {
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

bool ChromiumFollowService::IsWebSiteFollowed(WebPageURLs* web_page_urls) {
  return false;
}

NSURL* ChromiumFollowService::GetRecommendedSiteURL(
    WebPageURLs* web_page_urls) {
  return nil;
}

NSArray<FollowedWebSite*>* ChromiumFollowService::GetFollowedWebSites() {
  return @[];
}

void ChromiumFollowService::LoadFollowedWebSites() {
  // Do nothing.
}

void ChromiumFollowService::FollowWebSite(WebPageURLs* web_page_urls,
                                          FollowSource source,
                                          ResultCallback callback) {
  FollowedWebSite* web_channel = [[FollowedWebSite alloc] init];
  web_channel.title = @"";
  web_channel.webPageURL = web_page_urls.URL;
  web_channel.RSSURL = [web_page_urls.RSSURLs firstObject];
  web_channel.state = FollowedWebSiteStateStateUnknown;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), FollowResult::Failure, web_channel));
}

void ChromiumFollowService::UnfollowWebSite(WebPageURLs* web_page_urls,
                                            FollowSource source,
                                            ResultCallback callback) {
  FollowedWebSite* web_channel = [[FollowedWebSite alloc] init];
  web_channel.title = @"";
  web_channel.webPageURL = web_page_urls.URL;
  web_channel.RSSURL = [web_page_urls.RSSURLs firstObject];
  web_channel.state = FollowedWebSiteStateStateUnknown;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), FollowResult::Failure, web_channel));
}

void ChromiumFollowService::AddObserver(FollowServiceObserver* observer) {
  // Do nothing.
}

void ChromiumFollowService::RemoveObserver(FollowServiceObserver* observer) {
  // Do nothing.
}

}  // anonymous namespace

std::unique_ptr<FollowService> CreateFollowService(
    FollowConfiguration* configuration) {
  return std::make_unique<ChromiumFollowService>();
}

}  // namespace provider
}  // namespace ios
