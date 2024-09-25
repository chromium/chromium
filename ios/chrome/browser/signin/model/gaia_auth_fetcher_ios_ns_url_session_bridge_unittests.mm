// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/gaia_auth_fetcher_ios_ns_url_session_bridge.h"

#import <Foundation/Foundation.h>

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/gaia_auth_fetcher_ios_bridge.h"
#import "ios/net/cookies/system_cookie_util.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/net_errors.h"
#import "net/cookies/cookie_access_result.h"
#import "net/cookies/cookie_store.h"
#import "net/cookies/cookie_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

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
                       net::Error net_error,
                       int response_code) override {
    EXPECT_FALSE(fetch_complete_called_);
    fetch_complete_called_ = true;
    url_ = url;
    data_ = data;
    net_error_ = net_error;
    response_code_ = response_code;
  }

  // Returns true if has been called().
  bool GetFetchCompleteCalled() const { return fetch_complete_called_; }

  // Returns `url` from FetchComplete().
  const GURL& GetURL() const { return url_; }

  // Returns `data` from FetchComplete().
  const std::string& GetData() const { return data_; }

  // Returns `net_error` from FetchComplete().
  net::Error GetNetError() const { return net_error_; }

  // Returns `response_code` from FetchComplete().
  int GetResponseCode() const { return response_code_; }

 private:
  // true if has been called().
  bool fetch_complete_called_;
  // `url` from FetchComplete().
  GURL url_;
  // `data` from FetchComplete().
  std::string data_;
  // `net_error` from FetchComplete().
  net::Error net_error_;
  // `response_code` from FetchComplete().
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
  raw_ptr<GaiaAuthFetcherIOSNSURLSessionBridgeTest> test_;
};

}  // namespace

class GaiaAuthFetcherIOSNSURLSessionBridgeTest : public PlatformTest {
 protected:
  void SetUp() override;
  void TearDown() override;

  // Create a NSURLSession mock, and saves its delegate.
  NSURLSession* CreateNSURLSession(
      id<NSURLSessionTaskDelegate> url_session_delegate);

  void ExpectCookies(NSArray<NSHTTPCookie*>* cookies);

  std::vector<net::CanonicalCookie> GetCookiesInCookieJar();

  bool AddAllCookiesInCookieManager(
      network::mojom::CookieManager* cookie_manager,
      const net::CanonicalCookie& cookie);

  bool SetCookiesInCookieManager(NSArray<NSHTTPCookie*>* cookies);

  std::string GetCookieDomain() { return std::string("example.com"); }
  GURL GetFetchGURL() { return GURL("https://www." + GetCookieDomain()); }

  NSHTTPCookie* GetCookie1();

  NSHTTPCookie* GetCookie2();

  NSHTTPURLResponse* CreateHTTPURLResponse(int status_code,
                                           NSArray<NSHTTPCookie*>* cookies);

  NSDictionary* GetHeaderFieldsWithCookies(NSArray<NSHTTPCookie*>* cookies);

  bool FetchURL(const GURL& url);

  friend TestGaiaAuthFetcherIOSNSURLSessionBridge;

  web::WebState* web_state() { return web_state_.get(); }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
  // Instance used for the tests.
  std::unique_ptr<TestGaiaAuthFetcherIOSNSURLSessionBridge>
      ns_url_session_bridge_;
  // Fake delegate for `ns_url_session_bridge_`.
  std::unique_ptr<FakeGaiaAuthFetcherIOSBridgeDelegate> delegate_;
  // Delegate for `url_session_mock_`, provided by `ns_url_session_bridge_`.
  id<NSURLSessionTaskDelegate> url_session_delegate_;

  NSURLSession* url_session_mock_;
  NSURLSessionDataTask* url_session_data_task_;
  NSURLSessionConfiguration* url_session_configuration_;
  DataTaskWithRequestCompletionHandler completion_handler_;

 private:
  base::OnceClosure quit_closure_;
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
  PlatformTest::SetUp();

  profile_ = TestProfileIOS::Builder().Build();

  web::WebState::CreateParams params(profile_.get());
  web_state_ = web::WebState::Create(params);
  web_state_->GetView();
  web_state_->SetKeepRenderProcessAlive(true);

  delegate_.reset(new FakeGaiaAuthFetcherIOSBridgeDelegate());
  ns_url_session_bridge_.reset(new TestGaiaAuthFetcherIOSNSURLSessionBridge(
      delegate_.get(), profile_.get(), this));
  url_session_configuration_ =
      NSURLSessionConfiguration.ephemeralSessionConfiguration;
  url_session_configuration_.HTTPShouldSetCookies = YES;

  url_session_mock_ = OCMStrictClassMock([NSURLSession class]);
  OCMStub([url_session_mock_ configuration])
      .andReturn(url_session_configuration_);
  url_session_data_task_ = [[NSURLSession sharedSession]
        dataTaskWithURL:net::NSURLWithGURL(GetFetchGURL())
      completionHandler:^(NSData* data, NSURLResponse* response,
                          NSError* error) {
        // Asynchronously returns from FetchURL() call after
        // NSURLSessionDataTask:resume.
        std::move(quit_closure_).Run();
      }];
  completion_handler_ = nil;
}

