// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/cookies/crw_wk_http_cookie_store.h"

#import <WebKit/WebKit.h>

#import "base/functional/callback_helpers.h"
#import "base/run_loop.h"
#import "ios/net/cookies/cookie_store_ios_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// TODO(crbug.com/352534785): Remove the dependency of OCMock.
class CRWWKHTTPCookieStoreTest : public PlatformTest {
 public:
  CRWWKHTTPCookieStoreTest()
      : crw_cookie_store_([[CRWWKHTTPCookieStore alloc] init]) {
    wk_website_data_store_ = CreateDataStore();
    mock_http_cookie_store_ =
        OCMPartialMock(wk_website_data_store_.httpCookieStore);
    crw_cookie_store_.websiteDataStore = wk_website_data_store_;
    NSURL* test_cookie_url = [NSURL URLWithString:@"http://foo.google.com/bar"];
    test_cookie_1_ = [NSHTTPCookie cookieWithProperties:@{
      NSHTTPCookiePath : test_cookie_url.path,
      NSHTTPCookieName : @"test1",
      NSHTTPCookieValue : @"value1",
      NSHTTPCookieDomain : test_cookie_url.host,
    }];
    test_cookie_2_ = [NSHTTPCookie cookieWithProperties:@{
      NSHTTPCookiePath : test_cookie_url.path,
      NSHTTPCookieName : @"test2",
      NSHTTPCookieValue : @"value2",
      NSHTTPCookieDomain : test_cookie_url.host,
    }];
  }

  // Returns a new WKWebSiteDataStore.
  WKWebsiteDataStore* CreateDataStore() {
    WKWebsiteDataStore* data_store =
        [WKWebsiteDataStore nonPersistentDataStore];
    // This is needed to force WKWebSiteDataStore to be created, otherwise the
    // cookie isn't set in some cases.
    NSSet* data_types = [NSSet setWithObject:WKWebsiteDataTypeCookies];
    [data_store
        fetchDataRecordsOfTypes:data_types
              completionHandler:^(NSArray<WKWebsiteDataRecord*>* records){
              }];
    return data_store;
  }

  // Adds `cookie` to the CRWWKHTTPCookieStore.
  [[nodiscard]] bool SetCookie(NSHTTPCookie* cookie) {
    base::RunLoop run_loop;
    [crw_cookie_store_ setCookie:cookie
               completionHandler:base::CallbackToBlock(run_loop.QuitClosure())];
    run_loop.Run();
    return true;
  }

  // Deletes `cookie` from the CRWWKHTTPCookieStore.
  [[nodiscard]] bool DeleteCookie(NSHTTPCookie* cookie) {
    base::RunLoop run_loop;
    [crw_cookie_store_
             deleteCookie:cookie
        completionHandler:base::CallbackToBlock(run_loop.QuitClosure())];
    run_loop.Run();
    return true;
  }

