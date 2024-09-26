// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/itunes_urls/model/itunes_urls_handler_tab_helper.h"

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/web_content_commands.h"
#import "ios/chrome/test/fakes/fake_web_content_handler.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class ITunesUrlsHandlerTabHelperTest : public PlatformTest {
 protected:
  ITunesUrlsHandlerTabHelperTest()
      : fake_handler_([[FakeWebContentHandler alloc] init]),
        profile_(TestProfileIOS::Builder().Build()) {
    web_state_.SetBrowserState(profile_->GetOriginalProfile());
    ITunesUrlsHandlerTabHelper::GetOrCreateForWebState(&web_state_)
        ->SetWebContentsHandler(fake_handler_);
  }

  // Calls ShouldAllowRequest for a request with the given `url_string` and
  // returns true if storekit was launched.
  bool VerifyStoreKitLaunched(NSString* url_string, bool main_frame) {
    const web::WebStatePolicyDecider::RequestInfo request_info(
        ui::PageTransition::PAGE_TRANSITION_LINK, main_frame,
        /*target_frame_is_cross_origin=*/false,
        /*target_window_is_cross_origin=*/false,
        /*has_user_gesture=*/false, /*user_tapped_recently=*/false);
    __block bool callback_called = false;
    __block web::WebStatePolicyDecider::PolicyDecision request_policy =
        web::WebStatePolicyDecider::PolicyDecision::Allow();
    auto callback =
        base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
          request_policy = decision;
          callback_called = true;
        });
    web_state_.ShouldAllowRequest(
        [NSURLRequest requestWithURL:[NSURL URLWithString:url_string]],
        request_info, std::move(callback));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(callback_called);
    return request_policy.ShouldCancelNavigation() &&
           fake_handler_.productParams != nil;
  }

  web::WebTaskEnvironment task_environment_;
  FakeWebContentHandler* fake_handler_;
  web::FakeWebState web_state_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Verifies that iTunes URLs are not handled when in off the record mode.
TEST_F(ITunesUrlsHandlerTabHelperTest, NoHandlingInOffTheRecordMode) {
  NSString* url = @"http://itunes.apple.com/us/app/app_name/id123";
  EXPECT_TRUE(VerifyStoreKitLaunched(url, /*main_frame=*/true));
  web_state_.SetBrowserState(profile_->GetOffTheRecordProfile());
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
  EXPECT_TRUE(VerifyStoreKitLaunched(
      @"http://apps.apple.com/app/bar/id243?at=12312", /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(
      @"http://apps.apple.com/app/bar/id243?at=12312", /*main_frame=*/false));
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
  EXPECT_FALSE(VerifyStoreKitLaunched(@"http://apps.apple.com/podcast/id12345",
                                      /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(
      @"itms-apps://itunes.apple.com/us/app/appname/id123",
      /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(
      @"http://test.itunes.apple.com/app/bar/id243?at=12312",
      /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(
      @"http://geo.apps.apple.com/app/bar/id243?at=12312",
      /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(
      @"http://itunes.apple.com/us/movie/testmovie/id12345",
      /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(
      @"http://itunes.apple.com/app-bundle/id12345", /*main_frame=*/true));
}

// Verifies that navigating to URLs for a product hosted on iTunes AppStore
// with supported media type launches storekit.
TEST_F(ITunesUrlsHandlerTabHelperTest, MatchingUrlsLaunchesStoreKit) {
  EXPECT_TRUE(VerifyStoreKitLaunched(
      @"http://apps.apple.com/us/app/app_name/id123", /*main_frame=*/true));
  NSString* product_id = @"id";
  NSString* af_tkn = @"at";
  NSDictionary* expected_params = @{product_id : @"123"};

  EXPECT_NSEQ(expected_params, fake_handler_.productParams);
  EXPECT_TRUE(VerifyStoreKitLaunched(@"http://itunes.apple.com/app/bar/id123?",
                                     /*main_frame=*/true));
  EXPECT_NSEQ(expected_params, fake_handler_.productParams);

  EXPECT_TRUE(VerifyStoreKitLaunched(@"http://apps.apple.com/app/id123",
                                     /*main_frame=*/true));
  expected_params = @{product_id : @"123"};
  EXPECT_NSEQ(expected_params, fake_handler_.productParams);

  EXPECT_TRUE(VerifyStoreKitLaunched(
      @"http://itunes.apple.com/app/test/id123?qux&baz#foo",
      /*main_frame=*/true));
  expected_params = @{product_id : @"123", @"qux" : @"", @"baz" : @""};
  EXPECT_NSEQ(expected_params, fake_handler_.productParams);

  EXPECT_TRUE(VerifyStoreKitLaunched(
      @"http://apps.apple.com/app/bar/id243?at=12312", /*main_frame=*/true));
  expected_params = @{product_id : @"243", af_tkn : @"12312"};
  EXPECT_NSEQ(expected_params, fake_handler_.productParams);

  EXPECT_TRUE(VerifyStoreKitLaunched(
      @"http://itunes.apple.com/app/bar/idabc?at=213&ct=123",
      /*main_frame=*/true));
  expected_params = @{product_id : @"abc", af_tkn : @"213", @"ct" : @"123"};
  EXPECT_NSEQ(expected_params, fake_handler_.productParams);

  EXPECT_TRUE(VerifyStoreKitLaunched(
      @"http://geo.itunes.apple.com/app/bar/id243?at=12312",
      /*main_frame=*/true));
  expected_params = @{product_id : @"243", af_tkn : @"12312"};
  EXPECT_NSEQ(expected_params, fake_handler_.productParams);

  EXPECT_TRUE(VerifyStoreKitLaunched(
      @"http://apps.apple.com/de/app/bar/id123?at=2&uo=4#foo",
      /*main_frame=*/true));
  expected_params = @{product_id : @"123", af_tkn : @"2", @"uo" : @"4"};
  EXPECT_NSEQ(expected_params, fake_handler_.productParams);
}