void GaiaAuthFetcherIOSNSURLSessionBridgeTest::TearDown() {
  ASSERT_OCMOCK_VERIFY((id)url_session_mock_);
  web_state_.reset();
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
      .andReturn(url_session_data_task_);
  return url_session_mock_;
}

std::vector<net::CanonicalCookie>
GaiaAuthFetcherIOSNSURLSessionBridgeTest::GetCookiesInCookieJar() {
  std::vector<net::CanonicalCookie> cookies_out;
  base::RunLoop run_loop;
  network::mojom::CookieManager* cookie_manager = profile_->GetCookieManager();
  cookie_manager->GetAllCookies(base::BindLambdaForTesting(
      [&run_loop,
       &cookies_out](const std::vector<net::CanonicalCookie>& cookies) {
        cookies_out = cookies;
        run_loop.Quit();
      }));
  run_loop.Run();

  return cookies_out;
}

void GaiaAuthFetcherIOSNSURLSessionBridgeTest::ExpectCookies(
    NSArray<NSHTTPCookie*>* expected_cookies) {
  std::vector<net::CanonicalCookie> actual_cookies = GetCookiesInCookieJar();

  NSMutableSet<NSString*>* expected_cookies_set = [NSMutableSet set];
  for (NSHTTPCookie* cookie in expected_cookies) {
    [expected_cookies_set addObject:GetStringWithNSHTTPCookie(cookie)];
  }
  NSMutableSet<NSString*>* actual_cookies_set = [NSMutableSet set];
  for (net::CanonicalCookie cookie : actual_cookies) {
    [actual_cookies_set addObject:GetStringWithCanonicalCookie(cookie)];
  }
  EXPECT_TRUE([expected_cookies_set isEqualToSet:actual_cookies_set])
      << base::SysNSStringToUTF8(
             [NSString stringWithFormat:@"expected = %@", expected_cookies_set])
      << base::SysNSStringToUTF8(
             [NSString stringWithFormat:@"\nactual = %@", actual_cookies_set]);
}

bool GaiaAuthFetcherIOSNSURLSessionBridgeTest::AddAllCookiesInCookieManager(
    network::mojom::CookieManager* cookie_manager,
    const net::CanonicalCookie& cookie) {
  base::RunLoop run_loop;
  net::CookieAccessResult result_out;
  net::CookieOptions options;
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusiveForSet());
  options.set_include_httponly();
  cookie_manager->SetCanonicalCookie(
      cookie, net::cookie_util::SimulatedCookieSource(cookie, "https"), options,
      base::BindLambdaForTesting(
          [&run_loop, &result_out](net::CookieAccessResult result) {
            result_out = result;
            run_loop.Quit();
          }));

  run_loop.Run();

  if (!result_out.status.IsInclude())
    LOG(ERROR) << "Failed to set cookie in cookie jar: " << result_out.status;

  return result_out.status.IsInclude();
}

