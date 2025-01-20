// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/internal_webui_config.h"

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "content/test/test_content_browser_client.h"

namespace content {

namespace {

const char kTestWebUIHost[] = "test";
const char kTestWebUIURL[] = "chrome://test";
const char kTestUserFacingWebUIHost[] = "test-user-facing";
const char kTestUserFacingWebUIURL[] = "chrome://test-user-facing";

class TestInternalOverrideWebUIController : public WebUIController {
 public:
  explicit TestInternalOverrideWebUIController(WebUI* web_ui)
      : WebUIController(web_ui) {
    web_ui->OverrideTitle(u"internal pages are disabled");
  }
};

class TestWebUIController : public WebUIController {
 public:
  explicit TestWebUIController(WebUI* web_ui) : WebUIController(web_ui) {
    web_ui->OverrideTitle(u"test title");
  }
};

class TestInternalWebUIConfig
    : public DefaultInternalWebUIConfig<TestWebUIController> {
 public:
  TestInternalWebUIConfig() : DefaultInternalWebUIConfig(kTestWebUIHost) {}
};

class TestUserFacingWebUIController : public WebUIController {
 public:
  explicit TestUserFacingWebUIController(content::WebUI* web_ui)
      : WebUIController(web_ui) {}
};

class TestUserFacingWebUIConfig
    : public DefaultWebUIConfig<TestUserFacingWebUIController> {
 public:
  TestUserFacingWebUIConfig()
      : DefaultWebUIConfig(kChromeUIScheme, kTestUserFacingWebUIHost) {}
};

}  // namespace

class InternalWebUIConfigTest : public RenderViewHostTestHarness {
 public:
  InternalWebUIConfigTest() = default;

 protected:
  class InternalConfigTestContentBrowserClient
      : public TestContentBrowserClient {
   public:
    std::unique_ptr<WebUIController> OverrideForInternalWebUI(
        WebUI* web_ui,
        const GURL& url) override {
      return should_override_internal_webui_
                 ? std::make_unique<TestInternalOverrideWebUIController>(web_ui)
                 : nullptr;
    }

    void set_override_for_internal_webui(bool should_override) {
      should_override_internal_webui_ = should_override;
    }

   private:
    bool should_override_internal_webui_ = false;
  };

  InternalConfigTestContentBrowserClient client;
  ScopedContentBrowserClientSetting setting{&client};
};

TEST_F(InternalWebUIConfigTest, CreateWebUIController) {
  // Check that DefaultInternalWebUIConfig creates TestWebUIController when
  // there is no override.
  std::unique_ptr<TestInternalWebUIConfig> config =
      std::make_unique<TestInternalWebUIConfig>();
  content::TestWebUI test_webui_no_override;
  std::unique_ptr<content::WebUIController> webui_controller_no_override =
      config->CreateWebUIController(&test_webui_no_override,
                                    GURL(kTestWebUIURL));
  EXPECT_NE(webui_controller_no_override, nullptr);
  EXPECT_EQ(u"test title",
            webui_controller_no_override->web_ui()->GetOverriddenTitle());

  // Does not create the test controller, because the
  // override controller is created instead.
  client.set_override_for_internal_webui(true);
  TestWebUI test_webui;
  std::unique_ptr<content::WebUIController> webui_controller =
      config->CreateWebUIController(&test_webui, GURL(kTestWebUIURL));
  EXPECT_NE(webui_controller, nullptr);
  EXPECT_EQ(u"internal pages are disabled",
            webui_controller->web_ui()->GetOverriddenTitle());
}

// Tests the IsInternalWebUI method.
TEST_F(InternalWebUIConfigTest, IsInternalWebUIConfig) {
  content::ScopedWebUIConfigRegistration registration(
      std::make_unique<TestInternalWebUIConfig>());
  EXPECT_TRUE(IsInternalWebUI(GURL(kTestWebUIURL)));
  content::ScopedWebUIConfigRegistration user_facing_registration(
      std::make_unique<TestUserFacingWebUIConfig>());
  EXPECT_FALSE(IsInternalWebUI(GURL(kTestUserFacingWebUIURL)));
}

}  // namespace content
