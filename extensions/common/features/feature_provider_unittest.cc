// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/feature_provider.h"

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/test/test_context_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kRegistryFeatureName[] = "feature";
constexpr SimpleFeatureData kRegistryFeatureData = {
    .feature = {.name = kRegistryFeatureName}};

}  // namespace

// Tests that a real manifest feature is available for the correct types of
// extensions and apps.
TEST(FeatureProviderTest, ManifestFeatureTypes) {
  // NOTE: This feature cannot have multiple rules, otherwise it is not a
  // SimpleFeature.
  const SimpleFeature* feature = static_cast<const SimpleFeature*>(
      FeatureProvider::GetManifestFeature("description"));
  ASSERT_TRUE(feature);
  const base::span<const Manifest::Type> extension_types =
      feature->extension_types();
  EXPECT_EQ(8u, extension_types.size());
  EXPECT_EQ(1, std::ranges::count(extension_types, Manifest::Type::kExtension));
  EXPECT_EQ(1, std::ranges::count(extension_types,
                                  Manifest::Type::kLegacyPackagedApp));
  EXPECT_EQ(1,
            std::ranges::count(extension_types, Manifest::Type::kPlatformApp));
  EXPECT_EQ(1, std::ranges::count(extension_types, Manifest::Type::kHostedApp));
  EXPECT_EQ(1, std::ranges::count(extension_types, Manifest::Type::kTheme));
  EXPECT_EQ(1,
            std::ranges::count(extension_types, Manifest::Type::kSharedModule));
  EXPECT_EQ(1, std::ranges::count(extension_types,
                                  Manifest::Type::kLoginScreenExtension));
  EXPECT_EQ(1, std::ranges::count(extension_types,
                                  Manifest::Type::kChromeOSSystemExtension));
}

TEST(FeatureProviderTest, GetManifestFeatureWithNonNullTerminatedStringView) {
  const std::string name_with_suffix = "description!";
  const std::string_view name(name_with_suffix.data(),
                              name_with_suffix.size() - 1);

  EXPECT_TRUE(FeatureProvider::GetManifestFeature(name));
}

TEST(FeatureProviderTest, GetByNameWithNonNullTerminatedStringView) {
  const std::string name_with_suffix = "manifest!";
  const std::string_view name(name_with_suffix.data(),
                              name_with_suffix.size() - 1);

  EXPECT_EQ(FeatureProvider::GetManifestFeatures(),
            FeatureProvider::GetByName(name));
}

// Tests that real manifest features have the correct availability for an
// extension.
TEST(FeatureProviderTest, ManifestFeatureAvailability) {
  const FeatureProvider* provider = FeatureProvider::GetByName("manifest");

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test extension").Build();

  const Feature* feature = provider->GetFeature("description");
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            feature
                ->IsAvailableToContext(extension.get(),
                                       mojom::ContextType::kUnspecified, GURL(),
                                       kUnspecifiedContextId, TestContextData())
                .result());

  // This is a generic extension, so an app-only feature isn't allowed.
  feature = provider->GetFeature("app.background");
  ASSERT_TRUE(feature);
  EXPECT_EQ(Feature::AvailabilityResult::kInvalidType,
            feature
                ->IsAvailableToContext(extension.get(),
                                       mojom::ContextType::kUnspecified, GURL(),
                                       kUnspecifiedContextId, TestContextData())
                .result());

  // A feature not listed in the manifest isn't allowed.
  feature = provider->GetFeature("background");
  ASSERT_TRUE(feature);
  EXPECT_EQ(Feature::AvailabilityResult::kNotPresent,
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
  const base::span<const Manifest::Type> extension_types =
      feature->extension_types();
  EXPECT_EQ(3u, extension_types.size());
  EXPECT_EQ(1, std::ranges::count(extension_types, Manifest::Type::kExtension));
  EXPECT_EQ(1, std::ranges::count(extension_types,
                                  Manifest::Type::kLegacyPackagedApp));
  EXPECT_EQ(1,
            std::ranges::count(extension_types, Manifest::Type::kPlatformApp));
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
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            feature
                ->IsAvailableToContext(app.get(),
                                       mojom::ContextType::kUnspecified, GURL(),
                                       kUnspecifiedContextId, TestContextData())
                .result());

  // A permission only available to allowlisted extensions returns availability
  // AvailabilityResult::kNotFoundInAllowlist.
  // bluetoothPrivate is unsupported in desktop-android build.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  feature = provider->GetFeature("bluetoothPrivate");
  ASSERT_TRUE(feature);
  EXPECT_EQ(Feature::AvailabilityResult::kNotFoundInAllowlist,
            feature
                ->IsAvailableToContext(app.get(),
                                       mojom::ContextType::kUnspecified, GURL(),
                                       kUnspecifiedContextId, TestContextData())
                .result());
