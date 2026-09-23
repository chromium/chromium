// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_config_map.h"

#include <memory>
#include <string>

#include "extensions/browser/extension_config_map_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

namespace {

class TestExtensionConfigProvider : public ExtensionConfigProvider {
 public:
  TestExtensionConfigProvider(ExtensionId extension_id,
                              std::string chrome_url_host,
                              bool js_error_reporting_enabled,
                              bool should_crash_on_js_error,
                              base::DictValue load_time_data)
      : ExtensionConfigProvider(std::move(extension_id)),
        chrome_url_host_(std::move(chrome_url_host)),
        js_error_reporting_enabled_(js_error_reporting_enabled),
        should_crash_on_js_error_(should_crash_on_js_error),
        load_time_data_(std::move(load_time_data)) {
    SetDefaultResource("/main.html");
    AddResourcePath("/alias", "/main.html");
    AddResourcePath("/ml", "/ml.html");
  }
  ~TestExtensionConfigProvider() override = default;

  base::DictValue GetLoadTimeData(content::BrowserContext& context) override {
    return load_time_data_.Clone();
  }

  std::string_view GetChromeURLHost() const override {
    return chrome_url_host_;
  }

  bool IsJsErrorReportingEnabled() const override {
    return js_error_reporting_enabled_;
  }

  bool ShouldCrashOnJsErrorInDevelopmentBuild() const override {
    return should_crash_on_js_error_;
  }

 private:
  std::string chrome_url_host_;
  bool js_error_reporting_enabled_;
  bool should_crash_on_js_error_;
  base::DictValue load_time_data_;
};

}  // namespace

class ExtensionConfigMapTest : public ExtensionsTest {
 public:
  ExtensionConfigMapTest() = default;
  ~ExtensionConfigMapTest() override = default;

  ExtensionConfigMap* config_map() {
    return ExtensionConfigMapFactory::GetOrCreateForBrowserContext(
        browser_context());
  }
};

TEST_F(ExtensionConfigMapTest, GetConfigProvider) {
  scoped_refptr<const Extension> component_extension =
      ExtensionBuilder("Component Extension")
          .SetLocation(mojom::ManifestLocation::kComponent)
          .Build();

  EXPECT_EQ(nullptr, config_map()->GetConfigProvider(*component_extension));

  base::DictValue expected_dict;
  expected_dict.Set("test_key", "test_val");

  config_map()->RegisterConfigProvider(
      std::make_unique<TestExtensionConfigProvider>(
          component_extension->id(), "test-host",
          /*js_error_reporting_enabled=*/true,
          /*should_crash_on_js_error=*/true, expected_dict.Clone()));

  ExtensionConfigProvider* provider =
      config_map()->GetConfigProvider(*component_extension);
  ASSERT_NE(nullptr, provider);
  EXPECT_TRUE(provider->IsJsErrorReportingEnabled());
  EXPECT_TRUE(provider->ShouldCrashOnJsErrorInDevelopmentBuild());
  EXPECT_EQ(expected_dict, provider->GetLoadTimeData(*browser_context()));
  EXPECT_TRUE(provider->IsDynamicResource("/strings.m.js"));
  EXPECT_FALSE(provider->IsDynamicResource("/other.js"));
  EXPECT_EQ(
      "import {loadTimeData} from "
      "'chrome://resources/js/load_time_data.js';\n"
      "loadTimeData.data = {\"test_key\":\"test_val\"};\n"
      "export {loadTimeData};\n",
      provider->GetDynamicResourceContent("/strings.m.js", *browser_context()));

  // Before the extension is added to ExtensionRegistry, lookup by host or ID
  // returns nullptr.
  EXPECT_EQ(nullptr,
            config_map()->GetConfigProviderByChromeURLHost("test-host"));
  EXPECT_EQ(nullptr, config_map()->GetConfigProviderByExtensionId(
                         component_extension->id()));

  ExtensionRegistry::Get(browser_context())->AddEnabled(component_extension);

  EXPECT_EQ(provider,
            config_map()->GetConfigProviderByChromeURLHost("test-host"));
  EXPECT_EQ(provider, config_map()->GetConfigProviderByExtensionId(
                          component_extension->id()));

  scoped_refptr<const Extension> external_component_extension =
      ExtensionBuilder("External Component Extension")
          .SetLocation(mojom::ManifestLocation::kExternalComponent)
          .Build();

  config_map()->RegisterConfigProvider(
      std::make_unique<TestExtensionConfigProvider>(
          external_component_extension->id(), "external-host",
          /*js_error_reporting_enabled=*/true,
          /*should_crash_on_js_error=*/true, base::DictValue()));

  ExtensionConfigProvider* external_provider =
      config_map()->GetConfigProvider(*external_component_extension);
  ASSERT_NE(nullptr, external_provider);
  EXPECT_TRUE(external_provider->IsJsErrorReportingEnabled());
  EXPECT_TRUE(external_provider->ShouldCrashOnJsErrorInDevelopmentBuild());

  scoped_refptr<const Extension> non_component_extension =
      ExtensionBuilder("Non Component Extension")
          .SetLocation(mojom::ManifestLocation::kInternal)
          .Build();

  config_map()->RegisterConfigProvider(
      std::make_unique<TestExtensionConfigProvider>(
          non_component_extension->id(), "non-component-host",
          /*js_error_reporting_enabled=*/true,
          /*should_crash_on_js_error=*/true, base::DictValue()));

  EXPECT_EQ(nullptr, config_map()->GetConfigProvider(*non_component_extension));
}

