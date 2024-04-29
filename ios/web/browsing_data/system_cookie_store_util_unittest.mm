// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/browsing_data/system_cookie_store_util.h"

#import <WebKit/WebKit.h>

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/run_loop.h"
#import "ios/net/cookies/cookie_store_ios_test_util.h"
#import "ios/net/cookies/system_cookie_store.h"
#import "ios/web/net/cookies/wk_cookie_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/test/web_test.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace web {

namespace {

// Returns a callback that store its parameter to `output`. `output` must
// outlive the returned callback invocation.
template <typename T>
base::OnceCallback<void(T)> CaptureOutput(T* output) {
  return base::BindOnce([](T* output, T param) { *output = param; }, output);
}

// Checks if `system_cookie` for `url` is present in `cookies`.
bool IsCookieSetInCookies(NSHTTPCookie* cookie,
                          NSURL* url,
                          NSArray<NSHTTPCookie*>* cookies) {
  for (NSHTTPCookie* item in cookies) {
    if ([item.path isEqualToString:url.path] &&
        [item.domain isEqualToString:url.host] &&
        [item.name isEqualToString:cookie.name]) {
      return [item.value isEqualToString:cookie.value];
    }
  }
  return false;
}

// Checks if `system_cookie` was set in WKHTTPCookieStore `cookie_store`.
bool IsCookieSetInWKCookieStore(NSHTTPCookie* system_cookie,
                                NSURL* url,
                                WKHTTPCookieStore* cookie_store) {
  bool present = false;

  base::RunLoop run_loop;
  base::OnceCallback<void(NSArray<NSHTTPCookie*>*)> callback =
      base::BindOnce(&IsCookieSetInCookies, system_cookie, url)
          .Then(CaptureOutput(&present))
          .Then(run_loop.QuitClosure());
  [cookie_store getAllCookies:base::CallbackToBlock(std::move(callback))];
  run_loop.Run();

  return present;
}

// Sets `cookie` in SystemCookieStore `store`, returning whether the operation
// was a success or not (i.e. timeout).
bool SetCookieInCookieStore(NSHTTPCookie* cookie,
                            net::SystemCookieStore* store) {
  bool success = false;

  base::RunLoop run_loop;
  store->SetCookieAsync(cookie, /*optional_creation_time=*/nullptr,
                        base::ReturnValueOnce(true)
                            .Then(CaptureOutput(&success))
                            .Then(run_loop.QuitClosure()));
  run_loop.Run();

  return success;
}

// Clears all cookies in SystemCookieStore `store`, returning whether the
// operation was a success or not (i.e. timeout).
bool ClearCookiesInCookieStore(net::SystemCookieStore* store) {
  bool success = false;

  base::RunLoop run_loop;
  store->ClearStoreAsync(base::ReturnValueOnce(true).Then(
      CaptureOutput(&success).Then(run_loop.QuitClosure())));
  run_loop.Run();

  return success;
}

}  // namespace

using SystemCookieStoreUtilTest = PlatformTest;

// Tests that web::CreateSystemCookieStore returns a SystemCookieStore object
// that is backed by the correct internal cookiestore based on the iOS version.
TEST_F(SystemCookieStoreUtilTest, CreateSystemCookieStore) {
  web::WebTaskEnvironment task_environment;
  net::ScopedTestingCookieStoreIOSClient scoped_cookie_store_ios_client(
      std::make_unique<net::TestCookieStoreIOSClient>());

  web::FakeBrowserState browser_state;
  browser_state.SetOffTheRecord(true);
  NSURL* test_cookie_url = [NSURL URLWithString:@"http://foo.google.com/bar"];
  NSHTTPCookie* test_cookie = [NSHTTPCookie cookieWithProperties:@{
    NSHTTPCookiePath : test_cookie_url.path,
    NSHTTPCookieName : @"x",
    NSHTTPCookieValue : @"d",
    NSHTTPCookieDomain : test_cookie_url.host,
  }];
  auto system_cookie_store = web::CreateSystemCookieStore(&browser_state);

  WKHTTPCookieStore* wk_cookie_store =
      web::WKCookieStoreForBrowserState(&browser_state);
  EXPECT_FALSE(IsCookieSetInWKCookieStore(test_cookie, test_cookie_url,
                                          wk_cookie_store));
  EXPECT_TRUE(SetCookieInCookieStore(test_cookie, system_cookie_store.get()));
  EXPECT_TRUE(IsCookieSetInWKCookieStore(test_cookie, test_cookie_url,
                                         wk_cookie_store));

  // Clear cookies that was set in the test.
  EXPECT_TRUE(ClearCookiesInCookieStore(system_cookie_store.get()));
}

}  // namespace web
