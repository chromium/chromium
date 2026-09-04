// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/simple_feature.h"

#include <stddef.h>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/complex_feature.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/features/feature_developer_mode_only.h"
#include "extensions/common/features/feature_flags.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/common/features/simple_feature_test_constants.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/switches.h"
#include "extensions/test/test_context_data.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::mojom::ManifestLocation;
using version_info::Channel;

namespace extensions {

namespace {

constexpr char kBazId[] = "bazabbbbccccddddeeeeffffgggghhhh";
constexpr char kNotId[] = "notabbbbccccddddeeeeffffgggghhhh";
constexpr char kTooLongId[] = "slightlytoooolongforanextensionid";
constexpr char kTooShortId[] = "tooshortforanextensionid";

// SHA1 of kBazId.
constexpr std::string_view kHashedBazId =
    "BF6D2F14A9126FD8F44E5050EF8A5FA08E2C1015";
// SHA1 of "monkey", used as an arbitrary non-matching extension ID.
constexpr std::string_view kHashedMonkeyId =
    "AB87D24BDC7452E55738DEB5F868E1F16DEA5ACE";

static_assert(kHashedBazId.size() == 40);
static_assert(kHashedMonkeyId.size() == 40);

// Single-element backing arrays for statically stored descriptors.
constexpr auto kPrivilegedExtensionOnly = std::to_array<mojom::ContextType>(
    {mojom::ContextType::kPrivilegedExtension});
constexpr auto kExtensionOnly =
    std::to_array<Manifest::Type>({Manifest::Type::kExtension});
constexpr auto kLegacyPackagedAppOnly =
    std::to_array<Manifest::Type>({Manifest::Type::kLegacyPackagedApp});

struct IsAvailableTestData {
  ExtensionId extension_id;
  Manifest::Type extension_type;
  ManifestLocation location;
  Feature::Platform platform;
  int manifest_version;
  int context_id;
  Feature::AvailabilityResult expected_result;
};

struct FeatureSessionTypeTestData {
  enum class Profile {
    kNone,
    kKiosk,
    kRegular,
    kRegularAndKiosk,
    kAutolaunchedKiosk,
  };

  std::string desc;
  Feature::AvailabilityResult expected_availability;
  mojom::FeatureSessionType current_session_type;
  Profile profile;
};

constexpr mojom::FeatureSessionType kKioskSessionType[] = {
    mojom::FeatureSessionType::kKiosk};
constexpr mojom::FeatureSessionType kRegularSessionType[] = {
    mojom::FeatureSessionType::kRegular};
constexpr mojom::FeatureSessionType kRegularAndKioskSessionTypes[] = {
    mojom::FeatureSessionType::kRegular, mojom::FeatureSessionType::kKiosk};
constexpr mojom::FeatureSessionType kAutolaunchedKioskSessionType[] = {
    mojom::FeatureSessionType::kAutolaunchedKiosk};
constexpr mojom::ContextType kWebPageContext[] = {mojom::ContextType::kWebPage};
constexpr Feature::Platform kChromeOsPlatform[] = {Feature::CHROMEOS_PLATFORM};
constexpr SimpleFeatureData kDefaultFeatureData = {};

Feature::AvailabilityResult IsAvailableInChannel(
    std::optional<Channel> channel_for_feature,
    Channel channel_for_testing) {
  ScopedCurrentChannel current_channel(channel_for_testing);
  static constexpr SimpleFeatureData kStable = {
      .config = {.channel = Channel::STABLE}};
  static constexpr SimpleFeatureData kBeta = {
      .config = {.channel = Channel::BETA}};
  static constexpr SimpleFeatureData kDev = {
      .config = {.channel = Channel::DEV}};
  static constexpr SimpleFeatureData kCanary = {
      .config = {.channel = Channel::CANARY}};
  static constexpr SimpleFeatureData kUnknown = {
      .config = {.channel = Channel::UNKNOWN}};

  auto get_availability = [](StaticFeatureData<SimpleFeatureData> data) {
    SimpleFeature feature(data);
    return feature
        .IsAvailableToManifest(
            HashedExtensionId(std::string(32, 'a')), Manifest::Type::kUnknown,
            ManifestLocation::kInvalidLocation, -1,
            Feature::GetCurrentPlatform(), kUnspecifiedContextId)
        .result();
  };
  if (!channel_for_feature) {
    return get_availability(StaticFeatureData(kDefaultFeatureData));
  }
  switch (*channel_for_feature) {
    case Channel::STABLE:
      return get_availability(StaticFeatureData(kStable));
    case Channel::BETA:
      return get_availability(StaticFeatureData(kBeta));
    case Channel::DEV:
      return get_availability(StaticFeatureData(kDev));
    case Channel::CANARY:
      return get_availability(StaticFeatureData(kCanary));
    case Channel::UNKNOWN:
      return get_availability(StaticFeatureData(kUnknown));
  }
}

}  // namespace

// The constructor only requires the array to end in a NUL, so an embedded one
// makes the length differ from the array bound.
TEST(StaticCStringTest, SizeExcludesTerminator) {
  static constexpr StaticCString kName("webRequestInternal");
  static_assert(kName.string_view().size() == 18u);
  EXPECT_EQ("webRequestInternal", kName.string_view());
}

TEST(StaticCStringTest, SizeStopsAtEmbeddedNul) {
  static constexpr StaticCString kEmbedded("a\0b");
  static_assert(kEmbedded.string_view().size() == 1u);
  EXPECT_EQ("a", kEmbedded.string_view());
}

TEST(StaticCStringTest, ReferencesLiteralWithoutCopying) {
  static constexpr char kLongName[] = "controlledFrameInternal";
  static constexpr StaticCString kLong(kLongName);
  static_assert(kLong.string_view().size() == 23u);
  static_assert(kLong.string_view().data() == kLongName);
}

TEST(StaticCStringTest, IsPointerSized) {
  static_assert(sizeof(StaticCString) == sizeof(const char*));
}

TEST(StaticCStringTest, DefaultIsAbsent) {
  static constexpr StaticCString kAbsent;
  static_assert(!kAbsent.has_value());
  static_assert(kAbsent.string_view().empty());

  static constexpr StaticCString kPresent("laser-beams");
  static_assert(kPresent.has_value());
}

TEST(SimpleFeatureDescriptorTest, ConstructsFromStaticData) {
  static constexpr auto kBlocklist =
      std::to_array<std::string_view>({kHashedBarId});
  static constexpr auto kAllowlist =
      std::to_array<std::string_view>({kHashedFooId});
  static constexpr SimpleFeatureData kData{
      .feature =
          {
              .name = "descriptor",
              .alias = StaticCString("alias"),
              .source = StaticCString("source"),
              .no_parent = true,
          },
      .config =
          {
              .blocklist = StaticSpan(kBlocklist),
              .allowlist = StaticSpan(kAllowlist),
              .extension_types = StaticSpan(kExtensionOnly),
              .contexts = StaticSpan(kPrivilegedExtensionOnly),
              .is_internal = true,
              .requires_delegated_availability_check = true,
          },
  };
  static constexpr SimpleFeatureData kContextData{
      .feature = {.name = "context"},
      .config = {.contexts = StaticSpan(kPrivilegedExtensionOnly)},
  };

  SimpleFeature feature{StaticFeatureData(kData)};
  SimpleFeature context_feature{StaticFeatureData(kContextData)};

  EXPECT_EQ("descriptor", feature.name());
  EXPECT_EQ("alias", feature.alias());
  EXPECT_EQ("source", feature.source());
  EXPECT_TRUE(feature.no_parent());
  EXPECT_TRUE(feature.IsInternal());
  EXPECT_TRUE(feature.RequiresDelegatedAvailabilityCheck());
  EXPECT_TRUE(feature.IsIdInBlocklist(HashedExtensionId(ExtensionId(kBarId))));
  EXPECT_TRUE(feature.IsIdInAllowlist(HashedExtensionId(ExtensionId(kFooId))));
  EXPECT_EQ(Feature::AvailabilityResult::kInvalidContext,
            context_feature
                .IsAvailableToContext(
                    nullptr, mojom::ContextType::kUnprivilegedExtension, GURL(),
                    kUnspecifiedContextId, TestContextData())
                .result());
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            context_feature
                .IsAvailableToContext(
                    nullptr, mojom::ContextType::kPrivilegedExtension, GURL(),
                    kUnspecifiedContextId, TestContextData())
                .result());
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            feature
                .IsAvailableToManifest(HashedExtensionId(ExtensionId(kFooId)),
                                       Manifest::Type::kExtension,
                                       ManifestLocation::kInvalidLocation,
                                       /*manifest_version=*/1,
                                       Feature::GetCurrentPlatform(),
                                       kUnspecifiedContextId)
                .result());
}

class SimpleFeatureTest : public testing::Test {
 public:
  SimpleFeatureTest(const SimpleFeatureTest&) = delete;
  SimpleFeatureTest& operator=(const SimpleFeatureTest&) = delete;

 protected:
  SimpleFeatureTest() : current_channel_(Channel::UNKNOWN) {}
  bool LocationIsAvailable(SimpleFeature::Location feature_location,
                           ManifestLocation manifest_location) {
    static constexpr SimpleFeatureData kComponent = {
        .config = {.location = SimpleFeature::Location::kComponent}};
    static constexpr SimpleFeatureData kExternalComponent = {
        .config = {.location = SimpleFeature::Location::kExternalComponent}};
    static constexpr SimpleFeatureData kPolicy = {
        .config = {.location = SimpleFeature::Location::kPolicy}};
    static constexpr SimpleFeatureData kUnpacked = {
        .config = {.location = SimpleFeature::Location::kUnpacked}};
    auto is_available =
        [manifest_location](StaticFeatureData<SimpleFeatureData> data) {
          SimpleFeature feature(data);
          return feature
                     .IsAvailableToManifest(
                         HashedExtensionId(), Manifest::Type::kUnknown,
                         manifest_location, -1, Feature::UNSPECIFIED_PLATFORM,
                         kUnspecifiedContextId)
                     .result() == Feature::AvailabilityResult::kIsAvailable;
        };
    switch (feature_location) {
      case SimpleFeature::Location::kComponent:
        return is_available(StaticFeatureData(kComponent));
      case SimpleFeature::Location::kExternalComponent:
        return is_available(StaticFeatureData(kExternalComponent));
      case SimpleFeature::Location::kPolicy:
        return is_available(StaticFeatureData(kPolicy));
      case SimpleFeature::Location::kUnpacked:
        return is_available(StaticFeatureData(kUnpacked));
    }
  }

