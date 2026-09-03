// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/complex_feature.h"

#include <array>
#include <string>
#include <string_view>

#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/features/feature_developer_mode_only.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/features/simple_feature_test_constants.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/test/test_context_data.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::mojom::ManifestLocation;
using version_info::Channel;

namespace extensions {

namespace {

// Single-element backing arrays for statically stored descriptors.
constexpr auto kPrivilegedExtensionOnly = std::to_array<mojom::ContextType>(
    {mojom::ContextType::kPrivilegedExtension});
constexpr auto kExtensionOnly =
    std::to_array<Manifest::Type>({Manifest::Type::kExtension});
constexpr auto kLegacyPackagedAppOnly =
    std::to_array<Manifest::Type>({Manifest::Type::kLegacyPackagedApp});

}  // namespace

TEST(ComplexFeatureTest, ConstructsEachStaticChildType) {
  static constexpr SimpleFeatureData kSimpleChildren[] = {
      {.feature = {.name = "missing"}},
      {.feature = {.name = "missing"}},
  };
  static constexpr SimpleFeatureData kManifestChildren[] = {
      {.feature = {.name = "test_key"}},
      {.feature = {.name = "test_key"}},
  };
  static constexpr SimpleFeatureData kPermissionChildren[] = {
      {.feature = {.name = "storage"}},
      {.feature = {.name = "storage"}},
  };
  static constexpr ComplexFeatureData kSimpleData{
      .feature = {.name = "simple"},
      .features = StaticSpan(kSimpleChildren),
      .feature_type = ComplexFeatureType::kSimple,
  };
  static constexpr ComplexFeatureData kManifestData{
      .feature = {.name = "manifest"},
      .features = StaticSpan(kManifestChildren),
      .feature_type = ComplexFeatureType::kManifest,
  };
  static constexpr ComplexFeatureData kPermissionData{
      .feature = {.name = "permission"},
      .features = StaticSpan(kPermissionChildren),
      .feature_type = ComplexFeatureType::kPermission,
  };

  ComplexFeature simple_feature{StaticFeatureData(kSimpleData)};
  ComplexFeature manifest_feature{StaticFeatureData(kManifestData)};
  ComplexFeature permission_feature{StaticFeatureData(kPermissionData)};

  scoped_refptr<const Extension> bare_extension =
      ExtensionBuilder("bare").Build();
  scoped_refptr<const Extension> manifest_extension =
      ExtensionBuilder("manifest").SetManifestKey("test_key", true).Build();
  scoped_refptr<const Extension> permission_extension =
      ExtensionBuilder("permission").AddAPIPermission("storage").Build();
  auto availability = [](const ComplexFeature& feature,
                         const Extension* extension) {
    return feature
        .IsAvailableToContext(extension,
                              mojom::ContextType::kPrivilegedExtension, GURL(),
                              kUnspecifiedContextId, TestContextData())
        .result();
  };

  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            availability(simple_feature, bare_extension.get()));
  EXPECT_EQ(Feature::AvailabilityResult::kNotPresent,
            availability(manifest_feature, bare_extension.get()));
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            availability(manifest_feature, manifest_extension.get()));
  EXPECT_EQ(Feature::AvailabilityResult::kNotPresent,
            availability(manifest_feature, permission_extension.get()));
  EXPECT_EQ(Feature::AvailabilityResult::kNotPresent,
            availability(permission_feature, manifest_extension.get()));
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            availability(permission_feature, permission_extension.get()));
}

TEST(ComplexFeatureDeathTest, RequiresConsistentNoParent) {
  // Keep the mismatch on the first child to cover the child that was
  // previously skipped by the consistency check.
  static constexpr SimpleFeatureData kFeatures[] = {
      {.feature = {.no_parent = true}},
      {},
  };
  static constexpr ComplexFeatureData kData = {
      .features = StaticSpan(kFeatures),
      .feature_type = ComplexFeatureType::kSimple,
  };

  EXPECT_DCHECK_DEATH_WITH(
      { ComplexFeature feature{StaticFeatureData(kData)}; },
      "no_parent across all sub features");
}

