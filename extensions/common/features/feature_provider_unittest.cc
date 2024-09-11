// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/feature_provider.h"

#include <algorithm>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/ranges/algorithm.h"
#include "base/test/bind.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/test/test_context_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// Tests that a real manifest feature is available for the correct types of
// extensions and apps.
TEST(FeatureProviderTest, ManifestFeatureTypes) {
  // NOTE: This feature cannot have multiple rules, otherwise it is not a
  // SimpleFeature.
  const SimpleFeature* feature = static_cast<const SimpleFeature*>(
      FeatureProvider::GetManifestFeature("description"));
  ASSERT_TRUE(feature);
  const std::vector<Manifest::Type>& extension_types =
      feature->extension_types();
  EXPECT_EQ(8u, extension_types.size());
  EXPECT_EQ(1, base::ranges::count(extension_types, Manifest::TYPE_EXTENSION));
  EXPECT_EQ(1, base::ranges::count(extension_types,
                                   Manifest::TYPE_LEGACY_PACKAGED_APP));
  EXPECT_EQ(1,
            base::ranges::count(extension_types, Manifest::TYPE_PLATFORM_APP));
  EXPECT_EQ(1, base::ranges::count(extension_types, Manifest::TYPE_HOSTED_APP));
  EXPECT_EQ(1, base::ranges::count(extension_types, Manifest::TYPE_THEME));
  EXPECT_EQ(1,
            base::ranges::count(extension_types, Manifest::TYPE_SHARED_MODULE));
  EXPECT_EQ(1, base::ranges::count(extension_types,
                                   Manifest::TYPE_LOGIN_SCREEN_EXTENSION));
  EXPECT_EQ(1, base::ranges::count(extension_types,
                                   Manifest::TYPE_CHROMEOS_SYSTEM_EXTENSION));
}

// Tests that real manifest features have the correct availability for an
// extension.
TEST(FeatureProviderTest, ManifestFeatureAvailability) {
  const FeatureProvider* provider = FeatureProvider::GetByName("manifest");

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test extension").Build();

  const Feature* feature = provider->GetFeature("description");
  EXPECT_EQ(Feature::IS_AVAILABLE,
            feature
                ->IsAvailableToContext(extension.get(),
                                       mojom::ContextType::kUnspecified, GURL(),
                                       kUnspecifiedContextId, TestContextData())
                .result());

  // This is a generic extension, so an app-only feature isn't allowed.
  feature = provider->GetFeature("app.background");
  ASSERT_TRUE(feature);
  EXPECT_EQ(Feature::INVALID_TYPE,
            feature
                ->IsAvailableToContext(extension.get(),
                                       mojom::ContextType::kUnspecified, GURL(),
                                       kUnspecifiedContextId, TestContextData())
                .result());

  // A feature not listed in the manifest isn't allowed.
  feature = provider->GetFeature("background");
  ASSERT_TRUE(feature);
  EXPECT_EQ(Feature::NOT_PRESENT,
            feature
                ->IsAvailableToContext(extension.get(),
                                       mojom::ContextType::kUnspecified, GURL(),
                                       kUnspecifiedContextId, TestContextData())
                .result());
}

// Tests that a real permission feature is available for the correct types of
// extensions and apps.
TEST(FeatureProviderTest, PermissionFeatureTypes) {
  // NOTE: This feature cannot have multiple rules, otherwise it is not a
  // SimpleFeature.
  const SimpleFeature* feature = static_cast<const SimpleFeature*>(
      FeatureProvider::GetPermissionFeature("alarms"));
  ASSERT_TRUE(feature);
  const std::vector<Manifest::Type>& extension_types =
      feature->extension_types();
  EXPECT_EQ(3u, extension_types.size());
  EXPECT_EQ(1, base::ranges::count(extension_types, Manifest::TYPE_EXTENSION));
  EXPECT_EQ(1, base::ranges::count(extension_types,
                                   Manifest::TYPE_LEGACY_PACKAGED_APP));
  EXPECT_EQ(1,
            base::ranges::count(extension_types, Manifest::TYPE_PLATFORM_APP));
}

// Tests that real permission features have the correct availability for an app.
TEST(FeatureProviderTest, PermissionFeatureAvailability) {
  const FeatureProvider* provider = FeatureProvider::GetByName("permission");

  scoped_refptr<const Extension> app =
      ExtensionBuilder("test app", ExtensionBuilder::Type::PLATFORM_APP)
          .AddAPIPermission("power")
          .Build();
  ASSERT_TRUE(app->is_platform_app());

  // A permission requested in the manifest is available.
  const Feature* feature = provider->GetFeature("power");
  EXPECT_EQ(Feature::IS_AVAILABLE,
            feature
                ->IsAvailableToContext(app.get(),
                                       mojom::ContextType::kUnspecified, GURL(),
                                       kUnspecifiedContextId, TestContextData())
                .result());

  // A permission only available to allowlisted extensions returns availability
  // NOT_FOUND_IN_ALLOWLIST.
  // TODO(crbug.com/40198321): Port //device/bluetooth to Fuchsia to
  // enable bluetooth extensions.
  // bluetoothPrivate is unsupported in desktop-android build.
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_DESKTOP_ANDROID)
  feature = provider->GetFeature("bluetoothPrivate");
  ASSERT_TRUE(feature);
  EXPECT_EQ(Feature::NOT_FOUND_IN_ALLOWLIST,
            feature
                ->IsAvailableToContext(app.get(),
                                       mojom::ContextType::kUnspecified, GURL(),
                                       kUnspecifiedContextId, TestContextData())
                .result());