 private:
  ScopedCurrentChannel current_channel_;
};

TEST_F(SimpleFeatureTest, IsAvailableNullCase) {
  const auto tests = std::to_array<IsAvailableTestData>({
      {"", Manifest::Type::kUnknown, ManifestLocation::kInvalidLocation,
       Feature::UNSPECIFIED_PLATFORM, -1, kUnspecifiedContextId,
       Feature::AvailabilityResult::kIsAvailable},
      {"random-extension", Manifest::Type::kUnknown,
       ManifestLocation::kInvalidLocation, Feature::UNSPECIFIED_PLATFORM, -1,
       kUnspecifiedContextId, Feature::AvailabilityResult::kIsAvailable},
      {"", Manifest::Type::kLegacyPackagedApp,
       ManifestLocation::kInvalidLocation, Feature::UNSPECIFIED_PLATFORM, -1,
       kUnspecifiedContextId, Feature::AvailabilityResult::kIsAvailable},
      {"", Manifest::Type::kUnknown, ManifestLocation::kInvalidLocation,
       Feature::UNSPECIFIED_PLATFORM, -1, kUnspecifiedContextId,
       Feature::AvailabilityResult::kIsAvailable},
      {"", Manifest::Type::kUnknown, ManifestLocation::kComponent,
       Feature::UNSPECIFIED_PLATFORM, -1, kUnspecifiedContextId,
       Feature::AvailabilityResult::kIsAvailable},
      {"", Manifest::Type::kUnknown, ManifestLocation::kInvalidLocation,
       Feature::CHROMEOS_PLATFORM, -1, kUnspecifiedContextId,
       Feature::AvailabilityResult::kIsAvailable},
      {"", Manifest::Type::kUnknown, ManifestLocation::kInvalidLocation,
       Feature::UNSPECIFIED_PLATFORM, 25, kUnspecifiedContextId,
       Feature::AvailabilityResult::kIsAvailable},
  });

  SimpleFeature feature{StaticFeatureData(kDefaultFeatureData)};
  for (const auto& test : tests) {
    EXPECT_EQ(test.expected_result,
              feature
                  .IsAvailableToManifest(HashedExtensionId(test.extension_id),
                                         test.extension_type, test.location,
                                         test.manifest_version, test.platform,
                                         test.context_id)
                  .result());
  }
}

TEST_F(SimpleFeatureTest, Allowlist) {
  const HashedExtensionId kIdFoo{ExtensionId(kFooId)};
  const HashedExtensionId kIdBar{ExtensionId(kBarId)};
  const HashedExtensionId kIdBaz{ExtensionId(kBazId)};
  static constexpr auto kAllowlist =
      std::to_array<std::string_view>({kHashedFooId, kHashedBarId});
  static constexpr SimpleFeatureData kData = {
      .config = {.allowlist = StaticSpan(kAllowlist)}};
  SimpleFeature feature{StaticFeatureData(kData)};

  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            feature
                .IsAvailableToManifest(kIdFoo, Manifest::Type::kUnknown,
                                       ManifestLocation::kInvalidLocation, -1,
                                       Feature::UNSPECIFIED_PLATFORM,
                                       kUnspecifiedContextId)
                .result());
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            feature
                .IsAvailableToManifest(kIdBar, Manifest::Type::kUnknown,
                                       ManifestLocation::kInvalidLocation, -1,
                                       Feature::UNSPECIFIED_PLATFORM,
                                       kUnspecifiedContextId)
                .result());

  EXPECT_EQ(Feature::AvailabilityResult::kNotFoundInAllowlist,
            feature
                .IsAvailableToManifest(kIdBaz, Manifest::Type::kUnknown,
                                       ManifestLocation::kInvalidLocation, -1,
                                       Feature::UNSPECIFIED_PLATFORM,
                                       kUnspecifiedContextId)
                .result());
  EXPECT_EQ(Feature::AvailabilityResult::kNotFoundInAllowlist,
            feature
                .IsAvailableToManifest(
                    HashedExtensionId(), Manifest::Type::kUnknown,
                    ManifestLocation::kInvalidLocation, -1,
                    Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                .result());

  static constexpr SimpleFeatureData kLegacyAppData = {
      .config = {.allowlist = StaticSpan(kAllowlist),
                 .extension_types = StaticSpan(kLegacyPackagedAppOnly)}};
  SimpleFeature legacy_app_feature{StaticFeatureData(kLegacyAppData)};
  EXPECT_EQ(Feature::AvailabilityResult::kNotFoundInAllowlist,
            legacy_app_feature
                .IsAvailableToManifest(
                    kIdBaz, Manifest::Type::kLegacyPackagedApp,
                    ManifestLocation::kInvalidLocation, -1,
                    Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                .result());
}

TEST_F(SimpleFeatureTest, HashedIdAllowlist) {
  static constexpr auto kAllowlist =
      std::to_array<std::string_view>({kHashedFooId});
  static constexpr SimpleFeatureData kData = {
      .config = {.allowlist = StaticSpan(kAllowlist)}};
  SimpleFeature feature{StaticFeatureData(kData)};

  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            feature
                .IsAvailableToManifest(HashedExtensionId(ExtensionId(kFooId)),
                                       Manifest::Type::kUnknown,
                                       ManifestLocation::kInvalidLocation, -1,
                                       Feature::UNSPECIFIED_PLATFORM,
                                       kUnspecifiedContextId)
                .result());
  EXPECT_NE(
      Feature::AvailabilityResult::kIsAvailable,
      feature
          .IsAvailableToManifest(
              HashedExtensionId(ExtensionId(kHashedFooId)),
              Manifest::Type::kUnknown, ManifestLocation::kInvalidLocation, -1,
              Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
          .result());
  EXPECT_EQ(
      Feature::AvailabilityResult::kNotFoundInAllowlist,
      feature
          .IsAvailableToManifest(
              HashedExtensionId(ExtensionId(kTooLongId)),
              Manifest::Type::kUnknown, ManifestLocation::kInvalidLocation, -1,
              Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
          .result());
  EXPECT_EQ(
      Feature::AvailabilityResult::kNotFoundInAllowlist,
      feature
          .IsAvailableToManifest(
              HashedExtensionId(ExtensionId(kTooShortId)),
              Manifest::Type::kUnknown, ManifestLocation::kInvalidLocation, -1,
              Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
          .result());
}

TEST_F(SimpleFeatureTest, CommandLineAllowlistMultipleIds) {
  const HashedExtensionId kHashedFoo((ExtensionId(kFooId)));
  const HashedExtensionId kHashedBar((ExtensionId(kBarId)));

  static constexpr auto kAllowlist =
      std::to_array<std::string_view>({kHashedBazId});
  static constexpr SimpleFeatureData kData = {
      .config = {.allowlist = StaticSpan(kAllowlist)}};
  SimpleFeature feature{StaticFeatureData(kData)};

  EXPECT_EQ(Feature::AvailabilityResult::kNotFoundInAllowlist,
            feature
                .IsAvailableToManifest(kHashedFoo, Manifest::Type::kUnknown,
                                       ManifestLocation::kInvalidLocation, -1,
                                       Feature::UNSPECIFIED_PLATFORM,
                                       kUnspecifiedContextId)
                .result());

  {
    // Allowlist both foo and bar via the command-line override.
    SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
        std::vector<std::string>{std::string(kFooId), std::string(kBarId)});

    // Both foo and bar now pass the allowlist check.
    EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
              feature
                  .IsAvailableToManifest(kHashedFoo, Manifest::Type::kUnknown,
                                         ManifestLocation::kInvalidLocation, -1,
                                         Feature::UNSPECIFIED_PLATFORM,
                                         kUnspecifiedContextId)
                  .result());
    EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
              feature
                  .IsAvailableToManifest(kHashedBar, Manifest::Type::kUnknown,
                                         ManifestLocation::kInvalidLocation, -1,
                                         Feature::UNSPECIFIED_PLATFORM,
                                         kUnspecifiedContextId)
                  .result());

    // An ID not in either list is still rejected.
    EXPECT_EQ(Feature::AvailabilityResult::kNotFoundInAllowlist,
              feature
                  .IsAvailableToManifest(HashedExtensionId(ExtensionId(kNotId)),
                                         Manifest::Type::kUnknown,
                                         ManifestLocation::kInvalidLocation, -1,
                                         Feature::UNSPECIFIED_PLATFORM,
                                         kUnspecifiedContextId)
                  .result());
  }

  // After the scoped override, foo is rejected again.
  EXPECT_EQ(Feature::AvailabilityResult::kNotFoundInAllowlist,
            feature
                .IsAvailableToManifest(kHashedFoo, Manifest::Type::kUnknown,
                                       ManifestLocation::kInvalidLocation, -1,
                                       Feature::UNSPECIFIED_PLATFORM,
                                       kUnspecifiedContextId)
                .result());
}

