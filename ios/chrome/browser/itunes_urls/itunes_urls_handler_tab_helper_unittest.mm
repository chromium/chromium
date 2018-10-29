// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/itunes_urls/itunes_urls_handler_tab_helper.h"

#import <Foundation/Foundation.h>

#include "base/test/metrics/histogram_tester.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/store_kit/store_kit_tab_helper.h"
#import "ios/chrome/test/fakes/fake_store_kit_launcher.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#import "ios/web/public/web_state/web_state_policy_decider.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kITunesURLsHandlingResultHistogram[] =
    "IOS.StoreKit.ITunesURLsHandlingResult";
}  // namespace

class ITunesUrlsHandlerTabHelperTest : public PlatformTest {
 protected:
  ITunesUrlsHandlerTabHelperTest()
      : fake_launcher_([[FakeStoreKitLauncher alloc] init]),
        chrome_browser_state_(TestChromeBrowserState::Builder().Build()) {
    web_state_.SetBrowserState(
        chrome_browser_state_->GetOriginalChromeBrowserState());
    StoreKitTabHelper::CreateForWebState(&web_state_);
    ITunesUrlsHandlerTabHelper::CreateForWebState(&web_state_);
    StoreKitTabHelper::FromWebState(&web_state_)->SetLauncher(fake_launcher_);
  }

  // Calls ShouldAllowRequest for a request with the given |url_string| and
  // returns true if storekit was launched.
  bool VerifyStoreKitLaunched(NSString* url_string, bool main_frame) {
    fake_launcher_.launchedProductID = nil;
    fake_launcher_.launchedProductParams = nil;
    web::WebStatePolicyDecider::RequestInfo request_info(
        ui::PageTransition::PAGE_TRANSITION_LINK, main_frame,
        /*has_user_gesture=*/false);
    bool request_allowed = web_state_.ShouldAllowRequest(
        [NSURLRequest requestWithURL:[NSURL URLWithString:url_string]],
        request_info);
    return !request_allowed && (fake_launcher_.launchedProductID != nil ||
                                fake_launcher_.launchedProductParams != nil);
  }

  web::TestWebThreadBundle thread_bundle_;
  FakeStoreKitLauncher* fake_launcher_;
  web::TestWebState web_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  base::HistogramTester histogram_tester_;
};

// Verifies that iTunes URLs are not handled when in off the record mode.
TEST_F(ITunesUrlsHandlerTabHelperTest, NoHandlingInOffTheRecordMode) {
  NSString* url = @"http://itunes.apple.com/us/app/app_name/id123";
  EXPECT_TRUE(VerifyStoreKitLaunched(url, /*main_frame=*/true));
  web_state_.SetBrowserState(
      chrome_browser_state_->GetOffTheRecordChromeBrowserState());
  EXPECT_FALSE(VerifyStoreKitLaunched(url, /*main_frame=*/true));
}

// Verifies that iTunes URLs are not handled when the request is from iframe.
TEST_F(ITunesUrlsHandlerTabHelperTest, NoHandlingInIframes) {
  EXPECT_TRUE(VerifyStoreKitLaunched(
      @"http://itunes.apple.com/us/app/app_name/id123", /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(
      @"http://itunes.apple.com/us/app/app_name/id123", /*main_frame=*/false));
  EXPECT_TRUE(VerifyStoreKitLaunched(
      @"http://itunes.apple.com/app/bar/id243?at=12312", /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(
      @"http://itunes.apple.com/app/bar/id243?at=12312", /*main_frame=*/false));
}

// Verifies that navigating to non iTunes product URLs, or not supported iTunes
// product type URLs does not launch storekit.
TEST_F(ITunesUrlsHandlerTabHelperTest, NonMatchingUrlsDoesntLaunchStoreKit) {
  EXPECT_FALSE(VerifyStoreKitLaunched(@"", /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(@"foobar", /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(@"foo://bar", /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(@"http://foo", /*main_frame=*/true));
  EXPECT_FALSE(
      VerifyStoreKitLaunched(@"http://foo?bar#qux", /*main_frame=*/true));
  EXPECT_FALSE(
      VerifyStoreKitLaunched(@"http://foo.bar/qux", /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(
      @"http://geo.itunes.apple.com/de/genre/apps/", /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(
      @"https://itunes.apple.com/us/tv-show/theshow/id1232",
      /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(
      @"http://itunes.apple.com/podcast/id12345", /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(
      @"itms-apps://itunes.apple.com/us/app/appname/id123",
      /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(
      @"http://itunes.apple.com/us/movie/testmovie/id12345",
      /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(
      @"http://itunes.apple.com/app-bundle/id12345", /*main_frame=*/true));
  histogram_tester_.ExpectTotalCount(kITunesURLsHandlingResultHistogram, 0);
}

// Verifies that navigating to URLs for a product hosted on iTunes AppStore
// with supported media type launches storekit.
TEST_F(ITunesUrlsHandlerTabHelperTest, MatchingUrlsLaunchesStoreKit) {
  EXPECT_TRUE(VerifyStoreKitLaunched(
      @"http://itunes.apple.com/us/app/app_name/id123", /*main_frame=*/true));
  NSString* product_id = @"id";
  NSString* af_tkn = @"at";
  NSDictionary* expected_params = @{product_id : @"123"};

  EXPECT_NSEQ(expected_params, fake_launcher_.launchedProductParams);
  EXPECT_TRUE(VerifyStoreKitLaunched(@"http://itunes.apple.com/app/bar/id123?",
                                     /*main_frame=*/true));
  EXPECT_NSEQ(expected_params, fake_launcher_.launchedProductParams);

  EXPECT_TRUE(VerifyStoreKitLaunched(
      @"http://foo.itunes.apple.com/app/test/id123?qux&baz#foo",
      /*main_frame=*/true));
  expected_params = @{product_id : @"123", @"qux" : @"", @"baz" : @""};
  EXPECT_NSEQ(expected_params, fake_launcher_.launchedProductParams);

  EXPECT_TRUE(VerifyStoreKitLaunched(
      @"http://itunes.apple.com/app/bar/id243?at=12312", /*main_frame=*/true));
  expected_params = @{product_id : @"243", af_tkn : @"12312"};
  EXPECT_NSEQ(expected_params, fake_launcher_.launchedProductParams);

  EXPECT_TRUE(VerifyStoreKitLaunched(
      @"http://itunes.apple.com/app/bar/idabc?at=213&ct=123",
      /*main_frame=*/true));
  expected_params = @{product_id : @"abc", af_tkn : @"213", @"ct" : @"123"};
  EXPECT_NSEQ(expected_params, fake_launcher_.launchedProductParams);

  EXPECT_TRUE(VerifyStoreKitLaunched(
      @"http://itunes.apple.com/de/app/bar/id123?at=2&uo=4#foo",
      /*main_frame=*/true));
  expected_params = @{product_id : @"123", af_tkn : @"2", @"uo" : @"4"};
  EXPECT_NSEQ(expected_params, fake_launcher_.launchedProductParams);

  histogram_tester_.ExpectUniqueSample(
      kITunesURLsHandlingResultHistogram,
      static_cast<base::HistogramBase::Sample>(
          ITunesUrlsStoreKitHandlingResult::kSingleAppUrlHandled),
      6);
  histogram_tester_.ExpectTotalCount(kITunesURLsHandlingResultHistogram, 6);
}
