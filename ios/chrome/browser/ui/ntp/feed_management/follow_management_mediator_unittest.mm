// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_management/follow_management_mediator.h"

#import "components/favicon/core/large_icon_service_impl.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/follow/follow_browser_agent.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/follow/followed_web_channel.h"
#import "ios/chrome/browser/ui/ntp/feed_management/follow_management_ui_updater.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/skia/include/core/SkBitmap.h"
#import "ui/gfx/codec/png_codec.h"

namespace {

// An example URL of a website to follow.
NSString* const kExampleURL = @"https://example.org/";

// FakeLargeIconService mimics a LargeIconService that returns a LargeIconResult
// with a test favicon image.
class FakeLargeIconService : public favicon::LargeIconServiceImpl {
 public:
  FakeLargeIconService()
      : favicon::LargeIconServiceImpl(
            /*favicon_service=*/nullptr,
            /*image_fetcher=*/nullptr,
            /*desired_size_in_dip_for_server_requests=*/0,
            /*icon_type_for_server_requests=*/
            favicon_base::IconType::kTouchIcon,
            /*google_server_client_param=*/"test_chrome") {}

  // Calls callback with LargeIconResult with valid bitmap.
  base::CancelableTaskTracker::TaskId
  GetLargeIconRawBitmapOrFallbackStyleForPageUrl(
      const GURL& page_url,
      int min_source_size_in_pixel,
      int desired_size_in_pixel,
      favicon_base::LargeIconCallback callback,
      base::CancelableTaskTracker* tracker) override {
    favicon_base::FaviconRawBitmapResult bitmapResult;
    bitmapResult.expired = false;

    // Create bitmap.
    scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes());
    SkBitmap bitmap;
    bitmap.allocN32Pixels(30, 30);
    gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &data->data());
    bitmapResult.bitmap_data = data;

    favicon_base::LargeIconResult result(bitmapResult);
    std::move(callback).Run(result);

    return 1;
  }
};

// Test fixture for FollowManagementMediator testing.
class FollowManagementMediatorTest : public PlatformTest {
 protected:
  FollowManagementMediatorTest() {
    chrome_browser_state_ = BuildChromeBrowserState();
    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());
    FollowBrowserAgent::CreateForBrowser(browser_.get());
    follow_browser_agent_ = FollowBrowserAgent::FromBrowser(browser_.get());
    favicon_loader_ = std::make_unique<FaviconLoader>(&large_icon_service_);
    mediator_ = [[FollowManagementMediator alloc]
        initWithBrowserAgent:follow_browser_agent_
               faviconLoader:favicon_loader_.get()];
  }

  ~FollowManagementMediatorTest() override { [mediator_ detach]; }

  // Builds a new TestChromeBrowserState.
  std::unique_ptr<TestChromeBrowserState> BuildChromeBrowserState() {
    TestChromeBrowserState::Builder test_cbs_builder;
    return test_cbs_builder.Build();
  }

  // Returns a WebPageURLs object with an example URL.
  WebPageURLs* ExampleWebPage() {
    NSURL* example_url = [NSURL URLWithString:kExampleURL];
    return [[WebPageURLs alloc] initWithURL:example_url
                                    RSSURLs:@[ example_url ]];
  }

  // Returns a FollowedWebChannel object with an example URL.
  FollowedWebChannel* ExampleWebChannel() {
    FollowedWebChannel* channel = [[FollowedWebChannel alloc] init];
    channel.title = @"";
    channel.webPageURL =
        [[CrURL alloc] initWithNSURL:[NSURL URLWithString:kExampleURL]];
    channel.rssURL = channel.webPageURL;
    return channel;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  FollowBrowserAgent* follow_browser_agent_;
  FakeLargeIconService large_icon_service_;
  std::unique_ptr<FaviconLoader> favicon_loader_;
  FollowManagementMediator* mediator_;
};

// Tests that the list of followed web sites can be loaded.
TEST_F(FollowManagementMediatorTest, TestLoad) {
  id mock_updater = OCMProtocolMock(@protocol(FollowManagementUIUpdater));
  OCMExpect([mock_updater updateFollowedWebSites]);
  [mediator_ addFollowManagementUIUpdater:mock_updater];

  [mediator_ loadFollowedWebSites];

  EXPECT_OCMOCK_VERIFY(mock_updater);
}

// Tests that a web site can be followed.
TEST_F(FollowManagementMediatorTest, TestFollow) {
  id mock_updater = OCMProtocolMock(@protocol(FollowManagementUIUpdater));
  OCMExpect([mock_updater addFollowedWebChannel:[OCMArg any]]);
  [mediator_ addFollowManagementUIUpdater:mock_updater];

  follow_browser_agent_->FollowWebSite(ExampleWebPage(),
                                       FollowSource::Management);

  EXPECT_OCMOCK_VERIFY(mock_updater);

  NSArray<FollowedWebChannel*>* followed = [mediator_ followedWebChannels];
  EXPECT_EQ(followed.count, 1u);
  EXPECT_NSEQ(followed[0].webPageURL.nsurl, [NSURL URLWithString:kExampleURL]);
}

// Tests that a web site can be unfollowed.
TEST_F(FollowManagementMediatorTest, TestUnfollow) {
  follow_browser_agent_->FollowWebSite(ExampleWebPage(),
                                       FollowSource::Management);
  EXPECT_EQ(1u, [mediator_ followedWebChannels].count);
  id mock_updater = OCMProtocolMock(@protocol(FollowManagementUIUpdater));
  OCMExpect([mock_updater removeFollowedWebChannel:[OCMArg any]]);
  [mediator_ addFollowManagementUIUpdater:mock_updater];

  [mediator_ unfollowFollowedWebChannel:ExampleWebChannel()];
  follow_browser_agent_->UnfollowWebSite(ExampleWebPage(),
                                         FollowSource::Management);

  EXPECT_OCMOCK_VERIFY(mock_updater);
  EXPECT_EQ(0u, [mediator_ followedWebChannels].count);
}

// Tests that a FollowManagementUIUpdater can be added and removed.
TEST_F(FollowManagementMediatorTest, TestFollowManagementUIUpdater) {
  id mock_updater = OCMStrictProtocolMock(@protocol(FollowManagementUIUpdater));
  OCMExpect([mock_updater addFollowedWebChannel:[OCMArg any]]);
  [mediator_ addFollowManagementUIUpdater:mock_updater];

  follow_browser_agent_->FollowWebSite(ExampleWebPage(),
                                       FollowSource::Management);
  EXPECT_OCMOCK_VERIFY(mock_updater);

  [mediator_ removeFollowManagementUIUpdater:mock_updater];

  follow_browser_agent_->FollowWebSite(ExampleWebPage(),
                                       FollowSource::Management);
  EXPECT_OCMOCK_VERIFY(mock_updater);
}

// Tests that the mediator acts as a TableViewFaviconDataSource.
TEST_F(FollowManagementMediatorTest, TestTableViewFaviconDataSource) {
  __block int block_call_count = 0;
  CrURL* url = [[CrURL alloc] initWithNSURL:[NSURL URLWithString:kExampleURL]];
  [mediator_ faviconForPageURL:url
                    completion:^(FaviconAttributes* attrs) {
                      EXPECT_NE(attrs.faviconImage, nil);
                      block_call_count++;
                    }];

  EXPECT_GT(block_call_count, 0);
}

}  // namespace
