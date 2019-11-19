// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_COOKIES_SYSTEM_COOKIE_STORE_UNITTEST_TEMPLATE_H_
#define IOS_NET_COOKIES_SYSTEM_COOKIE_STORE_UNITTEST_TEMPLATE_H_

#import "ios/net/cookies/system_cookie_store.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#include "ios/net/cookies/cookie_store_ios_test_util.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForCookiesTimeout;

namespace net {

namespace {

// Helper callbacks to be passed to SetCookieAsync/GetCookiesAsync.
class SystemCookieCallbackRunVerifier {
 public:
  SystemCookieCallbackRunVerifier()
      : did_run_with_cookies_(false),
        did_run_with_no_cookies_(false),
        cookies_(nil) {}

  void Reset() {
    did_run_with_cookies_ = false;
    did_run_with_no_cookies_ = false;
    cookies_ = nil;
  }

  // Waits for |RunWithCookies| to run, and returns false if it doesn't run.
  bool WaitForCallbackWithCookies() {
    return WaitUntilConditionOrTimeout(kWaitForCookiesTimeout, ^bool {
      base::RunLoop().RunUntilIdle();
      return did_run_with_cookies_;
    });
  }

  // Waits for |RunWithNoCookies| to run, and returns false if it doesn't run.
  bool WaitForCallbackWithNoCookies() {
    return WaitUntilConditionOrTimeout(kWaitForCookiesTimeout, ^bool {
      base::RunLoop().RunUntilIdle();
      return did_run_with_no_cookies_;
    });
  }

  // Returns the paremeter of the callback.
  NSArray<NSHTTPCookie*>* cookies() { return cookies_; }

  void RunWithCookies(NSArray<NSHTTPCookie*>* cookies) {
    ASSERT_FALSE(did_run_with_cookies_);
    cookies_ = cookies;
    did_run_with_cookies_ = true;
  }

  void RunWithNoCookies() {
    ASSERT_FALSE(did_run_with_no_cookies_);
    did_run_with_no_cookies_ = true;
  }

 private:
  bool did_run_with_cookies_;
  bool did_run_with_no_cookies_;
  NSArray<NSHTTPCookie*>* cookies_;
};

NSHTTPCookie* CreateCookie(NSString* name, NSString* value, NSURL* url) {
  return [NSHTTPCookie cookieWithProperties:@{
    NSHTTPCookiePath : url.path,
    NSHTTPCookieName : name,
    NSHTTPCookieValue : value,
    NSHTTPCookieDomain : url.host,
  }];
}

// Sets |cookie| in the SystemCookieStore |store|, doesn't care about callback
// and doesn't set creation time.
void SetCookieInStoreWithNoCallback(NSHTTPCookie* cookie,
                                    SystemCookieStore* store) {
  SystemCookieCallbackRunVerifier completion_verifier;
  store->SetCookieAsync(
      cookie, /*optional_creation_time=*/nullptr,
      base::BindOnce(&SystemCookieCallbackRunVerifier::RunWithNoCookies,
                     base::Unretained(&completion_verifier)));
  EXPECT_TRUE(completion_verifier.WaitForCallbackWithNoCookies());
}

}  // namespace

// This class defines tests that implementations of SystemCookieStore should
// pass in order to be conformant.
// To use this test, A delegate class should be created for each implementation.
// The delegate class has to implement the following functions:
//   GetCookieStore()
//     Returns a SystemCookieStore implementation object.
//   bool IsCookieSet(NSHttpCookie cookie, NSURL url)
//     Returns wether |cookie| is set for |url| in the internal cookie store or
//     not.
//   ClearCookies()
//     Deletes all cookies in the internal cookie store.
//   int CookiesCount()
//     Returns the number of cookies set in the internal cookie store.
template <typename SystemCookieStoreTestDelegate>
class SystemCookieStoreTest : public PlatformTest {
 public:
  SystemCookieStoreTest()
      : test_cookie_url1_([NSURL URLWithString:@"http://foo.google.com/bar"]),
        test_cookie_url2_([NSURL URLWithString:@"http://bar.xyz.abc/"]),
        test_cookie_url3_([NSURL URLWithString:@"http://123.foo.bar/"]) {
    ClearCookies();
  }

  // Gets the SystemCookieStore implementation class instance.
  SystemCookieStore* GetCookieStore() { return delegate_.GetCookieStore(); }

