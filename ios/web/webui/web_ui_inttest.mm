// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>
#import <string>

#import "base/functional/bind.h"
#import "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/grit/ios_web_resources.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/webui/web_ui_ios_controller.h"
#import "ios/web/public/webui/web_ui_ios_controller_factory.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#import "ios/web/test/grit/test_resources.h"
#import "ios/web/test/test_url_constants.h"
#import "ios/web/web_state/web_state_impl.h"
#import "url/gurl.h"
#import "url/scheme_host_port.h"

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using web::test::TapWebViewElementWithId;
using web::test::WaitForWebViewContainingText;

namespace web {

namespace {

// Hostname for test WebUI page.
const char kTestWebUIURLHost[] = "testwebui";
const char kTestWebUIURLHost2[] = "testwebui2";

// The element id of the link to load a webUI page.
const char kWebUIPageLinkID[] = "link";
// Text present on the sample WebUI page.
const char kWebUIPageText[] = "WebUI page";

// Controller for test WebUI.
class TestUI : public WebUIIOSController {
 public:
  // Constructs controller from `web_ui` and `ui_handler` which will communicate
  // with test WebUI page.
  TestUI(WebUIIOS* web_ui, const std::string& host, int resource_id)
      : WebUIIOSController(web_ui, host) {
    web::WebUIIOSDataSource* source =
        web::WebUIIOSDataSource::Create(kTestWebUIURLHost);

    source->SetDefaultResource(resource_id);

    web::WebState* web_state = web_ui->GetWebState();
    web::WebUIIOSDataSource::Add(web_state->GetBrowserState(), source);
  }

  ~TestUI() override = default;
};

// Factory that creates TestUI controller.
class TestWebUIControllerFactory : public WebUIIOSControllerFactory {
 public:
  // Constructs a controller factory.
  TestWebUIControllerFactory() {}

  // WebUIIOSControllerFactory overrides.
  std::unique_ptr<WebUIIOSController> CreateWebUIIOSControllerForURL(
      WebUIIOS* web_ui,
      const GURL& url) const override {
    if (!url.SchemeIs(kTestWebUIScheme))
      return nullptr;
    if (url.host() == kTestWebUIURLHost) {
      return std::make_unique<TestUI>(web_ui, url.host(), IDR_WEBUI_TEST_HTML);
    }
    DCHECK_EQ(url.host(), kTestWebUIURLHost2);
    return std::make_unique<TestUI>(web_ui, url.host(), IDR_WEBUI_TEST_HTML_2);
  }

  NSInteger GetErrorCodeForWebUIURL(const GURL& url) const override {
    if (url.SchemeIs(kTestWebUIScheme))
      return 0;
    return NSURLErrorUnsupportedURL;
  }
};
}  // namespace

// A test fixture for verifying WebUI.
class WebUITest : public WebTestWithWebState {
 protected:
  WebUITest() : WebTestWithWebState() {}

  void SetUp() override {
    WebTestWithWebState::SetUp();
    factory_ = std::make_unique<TestWebUIControllerFactory>();
    WebUIIOSControllerFactory::RegisterFactory(factory_.get());

    url::SchemeHostPort tuple(kTestWebUIScheme, kTestWebUIURLHost, 0);
    GURL url(tuple.Serialize());
    test::LoadUrl(web_state(), url);

    // LoadIfNecessary is needed because the view is not created (but needed)
    // when loading the page. TODO(crbug.com/41309809): Remove this call.
    web_state()->GetNavigationManager()->LoadIfNecessary();

    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
      base::RunLoop().RunUntilIdle();
      return !web_state()->IsLoading();
    }));

    ASSERT_EQ(url.spec(), BaseUrl());
  }

  void TearDown() override {
    WebUIIOSControllerFactory::DeregisterFactory(factory_.get());
    WebTestWithWebState::TearDown();
  }

 private:
  std::unique_ptr<TestWebUIControllerFactory> factory_;
};

// Tests that a web UI page is loaded and that the WebState correctly reports
// `WebStateImpl::HasWebUI`.
TEST_F(WebUITest, LoadWebUIPage) {
  ASSERT_TRUE(WebStateImpl::FromWebState(web_state())->HasWebUI());

  EXPECT_TRUE(WaitForWebViewContainingText(web_state(), kWebUIPageText));
}

// Tests that a web UI page is correctly loaded when navigated to from another
// webUI page.
TEST_F(WebUITest, LoadWebUIPageLoadViaLinkClick) {
  ASSERT_TRUE(TapWebViewElementWithId(web_state(), kWebUIPageLinkID));

  url::SchemeHostPort tuple(kTestWebUIScheme, kTestWebUIURLHost2, 0);
  GURL url(tuple.Serialize());

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return !web_state()->IsLoading() && url.spec() == BaseUrl();
  }));

  ASSERT_TRUE(WebStateImpl::FromWebState(web_state())->HasWebUI());
}

}  // namespace web