#endif  // !BUILDFLAG(IS_FUCHSIA)

  // A permission that isn't part of the manifest returns NOT_PRESENT.
  feature = provider->GetFeature("serial");
  ASSERT_TRUE(feature);
  EXPECT_EQ(Feature::NOT_PRESENT,
            feature
                ->IsAvailableToContext(app.get(),
                                       mojom::ContextType::kUnspecified, GURL(),
                                       kUnspecifiedContextId, TestContextData())
                .result());
}

TEST(FeatureProviderTest, GetChildren) {
  FeatureProvider provider;

  auto add_feature = [&provider](std::string_view name,
                                 bool no_parent = false) {
    auto feature = std::make_unique<SimpleFeature>();
    feature->set_name(name);
    feature->set_noparent(no_parent);
    provider.AddFeature(name, std::move(feature));
  };

  add_feature("parent");
  add_feature("parent.child");
  add_feature("parent.child.grandchild");
  add_feature("parent.other_child.other_grandchild");
  add_feature("parent.unparented_child", true);

  const Feature* parent = provider.GetFeature("parent");
  ASSERT_TRUE(parent);
  std::vector<const Feature*> children = provider.GetChildren(*parent);
  std::set<std::string> children_names;
  for (const Feature* child : children)
    children_names.insert(child->name());
  EXPECT_THAT(children_names, testing::UnorderedElementsAre(
                                  "parent.child", "parent.child.grandchild",
                                  "parent.other_child.other_grandchild"));
}

TEST(FeatureProviderTest, InstallFeatureDelegatedAvailabilityCheck) {
  Feature::FeatureDelegatedAvailabilityCheckMap map;
  static constexpr const char* kDelegatedFeatureName = "delegatedFeature";
  static constexpr const char* kNondelgatedFeatureName = "nondelegatedFeature";
  static constexpr const char* kMissingRequiresDelegatedCheckFeatureName =
      "missingRequiresDelegatedCheckFeature";

  auto delegated_availability_check =
      [&](const std::string& api_full_name, const Extension* extension,
          mojom::ContextType context, const GURL& url,
          Feature::Platform platform, int context_id, bool check_developer_mode,
          const ContextData& context_data) { return false; };
  map.emplace(kDelegatedFeatureName,
              base::BindLambdaForTesting(delegated_availability_check));
  map.emplace(kMissingRequiresDelegatedCheckFeatureName,
              base::BindLambdaForTesting(delegated_availability_check));
  ExtensionsClient::Get()->SetFeatureDelegatedAvailabilityCheckMap(
      std::move(map));

  FeatureProvider provider;

  // Verify that the delegated check handler is installed for a feature that
  // requires it and has a handler in the map.
  {
    auto feature = std::make_unique<SimpleFeature>();
    feature->set_name(kDelegatedFeatureName);
    feature->set_requires_delegated_availability_check(true);
    provider.AddFeature(kDelegatedFeatureName, std::move(feature));

    const auto* delegated_feature = provider.GetFeature(kDelegatedFeatureName);
    EXPECT_TRUE(
        delegated_feature->HasDelegatedAvailabilityCheckHandlerForTesting());
  }

  // Verify that a delegated check handler is not installed for a feature that
  // doesn't require it.
  {
    auto feature = std::make_unique<SimpleFeature>();
    feature->set_name(kNondelgatedFeatureName);
    provider.AddFeature(kNondelgatedFeatureName, std::move(feature));

    const auto* nondelegated_feature =
        provider.GetFeature(kNondelgatedFeatureName);
    EXPECT_FALSE(
        nondelegated_feature->HasDelegatedAvailabilityCheckHandlerForTesting());
  }

  // Verify that a delegated check handler is not installed for a feature that
  // doesn't require it but has a handler in the map.
  {
    auto feature = std::make_unique<SimpleFeature>();
    feature->set_name(kMissingRequiresDelegatedCheckFeatureName);
    provider.AddFeature(kMissingRequiresDelegatedCheckFeatureName,
                        std::move(feature));

    const auto* missing_requires_delegated_check_feature =
        provider.GetFeature(kMissingRequiresDelegatedCheckFeatureName);
    EXPECT_FALSE(missing_requires_delegated_check_feature
                     ->HasDelegatedAvailabilityCheckHandlerForTesting());
  }
}

}  // namespace extensions