TEST(ComplexFeatureTest, MultipleRulesAllowlist) {
  const HashedExtensionId kIdFoo{ExtensionId(kFooId)};
  const HashedExtensionId kIdBar{ExtensionId(kBarId)};
  static constexpr auto kFooAllowlist =
      std::to_array<std::string_view>({kHashedFooId});
  static constexpr auto kBarAllowlist =
      std::to_array<std::string_view>({kHashedBarId});
  static constexpr auto kFeatures = std::to_array<SimpleFeatureData>({
      {.config = {.allowlist = StaticSpan(kFooAllowlist),
                  .extension_types = StaticSpan(kExtensionOnly)}},
      {.config = {.allowlist = StaticSpan(kBarAllowlist),
                  .extension_types = StaticSpan(kLegacyPackagedAppOnly)}},
  });
  static constexpr ComplexFeatureData kData = {
      .features = StaticSpan(kFeatures),
      .feature_type = ComplexFeatureType::kSimple,
  };
  ComplexFeature feature{StaticFeatureData(kData)};

  // Test match 1st rule.
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            feature
                .IsAvailableToManifest(kIdFoo, Manifest::Type::kExtension,
                                       ManifestLocation::kInvalidLocation,
                                       Feature::UNSPECIFIED_PLATFORM,
                                       Feature::GetCurrentPlatform(),
                                       kUnspecifiedContextId)
                .result());

  // Test match 2nd rule.
  EXPECT_EQ(
      Feature::AvailabilityResult::kIsAvailable,
      feature
          .IsAvailableToManifest(
              kIdBar, Manifest::Type::kLegacyPackagedApp,
              ManifestLocation::kInvalidLocation, Feature::UNSPECIFIED_PLATFORM,
              Feature::GetCurrentPlatform(), kUnspecifiedContextId)
          .result());

  // Test allowlist with wrong extension type.
  EXPECT_NE(Feature::AvailabilityResult::kIsAvailable,
            feature
                .IsAvailableToManifest(kIdBar, Manifest::Type::kExtension,
                                       ManifestLocation::kInvalidLocation,
                                       Feature::UNSPECIFIED_PLATFORM,
                                       Feature::GetCurrentPlatform(),
                                       kUnspecifiedContextId)
                .result());
  EXPECT_NE(
      Feature::AvailabilityResult::kIsAvailable,
      feature
          .IsAvailableToManifest(
              kIdFoo, Manifest::Type::kLegacyPackagedApp,
              ManifestLocation::kInvalidLocation, Feature::UNSPECIFIED_PLATFORM,
              Feature::GetCurrentPlatform(), kUnspecifiedContextId)
          .result());
}

TEST(ComplexFeatureTest, AvailableToEnvironment) {
  constexpr int kDeveloperModeContextId = 1;
  constexpr int kRegularContextId = 2;
  static constexpr auto kFeatures = std::to_array<SimpleFeatureData>({
      {
          .feature = {.name = "first"},
          .config = {.developer_mode_only = true},
      },
      {
          .feature = {.name = "second"},
          .config = {.channel = Channel::BETA},
      },
  });
  static constexpr ComplexFeatureData kData = {
      .features = StaticSpan(kFeatures),
      .feature_type = ComplexFeatureType::kSimple,
  };
  ComplexFeature feature{StaticFeatureData(kData)};
  SetCurrentDeveloperMode(kDeveloperModeContextId, true);
  SetCurrentDeveloperMode(kRegularContextId, false);

  // A context satisfying the first rule makes the feature available.
  {
    ScopedCurrentChannel current_channel(Channel::STABLE);
    EXPECT_EQ(
        Feature::AvailabilityResult::kIsAvailable,
        feature.IsAvailableToEnvironment(kDeveloperModeContextId).result());
  }

  // A later rule can make the feature available when the first rule fails.
  {
    ScopedCurrentChannel current_channel(Channel::BETA);
    EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
              feature.IsAvailableToEnvironment(kRegularContextId).result());
  }

  // If every rule fails, the first rule's failure remains authoritative.
  {
    ScopedCurrentChannel current_channel(Channel::STABLE);
    Feature::Availability availability =
        feature.IsAvailableToEnvironment(kRegularContextId);
    EXPECT_EQ(Feature::AvailabilityResult::kRequiresDeveloperMode,
              availability.result());
    EXPECT_EQ("'first' requires the user to have developer mode enabled.",
              availability.message());
  }
}

