// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/gaia_auth_fetcher_ios_ns_url_session_bridge.h"

#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/signin/feature_flags.h"
#include "ios/chrome/browser/signin/gaia_auth_fetcher_ios_bridge.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#include "ios/net/cookies/system_cookie_util.h"
#include "ios/web/common/features.h"
#include "ios/web/public/test/web_task_environment.h"
#include "net/cookies/cookie_store.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

typedef void (^DataTaskWithRequestCompletionHandler)(
    NSData* _Nullable data,
    NSURLResponse* _Nullable response,
    NSError* _Nullable error);

class GaiaAuthFetcherIOSNSURLSessionBridgeTest;

namespace {

NSString* GetStringWithNSHTTPCookie(NSHTTPCookie* cookie) {
  return [NSString stringWithFormat:@"%@=%@; path=%@; domain=%@", cookie.name,
                                    cookie.value, cookie.path, cookie.domain];
}

NSString* GetStringWithCanonicalCookie(net::CanonicalCookie cookie) {
  return [NSString
      stringWithFormat:@"%s=%s; path=%s; domain=%s", cookie.Name().c_str(),
                       cookie.Value().c_str(), cookie.Path().c_str(),
                       cookie.Domain().c_str()];
}

// Delegate classe to test GaiaAuthFetcherIOSNSURLSessionBridge.
class FakeGaiaAuthFetcherIOSBridgeDelegate
    : public GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridgeDelegate {
 public:
  FakeGaiaAuthFetcherIOSBridgeDelegate()
      : GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridgeDelegate(),
        fetch_complete_called_(false) {}

  ~FakeGaiaAuthFetcherIOSBridgeDelegate() override {}

  // GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridgeDelegate.
  void OnFetchComplete(const GURL& url,
                       const std::string& data,
                       const net::URLRequestStatus& status,
                       int response_code) override {
    EXPECT_FALSE(fetch_complete_called_);
    fetch_complete_called_ = true;
    url_ = url;
    data_ = data;
    status_ = status;
    response_code_ = response_code;
  }

  // Returns true if has been called().
  bool GetFetchCompleteCalled() { return fetch_complete_called_; }

  // Returns |url| from FetchComplete().
  const GURL& GetURL() { return url_; }

  // Returns |data| from FetchComplete().
  const std::string& GetData() { return data_; }

  // Returns |status| from FetchComplete().
  net::URLRequestStatus GetStatus() { return status_; }

  // Returns |response_code| from FetchComplete().
  int GetResponseCode() { return response_code_; }

 private:
  // true if has been called().
  bool fetch_complete_called_;
  // |url| from FetchComplete().
  GURL url_;
  // |data| from FetchComplete().
  std::string data_;
  // |status| from FetchComplete().
  net::URLRequestStatus status_;
  // |response_code| from FetchComplete().
  int response_code_;
};

class TestGaiaAuthFetcherIOSNSURLSessionBridge
    : public GaiaAuthFetcherIOSNSURLSessionBridge {
 public:
  TestGaiaAuthFetcherIOSNSURLSessionBridge(
      GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridgeDelegate* delegate,
      web::BrowserState* browser_state,
      GaiaAuthFetcherIOSNSURLSessionBridgeTest* test);

  // GaiaAuthFetcherIOSNSURLSessionBridge.
  NSURLSession* CreateNSURLSession(
      id<NSURLSessionTaskDelegate> url_session_delegate) override;

 protected:
  GaiaAuthFetcherIOSNSURLSessionBridgeTest* test_;
};

}  // namespace

class GaiaAuthFetcherIOSNSURLSessionBridgeTest : public ChromeWebTest {
 protected:
  // ChromeWebTest.
  void SetUp() override;
  void TearDown() override;

  // Create a NSURLSession mock, and saves its delegate.
  NSURLSession* CreateNSURLSession(
      id<NSURLSessionTaskDelegate> url_session_delegate);

  void ExpectCookies(NSArray<NSHTTPCookie*>* cookies);

  void AllCookies(const std::vector<net::CanonicalCookie>& all_cookies);

