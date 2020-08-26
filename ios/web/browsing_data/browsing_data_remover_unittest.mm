// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/browsing_data/browsing_data_remover.h"

#import <WebKit/WebKit.h>

#include "base/bind.h"
#import "base/test/ios/wait_util.h"
#include "base/test/task_environment.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForCookiesTimeout;

namespace {

// Adds cookies to the default data store. Returns whether it succeed to add
// cookies. Declare the function to have the "WARN_UNUSED_RESULT".
bool AddCookie() WARN_UNUSED_RESULT;
bool AddCookie() {
  WKWebsiteDataStore* data_store = [WKWebsiteDataStore defaultDataStore];
  NSHTTPCookie* cookie = [NSHTTPCookie cookieWithProperties:@{
    NSHTTPCookiePath : @"path",
    NSHTTPCookieName : @"cookieName",
    NSHTTPCookieValue : @"value",
    NSHTTPCookieOriginURL : @"http://chromium.org"
  }];

  // This is needed to forces the DataStore to be created, otherwise the cookie
  // isn't set in some cases.
  __block bool cookie_set = false;
  NSSet* data_types = [NSSet setWithObject:WKWebsiteDataTypeCookies];
  [[WKWebsiteDataStore defaultDataStore]
      fetchDataRecordsOfTypes:data_types
            completionHandler:^(NSArray<WKWebsiteDataRecord*>* records){
            }];

  [data_store.httpCookieStore setCookie:cookie
                      completionHandler:^{
                        cookie_set = true;
                      }];

  return WaitUntilConditionOrTimeout(kWaitForCookiesTimeout, ^bool {
    return cookie_set;
  });
}

// Checks that the cookies data store has cookies or not, depending on
// |should_have_cookies|. Declare the function to have the "WARN_UNUSED_RESULT".
bool HasCookies(bool should_have_cookies) WARN_UNUSED_RESULT;
bool HasCookies(bool should_have_cookies) {
  __block bool has_cookies = false;
  __block bool completion_called = false;

  // This is needed to forces the DataStore to be created, otherwise the cookie
  // isn't fetched in some cases.
  NSSet* data_types = [NSSet setWithObject:WKWebsiteDataTypeCookies];
  [[WKWebsiteDataStore defaultDataStore]
      fetchDataRecordsOfTypes:data_types
            completionHandler:^(NSArray<WKWebsiteDataRecord*>* records){
            }];

  [[WKWebsiteDataStore defaultDataStore].httpCookieStore
      getAllCookies:^(NSArray<NSHTTPCookie*>* cookies) {
        has_cookies = cookies.count > 0;
        completion_called = true;
      }];

  bool completed = WaitUntilConditionOrTimeout(kWaitForCookiesTimeout, ^bool {
    return completion_called;
  });

  return completed && (has_cookies == should_have_cookies);
}

// Removes the |types| from the data remover associated with |browser_state|.
// Returns whether the completion block of the clear browsing data has been
// called. Declare the function to have the "WARN_UNUSED_RESULT".
bool RemoveCookies(web::BrowserState* browser_state,
                   web::ClearBrowsingDataMask types) WARN_UNUSED_RESULT;
bool RemoveCookies(web::BrowserState* browser_state,
                   web::ClearBrowsingDataMask types) {
  web::BrowsingDataRemover* remover =
      web::BrowsingDataRemover::FromBrowserState(browser_state);
  __block bool closure_called = false;
  base::OnceClosure closure = base::BindOnce(^{
    closure_called = true;
  });
  remover->ClearBrowsingData(
      types, base::Time::Now() - base::TimeDelta::FromMinutes(1),
      std::move(closure));

  return WaitUntilConditionOrTimeout(kWaitForCookiesTimeout, ^bool {
    return closure_called;
  });
}

}  // namespace

namespace web {

class BrowsingDataRemoverTest : public PlatformTest {
 protected:
  BrowsingDataRemover* GetRemover() {
    return BrowsingDataRemover::FromBrowserState(&browser_state_);
  }

  base::test::TaskEnvironment task_environment_;
  TestBrowserState browser_state_;
};

TEST_F(BrowsingDataRemoverTest, DifferentRemoverForDifferentBrowserState) {
  TestBrowserState browser_state_1;
  TestBrowserState browser_state_2;

  BrowsingDataRemover* remover_1 =
      BrowsingDataRemover::FromBrowserState(&browser_state_1);
  BrowsingDataRemover* remover_2 =
      BrowsingDataRemover::FromBrowserState(&browser_state_2);

  EXPECT_NE(remover_1, remover_2);

  BrowsingDataRemover* remover_1_again =
      BrowsingDataRemover::FromBrowserState(&browser_state_1);
  EXPECT_EQ(remover_1_again, remover_1);
}

// Tests that removing the cookies remove them from the cookie store.
TEST_F(BrowsingDataRemoverTest, RemoveCookie) {
  ASSERT_TRUE(AddCookie());
  ASSERT_TRUE(HasCookies(true));

  // Remove the cookies.
  EXPECT_TRUE(
      RemoveCookies(&browser_state_, ClearBrowsingDataMask::kRemoveCookies));

  EXPECT_TRUE(HasCookies(false));
}

// Tests that removing the anything but cookies leave the cookies in the store.
TEST_F(BrowsingDataRemoverTest, RemoveNothing) {
  ASSERT_TRUE(AddCookie());
  ASSERT_TRUE(HasCookies(true));

  // Remove other things than cookies.
  EXPECT_TRUE(RemoveCookies(&browser_state_,
                            ClearBrowsingDataMask::kRemoveAppCache |
                                ClearBrowsingDataMask::kRemoveIndexedDB));

  EXPECT_TRUE(HasCookies(true));
}

// Tests that removing nothing still call the closure.
TEST_F(BrowsingDataRemoverTest, KeepCookie) {
  ASSERT_TRUE(AddCookie());

  // Don't remove anything but check that the closure is still called.
  EXPECT_TRUE(
      RemoveCookies(&browser_state_, ClearBrowsingDataMask::kRemoveNothing));
}

}  // namespace web