  // Gets all cookies from CRWWKHTTPCookieStore and ensures that getAllCookies
  // callback was called.
  [[nodiscard]] NSArray<NSHTTPCookie*>* GetCookies() {
    base::RunLoop run_loop;
    __block NSArray<NSHTTPCookie*>* cookies = nil;
    [crw_cookie_store_ getAllCookies:base::CallbackToBlock(base::BindOnce(
                                         ^(base::OnceClosure quit_closure,
                                           NSArray<NSHTTPCookie*>* result) {
                                           cookies = result;
                                           std::move(quit_closure).Run();
                                         },
                                         run_loop.QuitClosure()))];
    run_loop.Run();
    return cookies;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  CRWWKHTTPCookieStore* crw_cookie_store_;
  WKWebsiteDataStore* wk_website_data_store_ = nil;
  id mock_http_cookie_store_ = nil;
  NSHTTPCookie* test_cookie_1_ = nil;
  NSHTTPCookie* test_cookie_2_ = nil;
};

// Tests that getting cookies are cached correctly for consecutive calls.
TEST_F(CRWWKHTTPCookieStoreTest, GetCookiesCachedCorrectly) {
  OCMExpect([mock_http_cookie_store_ getAllCookies:[OCMArg any]])
      .andForwardToRealObject();
  NSArray<NSHTTPCookie*>* result_1 = GetCookies();
  EXPECT_EQ(0U, result_1.count);

  // Internal getAllCookies shouldn't be called again.
  [[mock_http_cookie_store_ reject] getAllCookies:[OCMArg any]];
  NSArray<NSHTTPCookie*>* result_2 = GetCookies();

  // Check that the same exact object is returned.
  EXPECT_EQ(result_1, result_2);
  EXPECT_NSEQ(result_1, result_2);
  EXPECT_OCMOCK_VERIFY(mock_http_cookie_store_);
}

// Tests that `setCookie:` works correctly and invalidates the cache.
TEST_F(CRWWKHTTPCookieStoreTest, SetCookie) {
  // Verify that internal cookie store setCookie method was called.
  OCMExpect([mock_http_cookie_store_ setCookie:test_cookie_1_
                             completionHandler:[OCMArg any]])
      .andForwardToRealObject();
  EXPECT_TRUE(SetCookie(test_cookie_1_));

  // internal getAllCookies should be called.
  OCMExpect([mock_http_cookie_store_ getAllCookies:[OCMArg any]])
      .andForwardToRealObject();

  NSArray<NSHTTPCookie*>* result_1 = GetCookies();
  // Verify that there is a cookie in the cookie store.
  EXPECT_EQ(1U, result_1.count);

  // Verify that internal cookie store setCookie method was called.
  OCMExpect([mock_http_cookie_store_ setCookie:test_cookie_2_
                             completionHandler:[OCMArg any]])
      .andForwardToRealObject();
  EXPECT_TRUE(SetCookie(test_cookie_2_));

  // Verify that cache was invalidated and internal getAllCookies is called.
  OCMExpect([mock_http_cookie_store_ getAllCookies:[OCMArg any]])
      .andForwardToRealObject();
  NSArray<NSHTTPCookie*>* result_2 = GetCookies();

  // Check that the cookies returned was changed and cache was invalidated.
  EXPECT_NSNE(result_1, result_2);
  EXPECT_OCMOCK_VERIFY(mock_http_cookie_store_);
}

// Tests that `deleteCookie:` works correctly and invalidates the cache.
TEST_F(CRWWKHTTPCookieStoreTest, DeleteCookie) {
  EXPECT_TRUE(SetCookie(test_cookie_1_));
  EXPECT_TRUE(SetCookie(test_cookie_2_));
  NSArray<NSHTTPCookie*>* result_1 = GetCookies();
  EXPECT_EQ(2U, result_1.count);

  // Verify that internal cookie store deleteCookie method is called.
  OCMExpect([mock_http_cookie_store_ deleteCookie:test_cookie_2_
                                completionHandler:[OCMArg any]])
      .andForwardToRealObject();

  EXPECT_TRUE(DeleteCookie(test_cookie_2_));

  // Verify that cache was invalidated and internal getAllCookies is called.
  OCMExpect([mock_http_cookie_store_ getAllCookies:[OCMArg any]])
      .andForwardToRealObject();

  NSArray<NSHTTPCookie*>* result_2 = GetCookies();
  EXPECT_EQ(1U, result_2.count);

  EXPECT_OCMOCK_VERIFY(mock_http_cookie_store_);
}

// Tests that chacing work correctly after changing the internal cookieStore.
TEST_F(CRWWKHTTPCookieStoreTest, ChangeCookieStore) {
  EXPECT_TRUE(SetCookie(test_cookie_1_));
  // Verify that cache was invalidated and internal getAllCookies is called.
  OCMExpect([mock_http_cookie_store_ getAllCookies:[OCMArg any]])
      .andForwardToRealObject();
  NSArray<NSHTTPCookie*>* result_1 = GetCookies();
  EXPECT_EQ(1U, result_1.count);
  EXPECT_OCMOCK_VERIFY(mock_http_cookie_store_);

  // Change the internal cookie store.
  wk_website_data_store_ = CreateDataStore();
  mock_http_cookie_store_ =
      OCMPartialMock(wk_website_data_store_.httpCookieStore);
  crw_cookie_store_.websiteDataStore = wk_website_data_store_;

  // Verify that internal getAllCookies is called.
  OCMExpect([mock_http_cookie_store_ getAllCookies:[OCMArg any]])
      .andForwardToRealObject();
  NSArray<NSHTTPCookie*>* result_3 = GetCookies();
  // There should be no cookies in the new cookie store.
  EXPECT_EQ(0U, result_3.count);

  EXPECT_TRUE(SetCookie(test_cookie_2_));

  // Verify that cache was invalidated and internal getAllCookies is called.
  OCMExpect([mock_http_cookie_store_ getAllCookies:[OCMArg any]])
      .andForwardToRealObject();
  NSArray<NSHTTPCookie*>* result_4 = GetCookies();
  EXPECT_EQ(1U, result_4.count);
  EXPECT_OCMOCK_VERIFY(mock_http_cookie_store_);
}

// Tests that if the internal cookie store is nil, getAllCookie will still run
// its callback.
TEST_F(CRWWKHTTPCookieStoreTest, NilCookieStore) {
  crw_cookie_store_.websiteDataStore = nil;
  // GetCookies should return empty array when there is no cookie store.
  NSArray<NSHTTPCookie*>* result = GetCookies();
  EXPECT_EQ(0U, result.count);
}
