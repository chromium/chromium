// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <functional>

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "ios/net/protocol_handler_util.h"
#import "ios/testing/embedded_test_server_handlers.h"
#import "ios/web/common/features.h"
#import "ios/web/navigation/web_kit_constants.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/reload_type.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/security/security_style.h"
#import "ios/web/public/security/ssl_status.h"
#import "ios/web/public/test/element_selector.h"
#import "ios/web/public/test/error_test_util.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/fakes/fake_web_state_observer.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/test/test_url_constants.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/net_errors.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "url/gurl.h"

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

namespace {

// Waits for text for and error in NSURLErrorDomain and
// kCFURLErrorNetworkConnectionLost error code.
[[nodiscard]] bool WaitForErrorText(WebState* web_state, const GURL& url) {
  return test::WaitForWebViewContainingText(
      web_state, testing::GetErrorText(
                     web_state, url, web::testing::CreateConnectionLostError(),
                     /*is_post=*/false, /*is_otr=*/false,
                     /*cert_status=*/0));
}

// The error domain and code presented by `TestWebStatePolicyDecider` for
// cancelled navigations.
NSString* const kCancelledNavigationErrorDomain = @"Error domain";
const int kCancelledNavigationErrorCode = 123;
// Creates an error using kCancelledNavigationErrorDomain and
// kCancelledNavigationErrorCode.
NSError* CreateEmbedderError() {
  return [NSError errorWithDomain:kCancelledNavigationErrorDomain
                             code:kCancelledNavigationErrorCode
                         userInfo:nil];
}

// A WebStatePolicyDecider which cancels requests to URLs of the form
// "/echo-query?blocked" and displays an error.
class TestWebStatePolicyDecider : public WebStatePolicyDecider {
 public:
  explicit TestWebStatePolicyDecider(WebState* web_state)
      : WebStatePolicyDecider(web_state),
        path_("/echo-query"),
        allowed_query_("allowed"),
        blocked_request_query_("blocked-request"),
        blocked_response_query_("blocked-response") {}
  ~TestWebStatePolicyDecider() override = default;

  std::string allowed_url_spec() const { return path_ + "?" + allowed_query_; }
  std::string blocked_request_url_spec() const {
    return path_ + "?" + blocked_request_query_;
  }
  std::string blocked_response_url_spec() const {
    return path_ + "?" + blocked_response_query_;
  }

  const std::string& allowed_page_text() const { return allowed_query_; }

  // WebStatePolicyDecider overrides
  void ShouldAllowRequest(NSURLRequest* request,
                          RequestInfo request_info,
                          PolicyDecisionCallback callback) override {
    PolicyDecision decision = PolicyDecision::Allow();
    GURL URL = net::GURLWithNSURL(request.URL);
    if (URL.path() != path_ || URL.query() == blocked_request_query_)
      decision = PolicyDecision::CancelAndDisplayError(CreateEmbedderError());
    std::move(callback).Run(decision);
  }
  void ShouldAllowResponse(NSURLResponse* response,
                           ResponseInfo response_info,
                           PolicyDecisionCallback callback) override {
    PolicyDecision decision = PolicyDecision::Allow();
    GURL URL = net::GURLWithNSURL(response.URL);
    if (URL.path() != path_ || URL.query() != allowed_query_)
      decision = PolicyDecision::CancelAndDisplayError(CreateEmbedderError());
    std::move(callback).Run(decision);
  }

  const std::string path_;
  const std::string allowed_query_;
  const std::string blocked_request_query_;
  const std::string blocked_response_query_;
};

}  // namespace

// Test fixture for error page testing. Error page simply renders the arguments
// passed to WebClient::PrepareErrorPage, so the test also acts as integration
// test for PrepareErrorPage WebClient method.
class ErrorPageTest : public WebTestWithWebState {
 public:
  ErrorPageTest(const ErrorPageTest&) = delete;
  ErrorPageTest& operator=(const ErrorPageTest&) = delete;