  void AddCookiesToCookieManager(NSArray<NSHTTPCookie*>* cookies);

  std::string GetCookieDomain() { return std::string("example.com"); }
  GURL GetFetchGURL() { return GURL("http://www." + GetCookieDomain()); }

  NSHTTPCookie* GetCookie1();

  NSHTTPCookie* GetCookie2();

  NSHTTPURLResponse* CreateHTTPURLResponse(int status_code,
                                           NSArray<NSHTTPCookie*>* cookies);

  NSDictionary* GetHeaderFieldsWithCookies(NSArray<NSHTTPCookie*>* cookies);

  friend TestGaiaAuthFetcherIOSNSURLSessionBridge;

  // kWKHTTPSystemCookieStore and kUseNSURLSessionForGaiaSigninRequests should
  // be enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  // Browser state for the tests.
  std::unique_ptr<ios::ChromeBrowserState> browser_state_;
  // Instance used for the tests.
  std::unique_ptr<TestGaiaAuthFetcherIOSNSURLSessionBridge>
      ns_url_session_bridge_;
  // Fake delegate for |ns_url_session_bridge_|.
  std::unique_ptr<FakeGaiaAuthFetcherIOSBridgeDelegate> delegate_;
  // Cookies returned by the cookie manager.
  std::vector<net::CanonicalCookie> all_cookies_;
  // Delegate for |url_session_mock_|, provided by |ns_url_session_bridge_|.
  id<NSURLSessionTaskDelegate> url_session_delegate_;

  NSHTTPCookieStorage* http_cookie_storage_mock_;
  NSURLSession* url_session_mock_;
  NSURLSessionDataTask* url_session_data_task_mock_;
  NSURLSessionConfiguration* url_session_configuration_mock_;
  DataTaskWithRequestCompletionHandler completion_handler_;
};

#pragma mark - TestGaiaAuthFetcherIOSNSURLSessionBridge

TestGaiaAuthFetcherIOSNSURLSessionBridge::
    TestGaiaAuthFetcherIOSNSURLSessionBridge(
        GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridgeDelegate* delegate,
        web::BrowserState* browser_state,
        GaiaAuthFetcherIOSNSURLSessionBridgeTest* test)
    : GaiaAuthFetcherIOSNSURLSessionBridge(delegate, browser_state),
      test_(test) {}

NSURLSession* TestGaiaAuthFetcherIOSNSURLSessionBridge::CreateNSURLSession(
    id<NSURLSessionTaskDelegate> url_session_delegate) {
  return test_->CreateNSURLSession(url_session_delegate);
}

#pragma mark - GaiaAuthFetcherIOSNSURLSessionBridgeTest

void GaiaAuthFetcherIOSNSURLSessionBridgeTest::SetUp() {
  std::vector<base::Feature> enabled_features;
  std::vector<base::Feature> disabled_features;
  enabled_features.push_back(kUseNSURLSessionForGaiaSigninRequests);
  scoped_feature_list.InitWithFeatures(enabled_features, disabled_features);
  delegate_.reset(new FakeGaiaAuthFetcherIOSBridgeDelegate());
  browser_state_ = TestChromeBrowserState::Builder().Build();
  ns_url_session_bridge_.reset(new TestGaiaAuthFetcherIOSNSURLSessionBridge(
      delegate_.get(), browser_state_.get(), this));
  http_cookie_storage_mock_ = OCMStrictClassMock([NSHTTPCookieStorage class]);
  url_session_configuration_mock_ =
      OCMStrictClassMock(NSClassFromString(@"__NSCFURLSessionConfiguration"));
  OCMStub([url_session_configuration_mock_ HTTPCookieStorage])
      .andReturn(http_cookie_storage_mock_);
  url_session_mock_ = OCMStrictClassMock([NSURLSession class]);
  OCMStub([url_session_mock_ configuration])
      .andReturn(url_session_configuration_mock_);
  url_session_data_task_mock_ =
      OCMStrictClassMock([NSURLSessionDataTask class]);
  OCMExpect([url_session_data_task_mock_ resume]);
  completion_handler_ = nil;
}

