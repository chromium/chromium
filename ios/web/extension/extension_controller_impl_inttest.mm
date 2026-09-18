// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <WebKit/WebKit.h>

#import <memory>

#import "base/functional/bind.h"
#import "base/test/test_future.h"
#import "ios/web/extension/extension_controller_impl.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/extension/extension_controller.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace web {

// Integration tests for ExtensionControllerImpl.
class ExtensionControllerImplIntTest : public WebTestWithWebState {
 public:
  ExtensionControllerImplIntTest() = default;
  ~ExtensionControllerImplIntTest() override = default;

 protected:
  std::unique_ptr<BrowserState> CreateBrowserState() override {
    auto browser_state = std::make_unique<FakeBrowserState>();
    if (@available(iOS 18.4, *)) {
      controller_ = ExtensionController::Create();
      ExtensionControllerImpl* impl =
          static_cast<ExtensionControllerImpl*>(controller_.get());

      WKWebViewConfiguration* config = [[WKWebViewConfiguration alloc] init];
      config.webExtensionController = impl->GetWKWebExtensionController();
      WKWebViewConfigurationProvider::FromBrowserState(browser_state.get())
          .ResetWithWebViewConfiguration(config);
    }
    return browser_state;
  }

  API_AVAILABLE(ios(18.4))
  ExtensionController* extension_controller() { return controller_.get(); }

 private:
  API_AVAILABLE(ios(18.4))
  std::unique_ptr<ExtensionController> controller_;
};

// Tests that passing WKWebExtensionController to a WebState sets the JS value
// navigator.globalPrivacyControl to true.
TEST_F(ExtensionControllerImplIntTest, TestJavaScriptValueInWebState)
API_AVAILABLE(ios(18.4)) {
  ExtensionController* controller = extension_controller();
  ASSERT_TRUE(controller);

  base::test::TestFuture<bool> load_future;
  controller->LoadBuiltInExtension(BuiltInExtension::kGPC,
                                   load_future.GetCallback());
  ASSERT_TRUE(load_future.Get());
  EXPECT_TRUE(controller->IsBuiltInExtensionLoaded(BuiltInExtension::kGPC));

  LoadHtml(@"<html><body></body></html>");
  id result = ExecuteJavaScript(@"navigator.globalPrivacyControl");
  EXPECT_NSEQ(@YES, result);
}

// Tests that requests made from a WebState include the Sec-GPC header when
// GPC extension is loaded.
TEST_F(ExtensionControllerImplIntTest, TestRequestHeaderInWebState)
API_AVAILABLE(ios(18.4)) {
  net::EmbeddedTestServer server;
  net::test_server::RegisterDefaultHandlers(&server);
  base::test::TestFuture<net::test_server::HttpRequest> request_future;
  server.RegisterRequestMonitor(base::BindRepeating(
      [](base::RepeatingCallback<void(net::test_server::HttpRequest)> callback,
         const net::test_server::HttpRequest& request) {
        if (request.relative_url == "/echo") {
          callback.Run(request);
        }
      },
      request_future.GetSequenceBoundRepeatingCallback()));
  ASSERT_TRUE(server.Start());

  ExtensionController* controller = extension_controller();
  ASSERT_TRUE(controller);

  base::test::TestFuture<bool> load_future;
  controller->LoadBuiltInExtension(BuiltInExtension::kGPC,
                                   load_future.GetCallback());
  ASSERT_TRUE(load_future.Get());
  EXPECT_TRUE(controller->IsBuiltInExtensionLoaded(BuiltInExtension::kGPC));

  test::LoadUrl(web_state(), server.GetURL("/echo"));
  EXPECT_TRUE(test::WaitForPageToFinishLoading(web_state()));

  net::test_server::HttpRequest request = request_future.Take();
  auto it = request.headers.find("Sec-GPC");
  ASSERT_TRUE(it != request.headers.end());
  EXPECT_EQ(it->second, "1");
}

}  // namespace web
