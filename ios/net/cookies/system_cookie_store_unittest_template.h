// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_COOKIES_SYSTEM_COOKIE_STORE_UNITTEST_TEMPLATE_H_
#define IOS_NET_COOKIES_SYSTEM_COOKIE_STORE_UNITTEST_TEMPLATE_H_

#import <Foundation/Foundation.h>

#import "base/barrier_closure.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/ptr_util.h"
#import "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#import "ios/net/cookies/cookie_store_ios_test_util.h"
#import "ios/net/cookies/system_cookie_store.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace net {

namespace {

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
  base::RunLoop run_loop;
  store->SetCookieAsync(cookie, /*optional_creation_time=*/nullptr,
                        run_loop.QuitClosure());
  run_loop.Run();
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
  SetCookieInStoreWithNoCallback(system_cookie, this->GetCookieStore());
  EXPECT_TRUE(this->IsCookieSet(system_cookie, this->test_cookie_url1_));
}

// Tests that inserting multiple cookies with identical creation times works.
TYPED_TEST_P(SystemCookieStoreTest, SetCookieAsyncWithIdenticalCreationTime) {
  NSHTTPCookie* system_cookie_1 =
      CreateCookie(@"a", @"b", this->test_cookie_url1_);
  NSHTTPCookie* system_cookie_2 =
      CreateCookie(@"c", @"d", this->test_cookie_url2_);
  SystemCookieStore* cookie_store = this->GetCookieStore();
  const base::Time creation_time = base::Time::Now();

  base::RunLoop run_loop;
  base::RepeatingClosure closure =
      base::BarrierClosure(2, run_loop.QuitClosure());
  cookie_store->SetCookieAsync(system_cookie_1, &creation_time, closure);
  cookie_store->SetCookieAsync(system_cookie_2, &creation_time, closure);
  run_loop.Run();

  EXPECT_TRUE(this->IsCookieSet(system_cookie_1, this->test_cookie_url1_));
  EXPECT_TRUE(this->IsCookieSet(system_cookie_2, this->test_cookie_url2_));
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

  {
    base::RunLoop run_loop;
    __block NSArray<NSHTTPCookie*>* result_cookies = nil;
    cookie_store->GetCookiesForURLAsync(
        GURLWithNSURL(this->test_cookie_url1_),
        base::BindOnce(^(NSArray<NSHTTPCookie*>* result) {
          result_cookies = result;
        }).Then(run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(1u, result_cookies.count);
    NSHTTPCookie* result_cookie = result_cookies[0];
    EXPECT_NSEQ(input_cookie.name, result_cookie.name);
    EXPECT_NSEQ(input_cookie.value, result_cookie.value);
  }

  // Test GetAllCookies
  {
    base::RunLoop run_loop;
    __block NSArray<NSHTTPCookie*>* result_cookies = nil;
    cookie_store->GetAllCookiesAsync(
        base::BindOnce(^(NSArray<NSHTTPCookie*>* result) {
          result_cookies = result;
        }).Then(run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(3u, result_cookies.count);
    for (NSHTTPCookie* cookie in result_cookies) {
      NSHTTPCookie* existing_cookie = [input_cookies valueForKey:cookie.name];
      EXPECT_TRUE(existing_cookie);
      EXPECT_NSEQ(existing_cookie.name, cookie.name);
      EXPECT_NSEQ(existing_cookie.value, cookie.value);
      EXPECT_NSEQ(existing_cookie.domain, cookie.domain);
    }
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
  EXPECT_TRUE(this->IsCookieSet(system_cookie2, this->test_cookie_url2_));

  {
    base::RunLoop run_loop;
    cookie_store->DeleteCookieAsync(system_cookie2, run_loop.QuitClosure());
    run_loop.Run();

    EXPECT_FALSE(this->IsCookieSet(system_cookie2, this->test_cookie_url2_));
    EXPECT_EQ(1, this->CookiesCount());
  }

  {
    base::RunLoop run_loop;
    cookie_store->DeleteCookieAsync(system_cookie1, run_loop.QuitClosure());
    run_loop.Run();

    EXPECT_FALSE(this->IsCookieSet(system_cookie1, this->test_cookie_url1_));
    EXPECT_EQ(0, this->CookiesCount());
  }
}

TYPED_TEST_P(SystemCookieStoreTest, ClearCookiesAsync) {
  SystemCookieStore* cookie_store = this->GetCookieStore();
  SetCookieInStoreWithNoCallback(
      CreateCookie(@"a", @"b", this->test_cookie_url1_), cookie_store);
  SetCookieInStoreWithNoCallback(
      CreateCookie(@"x", @"d", this->test_cookie_url2_), cookie_store);
  EXPECT_EQ(2, this->CookiesCount());

  base::RunLoop run_loop;
  cookie_store->ClearStoreAsync(run_loop.QuitClosure());
  run_loop.Run();

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
                            SetCookieAsyncWithIdenticalCreationTime,
                            GetCookiesAsync,
                            DeleteCookiesAsync,
                            ClearCookiesAsync,
                            GetCookieAcceptPolicy);

}  // namespace net

#endif  // IOS_NET_COOKIES_SYSTEM_COOKIE_STORE_UNITTEST_TEMPLATE_H_
