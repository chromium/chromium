// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_session_cookie_storage.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Creates NSHTTPCookie with the given proprties.
NSHTTPCookie* MakeCookie(NSString* url_string,
                         NSString* name,
                         NSString* value) {
  NSURL* url = [NSURL URLWithString:url_string];
  return [NSHTTPCookie cookieWithProperties:@{
    NSHTTPCookiePath : url.path,
    NSHTTPCookieName : name,
    NSHTTPCookieValue : value,
    NSHTTPCookieDomain : url.host,
  }];
}
}  // namespace

// Test fixture for testing DownloadSessionCookieStorage class.
class DownloadSessionCookieStorageTest : public PlatformTest {
 public:
  DownloadSessionCookieStorageTest()
      : cookie_store_([[DownloadSessionCookieStorage alloc] init]) {}

  DownloadSessionCookieStorage* cookie_store_;
};

// Tests that setting cookies and getting cookies work correctly.
TEST_F(DownloadSessionCookieStorageTest, CookiesSetAndGet) {
  ASSERT_FALSE(cookie_store_.cookies.count);
  NSHTTPCookie* cookie = MakeCookie(@"http://foo.cookiestest.test/bar/test",
                                    /*name=*/@"test1", /*value=*/@"value1");
  [cookie_store_ setCookie:cookie];
  NSArray<NSHTTPCookie*>* cookies = cookie_store_.cookies;
  EXPECT_EQ(1U, cookies.count);
  EXPECT_NSEQ(cookie, cookies.firstObject);
}

// Tests that getting cookies for a specific URL works correctly
TEST_F(DownloadSessionCookieStorageTest, CookiesForURL) {
  ASSERT_FALSE(cookie_store_.cookies.count);

  NSHTTPCookie* test_cookie_1 =
      MakeCookie(/*url_string=*/@"http://foo.cookiestest.test/bar/test",
                 /*name=*/@"test1", /*value=*/@"value1");
  [cookie_store_ setCookie:test_cookie_1];

  NSHTTPCookie* test_cookie_2 =
      MakeCookie(/*url_string=*/@"http://foo.cookiestest.test/bar",
                 /*name=*/@"test2", /*value=*/@"value2");
  [cookie_store_ setCookie:test_cookie_2];

  NSHTTPCookie* test_cookie_3 =
      MakeCookie(/*url_string=*/@"http://abc.cookiestest.test/bar",
                 /*name=*/@"test3", /*value=*/@"value3");
  [cookie_store_ setCookie:test_cookie_3];

  NSArray<NSHTTPCookie*>* cookies = [cookie_store_
      cookiesForURL:
          [NSURL URLWithString:@"http://foo.cookiestest.test/bar/test/foo"]];
  EXPECT_EQ(2U, cookies.count);
  EXPECT_TRUE([cookies containsObject:test_cookie_1]);
  EXPECT_TRUE([cookies containsObject:test_cookie_2]);
}

// Tests that |getCookiesForTask| uses the correct URL to get cookies, and also
// invokes the completion handler successfully on the result.
TEST_F(DownloadSessionCookieStorageTest, GetCookiesForTask) {
  ASSERT_FALSE(cookie_store_.cookies.count);
  NSURL* test_cookie1_url =
      [NSURL URLWithString:@"http://foo.cookiestest.test/bar"];
  NSHTTPCookie* test_cookie_1 =
      MakeCookie(/*url_string=*/@"http://foo.cookiestest.test/bar",
                 /*name=*/@"test1", /*value=*/@"value1");
  [cookie_store_ setCookie:test_cookie_1];

  NSHTTPCookie* test_cookie_2 =
      MakeCookie(/*url_string=*/@"http://abc.cookiestest.test/bar",
                 /*name=*/@"test2", /*value=*/@"value2");
  [cookie_store_ setCookie:test_cookie_2];

  __block bool callback_called = false;
  NSURLSessionTask* task =
      [[NSURLSession sharedSession] dataTaskWithURL:test_cookie1_url];
  [cookie_store_ getCookiesForTask:task
                 completionHandler:^(NSArray<NSHTTPCookie*>* cookies) {
                   EXPECT_EQ(1U, cookies.count);
                   EXPECT_NSEQ(test_cookie_1, cookies.firstObject);
                   callback_called = true;
                 }];
  EXPECT_TRUE(callback_called);
}