#endif

  // A permission that isn't part of the manifest returns
  // AvailabilityResult::kNotPresent.
  feature = provider->GetFeature("unlimitedStorage");
  ASSERT_TRUE(feature);
  EXPECT_EQ(Feature::AvailabilityResult::kNotPresent,
            feature
                ->IsAvailableToContext(app.get(),
                                       mojom::ContextType::kUnspecified, GURL(),
                                       kUnspecifiedContextId, TestContextData())
                .result());
}

TEST(FeatureProviderTest, GetChildren) {
  static constexpr SimpleFeatureData kParent = {.feature = {.name = "parent"}};
  static constexpr SimpleFeatureData kChild = {
      .feature = {.name = "parent.child"}};
  static constexpr SimpleFeatureData kGrandchild = {
      .feature = {.name = "parent.child.grandchild"}};
  static constexpr SimpleFeatureData kOtherGrandchild = {
      .feature = {.name = "parent.other_child.other_grandchild"}};
  static constexpr SimpleFeatureData kUnparentedChild = {
      .feature = {.name = "parent.unparented_child", .no_parent = true}};

  SimpleFeature parent_feature{StaticFeatureData(kParent)};
  SimpleFeature child_feature{StaticFeatureData(kChild)};
  SimpleFeature grandchild_feature{StaticFeatureData(kGrandchild)};
  SimpleFeature other_grandchild_feature{StaticFeatureData(kOtherGrandchild)};
  SimpleFeature unparented_child_feature{StaticFeatureData(kUnparentedChild)};
  std::array<Feature*, 5> features = {
      &parent_feature, &child_feature, &grandchild_feature,
      &other_grandchild_feature, &unparented_child_feature};
  FeatureProvider provider;
  provider.AddStaticFeatures(features);

  const Feature* parent = provider.GetFeature("parent");
  ASSERT_TRUE(parent);
  std::vector<const Feature*> children = provider.GetChildren(*parent);
  std::set<std::string> children_names;
  for (const Feature* child : children)
    children_names.emplace(child->name());
  EXPECT_THAT(children_names, testing::UnorderedElementsAre(
                                  "parent.child", "parent.child.grandchild",
                                  "parent.other_child.other_grandchild"));
}

TEST(FeatureProviderTest, StaticFeaturesAreNotOwned) {
  SimpleFeature feature{StaticFeatureData(kRegistryFeatureData)};
  std::array<Feature*, 1> features = {&feature};

  {
    FeatureProvider provider;
    provider.AddStaticFeatures(features);
    EXPECT_EQ(&feature, provider.GetFeature(kRegistryFeatureName));
  }

  EXPECT_EQ(kRegistryFeatureName, feature.name());
}

TEST(FeatureProviderTest, FeatureNamesBackRegistryKeys) {
  SimpleFeature feature{StaticFeatureData(kRegistryFeatureData)};
  std::array<Feature*, 1> features = {&feature};
  FeatureProvider provider;
  provider.AddStaticFeatures(features);

  const FeatureMap& registry = provider.GetAllFeatures();
  ASSERT_EQ(1u, registry.size());
  const auto& [name, stored_feature] = *registry.begin();
  EXPECT_EQ(name.data(), stored_feature->name().data());
}

}  // namespace extensions