TEST(ComplexFeatureTest, IdLists) {
  const HashedExtensionId kIdFoo{ExtensionId(kFooId)};
  const HashedExtensionId kIdBar{ExtensionId(kBarId)};
  const HashedExtensionId kIdMissing{ExtensionId(std::string(32, 'c'))};
  static constexpr auto kFooList =
      std::to_array<std::string_view>({kHashedFooId});
  static constexpr auto kBarList =
      std::to_array<std::string_view>({kHashedBarId});
  static constexpr auto kFeatures = std::to_array<SimpleFeatureData>({
      {.config = {.blocklist = StaticSpan(kBarList),
                  .allowlist = StaticSpan(kFooList)}},
      {.config = {.blocklist = StaticSpan(kFooList),
                  .allowlist = StaticSpan(kBarList)}},
  });
  static constexpr ComplexFeatureData kData = {
      .features = StaticSpan(kFeatures),
      .feature_type = ComplexFeatureType::kSimple,
  };
  ComplexFeature feature{StaticFeatureData(kData)};

  EXPECT_TRUE(feature.IsIdInAllowlist(kIdFoo));
  EXPECT_TRUE(feature.IsIdInAllowlist(kIdBar));
  EXPECT_FALSE(feature.IsIdInAllowlist(kIdMissing));

  EXPECT_TRUE(feature.IsIdInBlocklist(kIdBar));
  EXPECT_TRUE(feature.IsIdInBlocklist(kIdFoo));
  EXPECT_FALSE(feature.IsIdInBlocklist(kIdMissing));
}

// Tests that dependencies are correctly checked.
TEST(ComplexFeatureTest, Dependencies) {
  static constexpr auto kCspDependency =
      std::to_array<std::string_view>({"manifest:content_security_policy"});
  static constexpr auto kVideoCaptureDependency =
      std::to_array<std::string_view>({"permission:videoCapture"});
  static constexpr auto kFeatures = std::to_array<SimpleFeatureData>({
      {.config = {.dependencies = StaticSpan(kCspDependency)}},
      {.config = {.dependencies = StaticSpan(kVideoCaptureDependency)}},
  });
  static constexpr ComplexFeatureData kData = {
      .features = StaticSpan(kFeatures),
      .feature_type = ComplexFeatureType::kSimple,
  };
  ComplexFeature feature{StaticFeatureData(kData)};

  // Available to extensions because of the content_security_policy rule.
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            feature
                .IsAvailableToManifest(HashedExtensionId(std::string(32, 'a')),
                                       Manifest::Type::kExtension,
                                       ManifestLocation::kInvalidLocation,
                                       Feature::UNSPECIFIED_PLATFORM,
                                       Feature::GetCurrentPlatform(),
                                       kUnspecifiedContextId)
                .result());

  // Available to platform apps because of the videoCapture rule.
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            feature
                .IsAvailableToManifest(HashedExtensionId(std::string(32, 'b')),
                                       Manifest::Type::kPlatformApp,
                                       ManifestLocation::kInvalidLocation,
                                       Feature::UNSPECIFIED_PLATFORM,
                                       Feature::GetCurrentPlatform(),
                                       kUnspecifiedContextId)
                .result());

  // Not available to hosted apps.
  EXPECT_EQ(Feature::AvailabilityResult::kInvalidType,
            feature
                .IsAvailableToManifest(HashedExtensionId(std::string(32, 'c')),
                                       Manifest::Type::kHostedApp,
                                       ManifestLocation::kInvalidLocation,
                                       Feature::UNSPECIFIED_PLATFORM,
                                       Feature::GetCurrentPlatform(),
                                       kUnspecifiedContextId)
                .result());
}