// Tests that |storeCookies:forTask:| works correctly and respects the
// |cookieAcceptPolicy|.
TEST_F(DownloadSessionCookieStorageTest, StoreCookiesForTask) {
  ASSERT_FALSE(cookie_store_.cookies.count);

  // Only accept Main Domain cookies.
  cookie_store_.cookieAcceptPolicy =
      NSHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain;

  NSURL* test_cookie1_url =
      [NSURL URLWithString:@"http://foo.test.cookiestest.test/abc"];
  NSHTTPCookie* test_cookie_1 =
      MakeCookie(/*url_string=*/@"http://foo.test.cookiestest.test/abc",
                 /*name=*/@"a", /*value=*/@"b");

  NSURL* test_cookie2_url =
      [NSURL URLWithString:@"http://abc.foo.cookiestest.test/abc"];
  NSHTTPCookie* test_cookie_2 =
      MakeCookie(/*url_string=*/@"http://abc.foo.cookiestest.test/abc",
                 /*name=*/@"a", /*value=*/@"b");

  NSMutableURLRequest* request =
      [NSMutableURLRequest requestWithURL:test_cookie1_url];
  request.mainDocumentURL =
      [NSURL URLWithString:@"http://foo.cookiestest.test/xyz"];

  NSURLSessionTask* task =
      [[NSURLSession sharedSession] dataTaskWithRequest:request];
  [cookie_store_ storeCookies:@[ test_cookie_1 ] forTask:task];
  EXPECT_FALSE(cookie_store_.cookies.count);

  request.URL = test_cookie2_url;
  task = [[NSURLSession sharedSession] dataTaskWithRequest:request];
  [cookie_store_ storeCookies:@[ test_cookie_2 ] forTask:task];
  EXPECT_EQ(1U, cookie_store_.cookies.count);
  EXPECT_NSEQ(test_cookie_2, cookie_store_.cookies.firstObject);

  // Accept all Domain cookies.
  cookie_store_.cookieAcceptPolicy = NSHTTPCookieAcceptPolicyAlways;

  request.URL = test_cookie1_url;
  task = [[NSURLSession sharedSession] dataTaskWithRequest:request];
  [cookie_store_ storeCookies:@[ test_cookie_1 ] forTask:task];
  NSArray<NSHTTPCookie*>* result = cookie_store_.cookies;
  EXPECT_EQ(2U, result.count);
  EXPECT_TRUE([result containsObject:test_cookie_1]);
}

// Tests that setCookiesForURL: respects the |cookieAcceptPolicy| settings.
TEST_F(DownloadSessionCookieStorageTest, SetCookiesForURL) {
  ASSERT_FALSE(cookie_store_.cookies.count);

  NSURL* main_doc_url =
      [NSURL URLWithString:@"http://foo.cookiestest.test/xyz"];

  // Only accept Main Domain cookies.
  cookie_store_.cookieAcceptPolicy =
      NSHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain;

  NSURL* test_cookie1_url =
      [NSURL URLWithString:@"http://foo.test.cookiestest.test/abc"];
  NSHTTPCookie* test_cookie_1 =
      MakeCookie(/*url_string=*/@"http://foo.test.cookiestest.test/abc",
                 /*name=*/@"a", /*value=*/@"b");
  [cookie_store_ setCookies:@[ test_cookie_1 ]
                     forURL:test_cookie1_url
            mainDocumentURL:main_doc_url];
  EXPECT_FALSE(cookie_store_.cookies.count);

  NSURL* test_cookie2_url =
      [NSURL URLWithString:@"http://abc.foo.cookiestest.test/abc"];
  NSHTTPCookie* test_cookie_2 =
      MakeCookie(/*url_string=*/@"http://abc.foo.cookiestest.test/abc",
                 /*name=*/@"a", /*value=*/@"b");
  [cookie_store_ setCookies:@[ test_cookie_2 ]
                     forURL:test_cookie2_url
            mainDocumentURL:main_doc_url];

  EXPECT_EQ(1U, cookie_store_.cookies.count);
  EXPECT_NSEQ(test_cookie_2, cookie_store_.cookies.firstObject);

  // Accept all Domain cookies.
  cookie_store_.cookieAcceptPolicy = NSHTTPCookieAcceptPolicyAlways;
  [cookie_store_ setCookies:@[ test_cookie_1 ]
                     forURL:test_cookie1_url
            mainDocumentURL:main_doc_url];
  NSArray<NSHTTPCookie*>* result = cookie_store_.cookies;
  EXPECT_EQ(2U, result.count);
  EXPECT_TRUE([result containsObject:test_cookie_1]);
}

// Tests that when |cookieAcceptPolicy| is set to
// |NSHTTPCookieAcceptPolicyNever|, no cookies will be saved.
TEST_F(DownloadSessionCookieStorageTest, NeverAcceptCookies) {
  ASSERT_FALSE(cookie_store_.cookies.count);
  // By default Cookies accept policy is NSHTTPCookieAcceptPolicyAlways.
  EXPECT_EQ(cookie_store_.cookieAcceptPolicy, NSHTTPCookieAcceptPolicyAlways);

  cookie_store_.cookieAcceptPolicy = NSHTTPCookieAcceptPolicyNever;
  NSURL* test_cookie_url = [NSURL URLWithString:@"http://foo.cookiestest.test"];
  NSHTTPCookie* cookie =
      MakeCookie(/*url_string=*/@"http://foo.cookiestest.test", /*name=*/@"a",
                 /*value=*/@"b");
  [cookie_store_ setCookie:cookie];
  EXPECT_FALSE(cookie_store_.cookies.count);

  [cookie_store_ setCookies:@[ cookie ]
                     forURL:test_cookie_url
            mainDocumentURL:test_cookie_url];
  EXPECT_FALSE(cookie_store_.cookies.count);

  NSURLSessionTask* task =
      [[NSURLSession sharedSession] dataTaskWithURL:test_cookie_url];
  [cookie_store_ storeCookies:@[ cookie ] forTask:task];
  EXPECT_FALSE(cookie_store_.cookies.count);
}
