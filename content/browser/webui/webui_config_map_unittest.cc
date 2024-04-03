// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/webui_config_map.h"

#include <string_view>

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

constexpr const char kChromeFoo[] = "chrome://foo";
constexpr const char kChromeBar[] = "chrome://bar";
constexpr const char kChromeUntrustedFoo[] = "chrome-untrusted://foo";
constexpr const char kChromeUntrustedBar[] = "chrome-untrusted://bar";

// A valid BrowserContext is not needed for these tests.
BrowserContext* kBrowserContext = nullptr;

class TestConfig : public WebUIConfig {
 public:
  TestConfig(std::string_view scheme, std::string_view host)
      : WebUIConfig(scheme, host) {}
  ~TestConfig() override = default;

  std::unique_ptr<WebUIController> CreateWebUIController(
      WebUI* web_ui,
      const GURL& url) override {
    // Unused in these tests.
    return nullptr;
  }
};

}  // namespace

TEST(WebUIConfigTest, AddAndRemoveChromeUrl) {
  auto& map = WebUIConfigMap::GetInstance();
  size_t initial_size = map.GetWebUIConfigList(nullptr).size();

  auto chrome_config = std::make_unique<TestConfig>("chrome", "foo");
  auto* chrome_config_ptr = chrome_config.get();
  map.AddWebUIConfig(std::move(chrome_config));

  EXPECT_EQ(initial_size + 1, map.GetWebUIConfigList(nullptr).size());

  EXPECT_EQ(chrome_config_ptr,
            map.GetConfig(kBrowserContext, GURL(kChromeFoo)));
  EXPECT_EQ(nullptr, map.GetConfig(kBrowserContext, GURL(kChromeBar)));
  EXPECT_EQ(nullptr, map.GetConfig(kBrowserContext, GURL(kChromeUntrustedFoo)));
  EXPECT_EQ(nullptr, map.GetConfig(kBrowserContext, GURL(kChromeUntrustedBar)));

  EXPECT_EQ(nullptr, map.RemoveConfig(GURL(kChromeBar)));
  EXPECT_EQ(nullptr, map.RemoveConfig(GURL(kChromeUntrustedFoo)));
  EXPECT_EQ(nullptr, map.RemoveConfig(GURL(kChromeUntrustedBar)));

  auto removed_config = map.RemoveConfig(GURL(kChromeFoo));
  EXPECT_EQ(removed_config.get(), chrome_config_ptr);
  EXPECT_EQ(initial_size, map.GetWebUIConfigList(nullptr).size());
}

TEST(WebUIConfigTest, AddAndRemoteChromeUntrustedUrl) {
  auto& map = WebUIConfigMap::GetInstance();
  size_t initial_size = map.GetWebUIConfigList(nullptr).size();

  auto untrusted_config =
      std::make_unique<TestConfig>("chrome-untrusted", "foo");
  auto* untrusted_config_ptr = untrusted_config.get();
  map.AddUntrustedWebUIConfig(std::move(untrusted_config));

  EXPECT_EQ(initial_size + 1, map.GetWebUIConfigList(nullptr).size());

  EXPECT_EQ(untrusted_config_ptr,
            map.GetConfig(kBrowserContext, GURL(kChromeUntrustedFoo)));
  EXPECT_EQ(nullptr, map.GetConfig(kBrowserContext, GURL(kChromeUntrustedBar)));
  EXPECT_EQ(nullptr, map.GetConfig(kBrowserContext, GURL(kChromeFoo)));
  EXPECT_EQ(nullptr, map.GetConfig(kBrowserContext, GURL(kChromeBar)));

  EXPECT_EQ(nullptr, map.RemoveConfig(GURL(kChromeUntrustedBar)));
  EXPECT_EQ(nullptr, map.RemoveConfig(GURL(kChromeFoo)));
  EXPECT_EQ(nullptr, map.RemoveConfig(GURL(kChromeBar)));

  auto removed_config = map.RemoveConfig(GURL(kChromeUntrustedFoo));
  EXPECT_EQ(removed_config.get(), untrusted_config_ptr);
  EXPECT_EQ(initial_size, map.GetWebUIConfigList(nullptr).size());
}

class WebUiConfigDeathTest : public testing::Test,
                             public ::testing::WithParamInterface<const char*> {
 public:
  WebUiConfigDeathTest() = default;
  WebUiConfigDeathTest(const WebUiConfigDeathTest&) = delete;
  WebUiConfigDeathTest& operator=(const WebUiConfigDeathTest&) = delete;
  ~WebUiConfigDeathTest() override = default;
};

// Regression test for https://crbug.com/1464456.
TEST_P(WebUiConfigDeathTest, GetAndRemoveNonChromeUrls) {
  auto& map = WebUIConfigMap::GetInstance();

  ScopedWebUIConfigRegistration chrome_config(
      std::make_unique<TestConfig>("chrome", "foo"));
  ScopedWebUIConfigRegistration untrusted_config(
      std::make_unique<TestConfig>("chrome-untrusted", "foo"));

  const GURL url(GetParam());
  EXPECT_EQ(nullptr, map.GetConfig(kBrowserContext, url));
  EXPECT_DEATH_IF_SUPPORTED(map.RemoveConfig(url), "");
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    WebUiConfigDeathTest,
    ::testing::Values(
        // filesystem: URLs
        "filesystem:chrome://foo/external/file.txt",
        "filesystem:chrome-untrusted://foo/external/file.txt",
        // blob: URLS
        "blob:chrome://foo/GUID",
        "blob:chrome-untrusted://foo/GUID",
        // HTTP/HTTPS URLS
        "http://foo.com",
        "https://foo.com"));

}  // namespace content