bool GaiaAuthFetcherIOSNSURLSessionBridgeTest::SetCookiesInCookieManager(
    NSArray<NSHTTPCookie*>* cookies) {
  network::mojom::CookieManager* cookie_manager = profile_->GetCookieManager();
  for (NSHTTPCookie* cookie in cookies) {
    std::unique_ptr<net::CanonicalCookie> canonical_cookie =
        net::CanonicalCookieFromSystemCookie(cookie, base::Time::Now());
    if (!canonical_cookie)
      continue;
    if (!AddAllCookiesInCookieManager(cookie_manager,
                                      *std::move(canonical_cookie)))
      return false;
  }
  return true;
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
      [NSString stringWithFormat:@"https://www.%s/", GetCookieDomain().c_str()];
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

bool GaiaAuthFetcherIOSNSURLSessionBridgeTest::FetchURL(const GURL& url) {
  DCHECK(url_session_data_task_);
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  ns_url_session_bridge_->Fetch(url, "", "", false);
  run_loop.Run();
  return true;
}

#pragma mark - Tests

// Tests to send a request with no cookies set in the cookie store and receive
// multiples cookies from the request.
TEST_F(GaiaAuthFetcherIOSNSURLSessionBridgeTest, FetchWithEmptyCookieStore) {
  ASSERT_FALSE(url_session_configuration_.HTTPCookieStorage.cookies.count);
  ASSERT_TRUE(FetchURL(GetFetchGURL()));
  ASSERT_TRUE(completion_handler_);

  NSHTTPURLResponse* http_url_reponse =
      CreateHTTPURLResponse(200, @[ GetCookie1(), GetCookie2() ]);
  completion_handler_([@"Test" dataUsingEncoding:NSUTF8StringEncoding],
                      http_url_reponse, nil);
  EXPECT_TRUE(delegate_->GetFetchCompleteCalled());
  EXPECT_EQ(delegate_->GetURL(), GetFetchGURL());
  EXPECT_EQ(delegate_->GetNetError(), net::OK);
  EXPECT_EQ(delegate_->GetResponseCode(), 200);
  EXPECT_EQ(delegate_->GetData(), std::string("Test"));
  ExpectCookies(@[ GetCookie1(), GetCookie2() ]);
}

// Tests to send a request with one cookie set in the cookie store and receive
// another cookies from the request.
TEST_F(GaiaAuthFetcherIOSNSURLSessionBridgeTest, FetchWithCookieStore) {
  NSHTTPCookie* cookie_to_send = GetCookie1();
  ASSERT_TRUE(SetCookiesInCookieManager(@[ cookie_to_send ]));

  ASSERT_TRUE(FetchURL(GetFetchGURL()));

  EXPECT_EQ(url_session_configuration_.HTTPCookieStorage.cookies.count, 1ul);
  // Check that sent cookie is equal to the cookie in the storage.
  EXPECT_NSEQ(GetStringWithNSHTTPCookie(cookie_to_send),
              GetStringWithNSHTTPCookie(
                  url_session_configuration_.HTTPCookieStorage.cookies[0]));

  ASSERT_TRUE(completion_handler_);

  NSHTTPURLResponse* http_url_reponse =
      CreateHTTPURLResponse(200, @[ GetCookie2() ]);
  completion_handler_(nil, http_url_reponse, nil);
  EXPECT_TRUE(delegate_->GetFetchCompleteCalled());
  EXPECT_EQ(delegate_->GetURL(), GetFetchGURL());
  EXPECT_EQ(delegate_->GetNetError(), net::OK);
  EXPECT_EQ(delegate_->GetResponseCode(), 200);
  EXPECT_EQ(delegate_->GetData(), std::string());
  ExpectCookies(@[ GetCookie1(), GetCookie2() ]);
}

// Tests to a request with a redirect. One cookie is received by the first
// request, and a second one by the redirected request.
TEST_F(GaiaAuthFetcherIOSNSURLSessionBridgeTest, FetchWithRedirect) {
  ASSERT_TRUE(FetchURL(GetFetchGURL()));
  ASSERT_FALSE(url_session_configuration_.HTTPCookieStorage.cookies.count);
  ASSERT_TRUE(completion_handler_);

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
                               task:url_session_data_task_
         willPerformHTTPRedirection:redirected_url_response
                         newRequest:redirected_url_request
                  completionHandler:completion_handler];
  EXPECT_TRUE(completion_handler_called);
  NSHTTPURLResponse* http_url_reponse =
      CreateHTTPURLResponse(200, @[ GetCookie2() ]);
  completion_handler_(nil, http_url_reponse, nil);
  EXPECT_TRUE(delegate_->GetFetchCompleteCalled());
  EXPECT_EQ(delegate_->GetURL(), GetFetchGURL());
  EXPECT_EQ(delegate_->GetNetError(), net::OK);
  EXPECT_EQ(delegate_->GetResponseCode(), 200);
  EXPECT_EQ(delegate_->GetData(), std::string());
  ExpectCookies(@[ GetCookie1(), GetCookie2() ]);
  ASSERT_OCMOCK_VERIFY((id)redirected_url_request);
}

// Tests to cancel the request.
TEST_F(GaiaAuthFetcherIOSNSURLSessionBridgeTest, FetchWithCancel) {
  ASSERT_TRUE(FetchURL(GetFetchGURL()));
  ASSERT_FALSE(url_session_configuration_.HTTPCookieStorage.cookies.count);
  ASSERT_TRUE(completion_handler_);

  ns_url_session_bridge_->Cancel();
  EXPECT_TRUE(delegate_->GetFetchCompleteCalled());
  EXPECT_EQ(delegate_->GetURL(), GetFetchGURL());
  EXPECT_EQ(delegate_->GetNetError(), net::ERR_ABORTED);
  EXPECT_EQ(delegate_->GetResponseCode(), 0);
  EXPECT_EQ(delegate_->GetData(), std::string());
}

// Tests a request with error.
TEST_F(GaiaAuthFetcherIOSNSURLSessionBridgeTest, FetchWithError) {
  ASSERT_TRUE(FetchURL(GetFetchGURL()));
  ASSERT_FALSE(url_session_configuration_.HTTPCookieStorage.cookies.count);
  ASSERT_TRUE(completion_handler_);

  NSHTTPURLResponse* http_url_reponse =
      CreateHTTPURLResponse(501, @[ GetCookie1(), GetCookie2() ]);
  completion_handler_(nil, http_url_reponse,
                      [NSError errorWithDomain:@"test" code:1 userInfo:nil]);
  EXPECT_TRUE(delegate_->GetFetchCompleteCalled());
  EXPECT_EQ(delegate_->GetURL(), GetFetchGURL());
  EXPECT_EQ(delegate_->GetNetError(), net::ERR_FAILED);
  EXPECT_EQ(delegate_->GetResponseCode(), 501);
  EXPECT_EQ(delegate_->GetData(), std::string());
  ExpectCookies(@[]);
}