TEST_F(ExtensionConfigMapTest, IsUnboundedElementAllowed) {
  scoped_refptr<const Extension> component_extension =
      ExtensionBuilder("Component Extension")
          .SetLocation(mojom::ManifestLocation::kComponent)
          .Build();
  ExtensionRegistry::Get(browser_context())->AddEnabled(component_extension);

  EXPECT_FALSE(
      config_map()->IsUnboundedElementAllowed(component_extension->id()));

  class UnboundedAllowedConfigProvider : public ExtensionConfigProvider {
   public:
    explicit UnboundedAllowedConfigProvider(ExtensionId id)
        : ExtensionConfigProvider(std::move(id)) {}
    bool IsUnboundedElementAllowed() const override { return true; }
  };

  config_map()->RegisterConfigProvider(
      std::make_unique<UnboundedAllowedConfigProvider>(
          component_extension->id()));
  EXPECT_TRUE(
      config_map()->IsUnboundedElementAllowed(component_extension->id()));
}

TEST_F(ExtensionConfigMapTest, HandleChromeURLAndReverse) {
  scoped_refptr<const Extension> component_extension =
      ExtensionBuilder("Component Extension")
          .SetLocation(mojom::ManifestLocation::kComponent)
          .Build();

  config_map()->RegisterConfigProvider(
      std::make_unique<TestExtensionConfigProvider>(
          component_extension->id(), "test-host",
          /*js_error_reporting_enabled=*/false,
          /*should_crash_on_js_error=*/false, base::DictValue()));
  ExtensionRegistry::Get(browser_context())->AddEnabled(component_extension);

  // Non-chrome:// URLs and unknown chrome hosts are ignored.
  GURL non_chrome_url("https://example.com/");
  EXPECT_FALSE(
      ExtensionConfigMap::HandleChromeURL(&non_chrome_url, browser_context()));
  GURL unknown_host_url("chrome://unknown-host/");
  EXPECT_FALSE(ExtensionConfigMap::HandleChromeURL(&unknown_host_url,
                                                   browser_context()));

  // Root chrome:// URL rewrites to default resource and reverses back to root.
  GURL url("chrome://test-host/");
  EXPECT_TRUE(ExtensionConfigMap::HandleChromeURL(&url, browser_context()));
  EXPECT_EQ(component_extension->ResolveExtensionURL("main.html"), url);
  EXPECT_TRUE(
      ExtensionConfigMap::HandleChromeURLReverse(&url, browser_context()));
  EXPECT_EQ(GURL("chrome://test-host/"), url);

  // Secondary alias mapping to the same resource canonicalizes back to the
  // first-registered path ("/").
  GURL alias_url("chrome://test-host/alias");
  EXPECT_TRUE(
      ExtensionConfigMap::HandleChromeURL(&alias_url, browser_context()));
  EXPECT_EQ(component_extension->ResolveExtensionURL("main.html"), alias_url);
  EXPECT_TRUE(ExtensionConfigMap::HandleChromeURLReverse(&alias_url,
                                                         browser_context()));
  EXPECT_EQ(GURL("chrome://test-host/"), alias_url);

  // Mapped sub-path with query and hash preserves query and hash in both
  // directions.
  GURL sub_url("chrome://test-host/ml?q=1#section");
  EXPECT_TRUE(ExtensionConfigMap::HandleChromeURL(&sub_url, browser_context()));
  EXPECT_EQ(component_extension->ResolveExtensionURL("ml.html?q=1#section"),
            sub_url);
  EXPECT_TRUE(
      ExtensionConfigMap::HandleChromeURLReverse(&sub_url, browser_context()));
  EXPECT_EQ(GURL("chrome://test-host/ml?q=1#section"), sub_url);

  // Unmapped sub-path falls back to the requested path in both directions.
  GURL unmapped_url("chrome://test-host/script.js");
  EXPECT_TRUE(
      ExtensionConfigMap::HandleChromeURL(&unmapped_url, browser_context()));
  EXPECT_EQ(component_extension->ResolveExtensionURL("script.js"),
            unmapped_url);
  EXPECT_TRUE(ExtensionConfigMap::HandleChromeURLReverse(&unmapped_url,
                                                         browser_context()));
  EXPECT_EQ(GURL("chrome://test-host/script.js"), unmapped_url);
}

}  // namespace extensions