void GaiaAuthFetcherIOSNSURLSessionBridgeTest::TearDown() {
  ASSERT_OCMOCK_VERIFY((id)http_cookie_storage_mock_);
  ASSERT_OCMOCK_VERIFY((id)url_session_mock_);
  ASSERT_OCMOCK_VERIFY((id)url_session_data_task_mock_);
  ASSERT_OCMOCK_VERIFY((id)url_session_configuration_mock_);
}

NSURLSession* GaiaAuthFetcherIOSNSURLSessionBridgeTest::CreateNSURLSession(
    id<NSURLSessionTaskDelegate> url_session_delegate) {
  url_session_delegate_ = url_session_delegate;
  id completion_handler = [OCMArg checkWithBlock:^BOOL(id value) {
    DCHECK(!completion_handler_);
    completion_handler_ = [value copy];
    return YES;
  }];
  OCMExpect([url_session_mock_
                dataTaskWithRequest:ns_url_session_bridge_->GetNSURLRequest()
                  completionHandler:completion_handler])
      .andReturn(url_session_data_task_mock_);
  return url_session_mock_;
}

void GaiaAuthFetcherIOSNSURLSessionBridgeTest::ExpectCookies(
    NSArray<NSHTTPCookie*>* expected_cookies) {
  network::mojom::CookieManager* cookie_manager =
      browser_state_->GetCookieManager();
  cookie_manager->GetAllCookies(
      base::BindOnce(&GaiaAuthFetcherIOSNSURLSessionBridgeTest::AllCookies,
                     base::Unretained(this)));
  WaitForBackgroundTasks();
  NSMutableSet<NSString*>* expected_cookies_set = [NSMutableSet set];
  for (NSHTTPCookie* cookie in expected_cookies) {
    [expected_cookies_set addObject:GetStringWithNSHTTPCookie(cookie)];
  }
  NSMutableSet<NSString*>* cookies_set = [NSMutableSet set];
  for (net::CanonicalCookie cookie : all_cookies_) {
    [cookies_set addObject:GetStringWithCanonicalCookie(cookie)];
  }
  EXPECT_TRUE([expected_cookies_set isEqualToSet:cookies_set]);
}

void GaiaAuthFetcherIOSNSURLSessionBridgeTest::AllCookies(
    const std::vector<net::CanonicalCookie>& all_cookies) {
  all_cookies_ = all_cookies;
}

void GaiaAuthFetcherIOSNSURLSessionBridgeTest::AddCookiesToCookieManager(
    NSArray<NSHTTPCookie*>* cookies) {
  network::mojom::CookieManager* cookie_manager =
      browser_state_->GetCookieManager();
  for (NSHTTPCookie* cookie in cookies) {
    net::CookieOptions options;
    options.set_include_httponly();
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);
    cookie_manager->SetCanonicalCookie(
        net::CanonicalCookieFromSystemCookie(cookie, base::Time::Now()),
        "https", options, base::DoNothing());
  }
  WaitForBackgroundTasks();
}

NSHTTPCookie* GaiaAuthFetcherIOSNSURLSessionBridgeTest::GetCookie1() {
  NSString* cookie_domain =
      [NSString stringWithFormat:@".%s", GetCookieDomain().c_str()];
  return [NSHTTPCookie cookieWithProperties:@{
    NSHTTPCookiePath : @"/",
    NSHTTPCookieName : @"COOKIE1",
    NSHTTPCookieValue : @"VALUE1",
    NSHTTPCookieDomain : cookie_domain,
  }];
}

NSHTTPCookie* GaiaAuthFetcherIOSNSURLSessionBridgeTest::GetCookie2() {
  NSString* cookie_domain =
      [NSString stringWithFormat:@".%s", GetCookieDomain().c_str()];
  return [NSHTTPCookie cookieWithProperties:@{
    NSHTTPCookiePath : @"/",
    NSHTTPCookieName : @"COOKIE2",
    NSHTTPCookieValue : @"VALUE2",
    NSHTTPCookieDomain : cookie_domain,
  }];
}