TEST_F(SimpleFeatureTest, CommandLineAllowlistMultipleIdsFromFlag) {
  const HashedExtensionId kHashedFoo((ExtensionId(kFooId)));
  const HashedExtensionId kHashedBar((ExtensionId(kBarId)));

  static constexpr auto kAllowlist =
      std::to_array<std::string_view>({kHashedFooId});
  static constexpr SimpleFeatureData kData = {
      .config = {.allowlist = StaticSpan(kAllowlist)}};
  SimpleFeature feature{StaticFeatureData(kData)};

  {
    auto allowlist = SimpleFeature::ScopedThreadUnsafeAllowlistForTest::
        CreateFromCommaSeparated(std::string(kFooId) + "," + kBarId);

    EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
              feature
                  .IsAvailableToManifest(kHashedFoo, Manifest::Type::kUnknown,
                                         ManifestLocation::kInvalidLocation, -1,
                                         Feature::UNSPECIFIED_PLATFORM,
                                         kUnspecifiedContextId)
                  .result());
    EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
              feature
                  .IsAvailableToManifest(kHashedBar, Manifest::Type::kUnknown,
                                         ManifestLocation::kInvalidLocation, -1,
                                         Feature::UNSPECIFIED_PLATFORM,
                                         kUnspecifiedContextId)
                  .result());

    EXPECT_EQ(Feature::AvailabilityResult::kNotFoundInAllowlist,
              feature
                  .IsAvailableToManifest(HashedExtensionId(ExtensionId(kNotId)),
                                         Manifest::Type::kUnknown,
                                         ManifestLocation::kInvalidLocation, -1,
                                         Feature::UNSPECIFIED_PLATFORM,
                                         kUnspecifiedContextId)
                  .result());
  }

  EXPECT_EQ(Feature::AvailabilityResult::kNotFoundInAllowlist,
            feature
                .IsAvailableToManifest(kHashedBar, Manifest::Type::kUnknown,
                                       ManifestLocation::kInvalidLocation, -1,
                                       Feature::UNSPECIFIED_PLATFORM,
                                       kUnspecifiedContextId)
                .result());
}

TEST_F(SimpleFeatureTest, Blocklist) {
  const HashedExtensionId kIdFoo{ExtensionId(kFooId)};
  const HashedExtensionId kIdBar{ExtensionId(kBarId)};
  const HashedExtensionId kIdBaz{ExtensionId(kBazId)};
  static constexpr auto kBlocklist =
      std::to_array<std::string_view>({kHashedFooId, kHashedBarId});
  static constexpr SimpleFeatureData kData = {
      .config = {.blocklist = StaticSpan(kBlocklist)}};
  SimpleFeature feature{StaticFeatureData(kData)};

  EXPECT_EQ(Feature::AvailabilityResult::kFoundInBlocklist,
            feature
                .IsAvailableToManifest(kIdFoo, Manifest::Type::kUnknown,
                                       ManifestLocation::kInvalidLocation, -1,
                                       Feature::UNSPECIFIED_PLATFORM,
                                       kUnspecifiedContextId)
                .result());
  EXPECT_EQ(Feature::AvailabilityResult::kFoundInBlocklist,
            feature
                .IsAvailableToManifest(kIdBar, Manifest::Type::kUnknown,
                                       ManifestLocation::kInvalidLocation, -1,
                                       Feature::UNSPECIFIED_PLATFORM,
                                       kUnspecifiedContextId)
                .result());

  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            feature
                .IsAvailableToManifest(kIdBaz, Manifest::Type::kUnknown,
                                       ManifestLocation::kInvalidLocation, -1,
                                       Feature::UNSPECIFIED_PLATFORM,
                                       kUnspecifiedContextId)
                .result());
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            feature
                .IsAvailableToManifest(
                    HashedExtensionId(), Manifest::Type::kUnknown,
                    ManifestLocation::kInvalidLocation, -1,
                    Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                .result());
}

