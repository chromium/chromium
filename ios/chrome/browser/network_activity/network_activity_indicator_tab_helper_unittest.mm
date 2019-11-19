// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/network_activity/network_activity_indicator_tab_helper.h"

#import <UIKit/UIKit.h>

#import "ios/web/public/test/fakes/test_web_state.h"
#import "ios/web/public/test/web_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::TestWebState;

using NetworkActivityIndicatorTabHelperTest = PlatformTest;

// Tests that the network activity for a single WebState correctly manages
// the state of the network activity indicator.
TEST_F(NetworkActivityIndicatorTabHelperTest, SingleWebStateActivity) {
  std::unique_ptr<TestWebState> web_state(new TestWebState());
  NetworkActivityIndicatorTabHelper::CreateForWebState(web_state.get(),
                                                       @"web_state1");

  EXPECT_FALSE(
      [[UIApplication sharedApplication] isNetworkActivityIndicatorVisible]);

  web_state->SetLoading(true);
  EXPECT_TRUE(
      [[UIApplication sharedApplication] isNetworkActivityIndicatorVisible]);

  web_state->SetLoading(false);
  EXPECT_FALSE(
      [[UIApplication sharedApplication] isNetworkActivityIndicatorVisible]);
}

// Tests that the network activity for multiple WebStates correctly manage
// the state of the network activity indicator.
TEST_F(NetworkActivityIndicatorTabHelperTest, MultipleWebStateActivity) {
  std::unique_ptr<TestWebState> web_state1(new TestWebState());
  NetworkActivityIndicatorTabHelper::CreateForWebState(web_state1.get(),
                                                       @"web_state1");

  std::unique_ptr<TestWebState> web_state2(new TestWebState());
  NetworkActivityIndicatorTabHelper::CreateForWebState(web_state2.get(),
                                                       @"web_state2");

  EXPECT_FALSE(
      [[UIApplication sharedApplication] isNetworkActivityIndicatorVisible]);

  web_state1->SetLoading(true);
  web_state2->SetLoading(true);
  EXPECT_TRUE(
      [[UIApplication sharedApplication] isNetworkActivityIndicatorVisible]);

  web_state1->SetLoading(false);
  EXPECT_TRUE(
      [[UIApplication sharedApplication] isNetworkActivityIndicatorVisible]);

  web_state2->SetLoading(false);
  EXPECT_FALSE(
      [[UIApplication sharedApplication] isNetworkActivityIndicatorVisible]);
}

// Tests that the network activity for a single WebState correctly stops when
// the WebState is deallocated.
TEST_F(NetworkActivityIndicatorTabHelperTest, WebStateDeallocated) {
  std::unique_ptr<TestWebState> web_state(new TestWebState());
  NetworkActivityIndicatorTabHelper::CreateForWebState(web_state.get(),
                                                       @"web_state1");

  EXPECT_FALSE(
      [[UIApplication sharedApplication] isNetworkActivityIndicatorVisible]);

  web_state->SetLoading(true);
  EXPECT_TRUE(
      [[UIApplication sharedApplication] isNetworkActivityIndicatorVisible]);

  web_state.reset(nil);
  EXPECT_FALSE(
      [[UIApplication sharedApplication] isNetworkActivityIndicatorVisible]);
}
