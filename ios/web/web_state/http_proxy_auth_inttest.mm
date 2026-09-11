// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Network/Network.h>
#import <WebKit/WebKit.h>

#import <atomic>
#import <memory>

#import "base/functional/bind.h"
#import "base/ios/ns_error_util.h"
#import "base/scoped_observation.h"
#import "base/strings/string_number_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "components/enterprise/net/core/features.h"
#import "ios/web/public/browser_state_utils.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_state_delegate.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state_observer.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "testing/gtest_mac.h"
#import "url/gurl.h"

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

namespace {

constexpr char kProxyTestUrl[] = "https://localhost/proxy-test";
constexpr char kExpectedAuthHeader[] = "Basic dGVzdF91c2VyOnRlc3RfcGFzc3dvcmQ=";
NSString* const kTestUser = @"test_user";
NSString* const kTestPassword = @"test_password";

// Observes navigation completion to capture error states.
class TestNavigationObserver : public WebStateObserver {
 public:
  explicit TestNavigationObserver(WebState* web_state)
      : scoped_observation_(this) {
    scoped_observation_.Observe(web_state);
  }
  ~TestNavigationObserver() override = default;

  void DidFinishNavigation(WebState* web_state,
                           NavigationContext* navigation_context) override {
    error_ = navigation_context->GetError();
    navigation_finished_future_.SetValue();
  }

  [[nodiscard]] bool Wait() { return navigation_finished_future_.Wait(); }
  NSError* error() const { return error_; }

 private:
  base::test::TestFuture<void> navigation_finished_future_;
  NSError* error_ = nil;
  base::ScopedObservation<WebState, WebStateObserver> scoped_observation_;
};

// WebStateDelegate implementation that only overrides OnAuthRequired to test
// the default fallback behavior of OnProxyAuthChallenge.
class FallbackAuthWebStateDelegate : public WebStateDelegate {
 public:
  FallbackAuthWebStateDelegate() = default;
  ~FallbackAuthWebStateDelegate() override = default;

  void OnAuthRequired(WebState* source,
                      NSURLProtectionSpace* protection_space,
                      NSURLCredential* credential,
                      HTTPAuthCallback callback) override {
    last_authentication_request_ =
        std::make_unique<FakeAuthenticationRequest>();
    last_authentication_request_->web_state = source;
    last_authentication_request_->protection_space = protection_space;
    last_authentication_request_->credential = credential;
    last_authentication_request_->http_auth_callback = std::move(callback);
  }

  FakeAuthenticationRequest* last_authentication_request() const {
    return last_authentication_request_.get();
  }

 private:
  std::unique_ptr<FakeAuthenticationRequest> last_authentication_request_;
};

}  // namespace

// Test fixture for WebStateDelegate::OnProxyAuthChallenge integration tests.
class HttpProxyAuthTest : public WebTestWithWebState {
 public:
  HttpProxyAuthTest() : HttpProxyAuthTest(/*enable_feature=*/true) {}
  explicit HttpProxyAuthTest(bool enable_feature) {
    if (enable_feature) {
      scoped_feature_list_.InitAndEnableFeature(
          enterprise_net::kEnableDynamicRouteFetching);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          enterprise_net::kEnableDynamicRouteFetching);
    }
  }

 protected:
  std::unique_ptr<BrowserState> CreateBrowserState() override {
    auto browser_state = std::make_unique<FakeBrowserState>();
    if (@available(iOS 18.1, *)) {
      if (proxy_server_.Started()) {
        nw_endpoint_t endpoint = nw_endpoint_create_host(
            proxy_server_.host_port_pair().host().c_str(),
            base::NumberToString(proxy_server_.port()).c_str());
        nw_proxy_config_t proxy_config =
            nw_proxy_config_create_http_connect(endpoint, /*tls_options=*/nil);
        nw_proxy_config_add_match_domain(proxy_config, "localhost");
        nw_proxy_config_add_match_domain(proxy_config, "127.0.0.1");
        WKWebsiteDataStore* data_store =
            GetDataStoreForBrowserState(browser_state.get());
        data_store.proxyConfigurations = @[ proxy_config ];
      }
    }
    return browser_state;
  }

  void SetUp() override {
    if (!@available(iOS 18.1, *)) {
      GTEST_SKIP() << "Proxy auth challenges require iOS 18.1+.";
    }

    proxy_server_.RegisterRequestHandler(base::BindRepeating(
        &HttpProxyAuthTest::HandleProxyAuthRequest, base::Unretained(this)));
    ASSERT_TRUE(proxy_server_.Start());

    WebTestWithWebState::SetUp();
    delegate_ = std::make_unique<FakeWebStateDelegate>();
    web_state()->SetDelegate(delegate_.get());
  }

  void TearDown() override {
    if (web_state()) {
      web_state()->SetDelegate(nullptr);
    }
    delegate_.reset();
    WebTestWithWebState::TearDown();
  }

  // Handles proxy authentication requests by returning HTTP 407 until valid
  // credentials are provided.
  std::unique_ptr<net::test_server::HttpResponse> HandleProxyAuthRequest(
      const net::test_server::HttpRequest& request) {
    auto auth_header_it = request.headers.find("Proxy-Authorization");
    if (auth_header_it == request.headers.end() ||
        auth_header_it->second != kExpectedAuthHeader) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_PROXY_AUTHENTICATION_REQUIRED);
      response->AddCustomHeader("Proxy-Authenticate",
                                "Basic realm=\"TestProxyRealm\"");
      response->AddCustomHeader("Proxy-Connection", "keep-alive");
      response->AddCustomHeader("Connection", "keep-alive");
      response->set_content("<html><body>Proxy Auth Required</body></html>");
      return response;
    }

    proxy_auth_received_ = true;
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    return response;
  }

  // Waits until WebStateDelegate::OnProxyAuthChallenge callback is called.
  [[nodiscard]] bool WaitForOnProxyAuthChallengeCallback() {
    return WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return delegate_->last_proxy_authentication_request() != nullptr;
    });
  }

  std::atomic<bool> proxy_auth_received_ = false;
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer proxy_server_;
  std::unique_ptr<FakeWebStateDelegate> delegate_;
};