TEST_F(SimpleFeatureTest, HashedIdBlocklist) {
  static constexpr auto kBlocklist =
      std::to_array<std::string_view>({kHashedFooId});
  static constexpr SimpleFeatureData kData = {
      .config = {.blocklist = StaticSpan(kBlocklist)}};
  SimpleFeature feature{StaticFeatureData(kData)};

  EXPECT_EQ(Feature::AvailabilityResult::kFoundInBlocklist,
            feature
                .IsAvailableToManifest(HashedExtensionId(ExtensionId(kFooId)),
                                       Manifest::Type::kUnknown,
                                       ManifestLocation::kInvalidLocation, -1,
                                       Feature::UNSPECIFIED_PLATFORM,
                                       kUnspecifiedContextId)
                .result());
  EXPECT_NE(
      Feature::AvailabilityResult::kFoundInBlocklist,
      feature
          .IsAvailableToManifest(
              HashedExtensionId(ExtensionId(kHashedFooId)),
              Manifest::Type::kUnknown, ManifestLocation::kInvalidLocation, -1,
              Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
          .result());
  EXPECT_EQ(
      Feature::AvailabilityResult::kIsAvailable,
      feature
          .IsAvailableToManifest(
              HashedExtensionId(ExtensionId(kTooLongId)),
              Manifest::Type::kUnknown, ManifestLocation::kInvalidLocation, -1,
              Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
          .result());
  EXPECT_EQ(
      Feature::AvailabilityResult::kIsAvailable,
      feature
          .IsAvailableToManifest(
              HashedExtensionId(ExtensionId(kTooShortId)),
              Manifest::Type::kUnknown, ManifestLocation::kInvalidLocation, -1,
              Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
          .result());
}

TEST_F(SimpleFeatureTest, PackageType) {
  static constexpr auto kExtensionAndLegacyPackagedAppTypes =
      std::to_array<Manifest::Type>(
          {Manifest::Type::kExtension, Manifest::Type::kLegacyPackagedApp});
  static constexpr SimpleFeatureData kData = {
      .config = {.extension_types =
                     StaticSpan(kExtensionAndLegacyPackagedAppTypes)}};
  SimpleFeature feature{StaticFeatureData(kData)};

  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            feature
                .IsAvailableToManifest(
                    HashedExtensionId(), Manifest::Type::kExtension,
                    ManifestLocation::kInvalidLocation, -1,
                    Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                .result());
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            feature
                .IsAvailableToManifest(
                    HashedExtensionId(), Manifest::Type::kLegacyPackagedApp,
                    ManifestLocation::kInvalidLocation, -1,
                    Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                .result());

  EXPECT_EQ(Feature::AvailabilityResult::kInvalidType,
            feature
                .IsAvailableToManifest(
                    HashedExtensionId(), Manifest::Type::kUnknown,
                    ManifestLocation::kInvalidLocation, -1,
                    Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                .result());
  EXPECT_EQ(Feature::AvailabilityResult::kInvalidType,
            feature
                .IsAvailableToManifest(
                    HashedExtensionId(), Manifest::Type::kTheme,
                    ManifestLocation::kInvalidLocation, -1,
                    Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                .result());
}

TEST_F(SimpleFeatureTest, StaticIdentityStrings) {
  static constexpr SimpleFeatureData kData = {
      .feature = {
          .name = "static name",
          .alias = StaticCString("static alias"),
          .source = StaticCString("static source"),
      }};
  SimpleFeature feature{StaticFeatureData(kData)};

  EXPECT_EQ("static name", feature.name());
  EXPECT_EQ("static alias", feature.alias());
  EXPECT_EQ("static source", feature.source());
}

TEST_F(SimpleFeatureTest, Context) {
  auto manifest = base::DictValue()
                      .Set("name", "test")
                      .Set("version", "1")
                      .Set("manifest_version", 21);
  manifest.SetByDottedPath("app.launch.local_path", "foo.html");

  std::u16string error;
  scoped_refptr<const Extension> extension(
      Extension::Create(base::FilePath(), ManifestLocation::kInternal, manifest,
                        Extension::NO_FLAGS, &error));
  EXPECT_EQ(u"", error);
  ASSERT_TRUE(extension.get());

  static constexpr auto kAllowlist =
      std::to_array<std::string_view>({kHashedMonkeyId});
  static constexpr SimpleFeatureData kAllowlistData = {
      .feature = {.name = "somefeature"},
      .config = {
          .allowlist = StaticSpan(kAllowlist),
          .extension_types = StaticSpan(kLegacyPackagedAppOnly),
          .contexts = StaticSpan(kPrivilegedExtensionOnly),
          .platforms = StaticSpan(kChromeOsPlatform),
          .min_manifest_version = 21,
          .max_manifest_version = 25,
      }};
  SimpleFeature allowlist_feature{StaticFeatureData(kAllowlistData)};
  EXPECT_EQ(Feature::AvailabilityResult::kNotFoundInAllowlist,
            allowlist_feature
                .IsAvailableToContext(extension.get(),
                                      mojom::ContextType::kPrivilegedExtension,
                                      Feature::CHROMEOS_PLATFORM,
                                      kUnspecifiedContextId, TestContextData())
                .result());
  static constexpr auto kThemeType =
      std::to_array<Manifest::Type>({Manifest::Type::kTheme});
  static constexpr SimpleFeatureData kThemeData = {
      .feature = {.name = "somefeature"},
      .config = {
          .extension_types = StaticSpan(kThemeType),
          .contexts = StaticSpan(kPrivilegedExtensionOnly),
          .platforms = StaticSpan(kChromeOsPlatform),
          .min_manifest_version = 21,
          .max_manifest_version = 25,
      }};
  SimpleFeature theme_feature{StaticFeatureData(kThemeData)};
  {
    Feature::Availability availability = theme_feature.IsAvailableToContext(
        extension.get(), mojom::ContextType::kPrivilegedExtension,
        Feature::CHROMEOS_PLATFORM, kUnspecifiedContextId, TestContextData());
    EXPECT_EQ(Feature::AvailabilityResult::kInvalidType, availability.result());
    EXPECT_EQ("'somefeature' is only allowed for themes, "
              "but this is a legacy packaged app.",
              availability.message());
  }

  static constexpr auto kUnprivilegedContentContexts =
      std::to_array<mojom::ContextType>(
          {mojom::ContextType::kUnprivilegedExtension,
           mojom::ContextType::kContentScript});
  static constexpr SimpleFeatureData kUnprivilegedContentData = {
      .feature = {.name = "somefeature"},
      .config = {
          .extension_types = StaticSpan(kLegacyPackagedAppOnly),
          .contexts = StaticSpan(kUnprivilegedContentContexts),
          .platforms = StaticSpan(kChromeOsPlatform),
          .min_manifest_version = 21,
          .max_manifest_version = 25,
      }};
  SimpleFeature unprivileged_content_feature{
      StaticFeatureData(kUnprivilegedContentData)};
  {
    Feature::Availability availability =
        unprivileged_content_feature.IsAvailableToContext(
            extension.get(), mojom::ContextType::kPrivilegedExtension,
            Feature::CHROMEOS_PLATFORM, kUnspecifiedContextId,
            TestContextData());
    EXPECT_EQ(Feature::AvailabilityResult::kInvalidContext,
              availability.result());
    EXPECT_EQ("'somefeature' is only allowed to run in extension iframes and "
              "content scripts, but this is a privileged page",
              availability.message());
  }

  static constexpr auto kUnprivilegedContentWebContexts =
      std::to_array<mojom::ContextType>(
          {mojom::ContextType::kUnprivilegedExtension,
           mojom::ContextType::kContentScript, mojom::ContextType::kWebPage});
  static constexpr SimpleFeatureData kUnprivilegedContentWebData = {
      .feature = {.name = "somefeature"},
      .config = {
          .extension_types = StaticSpan(kLegacyPackagedAppOnly),
          .contexts = StaticSpan(kUnprivilegedContentWebContexts),
          .platforms = StaticSpan(kChromeOsPlatform),
          .min_manifest_version = 21,
          .max_manifest_version = 25,
      }};
  SimpleFeature unprivileged_content_web_feature{
      StaticFeatureData(kUnprivilegedContentWebData)};
  {
    Feature::Availability availability =
        unprivileged_content_web_feature.IsAvailableToContext(
            extension.get(), mojom::ContextType::kPrivilegedExtension,
            Feature::CHROMEOS_PLATFORM, kUnspecifiedContextId,
            TestContextData());
    EXPECT_EQ(Feature::AvailabilityResult::kInvalidContext,
              availability.result());
    EXPECT_EQ("'somefeature' is only allowed to run in extension iframes, "
              "content scripts, and web pages, but this is a privileged page",
              availability.message());
  }

  {
    static constexpr SimpleFeatureData kComponentData = {
        .config = {.location = SimpleFeature::Location::kComponent}};
    SimpleFeature other_feature{StaticFeatureData(kComponentData)};
    EXPECT_EQ(Feature::AvailabilityResult::kInvalidLocation,
              other_feature
                  .IsAvailableToContext(
                      extension.get(), mojom::ContextType::kPrivilegedExtension,
                      Feature::CHROMEOS_PLATFORM, kUnspecifiedContextId,
                      TestContextData())
                  .result());
  }

  static constexpr SimpleFeatureData kBaseData = {
      .feature = {.name = "somefeature"},
      .config = {
          .extension_types = StaticSpan(kLegacyPackagedAppOnly),
          .contexts = StaticSpan(kPrivilegedExtensionOnly),
          .platforms = StaticSpan(kChromeOsPlatform),
          .min_manifest_version = 21,
          .max_manifest_version = 25,
      }};
  SimpleFeature feature{StaticFeatureData(kBaseData)};
  EXPECT_EQ(Feature::AvailabilityResult::kInvalidPlatform,
            feature
                .IsAvailableToContext(extension.get(),
                                      mojom::ContextType::kPrivilegedExtension,
                                      Feature::UNSPECIFIED_PLATFORM,
                                      kUnspecifiedContextId, TestContextData())
                .result());

  static constexpr SimpleFeatureData kMinVersionData = {
      .feature = {.name = "somefeature"},
      .config = {
          .extension_types = StaticSpan(kLegacyPackagedAppOnly),
          .contexts = StaticSpan(kPrivilegedExtensionOnly),
          .platforms = StaticSpan(kChromeOsPlatform),
          .min_manifest_version = 22,
          .max_manifest_version = 25,
      }};
  SimpleFeature min_version_feature{StaticFeatureData(kMinVersionData)};
  EXPECT_EQ(Feature::AvailabilityResult::kInvalidMinManifestVersion,
            min_version_feature
                .IsAvailableToContext(extension.get(),
                                      mojom::ContextType::kPrivilegedExtension,
                                      Feature::CHROMEOS_PLATFORM,
                                      kUnspecifiedContextId, TestContextData())
                .result());
  static constexpr SimpleFeatureData kMaxVersionData = {
      .feature = {.name = "somefeature"},
      .config = {
          .extension_types = StaticSpan(kLegacyPackagedAppOnly),
          .contexts = StaticSpan(kPrivilegedExtensionOnly),
          .platforms = StaticSpan(kChromeOsPlatform),
          .min_manifest_version = 21,
          .max_manifest_version = 18,
      }};
  SimpleFeature max_version_feature{StaticFeatureData(kMaxVersionData)};
  EXPECT_EQ(Feature::AvailabilityResult::kInvalidMaxManifestVersion,
            max_version_feature
                .IsAvailableToContext(extension.get(),
                                      mojom::ContextType::kPrivilegedExtension,
                                      Feature::CHROMEOS_PLATFORM,
                                      kUnspecifiedContextId, TestContextData())
                .result());
}

TEST_F(SimpleFeatureTest, SessionType) {
  auto manifest = base::DictValue()
                      .Set("name", "test")
                      .Set("version", "1")
                      .Set("manifest_version", 2);
  manifest.SetByDottedPath("app.launch.local_path", "foo.html");

  std::u16string error;
  scoped_refptr<const Extension> extension(
      Extension::Create(base::FilePath(), ManifestLocation::kInternal, manifest,
                        Extension::NO_FLAGS, &error));
  EXPECT_EQ(u"", error);
  ASSERT_TRUE(extension.get());

  const auto kTestData = std::to_array<FeatureSessionTypeTestData>({
      {"kiosk_feature in kiosk session",
       Feature::AvailabilityResult::kIsAvailable,
       mojom::FeatureSessionType::kKiosk,
       FeatureSessionTypeTestData::Profile::kKiosk},
      {"kiosk feature in regular session",
       Feature::AvailabilityResult::kInvalidSessionType,
       mojom::FeatureSessionType::kRegular,
       FeatureSessionTypeTestData::Profile::kKiosk},
      {"kiosk feature in unknown session",
       Feature::AvailabilityResult::kInvalidSessionType,
       mojom::FeatureSessionType::kUnknown,
       FeatureSessionTypeTestData::Profile::kKiosk},
      {"kiosk feature in initial session",
       Feature::AvailabilityResult::kInvalidSessionType,
       mojom::FeatureSessionType::kInitial,
       FeatureSessionTypeTestData::Profile::kKiosk},
      {"non kiosk feature in kiosk session",
       Feature::AvailabilityResult::kInvalidSessionType,
       mojom::FeatureSessionType::kKiosk,
       FeatureSessionTypeTestData::Profile::kRegular},
      {"non kiosk feature in regular session",
       Feature::AvailabilityResult::kIsAvailable,
       mojom::FeatureSessionType::kRegular,
       FeatureSessionTypeTestData::Profile::kRegular},
      {"non kiosk feature in unknown session",
       Feature::AvailabilityResult::kInvalidSessionType,
       mojom::FeatureSessionType::kUnknown,
       FeatureSessionTypeTestData::Profile::kRegular},
      {"non kiosk feature in initial session",
       Feature::AvailabilityResult::kInvalidSessionType,
       mojom::FeatureSessionType::kInitial,
       FeatureSessionTypeTestData::Profile::kRegular},
      {"session agnostic feature in kiosk session",
       Feature::AvailabilityResult::kIsAvailable,
       mojom::FeatureSessionType::kKiosk,
       FeatureSessionTypeTestData::Profile::kNone},
      {"session agnostic feature in auto-launched kiosk session",
       Feature::AvailabilityResult::kIsAvailable,
       mojom::FeatureSessionType::kAutolaunchedKiosk,
       FeatureSessionTypeTestData::Profile::kNone},
      {"session agnostic feature in regular session",
       Feature::AvailabilityResult::kIsAvailable,
       mojom::FeatureSessionType::kRegular,
       FeatureSessionTypeTestData::Profile::kNone},
      {"session agnostic feature in unknown session",
       Feature::AvailabilityResult::kIsAvailable,
       mojom::FeatureSessionType::kUnknown,
       FeatureSessionTypeTestData::Profile::kNone},
      {"feature with multiple session types",
       Feature::AvailabilityResult::kIsAvailable,
       mojom::FeatureSessionType::kRegular,
       FeatureSessionTypeTestData::Profile::kRegularAndKiosk},
      {"feature with multiple session types in unknown session",
       Feature::AvailabilityResult::kInvalidSessionType,
       mojom::FeatureSessionType::kUnknown,
       FeatureSessionTypeTestData::Profile::kRegularAndKiosk},
      {"feature with multiple session types in initial session",
       Feature::AvailabilityResult::kInvalidSessionType,
       mojom::FeatureSessionType::kInitial,
       FeatureSessionTypeTestData::Profile::kRegularAndKiosk},
      {"feature with auto-launched kiosk session type in regular session",
       Feature::AvailabilityResult::kInvalidSessionType,
       mojom::FeatureSessionType::kAutolaunchedKiosk,
       FeatureSessionTypeTestData::Profile::kRegular},
      {"feature with auto-launched kiosk session type in auto-launched kiosk",
       Feature::AvailabilityResult::kIsAvailable,
       mojom::FeatureSessionType::kAutolaunchedKiosk,
       FeatureSessionTypeTestData::Profile::kAutolaunchedKiosk},
      {"feature with kiosk session type in auto-launched kiosk session",
       Feature::AvailabilityResult::kIsAvailable,
       mojom::FeatureSessionType::kAutolaunchedKiosk,
       FeatureSessionTypeTestData::Profile::kKiosk},
  });

  for (const auto& entry : kTestData) {
    std::unique_ptr<base::AutoReset<mojom::FeatureSessionType>> current_session(
        ScopedCurrentFeatureSessionType(entry.current_session_type));

    static constexpr SimpleFeatureData kNone = {};
    static constexpr SimpleFeatureData kKiosk = {
        .config = {.session_types = StaticSpan(kKioskSessionType)}};
    static constexpr SimpleFeatureData kRegular = {
        .config = {.session_types = StaticSpan(kRegularSessionType)}};
    static constexpr SimpleFeatureData kRegularAndKiosk = {
        .config = {.session_types = StaticSpan(kRegularAndKioskSessionTypes)}};
    static constexpr SimpleFeatureData kAutolaunchedKiosk = {
        .config = {.session_types = StaticSpan(kAutolaunchedKioskSessionType)}};
    auto run_checks = [&](StaticFeatureData<SimpleFeatureData> data) {
      SimpleFeature feature(data);
      EXPECT_EQ(
          entry.expected_availability,
          feature
              .IsAvailableToContext(extension.get(),
                                    mojom::ContextType::kPrivilegedExtension,
                                    Feature::CHROMEOS_PLATFORM,
                                    kUnspecifiedContextId, TestContextData())
              .result())
          << "Failed test '" << entry.desc << "'.";

      EXPECT_EQ(entry.expected_availability,
                feature
                    .IsAvailableToManifest(
                        extension->hashed_id(), Manifest::Type::kUnknown,
                        ManifestLocation::kInvalidLocation, -1,
                        Feature::CHROMEOS_PLATFORM, kUnspecifiedContextId)
                    .result())
          << "Failed test '" << entry.desc << "'.";
    };
    switch (entry.profile) {
      case FeatureSessionTypeTestData::Profile::kNone:
        run_checks(StaticFeatureData(kNone));
        break;
      case FeatureSessionTypeTestData::Profile::kKiosk:
        run_checks(StaticFeatureData(kKiosk));
        break;
      case FeatureSessionTypeTestData::Profile::kRegular:
        run_checks(StaticFeatureData(kRegular));
        break;
      case FeatureSessionTypeTestData::Profile::kRegularAndKiosk:
        run_checks(StaticFeatureData(kRegularAndKiosk));
        break;
      case FeatureSessionTypeTestData::Profile::kAutolaunchedKiosk:
        run_checks(StaticFeatureData(kAutolaunchedKiosk));
        break;
    }
  }
}

TEST_F(SimpleFeatureTest, Location) {
  // Component extensions can access any location.
  EXPECT_TRUE(LocationIsAvailable(SimpleFeature::Location::kComponent,
                                  ManifestLocation::kComponent));
  EXPECT_TRUE(LocationIsAvailable(SimpleFeature::Location::kExternalComponent,
                                  ManifestLocation::kComponent));
  EXPECT_TRUE(LocationIsAvailable(SimpleFeature::Location::kPolicy,
                                  ManifestLocation::kComponent));
  EXPECT_TRUE(LocationIsAvailable(SimpleFeature::Location::kUnpacked,
                                  ManifestLocation::kComponent));

  // Only component extensions can access the "component" location.
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::Location::kComponent,
                                   ManifestLocation::kInvalidLocation));
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::Location::kComponent,
                                   ManifestLocation::kUnpacked));
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::Location::kComponent,
                                   ManifestLocation::kExternalComponent));
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::Location::kComponent,
                                   ManifestLocation::kExternalPrefDownload));
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::Location::kComponent,
                                   ManifestLocation::kExternalPolicy));
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::Location::kComponent,
                                   ManifestLocation::kExternalPolicyDownload));

  // Policy extensions can access the "policy" location.
  EXPECT_TRUE(LocationIsAvailable(SimpleFeature::Location::kPolicy,
                                  ManifestLocation::kExternalPolicy));
  EXPECT_TRUE(LocationIsAvailable(SimpleFeature::Location::kPolicy,
                                  ManifestLocation::kExternalPolicyDownload));

  // Non-policy (except component) extensions cannot access policy.
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::Location::kPolicy,
                                   ManifestLocation::kExternalComponent));
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::Location::kPolicy,
                                   ManifestLocation::kInvalidLocation));
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::Location::kPolicy,
                                   ManifestLocation::kUnpacked));
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::Location::kPolicy,
                                   ManifestLocation::kExternalPrefDownload));

  // External component extensions can access the "external_component"
  // location.
  EXPECT_TRUE(LocationIsAvailable(SimpleFeature::Location::kExternalComponent,
                                  ManifestLocation::kExternalComponent));

  // Only unpacked and command line extensions can access the "unpacked"
  // location.
  EXPECT_TRUE(LocationIsAvailable(SimpleFeature::Location::kUnpacked,
                                  ManifestLocation::kUnpacked));
  EXPECT_TRUE(LocationIsAvailable(SimpleFeature::Location::kUnpacked,
                                  ManifestLocation::kCommandLine));
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::Location::kUnpacked,
                                   ManifestLocation::kInternal));
}

