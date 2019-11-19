// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "ios/testing/embedded_test_server_handlers.h"
#include "ios/web/common/features.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/navigation/reload_type.h"
#include "ios/web/public/security/security_style.h"
#include "ios/web/public/security/ssl_status.h"
#include "ios/web/public/test/element_selector.h"
#import "ios/web/public/test/error_test_util.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#import "ios/web/public/test/fakes/test_web_client.h"
#include "ios/web/public/test/fakes/test_web_state_observer.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/request_handler_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

namespace {

// Waits for text for and error in NSURLErrorDomain and
// kCFURLErrorNetworkConnectionLost error code.
bool WaitForErrorText(WebState* web_state, const GURL& url) WARN_UNUSED_RESULT;
bool WaitForErrorText(WebState* web_state, const GURL& url) {
  return test::WaitForWebViewContainingText(
      web_state,
      testing::GetErrorText(web_state, url, "NSURLErrorDomain",
                            /*error_code=*/NSURLErrorNetworkConnectionLost,
                            /*is_post=*/false, /*is_otr=*/false,
                            /*has_ssl_info=*/false));
}
}  // namespace

// ErrorPageTest is parameterized on this enum to test both
// LegacyNavigationManagerImpl and WKBasedNavigationManagerImpl.
enum class NavigationManagerChoice {
  LEGACY,
  WK_BASED,
};

// Test fixture for error page testing. Error page simply renders the arguments
// passed to WebClient::PrepareErrorPage, so the test also acts as integration
// test for PrepareErrorPage WebClient method.
class ErrorPageTest
    : public WebTestWithWebState,
      public ::testing::WithParamInterface<NavigationManagerChoice> {
 protected:
  ErrorPageTest() : WebTestWithWebState(std::make_unique<TestWebClient>()) {
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

    if (GetParam() == NavigationManagerChoice::LEGACY) {
      scoped_feature_list_.InitAndDisableFeature(
          web::features::kSlimNavigationManager);
    } else {
      scoped_feature_list_.InitAndEnableFeature(
          web::features::kSlimNavigationManager);
    }
  }

  void SetUp() override {
    WebTestWithWebState::SetUp();

    web_state_observer_ = std::make_unique<TestWebStateObserver>(web_state());
    ASSERT_TRUE(server_.Start());
  }

  TestDidChangeVisibleSecurityStateInfo* security_state_info() {
    return web_state_observer_->did_change_visible_security_state_info();
  }

  net::EmbeddedTestServer server_;
  bool server_responds_with_content_ = false;

 private:
  std::unique_ptr<TestWebStateObserver> web_state_observer_;
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(ErrorPageTest);
};

// Tests that the error page is correctly displayed after navigating back to it
// multiple times. See http://crbug.com/944037 .
// TODO(crbug.com/954231): this test is flaky on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_BackForwardErrorPage BackForwardErrorPage
#else
#define MAYBE_BackForwardErrorPage FLAKY_BackForwardErrorPage
#endif
TEST_P(ErrorPageTest, MAYBE_BackForwardErrorPage) {
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
TEST_P(ErrorPageTest, ReloadOfflinePage) {
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
TEST_P(ErrorPageTest, ReloadErrorPage) {
  // No response leads to -1005 error code (NSURLErrorNetworkConnectionLost).
  server_responds_with_content_ = false;
  test::LoadUrl(web_state(), server_.GetURL("/echo-query?foo"));
  ASSERT_TRUE(WaitForErrorText(web_state(), server_.GetURL("/echo-query?foo")));
  ASSERT_FALSE(security_state_info());

  // Reload the page, which should load without errors.
  server_responds_with_content_ = true;
  web_state()->GetNavigationManager()->Reload(ReloadType::NORMAL,
                                              /*check_for_repost=*/false);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "foo"));
}

// Sucessfully loads the page, stops the server and reloads the page.
TEST_P(ErrorPageTest, ReloadPageAfterServerIsDown) {
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
  EXPECT_EQ(SECURITY_STYLE_UNKNOWN,
            security_state_info()->visible_ssl_status->security_style);
}

