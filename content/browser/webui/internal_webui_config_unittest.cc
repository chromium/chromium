// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/internal_webui_config.h"

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"

namespace content {

namespace {

const char kTestWebUIHost[] = "test";
const char kTestWebUIURL[] = "chrome://test";
const char kTestUserFacingWebUIHost[] = "test-user-facing";
const char kTestUserFacingWebUIURL[] = "chrome://test-user-facing";

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

using InternalWebUIConfigTest = RenderViewHostTestHarness;

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