TEST_F(SimpleFeatureTest, Platform) {
  static constexpr SimpleFeatureData kData = {
      .config = {.platforms = StaticSpan(kChromeOsPlatform)}};
  SimpleFeature feature{StaticFeatureData(kData)};
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            feature
                .IsAvailableToManifest(
                    HashedExtensionId(), Manifest::Type::kUnknown,
                    ManifestLocation::kInvalidLocation, -1,
                    Feature::CHROMEOS_PLATFORM, kUnspecifiedContextId)
                .result());
  EXPECT_EQ(Feature::AvailabilityResult::kInvalidPlatform,
            feature
                .IsAvailableToManifest(
                    HashedExtensionId(), Manifest::Type::kUnknown,
                    ManifestLocation::kInvalidLocation, -1,
                    Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                .result());
}

TEST_F(SimpleFeatureTest, ManifestVersion) {
  static constexpr SimpleFeatureData kMinData = {
      .config = {.min_manifest_version = 5}};
  SimpleFeature feature{StaticFeatureData(kMinData)};

  EXPECT_EQ(Feature::AvailabilityResult::kInvalidMinManifestVersion,
            feature
                .IsAvailableToManifest(
                    HashedExtensionId(), Manifest::Type::kUnknown,
                    ManifestLocation::kInvalidLocation, 0,
                    Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                .result());
  EXPECT_EQ(Feature::AvailabilityResult::kInvalidMinManifestVersion,
            feature
                .IsAvailableToManifest(
                    HashedExtensionId(), Manifest::Type::kUnknown,
                    ManifestLocation::kInvalidLocation, 4,
                    Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                .result());

  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            feature
                .IsAvailableToManifest(
                    HashedExtensionId(), Manifest::Type::kUnknown,
                    ManifestLocation::kInvalidLocation, 5,
                    Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                .result());
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            feature
                .IsAvailableToManifest(
                    HashedExtensionId(), Manifest::Type::kUnknown,
                    ManifestLocation::kInvalidLocation, 10,
                    Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                .result());

  static constexpr SimpleFeatureData kMinMaxData = {
      .config = {.min_manifest_version = 5, .max_manifest_version = 8}};
  SimpleFeature min_max_feature{StaticFeatureData(kMinMaxData)};

  EXPECT_EQ(Feature::AvailabilityResult::kInvalidMaxManifestVersion,
            min_max_feature
                .IsAvailableToManifest(
                    HashedExtensionId(), Manifest::Type::kUnknown,
                    ManifestLocation::kInvalidLocation, 10,
                    Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                .result());
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            min_max_feature
                .IsAvailableToManifest(
                    HashedExtensionId(), Manifest::Type::kUnknown,
                    ManifestLocation::kInvalidLocation, 8,
                    Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                .result());
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            min_max_feature
                .IsAvailableToManifest(
                    HashedExtensionId(), Manifest::Type::kUnknown,
                    ManifestLocation::kInvalidLocation, 7,
                    Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                .result());
}

TEST_F(SimpleFeatureTest, CommandLineSwitch) {
  static constexpr SimpleFeatureData kData = {
      .config = {.command_line_switch = StaticCString("laser-beams")}};
  SimpleFeature feature{StaticFeatureData(kData)};
  {
    EXPECT_EQ(Feature::AvailabilityResult::kMissingCommandLineSwitch,
              feature.IsAvailableToEnvironment(kUnspecifiedContextId).result());
  }
  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitch("laser-beams");
    EXPECT_EQ(Feature::AvailabilityResult::kMissingCommandLineSwitch,
              feature.IsAvailableToEnvironment(kUnspecifiedContextId).result());
  }
  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitch(
        "enable-laser-beams");
    EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
              feature.IsAvailableToEnvironment(kUnspecifiedContextId).result());
  }
  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitch(
        "disable-laser-beams");
    EXPECT_EQ(Feature::AvailabilityResult::kMissingCommandLineSwitch,
              feature.IsAvailableToEnvironment(kUnspecifiedContextId).result());
  }
  {
    // --laser-beams=1 should enable the feature.
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        "laser-beams", "1");
    EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
              feature.IsAvailableToEnvironment(kUnspecifiedContextId).result());
  }
  {
    // --laser-beams=0 should not enable the feature.
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        "laser-beams", "0");
    EXPECT_EQ(Feature::AvailabilityResult::kMissingCommandLineSwitch,
              feature.IsAvailableToEnvironment(kUnspecifiedContextId).result());
  }
  {
    // --laser-beams=2 (non-"1" value) should not enable the feature.
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        "laser-beams", "2");
    EXPECT_EQ(Feature::AvailabilityResult::kMissingCommandLineSwitch,
              feature.IsAvailableToEnvironment(kUnspecifiedContextId).result());
  }
  {
    // --laser-beams (no value) should not enable the feature.
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        "laser-beams", "");
    EXPECT_EQ(Feature::AvailabilityResult::kMissingCommandLineSwitch,
              feature.IsAvailableToEnvironment(kUnspecifiedContextId).result());
  }
}