// Sucessfully loads the page, goes back, stops the server, goes forward and
// reloads.
TEST_P(ErrorPageTest, GoForwardAfterServerIsDownAndReload) {
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
  EXPECT_EQ(SECURITY_STYLE_UNKNOWN,
            security_state_info()->visible_ssl_status->security_style);
#endif  // TARGET_IPHONE_SIMULATOR
}

// Sucessfully loads the page, then loads the URL which fails to load, then
// sucessfully goes back to the first page and goes forward to error page.
// Back-forward navigations are browser-initiated.
TEST_P(ErrorPageTest, GoBackFromErrorPageAndForwardToErrorPage) {
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
  EXPECT_EQ(SECURITY_STYLE_UNKNOWN,
            security_state_info()->visible_ssl_status->security_style);
}

// Sucessfully loads the page, then loads the URL which fails to load, then
// sucessfully goes back to the first page and goes forward to error page.
// Back-forward navigations are renderer-initiated.
TEST_P(ErrorPageTest,
       RendererInitiatedGoBackFromErrorPageAndForwardToErrorPage) {
  if (GetParam() == NavigationManagerChoice::WK_BASED) {
    // TODO(crbug.com/867927): Re-enable this test.
    return;
  }

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
TEST_P(ErrorPageTest, RedirectToFailingURL) {
  // No response leads to -1005 error code (NSURLErrorNetworkConnectionLost).
  server_responds_with_content_ = false;
  test::LoadUrl(web_state(), server_.GetURL("/server-redirect?echo-query"));
  // Error is displayed after the resdirection to /echo-query.
  ASSERT_TRUE(WaitForErrorText(web_state(), server_.GetURL("/echo-query")));
}

// Loads the page with iframe, and that iframe fails to load. There should be no
// error page if the main frame has sucessfully loaded.
TEST_P(ErrorPageTest, ErrorPageInIFrame) {
  test::LoadUrl(web_state(), server_.GetURL("/iframe?echo-query"));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return test::IsWebViewContainingElement(
        web_state(),
        [ElementSelector selectorWithCSSSelector:"iframe[src*='echo-query']"]);
  }));
}

// Loads the URL with off the record browser state.
TEST_P(ErrorPageTest, OtrError) {
  TestBrowserState browser_state;
  browser_state.SetOffTheRecord(true);
  WebState::CreateParams params(&browser_state);
  auto web_state = WebState::Create(params);

  // No response leads to -1005 error code (NSURLErrorNetworkConnectionLost).
  server_responds_with_content_ = false;
  test::LoadUrl(web_state.get(), server_.GetURL("/echo-query?foo"));
  // LoadIfNecessary is needed because the view is not created (but needed) when
  // loading the page. TODO(crbug.com/705819): Remove this call.
  web_state->GetNavigationManager()->LoadIfNecessary();
  ASSERT_TRUE(test::WaitForWebViewContainingText(
      web_state.get(),
      testing::GetErrorText(web_state.get(), server_.GetURL("/echo-query?foo"),
                            "NSURLErrorDomain",
                            /*error_code=*/NSURLErrorNetworkConnectionLost,
                            /*is_post=*/false, /*is_otr=*/true,
                            /*has_ssl_info=*/false)));
}

// Loads the URL with form which fails to submit.
TEST_P(ErrorPageTest, FormSubmissionError) {
  test::LoadUrl(web_state(), server_.GetURL("/form?close-socket"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(),
                                                 ::testing::kTestFormPage));

  // Submit the form using JavaScript.
  ExecuteJavaScript(@"document.getElementById('form').submit();");

  // Error is displayed after the form submission navigation.
  ASSERT_TRUE(test::WaitForWebViewContainingText(
      web_state(),
      testing::GetErrorText(
          web_state(), server_.GetURL("/close-socket"), "NSURLErrorDomain",
          /*error_code=*/NSURLErrorNetworkConnectionLost,
          /*is_post=*/true, /*is_otr=*/false, /*has_ssl_info=*/false)));
}

// Loads an item and checks that virtualURL and URL after displaying the error
// are correct.
TEST_P(ErrorPageTest, URLAndVirtualURLAfterError) {
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

INSTANTIATE_TEST_SUITE_P(ProgrammaticErrorPageTest,
                         ErrorPageTest,
                         ::testing::Values(NavigationManagerChoice::LEGACY,
                                           NavigationManagerChoice::WK_BASED));
}  // namespace web