 protected:
  ErrorPageTest() : WebTestWithWebState(std::make_unique<FakeWebClient>()) {
    RegisterDefaultHandlers(&server_);
    server_.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/echo-query",
        base::BindRepeating(::testing::HandleEchoQueryOrCloseSocket,
                            std::cref(server_responds_with_content_))));
    server_.RegisterRequestHandler(
        base::BindRepeating(&net::test_server::HandlePrefixedRequest, "/iframe",
                            base::BindRepeating(::testing::HandleIFrame)));
    server_.RegisterRequestHandler(
        base::BindRepeating(&net::test_server::HandlePrefixedRequest, "/form",
                            base::BindRepeating(::testing::HandleForm)));
  }

  void SetUp() override {
    WebTestWithWebState::SetUp();

    web_state_observer_ = std::make_unique<FakeWebStateObserver>(web_state());
    ASSERT_TRUE(server_.Start());
  }

  TestDidChangeVisibleSecurityStateInfo* security_state_info() {
    return web_state_observer_->did_change_visible_security_state_info();
  }

  net::EmbeddedTestServer server_;
  bool server_responds_with_content_ = false;

 private:
  std::unique_ptr<FakeWebStateObserver> web_state_observer_;
};

// Tests that the error page is correctly displayed after navigating back to it
// multiple times. See http://crbug.com/944037 .
TEST_F(ErrorPageTest, BackForwardErrorPage) {
  test::LoadUrl(web_state(), server_.GetURL("/close-socket"));
  ASSERT_TRUE(WaitForErrorText(web_state(), server_.GetURL("/close-socket")));

  test::LoadUrl(web_state(), server_.GetURL("/echo"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "Echo"));

  web_state()->GetNavigationManager()->GoBack();
  ASSERT_TRUE(WaitForErrorText(web_state(), server_.GetURL("/close-socket")));

  web_state()->GetNavigationManager()->GoForward();
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "Echo"));

  web_state()->GetNavigationManager()->GoBack();
  ASSERT_TRUE(WaitForErrorText(web_state(), server_.GetURL("/close-socket")));

  // Make sure that the forward history isn't destroyed.
  web_state()->GetNavigationManager()->GoForward();
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "Echo"));
}

// Tests that reloading a page that is no longer accessible doesn't destroy
// forward history.
TEST_F(ErrorPageTest, ReloadOfflinePage) {
  server_responds_with_content_ = true;

  test::LoadUrl(web_state(), server_.GetURL("/echo-query?foo"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "foo"));

  test::LoadUrl(web_state(), server_.GetURL("/echoall?bar"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "bar"));

  web_state()->GetNavigationManager()->GoBack();
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "foo"));

  server_responds_with_content_ = false;
  web_state()->GetNavigationManager()->Reload(ReloadType::NORMAL,
                                              /*check_for_repost=*/false);

  ASSERT_TRUE(WaitForErrorText(web_state(), server_.GetURL("/echo-query?foo")));
  server_responds_with_content_ = true;

  // Make sure that forward history hasn't been destroyed.
  ASSERT_TRUE(web_state()->GetNavigationManager()->CanGoForward());
  web_state()->GetNavigationManager()->GoForward();
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "bar"));
}

// Loads the URL which fails to load, then sucessfully reloads the page.
TEST_F(ErrorPageTest, ReloadErrorPage) {
  // No response leads to -1005 error code (NSURLErrorNetworkConnectionLost).
  server_responds_with_content_ = false;
  test::LoadUrl(web_state(), server_.GetURL("/echo-query?foo"));
  ASSERT_TRUE(WaitForErrorText(web_state(), server_.GetURL("/echo-query?foo")));
  ASSERT_TRUE(security_state_info());
  ASSERT_TRUE(security_state_info()->visible_ssl_status);
  EXPECT_EQ(SECURITY_STYLE_UNAUTHENTICATED,
            security_state_info()->visible_ssl_status->security_style);

  // Reload the page, which should load without errors.
  server_responds_with_content_ = true;
  web_state()->GetNavigationManager()->Reload(ReloadType::NORMAL,
                                              /*check_for_repost=*/false);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "foo"));
}

