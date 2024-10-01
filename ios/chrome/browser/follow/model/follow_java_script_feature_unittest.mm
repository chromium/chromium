// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/follow/model/follow_java_script_feature.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/web/model/chrome_web_client.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

class FollowJavaScriptFeatureTest : public PlatformTest {
 public:
  FollowJavaScriptFeatureTest()
      : web_client_(std::make_unique<ChromeWebClient>()) {
    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);
  }

  web::WebState* web_state() { return web_state_.get(); }

 protected:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
};

// Tests that the completion block is called with no results for an empty page.
TEST_F(FollowJavaScriptFeatureTest, NoRSSLink) {
  GURL example_url = GURL("http://example.com");
  web::test::LoadHtml(@"<html></html>", example_url, web_state());

  __block bool completion_called = false;

  FollowJavaScriptFeature::GetInstance()->GetWebPageURLs(
      web_state(), base::BindOnce(^(WebPageURLs* urls) {
        ASSERT_TRUE(urls);
        EXPECT_NSEQ(net::NSURLWithGURL(example_url), urls.URL);
        EXPECT_EQ(0ul, urls.RSSURLs.count);
        completion_called = true;
      }));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return completion_called;
  }));
}

// Tests that the completion block is called with a single alternate rss+xml
// link.
TEST_F(FollowJavaScriptFeatureTest, SingleAlternateRssXMLLink) {
  GURL example_url = GURL("http://example.com");
  web::test::LoadHtml(
      @"<html><head>"
       "<link rel=\"alternate\" type=\"application/rss+xml\" href=\"/rss\"/>"
       "</head></html>",
      example_url, web_state());

  __block bool completion_called = false;

  FollowJavaScriptFeature::GetInstance()->GetWebPageURLs(
      web_state(), base::BindOnce(^(WebPageURLs* urls) {
        ASSERT_TRUE(urls);
        EXPECT_NSEQ(net::NSURLWithGURL(example_url), urls.URL);
        ASSERT_EQ(1ul, urls.RSSURLs.count);
        EXPECT_NSEQ(@"http://example.com/rss",
                    [urls.RSSURLs[0] absoluteString]);
        completion_called = true;
      }));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return completion_called;
  }));
}

// Tests that the completion block is called with a single alternate rss+atom
// link.
TEST_F(FollowJavaScriptFeatureTest, SingleAlternateRssAtomLink) {
  GURL example_url = GURL("http://example.com");
  web::test::LoadHtml(
      @"<html><head>"
       "<link rel=\"alternate\" type=\"application/rss+atom\" href=\"/rss\"/>"
       "</head></html>",
      example_url, web_state());

  __block bool completion_called = false;

  FollowJavaScriptFeature::GetInstance()->GetWebPageURLs(
      web_state(), base::BindOnce(^(WebPageURLs* urls) {
        ASSERT_TRUE(urls);
        EXPECT_NSEQ(net::NSURLWithGURL(example_url), urls.URL);
        ASSERT_EQ(1ul, urls.RSSURLs.count);
        EXPECT_NSEQ(@"http://example.com/rss",
                    [urls.RSSURLs[0] absoluteString]);
        completion_called = true;
      }));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return completion_called;
  }));
}

// Tests that the completion block is called with a single alternate atom+xml
// link.
TEST_F(FollowJavaScriptFeatureTest, SingleAlternateAtomXMLLink) {
  GURL example_url = GURL("http://example.com");
  web::test::LoadHtml(
      @"<html><head>"
       "<link rel=\"alternate\" type=\"application/atom+xml\" href=\"/rss\"/>"
       "</head></html>",
      example_url, web_state());

  __block bool completion_called = false;

  FollowJavaScriptFeature::GetInstance()->GetWebPageURLs(
      web_state(), base::BindOnce(^(WebPageURLs* urls) {
        ASSERT_TRUE(urls);
        EXPECT_NSEQ(net::NSURLWithGURL(example_url), urls.URL);
        ASSERT_EQ(1ul, urls.RSSURLs.count);
        EXPECT_NSEQ(@"http://example.com/rss",
                    [urls.RSSURLs[0] absoluteString]);
        completion_called = true;
      }));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return completion_called;
  }));
}

