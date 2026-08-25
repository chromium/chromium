// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_config_map.h"

#include <memory>

#include "extensions/browser/extension_config_map_factory.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class TestExtensionConfigProvider : public ExtensionConfigProvider {
 public:
  TestExtensionConfigProvider(ExtensionId extension_id,
                              bool js_error_reporting_enabled,
                              bool should_crash_on_js_error)
      : ExtensionConfigProvider(std::move(extension_id)),
        js_error_reporting_enabled_(js_error_reporting_enabled),
        should_crash_on_js_error_(should_crash_on_js_error) {}
  ~TestExtensionConfigProvider() override = default;

  bool IsJsErrorReportingEnabled() const override {
    return js_error_reporting_enabled_;
  }

  bool ShouldCrashOnJsErrorInDevelopmentBuild() const override {
    return should_crash_on_js_error_;
  }

 private:
  bool js_error_reporting_enabled_;
  bool should_crash_on_js_error_;
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

  config_map()->RegisterConfigProvider(
      std::make_unique<TestExtensionConfigProvider>(
          component_extension->id(),
          /*js_error_reporting_enabled=*/true,
          /*should_crash_on_js_error=*/true));

  const ExtensionConfigProvider* provider =
      config_map()->GetConfigProvider(*component_extension);
  ASSERT_NE(nullptr, provider);
  EXPECT_TRUE(provider->IsJsErrorReportingEnabled());
  EXPECT_TRUE(provider->ShouldCrashOnJsErrorInDevelopmentBuild());

  scoped_refptr<const Extension> external_component_extension =
      ExtensionBuilder("External Component Extension")
          .SetLocation(mojom::ManifestLocation::kExternalComponent)
          .Build();

  config_map()->RegisterConfigProvider(
      std::make_unique<TestExtensionConfigProvider>(
          external_component_extension->id(),
          /*js_error_reporting_enabled=*/true,
          /*should_crash_on_js_error=*/true));

  const ExtensionConfigProvider* external_provider =
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
          non_component_extension->id(),
          /*js_error_reporting_enabled=*/true,
          /*should_crash_on_js_error=*/true));

  EXPECT_EQ(nullptr, config_map()->GetConfigProvider(*non_component_extension));
}

}  // namespace extensions