// Sucessfully loads the page, stops the server and reloads the page.
TEST_F(ErrorPageTest, ReloadPageAfterServerIsDown) {
  // Sucessfully load the page.
  server_responds_with_content_ = true;
  test::LoadUrl(web_state(), server_.GetURL("/echo-query?foo"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "foo"));

  // Reload the page, no response leads to -1005 error code
  // (NSURLErrorNetworkConnectionLost).
  server_responds_with_content_ = false;
  web_state()->GetNavigationManager()->Reload(ReloadType::NORMAL,
                                              /*check_for_repost=*/false);
  ASSERT_TRUE(WaitForErrorText(web_state(), server_.GetURL("/echo-query?foo")));
  ASSERT_TRUE(security_state_info());
  ASSERT_TRUE(security_state_info()->visible_ssl_status);
  EXPECT_EQ(SECURITY_STYLE_UNAUTHENTICATED,
            security_state_info()->visible_ssl_status->security_style);
}

// Sucessfully loads the page, goes back, stops the server, goes forward and
// reloads.
TEST_F(ErrorPageTest, GoForwardAfterServerIsDownAndReload) {
  // First page loads sucessfully.
  test::LoadUrl(web_state(), server_.GetURL("/echo"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "Echo"));

  // Second page loads sucessfully.
  server_responds_with_content_ = true;
  test::LoadUrl(web_state(), server_.GetURL("/echo-query?foo"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "foo"));

  // Go back to the first page.
  web_state()->GetNavigationManager()->GoBack();
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "Echo"));

#if TARGET_IPHONE_SIMULATOR
  // Go forward. The response will be retrieved from the page cache and will not
  // present the error page. Page cache may not always exist on device (which is
  // more memory constrained), so this part of the test is simulator-only.
  server_responds_with_content_ = false;
  web_state()->GetNavigationManager()->GoForward();
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "foo"));

  // Reload bypasses the cache.
  web_state()->GetNavigationManager()->Reload(ReloadType::NORMAL,
                                              /*check_for_repost=*/false);
  ASSERT_TRUE(WaitForErrorText(web_state(), server_.GetURL("/echo-query?foo")));
  ASSERT_TRUE(security_state_info());
  ASSERT_TRUE(security_state_info()->visible_ssl_status);
  EXPECT_EQ(SECURITY_STYLE_UNAUTHENTICATED,
            security_state_info()->visible_ssl_status->security_style);
#endif  // TARGET_IPHONE_SIMULATOR
}

// Sucessfully loads the page, then loads the URL which fails to load, then
// sucessfully goes back to the first page and goes forward to error page.
// Back-forward navigations are browser-initiated.
TEST_F(ErrorPageTest, GoBackFromErrorPageAndForwardToErrorPage) {
  // First page loads sucessfully.
  test::LoadUrl(web_state(), server_.GetURL("/echo"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "Echo"));

  // Second page fails to load.
  test::LoadUrl(web_state(), server_.GetURL("/close-socket"));
  ASSERT_TRUE(WaitForErrorText(web_state(), server_.GetURL("/close-socket")));

  // Going back should sucessfully load the first page.
  web_state()->GetNavigationManager()->GoBack();
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "Echo"));

  // Going forward fails the load.
  web_state()->GetNavigationManager()->GoForward();
  ASSERT_TRUE(WaitForErrorText(web_state(), server_.GetURL("/close-socket")));
  ASSERT_TRUE(security_state_info());
  ASSERT_TRUE(security_state_info()->visible_ssl_status);
  EXPECT_EQ(SECURITY_STYLE_UNAUTHENTICATED,
            security_state_info()->visible_ssl_status->security_style);
}

// Sucessfully loads the page, then loads the URL which fails to load, then
// sucessfully goes back to the first page and goes forward to error page.
// Back-forward navigations are renderer-initiated.
// TODO(crbug.com/41404136): Re-enable this test.
TEST_F(ErrorPageTest,
       DISABLED_RendererInitiatedGoBackFromErrorPageAndForwardToErrorPage) {
  // First page loads sucessfully.
  test::LoadUrl(web_state(), server_.GetURL("/echo"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "Echo"));

  // Second page fails to load.
  test::LoadUrl(web_state(), server_.GetURL("/close-socket"));
  ASSERT_TRUE(WaitForErrorText(web_state(), server_.GetURL("/close-socket")));

  // Going back should sucessfully load the first page.
  ExecuteJavaScript(@"window.history.back();");
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "Echo"));

  // Going forward fails the load.
  ExecuteJavaScript(@"window.history.forward();");
  ASSERT_TRUE(WaitForErrorText(web_state(), server_.GetURL("/close-socket")));
  ASSERT_TRUE(security_state_info());
  ASSERT_TRUE(security_state_info()->visible_ssl_status);
  EXPECT_EQ(SECURITY_STYLE_UNKNOWN,
            security_state_info()->visible_ssl_status->security_style);
}