// Tests successful proxy basic authentication.
TEST_F(HttpProxyAuthTest, SuccessfulProxyAuth) {
  proxy_auth_received_ = false;
  GURL url(kProxyTestUrl);
  test::LoadUrl(web_state(), url);
  ASSERT_TRUE(WaitForOnProxyAuthChallengeCallback());

  // Verify that callback receives correct WebState and proxy protection space.
  FakeProxyAuthenticationRequest* auth_request =
      delegate_->last_proxy_authentication_request();
  ASSERT_TRUE(auth_request);
  EXPECT_EQ(web_state(), auth_request->web_state);

  NSURLProtectionSpace* protection_space = auth_request->protection_space;
  ASSERT_TRUE(protection_space);
  EXPECT_TRUE([protection_space isProxy]);
  EXPECT_NSEQ(NSURLAuthenticationMethodHTTPBasic,
              protection_space.authenticationMethod);

  ASSERT_TRUE(
      [auth_request->failure_response isKindOfClass:[NSHTTPURLResponse class]]);
  NSHTTPURLResponse* http_response =
      static_cast<NSHTTPURLResponse*>(auth_request->failure_response);
  EXPECT_EQ(net::HTTP_PROXY_AUTHENTICATION_REQUIRED, http_response.statusCode);

  // Provide credentials and verify the proxy server receives the authenticated
  // request.
  ASSERT_TRUE(web_state()->IsLoading());
  std::move(auth_request->proxy_auth_callback)
      .Run(kTestUser, kTestPassword, nil);

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return proxy_auth_received_.load();
  }));
}

// Tests cancelling proxy authentication without an explicit error.
TEST_F(HttpProxyAuthTest, ProxyAuthUserCancelled) {
  TestNavigationObserver observer(web_state());
  GURL url(kProxyTestUrl);
  test::LoadUrl(web_state(), url);
  ASSERT_TRUE(WaitForOnProxyAuthChallengeCallback());

  FakeProxyAuthenticationRequest* auth_request =
      delegate_->last_proxy_authentication_request();
  ASSERT_TRUE(auth_request);

  std::move(auth_request->proxy_auth_callback)
      .Run(/*username=*/nil, /*password=*/nil, /*error=*/nil);

  // TODO(crbug.com/559132741): WebKit automatically retries the proxy CONNECT
  // request after the initial challenge is cancelled or rejected. Until we
  // find a way to avoid WebKit retrying the request, wait for the second
  // challenge and cancel it so the navigation failure can complete cleanly.
  delegate_->ClearLastProxyAuthenticationRequest();
  ASSERT_TRUE(WaitForOnProxyAuthChallengeCallback());
  FakeProxyAuthenticationRequest* retry_request =
      delegate_->last_proxy_authentication_request();
  ASSERT_TRUE(retry_request);
  std::move(retry_request->proxy_auth_callback)
      .Run(/*username=*/nil, /*password=*/nil, /*error=*/nil);

  EXPECT_TRUE(observer.Wait());
}