NSHTTPURLResponse*
GaiaAuthFetcherIOSNSURLSessionBridgeTest::CreateHTTPURLResponse(
    int status_code,
    NSArray<NSHTTPCookie*>* cookies) {
  NSString* url_string =
      [NSString stringWithFormat:@"http://www.%s/", GetCookieDomain().c_str()];
  NSURL* url = [NSURL URLWithString:url_string];
  return [[NSHTTPURLResponse alloc]
       initWithURL:url
        statusCode:status_code
       HTTPVersion:@"HTTP/1.1"
      headerFields:GetHeaderFieldsWithCookies(cookies)];
}

NSDictionary*
GaiaAuthFetcherIOSNSURLSessionBridgeTest::GetHeaderFieldsWithCookies(
    NSArray<NSHTTPCookie*>* cookies) {
  NSMutableString* cookie_string = [NSMutableString string];
  for (NSHTTPCookie* cookie in cookies) {
    if (cookie_string.length != 0)
      [cookie_string appendString:@", "];
    [cookie_string appendString:GetStringWithNSHTTPCookie(cookie)];
  }
  return @{@"Set-Cookie" : cookie_string};
}

#pragma mark - Tests

// Tests to send a request with no cookies set in the cookie store and receive
// multiples cookies from the request.
TEST_F(GaiaAuthFetcherIOSNSURLSessionBridgeTest, FetchWithEmptyCookieStore) {
  ns_url_session_bridge_->Fetch(GetFetchGURL(), "", "", false);
  OCMExpect([http_cookie_storage_mock_
      storeCookies:@[]
           forTask:url_session_data_task_mock_]);
  WaitForBackgroundTasks();
  EXPECT_NE(nullptr, completion_handler_);
  NSHTTPURLResponse* http_url_reponse =
      CreateHTTPURLResponse(200, @[ GetCookie1(), GetCookie2() ]);
  completion_handler_([@"Test" dataUsingEncoding:NSUTF8StringEncoding],
                      http_url_reponse, nil);
  EXPECT_TRUE(delegate_->GetFetchCompleteCalled());
  EXPECT_EQ(delegate_->GetURL(), GetFetchGURL());
  EXPECT_EQ(delegate_->GetStatus().status(), net::URLRequestStatus::SUCCESS);
  EXPECT_EQ(delegate_->GetResponseCode(), 200);
  EXPECT_EQ(delegate_->GetData(), std::string("Test"));
  ExpectCookies(@[ GetCookie1(), GetCookie2() ]);
}

// Tests to send a request with one cookie set in the cookie store and receive
// another cookies from the request.
TEST_F(GaiaAuthFetcherIOSNSURLSessionBridgeTest, FetchWithCookieStore) {
  NSArray* cookies_to_send = @[ GetCookie1() ];
  AddCookiesToCookieManager(cookies_to_send);
  ns_url_session_bridge_->Fetch(GetFetchGURL(), "", "", false);
  OCMExpect([http_cookie_storage_mock_
      storeCookies:cookies_to_send
           forTask:url_session_data_task_mock_]);
  WaitForBackgroundTasks();
  EXPECT_NE(nullptr, completion_handler_);
  NSHTTPURLResponse* http_url_reponse =
      CreateHTTPURLResponse(200, @[ GetCookie2() ]);
  completion_handler_(nil, http_url_reponse, nil);
  EXPECT_TRUE(delegate_->GetFetchCompleteCalled());
  EXPECT_EQ(delegate_->GetURL(), GetFetchGURL());
  EXPECT_EQ(delegate_->GetStatus().status(), net::URLRequestStatus::SUCCESS);
  EXPECT_EQ(delegate_->GetResponseCode(), 200);
  EXPECT_EQ(delegate_->GetData(), std::string());
  ExpectCookies(@[ GetCookie1(), GetCookie2() ]);
}