// Loads the URL which redirects to unresponsive server.
TEST_F(ErrorPageTest, RedirectToFailingURL) {
  // No response leads to -1005 error code (NSURLErrorNetworkConnectionLost).
  server_responds_with_content_ = false;
  test::LoadUrl(web_state(), server_.GetURL("/server-redirect?echo-query"));
  // Error is displayed after the resdirection to /echo-query.
  ASSERT_TRUE(WaitForErrorText(web_state(), server_.GetURL("/echo-query")));
}

// Loads the page with iframe, and that iframe fails to load. There should be no
// error page if the main frame has sucessfully loaded.
TEST_F(ErrorPageTest, ErrorPageInIFrame) {
  test::LoadUrl(web_state(), server_.GetURL("/iframe?echo-query"));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return test::IsWebViewContainingElement(
        web_state(),
        [ElementSelector selectorWithCSSSelector:"iframe[src*='echo-query']"]);
  }));
}

// Loads the URL with off the record browser state.
TEST_F(ErrorPageTest, OtrError) {
  FakeBrowserState browser_state;
  browser_state.SetOffTheRecord(true);
  WebState::CreateParams params(&browser_state);
  auto web_state = WebState::Create(params);

  // No response leads to -1005 error code (NSURLErrorNetworkConnectionLost).
  server_responds_with_content_ = false;
  test::LoadUrl(web_state.get(), server_.GetURL("/echo-query?foo"));
  // LoadIfNecessary is needed because the view is not created (but needed) when
  // loading the page. TODO(crbug.com/41309809): Remove this call.
  web_state->GetNavigationManager()->LoadIfNecessary();
  ASSERT_TRUE(test::WaitForWebViewContainingText(
      web_state.get(),
      testing::GetErrorText(web_state.get(), server_.GetURL("/echo-query?foo"),
                            web::testing::CreateConnectionLostError(),
                            /*is_post=*/false, /*is_otr=*/true,
                            /*cert_status=*/0)));
}

// Loads the URL with form which fails to submit.
TEST_F(ErrorPageTest, FormSubmissionError) {
  test::LoadUrl(web_state(), server_.GetURL("/form?close-socket"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(),
                                                 ::testing::kTestFormPage));

  // Submit the form using JavaScript.
  ExecuteJavaScript(@"document.getElementById('form').submit();");

  // Error is displayed after the form submission navigation.
  ASSERT_TRUE(test::WaitForWebViewContainingText(
      web_state(), testing::GetErrorText(
                       web_state(), server_.GetURL("/close-socket"),
                       web::testing::CreateConnectionLostError(),
                       /*is_post=*/true, /*is_otr=*/false, /*cert_status=*/0)));
}

// Loads an item and checks that virtualURL and URL after displaying the error
// are correct.
TEST_F(ErrorPageTest, URLAndVirtualURLAfterError) {
  GURL url(server_.GetURL("/close-socket"));
  GURL virtual_url("http://virual_url.test");
  web::NavigationManager::WebLoadParams params(url);
  params.virtual_url = virtual_url;
  web::NavigationManager* manager = web_state()->GetNavigationManager();
  manager->LoadURLWithParams(params);
  manager->LoadIfNecessary();
  ASSERT_TRUE(WaitForErrorText(web_state(), url));

  EXPECT_EQ(url, manager->GetLastCommittedItem()->GetURL());
  EXPECT_EQ(virtual_url, manager->GetLastCommittedItem()->GetVirtualURL());
}