// Tests cancelling proxy authentication with a custom NSError.
TEST_F(HttpProxyAuthTest, ProxyAuthCustomCancellationError) {
  TestNavigationObserver observer(web_state());
  GURL url(kProxyTestUrl);
  test::LoadUrl(web_state(), url);
  ASSERT_TRUE(WaitForOnProxyAuthChallengeCallback());

  FakeProxyAuthenticationRequest* auth_request =
      delegate_->last_proxy_authentication_request();
  ASSERT_TRUE(auth_request);

  NSError* custom_error =
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorUserCancelledAuthentication
                      userInfo:nil];

  std::move(auth_request->proxy_auth_callback)
      .Run(/*username=*/nil, /*password=*/nil, custom_error);

  // TODO(crbug.com/559132741): WebKit automatically retries the proxy CONNECT
  // request after the initial challenge is cancelled or rejected. Until we
  // find a way to avoid WebKit retrying the request, wait for the second
  // challenge and cancel it with the custom error so the navigation failure
  // can complete cleanly.
  delegate_->ClearLastProxyAuthenticationRequest();
  ASSERT_TRUE(WaitForOnProxyAuthChallengeCallback());
  FakeProxyAuthenticationRequest* retry_request =
      delegate_->last_proxy_authentication_request();
  ASSERT_TRUE(retry_request);
  std::move(retry_request->proxy_auth_callback)
      .Run(/*username=*/nil, /*password=*/nil, custom_error);

  ASSERT_TRUE(observer.Wait());
  EXPECT_NSEQ(custom_error,
              base::ios::GetFinalUnderlyingErrorFromError(observer.error()));
}

// Tests successful proxy basic authentication when WebStateDelegate does not
// override OnProxyAuthChallenge and falls back to OnAuthRequired.
TEST_F(HttpProxyAuthTest, ProxyAuthFallbackToOnAuthRequired) {
  FallbackAuthWebStateDelegate fallback_delegate;
  FallbackAuthWebStateDelegate* fallback_delegate_ptr = &fallback_delegate;
  web_state()->SetDelegate(&fallback_delegate);

  proxy_auth_received_ = false;
  GURL url(kProxyTestUrl);
  test::LoadUrl(web_state(), url);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    return fallback_delegate_ptr->last_authentication_request() != nullptr;
  }));

  FakeAuthenticationRequest* auth_request =
      fallback_delegate.last_authentication_request();
  ASSERT_TRUE(auth_request);
  EXPECT_EQ(web_state(), auth_request->web_state);
  EXPECT_TRUE([auth_request->protection_space isProxy]);

  ASSERT_TRUE(web_state()->IsLoading());
  std::move(auth_request->http_auth_callback).Run(kTestUser, kTestPassword);

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return proxy_auth_received_.load();
  }));

  web_state()->SetDelegate(nullptr);
}

// Test fixture with enterprise_net::kEnableDynamicRouteFetching disabled.
class HttpProxyAuthDisabledFeatureTest : public HttpProxyAuthTest {
 public:
  HttpProxyAuthDisabledFeatureTest()
      : HttpProxyAuthTest(/*enable_feature=*/false) {}
};

// Tests that OnProxyAuthChallenge is not triggered when the feature is
// disabled.
TEST_F(HttpProxyAuthDisabledFeatureTest, ProxyAuthChallengeNotTriggered) {
  TestNavigationObserver observer(web_state());
  GURL url(kProxyTestUrl);
  test::LoadUrl(web_state(), url);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    return delegate_->last_authentication_request() != nullptr;
  }));

  FakeAuthenticationRequest* auth_request =
      delegate_->last_authentication_request();
  ASSERT_TRUE(auth_request);
  EXPECT_TRUE([auth_request->protection_space isProxy]);
  EXPECT_FALSE(delegate_->last_proxy_authentication_request());

  // Cancel the authentication request so WebKit's completion handler is called.
  std::move(auth_request->http_auth_callback).Run(nil, nil);

  // TODO(crbug.com/559132741): WebKit automatically retries the proxy CONNECT
  // request after the initial challenge is cancelled or rejected. Until we
  // find a way to avoid WebKit retrying the request, wait for the second
  // challenge and cancel it so the navigation failure can complete cleanly.
  delegate_->ClearLastAuthenticationRequest();
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    return delegate_->last_authentication_request() != nullptr;
  }));
  FakeAuthenticationRequest* retry_request =
      delegate_->last_authentication_request();
  ASSERT_TRUE(retry_request);
  std::move(retry_request->http_auth_callback).Run(nil, nil);

  EXPECT_TRUE(observer.Wait());
}

}  // namespace web