TEST_F(SimpleFeatureTest, FeatureFlags) {
  static BASE_FEATURE(kStubFeature1, base::FEATURE_ENABLED_BY_DEFAULT);
  static BASE_FEATURE(kStubFeature2, base::FEATURE_DISABLED_BY_DEFAULT);
  const base::Feature* kOverriddenFeatures[] = {&kStubFeature1, &kStubFeature2};
  auto scoped_feature_override =
      CreateScopedFeatureFlagsOverrideForTesting(kOverriddenFeatures);

  static constexpr SimpleFeatureData kFeature1Data = {
      .config = {.feature_flag = StaticCString("StubFeature1")}};
  SimpleFeature simple_feature_1{StaticFeatureData(kFeature1Data)};
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            simple_feature_1.IsAvailableToEnvironment(kUnspecifiedContextId)
                .result());

  static constexpr SimpleFeatureData kFeature2Data = {
      .config = {.feature_flag = StaticCString("StubFeature2")}};
  SimpleFeature simple_feature_2{StaticFeatureData(kFeature2Data)};
  EXPECT_EQ(Feature::AvailabilityResult::kFeatureFlagDisabled,
            simple_feature_2.IsAvailableToEnvironment(kUnspecifiedContextId)
                .result());

  // Ensure we take any base::Feature overrides into account.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kStubFeature2} /* enabled_features */,
                                       {kStubFeature1} /* disabled_features */);
  EXPECT_EQ(Feature::AvailabilityResult::kFeatureFlagDisabled,
            simple_feature_1.IsAvailableToEnvironment(kUnspecifiedContextId)
                .result());
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            simple_feature_2.IsAvailableToEnvironment(kUnspecifiedContextId)
                .result());
}

// Tests that all combinations of feature channel and Chrome channel correctly
// compute feature availability.
TEST_F(SimpleFeatureTest, SupportedChannel) {
  // stable supported.
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            IsAvailableInChannel(Channel::STABLE, Channel::UNKNOWN));
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            IsAvailableInChannel(Channel::STABLE, Channel::CANARY));
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            IsAvailableInChannel(Channel::STABLE, Channel::DEV));
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            IsAvailableInChannel(Channel::STABLE, Channel::BETA));
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            IsAvailableInChannel(Channel::STABLE, Channel::STABLE));

  // beta supported.
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            IsAvailableInChannel(Channel::BETA, Channel::UNKNOWN));
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            IsAvailableInChannel(Channel::BETA, Channel::CANARY));
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            IsAvailableInChannel(Channel::BETA, Channel::DEV));
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            IsAvailableInChannel(Channel::BETA, Channel::BETA));
  EXPECT_EQ(Feature::AvailabilityResult::kUnsupportedChannel,
            IsAvailableInChannel(Channel::BETA, Channel::STABLE));

  // dev supported.
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            IsAvailableInChannel(Channel::DEV, Channel::UNKNOWN));
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            IsAvailableInChannel(Channel::DEV, Channel::CANARY));
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            IsAvailableInChannel(Channel::DEV, Channel::DEV));
  EXPECT_EQ(Feature::AvailabilityResult::kUnsupportedChannel,
            IsAvailableInChannel(Channel::DEV, Channel::BETA));
  EXPECT_EQ(Feature::AvailabilityResult::kUnsupportedChannel,
            IsAvailableInChannel(Channel::DEV, Channel::STABLE));

  // canary supported.
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            IsAvailableInChannel(Channel::CANARY, Channel::UNKNOWN));
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            IsAvailableInChannel(Channel::CANARY, Channel::CANARY));
  EXPECT_EQ(Feature::AvailabilityResult::kUnsupportedChannel,
            IsAvailableInChannel(Channel::CANARY, Channel::DEV));
  EXPECT_EQ(Feature::AvailabilityResult::kUnsupportedChannel,
            IsAvailableInChannel(Channel::CANARY, Channel::BETA));
  EXPECT_EQ(Feature::AvailabilityResult::kUnsupportedChannel,
            IsAvailableInChannel(Channel::CANARY, Channel::STABLE));

  // trunk supported.
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            IsAvailableInChannel(Channel::UNKNOWN, Channel::UNKNOWN));
  EXPECT_EQ(Feature::AvailabilityResult::kUnsupportedChannel,
            IsAvailableInChannel(Channel::UNKNOWN, Channel::CANARY));
  EXPECT_EQ(Feature::AvailabilityResult::kUnsupportedChannel,
            IsAvailableInChannel(Channel::UNKNOWN, Channel::DEV));
  EXPECT_EQ(Feature::AvailabilityResult::kUnsupportedChannel,
            IsAvailableInChannel(Channel::UNKNOWN, Channel::BETA));
  EXPECT_EQ(Feature::AvailabilityResult::kUnsupportedChannel,
            IsAvailableInChannel(Channel::UNKNOWN, Channel::STABLE));

  // Verify that a feature without a channel specified is available in all
  // channels.
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            IsAvailableInChannel(std::nullopt, Channel::UNKNOWN));
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            IsAvailableInChannel(std::nullopt, Channel::CANARY));
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            IsAvailableInChannel(std::nullopt, Channel::DEV));
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            IsAvailableInChannel(std::nullopt, Channel::BETA));
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            IsAvailableInChannel(std::nullopt, Channel::STABLE));
}

// Tests simple feature availability across channels.
TEST_F(SimpleFeatureTest, SimpleFeatureAvailability) {
  static constexpr auto kFeatures = std::to_array<SimpleFeatureData>({
      {.config = {.extension_types = StaticSpan(kExtensionOnly),
                  .channel = Channel::BETA}},
      {.config = {.extension_types = StaticSpan(kLegacyPackagedAppOnly),
                  .channel = Channel::BETA}},
  });
  static constexpr ComplexFeatureData kData = {
      .features = StaticSpan(kFeatures),
      .feature_type = ComplexFeatureType::kSimple,
  };
  ComplexFeature complex_feature{StaticFeatureData(kData)};
  Feature* feature = &complex_feature;
  // Make sure both rules are applied correctly.

  const HashedExtensionId kId1(std::string(32, 'a'));
  const HashedExtensionId kId2(std::string(32, 'b'));
  {
    ScopedCurrentChannel current_channel(Channel::BETA);
    EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
              feature
                  ->IsAvailableToManifest(kId1, Manifest::Type::kExtension,
                                          ManifestLocation::kInvalidLocation,
                                          Feature::UNSPECIFIED_PLATFORM,
                                          kUnspecifiedContextId)
                  .result());
    EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
              feature
                  ->IsAvailableToManifest(
                      kId2, Manifest::Type::kLegacyPackagedApp,
                      ManifestLocation::kInvalidLocation,
                      Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                  .result());
  }
  {
    ScopedCurrentChannel current_channel(Channel::STABLE);
    EXPECT_NE(Feature::AvailabilityResult::kIsAvailable,
              feature
                  ->IsAvailableToManifest(kId1, Manifest::Type::kExtension,
                                          ManifestLocation::kInvalidLocation,
                                          Feature::UNSPECIFIED_PLATFORM,
                                          kUnspecifiedContextId)
                  .result());
    EXPECT_NE(Feature::AvailabilityResult::kIsAvailable,
              feature
                  ->IsAvailableToManifest(
                      kId2, Manifest::Type::kLegacyPackagedApp,
                      ManifestLocation::kInvalidLocation,
                      Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                  .result());
  }
}

// Tests complex feature availability across channels.
TEST_F(SimpleFeatureTest, ComplexFeatureAvailability) {
  static constexpr auto kFeatures = std::to_array<SimpleFeatureData>({
      {.config = {.extension_types = StaticSpan(kExtensionOnly),
                  .channel = Channel::UNKNOWN}},
      {.config = {.extension_types = StaticSpan(kLegacyPackagedAppOnly),
                  .channel = Channel::STABLE}},
  });
  static constexpr ComplexFeatureData kData = {
      .features = StaticSpan(kFeatures),
      .feature_type = ComplexFeatureType::kSimple,
  };
  ComplexFeature complex_feature{StaticFeatureData(kData)};

  const HashedExtensionId kId1(std::string(32, 'a'));
  const HashedExtensionId kId2(std::string(32, 'b'));
  Feature* feature = &complex_feature;
  {
    ScopedCurrentChannel current_channel(Channel::UNKNOWN);
    EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
              feature
                  ->IsAvailableToManifest(kId1, Manifest::Type::kExtension,
                                          ManifestLocation::kInvalidLocation,
                                          Feature::UNSPECIFIED_PLATFORM,
                                          kUnspecifiedContextId)
                  .result());
  }
  {
    ScopedCurrentChannel current_channel(Channel::BETA);
    EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
              feature
                  ->IsAvailableToManifest(
                      kId2, Manifest::Type::kLegacyPackagedApp,
                      ManifestLocation::kInvalidLocation,
                      Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                  .result());
  }
  {
    ScopedCurrentChannel current_channel(Channel::BETA);
    EXPECT_NE(Feature::AvailabilityResult::kIsAvailable,
              feature
                  ->IsAvailableToManifest(kId1, Manifest::Type::kExtension,
                                          ManifestLocation::kInvalidLocation,
                                          Feature::UNSPECIFIED_PLATFORM,
                                          kUnspecifiedContextId)
                  .result());
  }
}