  // Returns wether |system_cookie| is set in |delegate_| cookiestore or not.
  bool IsCookieSet(NSHTTPCookie* system_cookie, NSURL* url) {
    return delegate_.IsCookieSet(system_cookie, url);
  }

  // Clears the |delegate_| cookie store.
  void ClearCookies() { delegate_.ClearCookies(); }

  // Returns the number of cookies set in the |delegate_| cookie store.
  int CookiesCount() { return delegate_.CookiesCount(); }

 protected:
  NSURL* test_cookie_url1_;
  NSURL* test_cookie_url2_;
  NSURL* test_cookie_url3_;

 private:
  SystemCookieStoreTestDelegate delegate_;
};

TYPED_TEST_SUITE_P(SystemCookieStoreTest);

TYPED_TEST_P(SystemCookieStoreTest, SetCookieAsync) {
  NSHTTPCookie* system_cookie =
      CreateCookie(@"a", @"b", this->test_cookie_url1_);
  SystemCookieCallbackRunVerifier callback_verifier;
  SystemCookieStore* cookie_store = this->GetCookieStore();
  cookie_store->SetCookieAsync(
      system_cookie, /*optional_creation_time=*/nullptr,
      base::BindOnce(&SystemCookieCallbackRunVerifier::RunWithNoCookies,
                     base::Unretained(&callback_verifier)));
  EXPECT_TRUE(callback_verifier.WaitForCallbackWithNoCookies());
  EXPECT_TRUE(this->IsCookieSet(system_cookie, this->test_cookie_url1_));
}

// Tests cases of GetAllCookiesAsync and GetCookiesForURLAsync.
TYPED_TEST_P(SystemCookieStoreTest, GetCookiesAsync) {
  SystemCookieStore* cookie_store = this->GetCookieStore();
  NSMutableDictionary* input_cookies = [[NSMutableDictionary alloc] init];
  NSHTTPCookie* system_cookie =
      CreateCookie(@"a", @"b", this->test_cookie_url1_);
  [input_cookies setValue:system_cookie forKey:@"a"];
  SetCookieInStoreWithNoCallback(system_cookie, cookie_store);

  system_cookie = CreateCookie(@"x", @"d", this->test_cookie_url2_);
  [input_cookies setValue:system_cookie forKey:@"x"];
  SetCookieInStoreWithNoCallback(system_cookie, cookie_store);
  system_cookie = CreateCookie(@"l", @"m", this->test_cookie_url3_);
  [input_cookies setValue:system_cookie forKey:@"l"];
  SetCookieInStoreWithNoCallback(system_cookie, cookie_store);

  // Test GetCookieForURLAsync.
  NSHTTPCookie* input_cookie = [input_cookies valueForKey:@"a"];
  SystemCookieCallbackRunVerifier callback_verifier;
  cookie_store->GetCookiesForURLAsync(
      GURLWithNSURL(this->test_cookie_url1_),
      base::BindOnce(&SystemCookieCallbackRunVerifier::RunWithCookies,
                     base::Unretained(&callback_verifier)));
  EXPECT_TRUE(callback_verifier.WaitForCallbackWithCookies());
  EXPECT_EQ(1u, callback_verifier.cookies().count);
  NSHTTPCookie* result_cookie = callback_verifier.cookies()[0];
  EXPECT_TRUE([input_cookie.name isEqualToString:result_cookie.name]);
  EXPECT_TRUE([input_cookie.value isEqualToString:result_cookie.value]);

  // Test GetAllCookies
  callback_verifier.Reset();
  cookie_store->GetAllCookiesAsync(
      base::BindOnce(&SystemCookieCallbackRunVerifier::RunWithCookies,
                     base::Unretained(&callback_verifier)));
  EXPECT_TRUE(callback_verifier.WaitForCallbackWithCookies());
  NSArray<NSHTTPCookie*>* result_cookies = callback_verifier.cookies();
  EXPECT_EQ(3u, result_cookies.count);
  for (NSHTTPCookie* cookie in result_cookies) {
    NSHTTPCookie* existing_cookie = [input_cookies valueForKey:cookie.name];
    EXPECT_TRUE(existing_cookie);
    EXPECT_TRUE([existing_cookie.name isEqualToString:cookie.name]);
    EXPECT_TRUE([existing_cookie.value isEqualToString:cookie.value]);
    EXPECT_TRUE([existing_cookie.domain isEqualToString:cookie.domain]);
  }
}

// Tests deleting cookies for different URLs and for different
// cookie key/value pairs.
TYPED_TEST_P(SystemCookieStoreTest, DeleteCookiesAsync) {
  SystemCookieStore* cookie_store = this->GetCookieStore();
  NSHTTPCookie* system_cookie1 =
      CreateCookie(@"a", @"b", this->test_cookie_url1_);
  SetCookieInStoreWithNoCallback(system_cookie1, cookie_store);
  NSHTTPCookie* system_cookie2 =
      CreateCookie(@"x", @"d", this->test_cookie_url2_);
  cookie_store->SetCookieAsync(system_cookie2,
                               /*optional_creation_time=*/nullptr,
                               SystemCookieStore::SystemCookieCallback());
  SetCookieInStoreWithNoCallback(system_cookie2, cookie_store);
  EXPECT_EQ(2, this->CookiesCount());
  SystemCookieCallbackRunVerifier callback_verifier;

  EXPECT_TRUE(this->IsCookieSet(system_cookie2, this->test_cookie_url2_));
  cookie_store->DeleteCookieAsync(
      system_cookie2,
      base::BindOnce(&SystemCookieCallbackRunVerifier::RunWithNoCookies,
                     base::Unretained(&callback_verifier)));
  EXPECT_TRUE(callback_verifier.WaitForCallbackWithNoCookies());
  EXPECT_FALSE(this->IsCookieSet(system_cookie2, this->test_cookie_url2_));
  EXPECT_EQ(1, this->CookiesCount());

  callback_verifier.Reset();
  cookie_store->DeleteCookieAsync(
      system_cookie1,
      base::BindOnce(&SystemCookieCallbackRunVerifier::RunWithNoCookies,
                     base::Unretained(&callback_verifier)));
  EXPECT_TRUE(callback_verifier.WaitForCallbackWithNoCookies());
  EXPECT_FALSE(this->IsCookieSet(system_cookie1, this->test_cookie_url1_));
  EXPECT_EQ(0, this->CookiesCount());
}

TYPED_TEST_P(SystemCookieStoreTest, ClearCookiesAsync) {
  SystemCookieStore* cookie_store = this->GetCookieStore();
  SetCookieInStoreWithNoCallback(
      CreateCookie(@"a", @"b", this->test_cookie_url1_), cookie_store);
  SetCookieInStoreWithNoCallback(
      CreateCookie(@"x", @"d", this->test_cookie_url2_), cookie_store);
  EXPECT_EQ(2, this->CookiesCount());

  SystemCookieCallbackRunVerifier callback_verifier;
  cookie_store->ClearStoreAsync(
      base::BindOnce(&SystemCookieCallbackRunVerifier::RunWithNoCookies,
                     base::Unretained(&callback_verifier)));
  EXPECT_TRUE(callback_verifier.WaitForCallbackWithNoCookies());
  EXPECT_EQ(0, this->CookiesCount());
}

TYPED_TEST_P(SystemCookieStoreTest, GetCookieAcceptPolicy) {
  SystemCookieStore* cookie_store = this->GetCookieStore();
  EXPECT_EQ([NSHTTPCookieStorage sharedHTTPCookieStorage].cookieAcceptPolicy,
            cookie_store->GetCookieAcceptPolicy());
  [NSHTTPCookieStorage sharedHTTPCookieStorage].cookieAcceptPolicy =
      NSHTTPCookieAcceptPolicyNever;
  EXPECT_EQ(NSHTTPCookieAcceptPolicyNever,
            cookie_store->GetCookieAcceptPolicy());
  [NSHTTPCookieStorage sharedHTTPCookieStorage].cookieAcceptPolicy =
      NSHTTPCookieAcceptPolicyAlways;
  EXPECT_EQ(NSHTTPCookieAcceptPolicyAlways,
            cookie_store->GetCookieAcceptPolicy());
}

REGISTER_TYPED_TEST_SUITE_P(SystemCookieStoreTest,
                            SetCookieAsync,
                            GetCookiesAsync,
                            DeleteCookiesAsync,
                            ClearCookiesAsync,
                            GetCookieAcceptPolicy);

}  // namespace net

#endif  // IOS_NET_COOKIES_SYSTEM_COOKIE_STORE_UNITTEST_TEMPLATE_H
