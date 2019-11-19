// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/feature_provider.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include "base/stl_util.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/manifest.h"
#include "extensions/common/value_builder.h"
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
  EXPECT_EQ(7u, extension_types.size());
  EXPECT_EQ(1, base::STLCount(extension_types, Manifest::TYPE_EXTENSION));
  EXPECT_EQ(
      1, base::STLCount(extension_types, Manifest::TYPE_LEGACY_PACKAGED_APP));
  EXPECT_EQ(1, base::STLCount(extension_types, Manifest::TYPE_PLATFORM_APP));
  EXPECT_EQ(1, base::STLCount(extension_types, Manifest::TYPE_HOSTED_APP));
  EXPECT_EQ(1, base::STLCount(extension_types, Manifest::TYPE_THEME));
  EXPECT_EQ(1, base::STLCount(extension_types, Manifest::TYPE_SHARED_MODULE));
  EXPECT_EQ(1, base::STLCount(extension_types,
                              Manifest::TYPE_LOGIN_SCREEN_EXTENSION));
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
                                       Feature::UNSPECIFIED_CONTEXT, GURL())
                .result());

  // This is a generic extension, so an app-only feature isn't allowed.
  feature = provider->GetFeature("app.background");
  ASSERT_TRUE(feature);
  EXPECT_EQ(Feature::INVALID_TYPE,
            feature
                ->IsAvailableToContext(extension.get(),
                                       Feature::UNSPECIFIED_CONTEXT, GURL())
                .result());

  // A feature not listed in the manifest isn't allowed.
  feature = provider->GetFeature("background");
  ASSERT_TRUE(feature);
  EXPECT_EQ(Feature::NOT_PRESENT,
            feature
                ->IsAvailableToContext(extension.get(),
                                       Feature::UNSPECIFIED_CONTEXT, GURL())
                .result());
}

// Tests that a real permission feature is available for the correct types of
// extensions and apps.
TEST(FeatureProviderTest, PermissionFeatureTypes) {
  // NOTE: This feature cannot have multiple rules, otherwise it is not a
  // SimpleFeature.
  const SimpleFeature* feature = static_cast<const SimpleFeature*>(
      FeatureProvider::GetPermissionFeature("power"));
  ASSERT_TRUE(feature);
  const std::vector<Manifest::Type>& extension_types =
      feature->extension_types();
  EXPECT_EQ(3u, extension_types.size());
  EXPECT_EQ(1, base::STLCount(extension_types, Manifest::TYPE_EXTENSION));
  EXPECT_EQ(
      1, base::STLCount(extension_types, Manifest::TYPE_LEGACY_PACKAGED_APP));
  EXPECT_EQ(1, base::STLCount(extension_types, Manifest::TYPE_PLATFORM_APP));
}

// Tests that real permission features have the correct availability for an app.
TEST(FeatureProviderTest, PermissionFeatureAvailability) {
  const FeatureProvider* provider = FeatureProvider::GetByName("permission");

  scoped_refptr<const Extension> app =
      ExtensionBuilder("test app", ExtensionBuilder::Type::PLATFORM_APP)
          .AddPermission("power")
          .Build();
  ASSERT_TRUE(app->is_platform_app());

  // A permission requested in the manifest is available.
  const Feature* feature = provider->GetFeature("power");
  EXPECT_EQ(Feature::IS_AVAILABLE,
            feature
                ->IsAvailableToContext(app.get(), Feature::UNSPECIFIED_CONTEXT,
                                       GURL())
                .result());

  // A permission only available to whitelisted extensions returns availability
  // NOT_FOUND_IN_WHITELIST.
  feature = provider->GetFeature("bluetoothPrivate");
  ASSERT_TRUE(feature);
  EXPECT_EQ(Feature::NOT_FOUND_IN_WHITELIST,
            feature
                ->IsAvailableToContext(app.get(), Feature::UNSPECIFIED_CONTEXT,
                                       GURL())
                .result());

  // A permission that isn't part of the manifest returns NOT_PRESENT.
  feature = provider->GetFeature("serial");
  ASSERT_TRUE(feature);
  EXPECT_EQ(Feature::NOT_PRESENT,
            feature
                ->IsAvailableToContext(app.get(), Feature::UNSPECIFIED_CONTEXT,
                                       GURL())
                .result());
}

TEST(FeatureProviderTest, GetChildren) {
  FeatureProvider provider;

  auto add_feature = [&provider](base::StringPiece name,
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

}  // namespace extensions