TEST(ComplexFeatureTest, RequiresDelegatedAvailabilityCheck) {
  // Test a complex feature where |requires_delegated_availability_check| hasn't
  // been set on any of its simple features.
  {
    static constexpr std::array<SimpleFeatureData, 2> kFeatures = {};
    static constexpr ComplexFeatureData kData = {
        .features = StaticSpan(kFeatures),
        .feature_type = ComplexFeatureType::kSimple,
    };
    ComplexFeature complex_feature{StaticFeatureData(kData)};
    EXPECT_FALSE(complex_feature.RequiresDelegatedAvailabilityCheck());
    EXPECT_FALSE(complex_feature.HasDelegatedAvailabilityCheckHandler());
  }

  auto make_delegated_availability_check = [](uint32_t* call_count) {
    return base::BindLambdaForTesting(
        [call_count](const std::string& api_full_name,
                     const Extension* extension, mojom::ContextType context,
                     const GURL& url, Feature::Platform platform,
                     int context_id, bool check_developer_mode,
                     const ContextData& context_data) {
          ++*call_count;
          return *call_count % 2u == 0u;
        });
  };

  // Test a complex feature where |requires_delegated_availability_check| is set
  // on multiple sub-features. The first sub-feature that requires the
  // availability check should fail, while the second sub-feature should pass.
  // In this case, the delegated availability check handler should be called
  // twice.
  {
    static constexpr auto kFeatures = std::to_array<SimpleFeatureData>({
        {.config = {.contexts = StaticSpan(kPrivilegedExtensionOnly)}},
        {.config = {.requires_delegated_availability_check = true}},
        {.config = {.requires_delegated_availability_check = true}},
    });
    static constexpr ComplexFeatureData kData = {
        .features = StaticSpan(kFeatures),
        .feature_type = ComplexFeatureType::kSimple,
    };
    ComplexFeature complex_feature{StaticFeatureData(kData)};
    EXPECT_TRUE(complex_feature.RequiresDelegatedAvailabilityCheck());
    EXPECT_FALSE(complex_feature.HasDelegatedAvailabilityCheckHandler());

    // A call to SetDelegatedAvailabilityCheckHandler() should set the
    // handler to the sub-features that require it.
    uint32_t delegated_availability_check_call_count = 0;
    complex_feature.SetDelegatedAvailabilityCheckHandler(
        make_delegated_availability_check(
            &delegated_availability_check_call_count));
    EXPECT_TRUE(complex_feature.HasDelegatedAvailabilityCheckHandler());

    // This feature should be available the second time that the delegated
    // availability check is called.
    EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
              complex_feature
                  .IsAvailableToContext(
                      /*extension=*/nullptr, mojom::ContextType::kUnspecified,
                      GURL(), kUnspecifiedContextId, TestContextData())
                  .result());
    EXPECT_EQ(2u, delegated_availability_check_call_count);
  }

  static constexpr auto kDescriptorFeatures = std::to_array<SimpleFeatureData>({
      {
          .feature = {.name = "descriptor"},
          .config =
              {
                  .contexts = StaticSpan(kPrivilegedExtensionOnly),
              },
      },
      {
          .feature = {.name = "descriptor"},
          .config =
              {
                  .requires_delegated_availability_check = true,
              },
      },
      {
          .feature = {.name = "descriptor"},
          .config =
              {
                  .requires_delegated_availability_check = true,
              },
      },
  });
  static constexpr ComplexFeatureData kDescriptor = {
      .feature = {.name = "descriptor"},
      .features = StaticSpan(kDescriptorFeatures),
      .feature_type = ComplexFeatureType::kSimple,
  };
  ComplexFeature descriptor_feature{StaticFeatureData(kDescriptor)};
  EXPECT_TRUE(descriptor_feature.RequiresDelegatedAvailabilityCheck());
  uint32_t descriptor_check_call_count = 0;
  descriptor_feature.SetDelegatedAvailabilityCheckHandler(
      make_delegated_availability_check(&descriptor_check_call_count));
  for (int i = 0; i < 2; ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
              descriptor_feature
                  .IsAvailableToContext(
                      /*extension=*/nullptr, mojom::ContextType::kUnspecified,
                      GURL(), kUnspecifiedContextId, TestContextData())
                  .result());
  }
  EXPECT_EQ(4u, descriptor_check_call_count);
  EXPECT_TRUE(descriptor_feature.HasDelegatedAvailabilityCheckHandler());
}

