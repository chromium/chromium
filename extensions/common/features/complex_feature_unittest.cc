// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/complex_feature.h"

#include <string>
#include <utility>

#include "extensions/common/features/simple_feature.h"
#include "extensions/common/manifest.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::mojom::ManifestLocation;

namespace extensions {

TEST(ComplexFeatureTest, MultipleRulesAllowlist) {
  const HashedExtensionId kIdFoo("fooabbbbccccddddeeeeffffgggghhhh");
  const HashedExtensionId kIdBar("barabbbbccccddddeeeeffffgggghhhh");
  std::vector<Feature*> features;

  {
    // Rule: "extension", allowlist "foo".
    std::unique_ptr<SimpleFeature> simple_feature(new SimpleFeature());
    simple_feature->set_allowlist({kIdFoo.value().c_str()});
    simple_feature->set_extension_types({Manifest::TYPE_EXTENSION});
    features.push_back(simple_feature.release());
  }

  {
    // Rule: "legacy_packaged_app", allowlist "bar".
    std::unique_ptr<SimpleFeature> simple_feature(new SimpleFeature());
    simple_feature->set_allowlist({kIdBar.value().c_str()});
    simple_feature->set_extension_types({Manifest::TYPE_LEGACY_PACKAGED_APP});
    features.push_back(simple_feature.release());
  }

  std::unique_ptr<ComplexFeature> feature(new ComplexFeature(&features));

  // Test match 1st rule.
  EXPECT_EQ(Feature::IS_AVAILABLE,
            feature
                ->IsAvailableToManifest(kIdFoo, Manifest::TYPE_EXTENSION,
                                        ManifestLocation::kInvalidLocation,
                                        Feature::UNSPECIFIED_PLATFORM,
                                        Feature::GetCurrentPlatform(),
                                        kUnspecifiedContextId)
                .result());

  // Test match 2nd rule.
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature
          ->IsAvailableToManifest(
              kIdBar, Manifest::TYPE_LEGACY_PACKAGED_APP,
              ManifestLocation::kInvalidLocation, Feature::UNSPECIFIED_PLATFORM,
              Feature::GetCurrentPlatform(), kUnspecifiedContextId)
          .result());

  // Test allowlist with wrong extension type.
  EXPECT_NE(Feature::IS_AVAILABLE,
            feature
                ->IsAvailableToManifest(kIdBar, Manifest::TYPE_EXTENSION,
                                        ManifestLocation::kInvalidLocation,
                                        Feature::UNSPECIFIED_PLATFORM,
                                        Feature::GetCurrentPlatform(),
                                        kUnspecifiedContextId)
                .result());
  EXPECT_NE(
      Feature::IS_AVAILABLE,
      feature
          ->IsAvailableToManifest(
              kIdFoo, Manifest::TYPE_LEGACY_PACKAGED_APP,
              ManifestLocation::kInvalidLocation, Feature::UNSPECIFIED_PLATFORM,
              Feature::GetCurrentPlatform(), kUnspecifiedContextId)
          .result());
}

// Tests that dependencies are correctly checked.
TEST(ComplexFeatureTest, Dependencies) {
  std::vector<Feature*> features;

  {
    // Rule which depends on an extension-only feature
    // (content_security_policy).
    std::unique_ptr<SimpleFeature> simple_feature(new SimpleFeature());
    simple_feature->set_dependencies({"manifest:content_security_policy"});
    features.push_back(simple_feature.release());
  }

  {
    // Rule which depends on an platform-app-only feature (serial).
    std::unique_ptr<SimpleFeature> simple_feature(new SimpleFeature());
    simple_feature->set_dependencies({"permission:serial"});
    features.push_back(simple_feature.release());
  }

  std::unique_ptr<ComplexFeature> feature(new ComplexFeature(&features));

  // Available to extensions because of the content_security_policy rule.
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature
          ->IsAvailableToManifest(
              HashedExtensionId(std::string(32, 'a')), Manifest::TYPE_EXTENSION,
              ManifestLocation::kInvalidLocation, Feature::UNSPECIFIED_PLATFORM,
              Feature::GetCurrentPlatform(), kUnspecifiedContextId)
          .result());

  // Available to platform apps because of the serial rule.
  EXPECT_EQ(Feature::IS_AVAILABLE,
            feature
                ->IsAvailableToManifest(HashedExtensionId(std::string(32, 'b')),
                                        Manifest::TYPE_PLATFORM_APP,
                                        ManifestLocation::kInvalidLocation,
                                        Feature::UNSPECIFIED_PLATFORM,
                                        Feature::GetCurrentPlatform(),
                                        kUnspecifiedContextId)
                .result());

  // Not available to hosted apps.
  EXPECT_EQ(Feature::INVALID_TYPE,
            feature
                ->IsAvailableToManifest(HashedExtensionId(std::string(32, 'c')),
                                        Manifest::TYPE_HOSTED_APP,
                                        ManifestLocation::kInvalidLocation,
                                        Feature::UNSPECIFIED_PLATFORM,
                                        Feature::GetCurrentPlatform(),
                                        kUnspecifiedContextId)
                .result());
}

TEST(ComplexFeatureTest, RequiresDelegatedAvailabilityCheck) {
  std::vector<Feature*> features;

  // Test a complex feature where requires_delegated_availability_check hasn't
  // been set on any of its simple features.
  {
    {
      // Rule which doesn't set requires_delegated_availability_check.
      auto simple_feature = std::make_unique<SimpleFeature>();
      features.push_back(simple_feature.release());
    }
    {
      // Rule which doesn't set requires_delegated_availability_check.
      auto simple_feature = std::make_unique<SimpleFeature>();
      features.push_back(simple_feature.release());
    }

    ComplexFeature complex_feature(&features);
    EXPECT_FALSE(complex_feature.RequiresDelegatedAvailabilityCheck());
  }

  // Test a complex feature where requires_delegated_availability_check is set.
  {
    {
      // Rule which doesn't set requires_delegated_availability_check.
      auto simple_feature = std::make_unique<SimpleFeature>();
      features.push_back(simple_feature.release());
    }
    {
      // Rule which doesn't set requires_delegated_availability_check.
      auto simple_feature = std::make_unique<SimpleFeature>();
      features.push_back(simple_feature.release());
    }
    {
      // Rule which sets requires_delegated_availability_check to true.
      auto simple_feature = std::make_unique<SimpleFeature>();
      simple_feature->set_requires_delegated_availability_check(true);
      features.push_back(simple_feature.release());
    }
    ComplexFeature complex_feature(&features);
    EXPECT_TRUE(complex_feature.RequiresDelegatedAvailabilityCheck());
  }
}

}  // namespace extensions