// Tests that the completion block is called with a single service.feed rss+xml
// link.
TEST_F(FollowJavaScriptFeatureTest, SingleServiceFeedRssXMLLink) {
  GURL example_url = GURL("http://example.com");
  web::test::LoadHtml(
      @"<html><head>"
       "<link rel=\"service.feed\" type=\"application/rss+xml\" href=\"/rss\"/>"
       "</head></html>",
      example_url, web_state());

  __block bool completion_called = false;

  FollowJavaScriptFeature::GetInstance()->GetWebPageURLs(
      web_state(), base::BindOnce(^(WebPageURLs* urls) {
        ASSERT_TRUE(urls);
        EXPECT_NSEQ(net::NSURLWithGURL(example_url), urls.URL);
        ASSERT_EQ(1ul, urls.RSSURLs.count);
        EXPECT_NSEQ(@"http://example.com/rss",
                    [urls.RSSURLs[0] absoluteString]);
        completion_called = true;
      }));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return completion_called;
  }));
}

// Tests that the completion block is called with a single service.feed rss+atom
// link.
TEST_F(FollowJavaScriptFeatureTest, SingleServiceFeedRssAtomLink) {
  GURL example_url = GURL("http://example.com");
  web::test::LoadHtml(@"<html><head>"
                       "<link rel=\"service.feed\" "
                       "type=\"application/rss+atom\" href=\"/rss\"/>"
                       "</head></html>",
                      example_url, web_state());

  __block bool completion_called = false;

  FollowJavaScriptFeature::GetInstance()->GetWebPageURLs(
      web_state(), base::BindOnce(^(WebPageURLs* urls) {
        ASSERT_TRUE(urls);
        EXPECT_NSEQ(net::NSURLWithGURL(example_url), urls.URL);
        ASSERT_EQ(1ul, urls.RSSURLs.count);
        EXPECT_NSEQ(@"http://example.com/rss",
                    [urls.RSSURLs[0] absoluteString]);
        completion_called = true;
      }));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return completion_called;
  }));
}

// Tests that the completion block is called with a single service.feed atom+xml
// link.
TEST_F(FollowJavaScriptFeatureTest, SingleServiceFeedAtomXMLLink) {
  GURL example_url = GURL("http://example.com");
  web::test::LoadHtml(@"<html><head>"
                       "<link rel=\"service.feed\" "
                       "type=\"application/atom+xml\" href=\"/rss\"/>"
                       "</head></html>",
                      example_url, web_state());

  __block bool completion_called = false;

  FollowJavaScriptFeature::GetInstance()->GetWebPageURLs(
      web_state(), base::BindOnce(^(WebPageURLs* urls) {
        ASSERT_TRUE(urls);
        EXPECT_NSEQ(net::NSURLWithGURL(example_url), urls.URL);
        ASSERT_EQ(1ul, urls.RSSURLs.count);
        EXPECT_NSEQ(@"http://example.com/rss",
                    [urls.RSSURLs[0] absoluteString]);
        completion_called = true;
      }));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return completion_called;
  }));
}

// Tests that the completion block is called with multiple rss links.
TEST_F(FollowJavaScriptFeatureTest, MultipleRSSLinks) {
  GURL example_url = GURL("http://example.com");
  web::test::LoadHtml(
      @"<html><head>"
       "<link rel=\"alternate\" type=\"application/rss+xml\" href=\"/rss\"/>"
       "<link rel=\"alternate\" type=\"application/rss+xml\" href=\"/rss2\"/>"
       "<link rel=\"alternate\" type=\"application/rss+xml\" href=\"/rss3\"/>"
       "</head></html>",
      example_url, web_state());

  __block bool completion_called = false;

  FollowJavaScriptFeature::GetInstance()->GetWebPageURLs(
      web_state(), base::BindOnce(^(WebPageURLs* urls) {
        ASSERT_TRUE(urls);
        EXPECT_NSEQ(net::NSURLWithGURL(example_url), urls.URL);
        ASSERT_EQ(3ul, urls.RSSURLs.count);
        EXPECT_NSEQ(@"http://example.com/rss",
                    [urls.RSSURLs[0] absoluteString]);
        EXPECT_NSEQ(@"http://example.com/rss2",
                    [urls.RSSURLs[1] absoluteString]);
        EXPECT_NSEQ(@"http://example.com/rss3",
                    [urls.RSSURLs[2] absoluteString]);
        completion_called = true;
      }));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return completion_called;
  }));
}