TEST(ComplexFeatureTest, PreservesFirstFailureMessage) {
  static constexpr auto kFeatures = std::to_array<SimpleFeatureData>({
      {
          .feature = {.name = "first"},
          .config = {.extension_types = StaticSpan(kExtensionOnly)},
      },
      {
          .feature = {.name = "second"},
          .config = {.min_manifest_version = 5},
      },
  });
  static constexpr ComplexFeatureData kData = {
      .features = StaticSpan(kFeatures),
      .feature_type = ComplexFeatureType::kSimple,
  };
  ComplexFeature feature{StaticFeatureData(kData)};

  Feature::Availability availability = feature.IsAvailableToManifest(
      HashedExtensionId(), Manifest::Type::kLegacyPackagedApp,
      ManifestLocation::kInvalidLocation, 4, Feature::UNSPECIFIED_PLATFORM,
      kUnspecifiedContextId);
  EXPECT_EQ(Feature::AvailabilityResult::kInvalidType, availability.result());
  EXPECT_EQ(
      "'first' is only allowed for extensions, "
      "but this is a legacy packaged app.",
      availability.message());
}

TEST(ComplexFeatureTest, DescriptorChildTypes) {
  static constexpr auto kDescriptorFeatures = std::to_array<SimpleFeatureData>({
      {
          .feature = {.name = "alarms"},
          .config =
              {
                  .extension_types = StaticSpan(kExtensionOnly),
                  .contexts = StaticSpan(kPrivilegedExtensionOnly),
              },
      },
      {
          .feature = {.name = "alarms"},
          .config =
              {
                  .extension_types = StaticSpan(kExtensionOnly),
                  .contexts = StaticSpan(kPrivilegedExtensionOnly),
              },
      },
  });
  static constexpr ComplexFeatureData kSimpleData = {
      .feature = {.name = "alarms"},
      .features = StaticSpan(kDescriptorFeatures),
      .feature_type = ComplexFeatureType::kSimple,
  };
  static constexpr ComplexFeatureData kManifestData = {
      .feature = {.name = "alarms"},
      .features = StaticSpan(kDescriptorFeatures),
      .feature_type = ComplexFeatureType::kManifest,
  };
  static constexpr ComplexFeatureData kPermissionData = {
      .feature = {.name = "alarms"},
      .features = StaticSpan(kDescriptorFeatures),
      .feature_type = ComplexFeatureType::kPermission,
  };
  const auto extension = ExtensionBuilder("test").Build();

  auto get_availability = [&](ComplexFeature& feature) {
    return feature
        .IsAvailableToContext(extension.get(),
                              mojom::ContextType::kPrivilegedExtension, GURL(),
                              kUnspecifiedContextId, TestContextData())
        .result();
  };
  ComplexFeature simple_feature{StaticFeatureData(kSimpleData)};
  ComplexFeature manifest_feature{StaticFeatureData(kManifestData)};
  ComplexFeature permission_feature{StaticFeatureData(kPermissionData)};
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            get_availability(simple_feature));
  EXPECT_EQ(Feature::AvailabilityResult::kNotPresent,
            get_availability(manifest_feature));
  EXPECT_EQ(Feature::AvailabilityResult::kNotPresent,
            get_availability(permission_feature));
}

}  // namespace extensions