TEST(SimpleFeatureUnitTest, TestRequiresDelegatedAvailabilityCheck) {
  // Test a feature that requires a delegated availability check, but the check
  // fails.
  static constexpr char kDisallowedFeatureName[] = "DisallowedFeature";
  static constexpr char kAllowedFeatureName[] = "AllowedFeature";
  std::string_view expected_feature_name = kDisallowedFeatureName;
  uint32_t delegated_availability_check_call_count = 0;
  auto delegated_availability_check = base::BindLambdaForTesting(
      [&](const std::string& api_full_name, const Extension* extension,
          mojom::ContextType context, const GURL& url,
          Feature::Platform platform, int context_id, bool check_developer_mode,
          const ContextData& context_data) {
        ++delegated_availability_check_call_count;
        EXPECT_EQ(expected_feature_name, api_full_name);
        return api_full_name == kAllowedFeatureName;
      });

  const GURL kTestPage = GURL("https://www.example.com");
  static constexpr auto kMatches =
      std::to_array<std::string_view>({"https://www.example.com/"});
  static constexpr SimpleFeatureData kUnnamedData = {
      .config = {
          .contexts = StaticSpan(kWebPageContext),
          .match_patterns = StaticSpan(kMatches),
          .requires_delegated_availability_check = true,
      }};
  static constexpr SimpleFeatureData kDisallowedData = {
      .feature = {.name = kDisallowedFeatureName},
      .config = {
          .contexts = StaticSpan(kWebPageContext),
          .match_patterns = StaticSpan(kMatches),
          .requires_delegated_availability_check = true,
      }};
  static constexpr SimpleFeatureData kAllowedData = {
      .feature = {.name = kAllowedFeatureName},
      .config = {
          .contexts = StaticSpan(kWebPageContext),
          .match_patterns = StaticSpan(kMatches),
          .requires_delegated_availability_check = true,
      }};
  static constexpr SimpleFeatureData kDevData = {
      .feature = {.name = kAllowedFeatureName},
      .config = {
          .contexts = StaticSpan(kWebPageContext),
          .match_patterns = StaticSpan(kMatches),
          .channel = Channel::DEV,
          .requires_delegated_availability_check = true,
      }};
  static constexpr SimpleFeatureData kStableData = {
      .feature = {.name = kAllowedFeatureName},
      .config = {
          .contexts = StaticSpan(kWebPageContext),
          .match_patterns = StaticSpan(kMatches),
          .channel = Channel::STABLE,
          .requires_delegated_availability_check = true,
      }};
  SimpleFeature unnamed_feature{StaticFeatureData(kUnnamedData)};
  {
    // Test a feature that requires a delegated availability check but is
    // missing the check handler.
    EXPECT_EQ(Feature::AvailabilityResult::kMissingDelegatedAvailabilityCheck,
              unnamed_feature
                  .IsAvailableToContext(
                      /*extension=*/nullptr, mojom::ContextType::kWebPage,
                      kTestPage, kUnspecifiedContextId, TestContextData())
                  .result());
  }

  SimpleFeature disallowed_feature{StaticFeatureData(kDisallowedData)};
  disallowed_feature.SetDelegatedAvailabilityCheckHandler(
      delegated_availability_check);
  {
    // Test a feature that requires a delegated availability check and the check
    // is not successful.
    EXPECT_EQ(Feature::AvailabilityResult::kFailedDelegatedAvailabilityCheck,
              disallowed_feature
                  .IsAvailableToContext(
                      /*extension=*/nullptr, mojom::ContextType::kWebPage,
                      kTestPage, kUnspecifiedContextId, TestContextData())
                  .result());
    EXPECT_EQ(1u, delegated_availability_check_call_count);
  }

  expected_feature_name = kAllowedFeatureName;
  SimpleFeature allowed_feature{StaticFeatureData(kAllowedData)};
  allowed_feature.SetDelegatedAvailabilityCheckHandler(
      delegated_availability_check);
  {
    // Test a feature that requires a delegated availability check and the check
    // is successful.
    EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
              allowed_feature
                  .IsAvailableToContext(
                      /*extension=*/nullptr, mojom::ContextType::kWebPage,
                      kTestPage, kUnspecifiedContextId, TestContextData())
                  .result());
    EXPECT_EQ(2u, delegated_availability_check_call_count);
  }

  SimpleFeature dev_feature{StaticFeatureData(kDevData)};
  dev_feature.SetDelegatedAvailabilityCheckHandler(
      delegated_availability_check);
  {
    // Test a feature that requires a delegated availability check and the check
    // would be successful, but actually isn't called since the environment
    // check fails.
    ScopedCurrentChannel current_channel(Channel::STABLE);
    EXPECT_EQ(Feature::AvailabilityResult::kUnsupportedChannel,
              dev_feature
                  .IsAvailableToContext(
                      /*extension=*/nullptr, mojom::ContextType::kWebPage,
                      kTestPage, kUnspecifiedContextId, TestContextData())
                  .result());
    EXPECT_EQ(2u, delegated_availability_check_call_count);
  }
  SimpleFeature stable_feature{StaticFeatureData(kStableData)};
  stable_feature.SetDelegatedAvailabilityCheckHandler(
      delegated_availability_check);
  {
    // Test a feature that requires a delegated availability check and the check
    // would be successful, then confirm the check is called because the
    // environment check passes.
    ScopedCurrentChannel current_channel(Channel::STABLE);
    EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
              stable_feature
                  .IsAvailableToContext(
                      /*extension=*/nullptr, mojom::ContextType::kWebPage,
                      kTestPage, kUnspecifiedContextId, TestContextData())
                  .result());
    EXPECT_EQ(3u, delegated_availability_check_call_count);
  }

  const GURL kTestPageNotInMatchList = GURL("https://www.not.example.com");
  {
    // Test a feature that requires a delegated availability check and the check
    // would be successful, but the URL is not contained in the matchlist.
    EXPECT_EQ(Feature::AvailabilityResult::kInvalidUrl,
              stable_feature
                  .IsAvailableToContext(
                      /*extension=*/nullptr, mojom::ContextType::kWebPage,
                      kTestPageNotInMatchList, kUnspecifiedContextId,
                      TestContextData())
                  .result());
    EXPECT_EQ(4u, delegated_availability_check_call_count);
  }
}

TEST(SimpleFeatureUnitTest, TestChannelsWithoutExtension) {
  static constexpr auto kWebUiContext =
      std::to_array<mojom::ContextType>({mojom::ContextType::kWebUi});
  static constexpr auto kMatches =
      std::to_array<std::string_view>({"chrome://settings/*"});
  static constexpr SimpleFeatureData kData = {
      .config = {
          .contexts = StaticSpan(kWebUiContext),
          .match_patterns = StaticSpan(kMatches),
          .channel = Channel::UNKNOWN,
      }};
  SimpleFeature feature{StaticFeatureData(kData)};

  const GURL kAllowlistedUrl(content::GetWebUIURL("settings/foo"));
  const GURL kOtherUrl("https://example.com");

  {
    // It should be available on trunk.
    ScopedCurrentChannel current_channel(Channel::UNKNOWN);
    EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
              feature
                  .IsAvailableToContext(nullptr, mojom::ContextType::kWebUi,
                                        kAllowlistedUrl, kUnspecifiedContextId,
                                        TestContextData())
                  .result());
  }
  {
    // It should be unavailable on beta.
    ScopedCurrentChannel current_channel(Channel::BETA);
    EXPECT_EQ(Feature::AvailabilityResult::kUnsupportedChannel,
              feature
                  .IsAvailableToContext(nullptr, mojom::ContextType::kWebUi,
                                        kAllowlistedUrl, kUnspecifiedContextId,
                                        TestContextData())
                  .result());
  }
}

// Verifies matches are evaluated correctly and that repeated checks against the
// same feature are stable (patterns are parsed transiently per check).
TEST(SimpleFeatureUnitTest, MatchesEvaluation) {
  static constexpr auto kMatches =
      std::to_array<std::string_view>({"https://example.com/*"});
  static constexpr SimpleFeatureData kData = {
      .config = {
          .contexts = StaticSpan(kWebPageContext),
          .match_patterns = StaticSpan(kMatches),
      }};
  SimpleFeature feature{StaticFeatureData(kData)};

  const GURL kMatch("https://example.com/path");
  const GURL kNoMatch("https://other.example/path");

  // Repeated checks must be stable across matching and non-matching URLs.
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(
        Feature::AvailabilityResult::kIsAvailable,
        feature
            .IsAvailableToContext(nullptr, mojom::ContextType::kWebPage, kMatch,
                                  kUnspecifiedContextId, TestContextData())
            .result());
    EXPECT_EQ(Feature::AvailabilityResult::kInvalidUrl,
              feature
                  .IsAvailableToContext(nullptr, mojom::ContextType::kWebPage,
                                        kNoMatch, kUnspecifiedContextId,
                                        TestContextData())
                  .result());
  }
}

// A web-exposed feature with no matches set is never available to a web
// context, regardless of URL (default-deny), whether matches is left unset or
// explicitly empty.
TEST(SimpleFeatureUnitTest, MatchesEmptyDenies) {
  static constexpr SimpleFeatureData kUnsetMatches = {
      .config = {.contexts = StaticSpan(kWebPageContext)}};
  static constexpr SimpleFeatureData kEmptyMatches = {
      .config = {
          .contexts = StaticSpan(kWebPageContext),
          .match_patterns = StaticSpan<std::string_view>(),
      }};
  auto expect_denied = [](StaticFeatureData<SimpleFeatureData> data) {
    SimpleFeature feature(data);
    EXPECT_EQ(
        Feature::AvailabilityResult::kInvalidUrl,
        feature
            .IsAvailableToContext(nullptr, mojom::ContextType::kWebPage,
                                  GURL("https://example.com/"),
                                  kUnspecifiedContextId, TestContextData())
            .result());
  };
  expect_denied(StaticFeatureData(kUnsetMatches));
  expect_denied(StaticFeatureData(kEmptyMatches));
}