// Tests that an error page is displayed when a WebStatePolicyDecider returns a
// PolicyDecision created with PolicyDecision::CancelAndDisplayError() from
// WebStatePolicyDecider::ShouldAllowRequest() and that the error page loads
// correctly when navigating forward to the error page.
TEST_F(ErrorPageTest, ShouldAllowRequestCancelAndDisplayErrorForwardNav) {
  server_responds_with_content_ = true;

  TestWebStatePolicyDecider policy_decider(web_state());

  // Load successful page.
  GURL allowed_url = server_.GetURL(policy_decider.allowed_url_spec());
  test::LoadUrl(web_state(), allowed_url);
  ASSERT_TRUE(test::WaitForWebViewContainingText(
      web_state(), policy_decider.allowed_page_text()));

  // Load page which is blocked.
  GURL blocked_url = server_.GetURL(policy_decider.blocked_request_url_spec());
  test::LoadUrl(web_state(), blocked_url);
  NSError* error = testing::CreateErrorWithUnderlyingErrorChain(
      {{kCancelledNavigationErrorDomain, kCancelledNavigationErrorCode}});
  std::string error_text =
      testing::GetErrorText(web_state(), blocked_url, error,
                            /*is_post=*/false, /*is_otr=*/false,
                            /*cert_status=*/0);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), error_text));

  // Go back/forward to validate going forward to error page.
  web_state()->GetNavigationManager()->GoBack();
  ASSERT_TRUE(test::WaitForWebViewContainingText(
      web_state(), policy_decider.allowed_page_text()));
  web_state()->GetNavigationManager()->GoForward();
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), error_text));
}

// Tests that an error page is displayed when a WebStatePolicyDecider returns a
// PolicyDecision created with PolicyDecision::CancelAndDisplayError() from
// WebStatePolicyDecider::ShouldAllowRequest() and that the error page loads
// correctly when navigating back to the error page.
TEST_F(ErrorPageTest, ShouldAllowRequestCancelAndDisplayErrorBackNav) {
  server_responds_with_content_ = true;

  TestWebStatePolicyDecider policy_decider(web_state());

  // Load page which is blocked.
  GURL blocked_url = server_.GetURL(policy_decider.blocked_request_url_spec());
  test::LoadUrl(web_state(), blocked_url);
  NSError* error = testing::CreateErrorWithUnderlyingErrorChain(
      {{kCancelledNavigationErrorDomain, kCancelledNavigationErrorCode}});
  std::string error_text =
      testing::GetErrorText(web_state(), blocked_url, error,
                            /*is_post=*/false, /*is_otr=*/false,
                            /*cert_status=*/0);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), error_text));

  // Load successful page.
  GURL allowed_url = server_.GetURL(policy_decider.allowed_url_spec());
  test::LoadUrl(web_state(), allowed_url);
  ASSERT_TRUE(test::WaitForWebViewContainingText(
      web_state(), policy_decider.allowed_page_text()));

  // Go back to validate going back to error page.
  web_state()->GetNavigationManager()->GoBack();
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), error_text));
}

// Tests that an error page is displayed when a WebStatePolicyDecider executes
// the PolicyDecisionCallback with PolicyDecision::CancelAndDisplayError() from
// WebStatePolicyDecider::ShouldAllowResponse() and that the error page loads
// correctly when navigating forward to the error page.
TEST_F(ErrorPageTest, ShouldAllowResponseCancelAndDisplayErrorForwardNav) {
  server_responds_with_content_ = true;
  TestWebStatePolicyDecider policy_decider(web_state());

  // Load successful page.
  GURL allowed_url = server_.GetURL(policy_decider.allowed_url_spec());
  test::LoadUrl(web_state(), allowed_url);
  ASSERT_TRUE(test::WaitForWebViewContainingText(
      web_state(), policy_decider.allowed_page_text()));

  // Load page which is blocked.
  GURL blocked_url = server_.GetURL(policy_decider.blocked_response_url_spec());
  test::LoadUrl(web_state(), blocked_url);
  NSError* error = testing::CreateErrorWithUnderlyingErrorChain(
      {{base::SysUTF8ToNSString(kWebKitErrorDomain),
        kWebKitErrorFrameLoadInterruptedByPolicyChange},
       {net::kNSErrorDomain, net::ERR_FAILED},
       {kCancelledNavigationErrorDomain, kCancelledNavigationErrorCode}});
  std::string error_text =
      testing::GetErrorText(web_state(), blocked_url, error,
                            /*is_post=*/false, /*is_otr=*/false,
                            /*cert_status=*/0);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), error_text));

  // Go back/forward to validate going forward to error page.
  web_state()->GetNavigationManager()->GoBack();
  ASSERT_TRUE(test::WaitForWebViewContainingText(
      web_state(), policy_decider.allowed_page_text()));
  web_state()->GetNavigationManager()->GoForward();
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), error_text));
}