// Tests to a request with a redirect. One cookie is received by the first
// request, and a second one by the redirected request.
TEST_F(GaiaAuthFetcherIOSNSURLSessionBridgeTest, FetchWithRedirect) {
  ns_url_session_bridge_->Fetch(GetFetchGURL(), "", "", false);
  OCMExpect([http_cookie_storage_mock_
      storeCookies:@[]
           forTask:url_session_data_task_mock_]);
  WaitForBackgroundTasks();
  EXPECT_NE(nullptr, completion_handler_);
  NSURLRequest* redirected_url_request =
      OCMStrictClassMock([NSURLRequest class]);
  __block bool completion_handler_called = false;
  void (^completion_handler)(NSURLRequest*) = ^(NSURLRequest* url_request) {
    EXPECT_EQ(redirected_url_request, url_request);
    completion_handler_called = true;
  };
  NSHTTPURLResponse* redirected_url_response =
      CreateHTTPURLResponse(301, @[ GetCookie1() ]);
  [url_session_delegate_ URLSession:url_session_mock_
                               task:url_session_data_task_mock_
         willPerformHTTPRedirection:redirected_url_response
                         newRequest:redirected_url_request
                  completionHandler:completion_handler];
  EXPECT_TRUE(completion_handler_called);
  NSHTTPURLResponse* http_url_reponse =
      CreateHTTPURLResponse(200, @[ GetCookie2() ]);
  completion_handler_(nil, http_url_reponse, nil);
  EXPECT_TRUE(delegate_->GetFetchCompleteCalled());
  EXPECT_EQ(delegate_->GetURL(), GetFetchGURL());
  EXPECT_EQ(delegate_->GetStatus().status(), net::URLRequestStatus::SUCCESS);
  EXPECT_EQ(delegate_->GetResponseCode(), 200);
  EXPECT_EQ(delegate_->GetData(), std::string());
  ExpectCookies(@[ GetCookie1(), GetCookie2() ]);
  ASSERT_OCMOCK_VERIFY((id)redirected_url_request);
}

// Tests to cancel the request.
TEST_F(GaiaAuthFetcherIOSNSURLSessionBridgeTest, FetchWithCancel) {
  ns_url_session_bridge_->Fetch(GetFetchGURL(), "", "", false);
  OCMExpect([http_cookie_storage_mock_
      storeCookies:@[]
           forTask:url_session_data_task_mock_]);
  WaitForBackgroundTasks();
  EXPECT_NE(nullptr, completion_handler_);
  OCMExpect([url_session_data_task_mock_ cancel]);
  ns_url_session_bridge_->Cancel();
  WaitForBackgroundTasks();
  EXPECT_TRUE(delegate_->GetFetchCompleteCalled());
  EXPECT_EQ(delegate_->GetURL(), GetFetchGURL());
  EXPECT_EQ(delegate_->GetStatus().status(), net::URLRequestStatus::CANCELED);
  EXPECT_EQ(delegate_->GetResponseCode(), 0);
  EXPECT_EQ(delegate_->GetData(), std::string());
}

// Tests a request with error.
TEST_F(GaiaAuthFetcherIOSNSURLSessionBridgeTest, FetchWithError) {
  ns_url_session_bridge_->Fetch(GetFetchGURL(), "", "", false);
  OCMExpect([http_cookie_storage_mock_
      storeCookies:@[]
           forTask:url_session_data_task_mock_]);
  WaitForBackgroundTasks();
  EXPECT_NE(nullptr, completion_handler_);
  NSHTTPURLResponse* http_url_reponse =
      CreateHTTPURLResponse(501, @[ GetCookie1(), GetCookie2() ]);
  completion_handler_(nil, http_url_reponse,
                      [NSError errorWithDomain:@"test" code:1 userInfo:nil]);
  EXPECT_TRUE(delegate_->GetFetchCompleteCalled());
  EXPECT_EQ(delegate_->GetURL(), GetFetchGURL());
  EXPECT_EQ(delegate_->GetStatus().status(), net::URLRequestStatus::FAILED);
  EXPECT_EQ(delegate_->GetResponseCode(), 501);
  EXPECT_EQ(delegate_->GetData(), std::string());
  ExpectCookies(@[]);
}