TEST(SimpleFeatureUnitTest, EmptyContextsRestrictsAllContexts) {
  static constexpr auto kAllContexts = std::to_array<mojom::ContextType>({
      mojom::ContextType::kPrivilegedExtension,
      mojom::ContextType::kUnprivilegedExtension,
      mojom::ContextType::kContentScript,
      mojom::ContextType::kPrivilegedWebPage,
      mojom::ContextType::kWebPage,
      mojom::ContextType::kWebUi,
      mojom::ContextType::kUntrustedWebUi,
      mojom::ContextType::kOffscreenExtension,
      mojom::ContextType::kUserScript,
      mojom::ContextType::kUnspecified,
  });
  static_assert(kAllContexts.size() ==
                static_cast<size_t>(mojom::ContextType::kMaxValue) + 1);

  static constexpr SimpleFeatureData kData = {
      .config = {.contexts = StaticSpan<mojom::ContextType>()}};
  SimpleFeature feature{StaticFeatureData(kData)};
  for (mojom::ContextType context : kAllContexts) {
    EXPECT_EQ(
        Feature::AvailabilityResult::kInvalidContext,
        feature
            .IsAvailableToContext(nullptr, context, GURL(),
                                  kUnspecifiedContextId, TestContextData())
            .result())
        << "context " << static_cast<int>(context);
  }
}

TEST(SimpleFeatureUnitTest, UnsetContextsHasNoContextRestriction) {
  // URL-matching contexts are gated separately by `matches`.
  SimpleFeature feature{StaticFeatureData(kDefaultFeatureData)};
  for (mojom::ContextType context : {mojom::ContextType::kPrivilegedExtension,
                                     mojom::ContextType::kUnprivilegedExtension,
                                     mojom::ContextType::kContentScript}) {
    EXPECT_EQ(
        Feature::AvailabilityResult::kIsAvailable,
        feature
            .IsAvailableToContext(nullptr, context, GURL(),
                                  kUnspecifiedContextId, TestContextData())
            .result())
        << "context " << static_cast<int>(context);
  }
}

TEST(SimpleFeatureUnitTest, TestAvailableToEnvironment) {
  {
    // Test with no environment restrictions, but with other restrictions. The
    // result should always be available.
    static constexpr SimpleFeatureData kData = {
        .config = {
            .extension_types = StaticSpan(kExtensionOnly),
            .contexts = StaticSpan(kPrivilegedExtensionOnly),
            .min_manifest_version = 2,
        }};
    SimpleFeature feature{StaticFeatureData(kData)};
    EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
              feature.IsAvailableToEnvironment(kUnspecifiedContextId).result());
  }

  {
    // Test with channel restrictions.
    static constexpr SimpleFeatureData kData = {
        .config = {.channel = Channel::BETA}};
    SimpleFeature feature{StaticFeatureData(kData)};
    {
      ScopedCurrentChannel current_channel(Channel::BETA);
      EXPECT_EQ(
          Feature::AvailabilityResult::kIsAvailable,
          feature.IsAvailableToEnvironment(kUnspecifiedContextId).result());
    }
    {
      ScopedCurrentChannel current_channel(Channel::STABLE);
      EXPECT_EQ(
          Feature::AvailabilityResult::kUnsupportedChannel,
          feature.IsAvailableToEnvironment(kUnspecifiedContextId).result());
    }
  }

  {
    // Test with command-line restrictions.
    static constexpr char kFakeSwitch[] = "some-fake-switch";
    static constexpr SimpleFeatureData kData = {
        .config = {.command_line_switch = StaticCString(kFakeSwitch)}};
    SimpleFeature feature{StaticFeatureData(kData)};

    EXPECT_EQ(Feature::AvailabilityResult::kMissingCommandLineSwitch,
              feature.IsAvailableToEnvironment(kUnspecifiedContextId).result());
    {
      base::test::ScopedCommandLine command_line;
      command_line.GetProcessCommandLine()->AppendSwitch(
          base::StringPrintf("enable-%s", kFakeSwitch));
      EXPECT_EQ(
          Feature::AvailabilityResult::kIsAvailable,
          feature.IsAvailableToEnvironment(kUnspecifiedContextId).result());
    }
  }

  // Note: if we wanted, we could add a ScopedCurrentPlatform() and add
  // platform-test restrictions?
}

TEST(SimpleFeatureUnitTest, AvailableToEnvironmentChecksDependencies) {
  static constexpr auto kPresentDependency =
      std::to_array<std::string_view>({"manifest:content_security_policy"});
  static constexpr auto kMissingDependency =
      std::to_array<std::string_view>({"manifest:missing"});
  static constexpr SimpleFeatureData kPresentData = {
      .config = {.dependencies = StaticSpan(kPresentDependency)}};
  static constexpr SimpleFeatureData kMissingData = {
      .config = {.dependencies = StaticSpan(kMissingDependency)}};
  SimpleFeature present_feature{StaticFeatureData(kPresentData)};
  SimpleFeature missing_feature{StaticFeatureData(kMissingData)};

  // An available dependency preserves environment availability.
  EXPECT_EQ(
      Feature::AvailabilityResult::kIsAvailable,
      present_feature.IsAvailableToEnvironment(kUnspecifiedContextId).result());

  // A missing dependency makes an otherwise unrestricted feature unavailable.
  EXPECT_EQ(
      Feature::AvailabilityResult::kNotPresent,
      missing_feature.IsAvailableToEnvironment(kUnspecifiedContextId).result());
}

TEST(SimpleFeatureUnitTest, TestExperimentalExtensionApisSwitch) {
  ScopedCurrentChannel current_channel(Channel::STABLE);

  static constexpr SimpleFeatureData kData = {
      .config = {.channel = Channel::UNKNOWN}};
  SimpleFeature feature{StaticFeatureData(kData)};
  auto test_feature = [&feature]() {
    return feature.IsAvailableToEnvironment(kUnspecifiedContextId).result();
  };

  // Reuse one feature so each query must observe the current command line.
  {
    base::test::ScopedCommandLine scoped_command_line;
    EXPECT_EQ(Feature::AvailabilityResult::kUnsupportedChannel, test_feature());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitch(
        switches::kEnableExperimentalExtensionApis);
    EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable, test_feature());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    EXPECT_EQ(Feature::AvailabilityResult::kUnsupportedChannel, test_feature());
  }
}

TEST_F(SimpleFeatureTest, RestrictDeveloperModeAPIs) {
  static constexpr int kContextId1 = 1;
  static constexpr int kContextId2 = 2;
  static constexpr SimpleFeatureData kDevModeOnlyData = {
      .config = {.developer_mode_only = true}};
  SimpleFeature dev_mode_only_feature{StaticFeatureData(kDevModeOnlyData)};
  SimpleFeature other_feature{StaticFeatureData(kDefaultFeatureData)};

  // With kDeveloperModeRestriction enabled, developer mode-only APIs
  // should be available if and only if the user is in dev mode.
  SetCurrentDeveloperMode(kContextId1, true);
  EXPECT_EQ(
      Feature::AvailabilityResult::kIsAvailable,
      dev_mode_only_feature.IsAvailableToEnvironment(kContextId1).result());
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            other_feature.IsAvailableToEnvironment(kContextId1).result());

  SetCurrentDeveloperMode(kContextId1, false);
  EXPECT_EQ(
      Feature::AvailabilityResult::kRequiresDeveloperMode,
      dev_mode_only_feature.IsAvailableToEnvironment(kContextId1).result());
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            other_feature.IsAvailableToEnvironment(kContextId1).result());

  SetCurrentDeveloperMode(kContextId2, true);
  EXPECT_EQ(
      Feature::AvailabilityResult::kIsAvailable,
      dev_mode_only_feature.IsAvailableToEnvironment(kContextId2).result());
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            other_feature.IsAvailableToEnvironment(kContextId2).result());

  SetCurrentDeveloperMode(kContextId2, false);
  EXPECT_EQ(
      Feature::AvailabilityResult::kRequiresDeveloperMode,
      dev_mode_only_feature.IsAvailableToEnvironment(kContextId2).result());
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            other_feature.IsAvailableToEnvironment(kContextId2).result());
}

TEST(SimpleFeatureUnitTest, DisallowForServiceWorkers) {
  static constexpr SimpleFeatureData kAllowedData = {
      .feature = {.name = "somefeature"},
      .config = {
          .extension_types = StaticSpan(kExtensionOnly),
          .contexts = StaticSpan(kPrivilegedExtensionOnly),
      }};
  static constexpr SimpleFeatureData kDisallowedData = {
      .feature = {.name = "somefeature"},
      .config = {
          .extension_types = StaticSpan(kExtensionOnly),
          .contexts = StaticSpan(kPrivilegedExtensionOnly),
          .disallow_for_service_workers = true,
      }};
  SimpleFeature feature{StaticFeatureData(kAllowedData)};

  auto extension = ExtensionBuilder("test")
                       .SetBackgroundContext(
                           ExtensionBuilder::BackgroundContext::SERVICE_WORKER)
                       .Build();
  ASSERT_TRUE(extension.get());
  EXPECT_TRUE(BackgroundInfo::IsServiceWorkerBased(extension.get()));

  // Expect the feature is allowed, since the default is to allow.
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            feature
                .IsAvailableToContext(
                    extension.get(), mojom::ContextType::kPrivilegedExtension,
                    extension->GetResourceURL(
                        ExtensionBuilder::kServiceWorkerScriptFile),
                    Feature::CHROMEOS_PLATFORM, kUnspecifiedContextId,
                    TestContextData())
                .result());

  // Check with a different script file, which should return available,
  // since it's not a service worker context.
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            feature
                .IsAvailableToContext(extension.get(),
                                      mojom::ContextType::kPrivilegedExtension,
                                      extension->GetResourceURL("other.js"),
                                      Feature::CHROMEOS_PLATFORM,
                                      kUnspecifiedContextId, TestContextData())
                .result());

  // A separately configured feature should be disallowed for service workers.
  SimpleFeature disallowed_feature{StaticFeatureData(kDisallowedData)};
  EXPECT_EQ(Feature::AvailabilityResult::kInvalidContext,
            disallowed_feature
                .IsAvailableToContext(
                    extension.get(), mojom::ContextType::kPrivilegedExtension,
                    extension->GetResourceURL(
                        ExtensionBuilder::kServiceWorkerScriptFile),
                    Feature::CHROMEOS_PLATFORM, kUnspecifiedContextId,
                    TestContextData())
                .result());
}

}  // namespace extensions