// Tests that an error page is displayed when a WebStatePolicyDecider executes
// the PolicyDecisionCallback with PolicyDecision::CancelAndDisplayError() from
// WebStatePolicyDecider::ShouldAllowResponse() and that the error page loads
// correctly when navigating back to the error page.
TEST_F(ErrorPageTest, ShouldAllowResponseCancelAndDisplayErrorBackNav) {
  server_responds_with_content_ = true;
  TestWebStatePolicyDecider policy_decider(web_state());

  // Load page which is blocked.
  GURL blocked_url = server_.GetURL(policy_decider.blocked_response_url_spec());
  test::LoadUrl(web_state(), blocked_url);
  NSError* error = testing::CreateErrorWithUnderlyingErrorChain(
      {{base::SysUTF8ToNSString(kWebKitErrorDomain),
        kWebKitErrorFrameLoadInterruptedByPolicyChange},
       {net::kNSErrorDomain, net::ERR_FAILED},
       {kCancelledNavigationErrorDomain, kCancelledNavigationErrorCode}});
  std::string error_text =
      testing::GetErrorText(web_state(), blocked_url, error,
                            /*is_post=*/false, /*is_otr=*/false,
                            /*cert_status=*/0);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), error_text));

  // Load successful page.
  GURL allowed_url = server_.GetURL(policy_decider.allowed_url_spec());
  test::LoadUrl(web_state(), allowed_url);
  ASSERT_TRUE(test::WaitForWebViewContainingText(
      web_state(), policy_decider.allowed_page_text()));

  // Go back to validate going back to error page.
  web_state()->GetNavigationManager()->GoBack();
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), error_text));
}

// Tests that restoring an invalid WebUI URL doesn't create a new navigation.
TEST_F(ErrorPageTest, RestorationFromInvalidURL) {
  server_responds_with_content_ = true;

  std::string scheme = kTestWebUIScheme;
  GURL invalid_webui = GURL(scheme + "://invalid");

  NSError* error = testing::CreateErrorWithUnderlyingErrorChain(
      {{@"NSURLErrorDomain", NSURLErrorUnsupportedURL},
       {net::kNSErrorDomain, net::ERR_INVALID_URL}});

  test::LoadUrl(web_state(), server_.GetURL("/echo-query?foo"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "foo"));
  test::LoadUrl(web_state(), invalid_webui);
  ASSERT_TRUE(test::WaitForWebViewContainingText(
      web_state(), testing::GetErrorText(web_state(), invalid_webui, error,
                                         /*is_post=*/false, /*is_otr=*/false,
                                         /*cert_status=*/0)));

  // Use Clone() to serialize and then load the session.
  auto cloned_web_state = web_state()->Clone();
  cloned_web_state->GetNavigationManager()->LoadIfNecessary();
  ASSERT_TRUE(test::WaitForWebViewContainingText(
      cloned_web_state.get(),
      testing::GetErrorText(cloned_web_state.get(), invalid_webui, error,
                            /*is_post=*/false, /*is_otr=*/false,
                            /*cert_status=*/0)));

  // Check that there is one item in the back list and no forward item.
  EXPECT_EQ(
      1UL, cloned_web_state->GetNavigationManager()->GetBackwardItems().size());
  EXPECT_EQ(0UL,
            cloned_web_state->GetNavigationManager()->GetForwardItems().size());

  cloned_web_state->GetNavigationManager()->GoBack();
  ASSERT_TRUE(
      test::WaitForWebViewContainingText(cloned_web_state.get(), "foo"));

  // Check that there is one item in the forward list and no back item.
  EXPECT_EQ(
      0UL, cloned_web_state->GetNavigationManager()->GetBackwardItems().size());
  EXPECT_EQ(1UL,
            cloned_web_state->GetNavigationManager()->GetForwardItems().size());
}

}  // namespace web
