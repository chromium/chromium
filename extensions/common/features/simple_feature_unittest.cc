// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/simple_feature.h"

#include <stddef.h>

#include <array>
#include <memory>
#include <optional>
#include <string>
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
  std::string desc;
  Feature::AvailabilityResult expected_availability;
  mojom::FeatureSessionType current_session_type;
  std::initializer_list<mojom::FeatureSessionType> feature_session_types;
};

Feature::AvailabilityResult IsAvailableInChannel(
    std::optional<Channel> channel_for_feature,
    Channel channel_for_testing) {
  ScopedCurrentChannel current_channel(channel_for_testing);

  SimpleFeature feature;
  if (channel_for_feature.has_value()) {
    feature.set_channel(channel_for_feature.value());
  }
  return feature
      .IsAvailableToManifest(
          HashedExtensionId(std::string(32, 'a')), Manifest::Type::kUnknown,
          ManifestLocation::kInvalidLocation, -1, Feature::GetCurrentPlatform(),
          kUnspecifiedContextId)
      .result();
}

}  // namespace

class SimpleFeatureTest : public testing::Test {
 public:
  SimpleFeatureTest(const SimpleFeatureTest&) = delete;
  SimpleFeatureTest& operator=(const SimpleFeatureTest&) = delete;

 protected:
  SimpleFeatureTest() : current_channel_(Channel::UNKNOWN) {}
  bool LocationIsAvailable(SimpleFeature::Location feature_location,
                           ManifestLocation manifest_location) {
    SimpleFeature feature;
    feature.set_location(feature_location);
    Feature::AvailabilityResult availability_result =
        feature
            .IsAvailableToManifest(HashedExtensionId(),
                                   Manifest::Type::kUnknown, manifest_location,
                                   -1, Feature::UNSPECIFIED_PLATFORM,
                                   kUnspecifiedContextId)
            .result();
    return availability_result == Feature::AvailabilityResult::kIsAvailable;
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

  SimpleFeature feature;
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
  const HashedExtensionId kIdFoo("fooabbbbccccddddeeeeffffgggghhhh");
  const HashedExtensionId kIdBar("barabbbbccccddddeeeeffffgggghhhh");
  const HashedExtensionId kIdBaz("bazabbbbccccddddeeeeffffgggghhhh");
  SimpleFeature feature;
  feature.set_allowlist({kIdFoo.value().c_str(), kIdBar.value().c_str()});

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

  feature.set_extension_types({Manifest::Type::kLegacyPackagedApp});
  EXPECT_EQ(Feature::AvailabilityResult::kNotFoundInAllowlist,
            feature
                .IsAvailableToManifest(
                    kIdBaz, Manifest::Type::kLegacyPackagedApp,
                    ManifestLocation::kInvalidLocation, -1,
                    Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                .result());
}

TEST_F(SimpleFeatureTest, HashedIdAllowlist) {
  // echo -n "fooabbbbccccddddeeeeffffgggghhhh" |
  //   sha1sum | tr '[:lower:]' '[:upper:]'
  const std::string kIdFoo("fooabbbbccccddddeeeeffffgggghhhh");
  const std::string kIdFooHashed("55BC7228A0D502A2A48C9BB16B07062A01E62897");
  SimpleFeature feature;

  feature.set_allowlist({kIdFooHashed.c_str()});

  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            feature
                .IsAvailableToManifest(
                    HashedExtensionId(kIdFoo), Manifest::Type::kUnknown,
                    ManifestLocation::kInvalidLocation, -1,
                    Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                .result());
  EXPECT_NE(Feature::AvailabilityResult::kIsAvailable,
            feature
                .IsAvailableToManifest(
                    HashedExtensionId(kIdFooHashed), Manifest::Type::kUnknown,
                    ManifestLocation::kInvalidLocation, -1,
                    Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                .result());
  EXPECT_EQ(
      Feature::AvailabilityResult::kNotFoundInAllowlist,
      feature
          .IsAvailableToManifest(
              HashedExtensionId("slightlytoooolongforanextensionid"),
              Manifest::Type::kUnknown, ManifestLocation::kInvalidLocation, -1,
              Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
          .result());
  EXPECT_EQ(
      Feature::AvailabilityResult::kNotFoundInAllowlist,
      feature
          .IsAvailableToManifest(
              HashedExtensionId("tooshortforanextensionid"),
              Manifest::Type::kUnknown, ManifestLocation::kInvalidLocation, -1,
              Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
          .result());
}

TEST_F(SimpleFeatureTest, Blocklist) {
  const HashedExtensionId kIdFoo("fooabbbbccccddddeeeeffffgggghhhh");
  const HashedExtensionId kIdBar("barabbbbccccddddeeeeffffgggghhhh");
  const HashedExtensionId kIdBaz("bazabbbbccccddddeeeeffffgggghhhh");
  SimpleFeature feature;
  feature.set_blocklist({kIdFoo.value().c_str(), kIdBar.value().c_str()});

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
  // echo -n "fooabbbbccccddddeeeeffffgggghhhh" |
  //   sha1sum | tr '[:lower:]' '[:upper:]'
  const std::string kIdFoo("fooabbbbccccddddeeeeffffgggghhhh");
  const std::string kIdFooHashed("55BC7228A0D502A2A48C9BB16B07062A01E62897");
  SimpleFeature feature;

  feature.set_blocklist({kIdFooHashed.c_str()});

  EXPECT_EQ(Feature::AvailabilityResult::kFoundInBlocklist,
            feature
                .IsAvailableToManifest(
                    HashedExtensionId(kIdFoo), Manifest::Type::kUnknown,
                    ManifestLocation::kInvalidLocation, -1,
                    Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                .result());
  EXPECT_NE(Feature::AvailabilityResult::kFoundInBlocklist,
            feature
                .IsAvailableToManifest(
                    HashedExtensionId(kIdFooHashed), Manifest::Type::kUnknown,
                    ManifestLocation::kInvalidLocation, -1,
                    Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                .result());
  EXPECT_EQ(
      Feature::AvailabilityResult::kIsAvailable,
      feature
          .IsAvailableToManifest(
              HashedExtensionId("slightlytoooolongforanextensionid"),
              Manifest::Type::kUnknown, ManifestLocation::kInvalidLocation, -1,
              Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
          .result());
  EXPECT_EQ(
      Feature::AvailabilityResult::kIsAvailable,
      feature
          .IsAvailableToManifest(
              HashedExtensionId("tooshortforanextensionid"),
              Manifest::Type::kUnknown, ManifestLocation::kInvalidLocation, -1,
              Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
          .result());
}

TEST_F(SimpleFeatureTest, PackageType) {
  SimpleFeature feature;
  feature.set_extension_types(
      {Manifest::Type::kExtension, Manifest::Type::kLegacyPackagedApp});

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

TEST_F(SimpleFeatureTest, Context) {
  SimpleFeature feature;
  feature.set_name("somefeature");
  feature.set_contexts({mojom::ContextType::kPrivilegedExtension});
  feature.set_extension_types({Manifest::Type::kLegacyPackagedApp});
  feature.set_platforms({Feature::CHROMEOS_PLATFORM});
  feature.set_min_manifest_version(21);
  feature.set_max_manifest_version(25);

  auto manifest = base::Value::Dict()
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

  feature.set_allowlist({"monkey"});
  EXPECT_EQ(Feature::AvailabilityResult::kNotFoundInAllowlist,
            feature
                .IsAvailableToContext(extension.get(),
                                      mojom::ContextType::kPrivilegedExtension,
                                      Feature::CHROMEOS_PLATFORM,
                                      kUnspecifiedContextId, TestContextData())
                .result());
  feature.set_allowlist({});

  feature.set_extension_types({Manifest::Type::kTheme});
  {
    Feature::Availability availability = feature.IsAvailableToContext(
        extension.get(), mojom::ContextType::kPrivilegedExtension,
        Feature::CHROMEOS_PLATFORM, kUnspecifiedContextId, TestContextData());
    EXPECT_EQ(Feature::AvailabilityResult::kInvalidType, availability.result());
    EXPECT_EQ("'somefeature' is only allowed for themes, "
              "but this is a legacy packaged app.",
              availability.message());
  }

  feature.set_extension_types({Manifest::Type::kLegacyPackagedApp});
  feature.set_contexts({mojom::ContextType::kUnprivilegedExtension,
                        mojom::ContextType::kContentScript});
  {
    Feature::Availability availability = feature.IsAvailableToContext(
        extension.get(), mojom::ContextType::kPrivilegedExtension,
        Feature::CHROMEOS_PLATFORM, kUnspecifiedContextId, TestContextData());
    EXPECT_EQ(Feature::AvailabilityResult::kInvalidContext,
              availability.result());
    EXPECT_EQ("'somefeature' is only allowed to run in extension iframes and "
              "content scripts, but this is a privileged page",
              availability.message());
  }

  feature.set_contexts({mojom::ContextType::kUnprivilegedExtension,
                        mojom::ContextType::kContentScript,
                        mojom::ContextType::kWebPage});
  {
    Feature::Availability availability = feature.IsAvailableToContext(
        extension.get(), mojom::ContextType::kPrivilegedExtension,
        Feature::CHROMEOS_PLATFORM, kUnspecifiedContextId, TestContextData());
    EXPECT_EQ(Feature::AvailabilityResult::kInvalidContext,
              availability.result());
    EXPECT_EQ("'somefeature' is only allowed to run in extension iframes, "
              "content scripts, and web pages, but this is a privileged page",
              availability.message());
  }

  {
    SimpleFeature other_feature;
    other_feature.set_location(SimpleFeature::COMPONENT_LOCATION);
    EXPECT_EQ(Feature::AvailabilityResult::kInvalidLocation,
              other_feature
                  .IsAvailableToContext(
                      extension.get(), mojom::ContextType::kPrivilegedExtension,
                      Feature::CHROMEOS_PLATFORM, kUnspecifiedContextId,
                      TestContextData())
                  .result());
  }

  feature.set_contexts({mojom::ContextType::kPrivilegedExtension});
  EXPECT_EQ(Feature::AvailabilityResult::kInvalidPlatform,
            feature
                .IsAvailableToContext(extension.get(),
                                      mojom::ContextType::kPrivilegedExtension,
                                      Feature::UNSPECIFIED_PLATFORM,
                                      kUnspecifiedContextId, TestContextData())
                .result());

  feature.set_min_manifest_version(22);
  EXPECT_EQ(Feature::AvailabilityResult::kInvalidMinManifestVersion,
            feature
                .IsAvailableToContext(extension.get(),
                                      mojom::ContextType::kPrivilegedExtension,
                                      Feature::CHROMEOS_PLATFORM,
                                      kUnspecifiedContextId, TestContextData())
                .result());
  feature.set_min_manifest_version(21);

  feature.set_max_manifest_version(18);
  EXPECT_EQ(Feature::AvailabilityResult::kInvalidMaxManifestVersion,
            feature
                .IsAvailableToContext(extension.get(),
                                      mojom::ContextType::kPrivilegedExtension,
                                      Feature::CHROMEOS_PLATFORM,
                                      kUnspecifiedContextId, TestContextData())
                .result());
  feature.set_max_manifest_version(25);
}

TEST_F(SimpleFeatureTest, SessionType) {
  auto manifest = base::Value::Dict()
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
       {mojom::FeatureSessionType::kKiosk}},
      {"kiosk feature in regular session",
       Feature::AvailabilityResult::kInvalidSessionType,
       mojom::FeatureSessionType::kRegular,
       {mojom::FeatureSessionType::kKiosk}},
      {"kiosk feature in unknown session",
       Feature::AvailabilityResult::kInvalidSessionType,
       mojom::FeatureSessionType::kUnknown,
       {mojom::FeatureSessionType::kKiosk}},
      {"kiosk feature in initial session",
       Feature::AvailabilityResult::kInvalidSessionType,
       mojom::FeatureSessionType::kInitial,
       {mojom::FeatureSessionType::kKiosk}},
      {"non kiosk feature in kiosk session",
       Feature::AvailabilityResult::kInvalidSessionType,
       mojom::FeatureSessionType::kKiosk,
       {mojom::FeatureSessionType::kRegular}},
      {"non kiosk feature in regular session",
       Feature::AvailabilityResult::kIsAvailable,
       mojom::FeatureSessionType::kRegular,
       {mojom::FeatureSessionType::kRegular}},
      {"non kiosk feature in unknown session",
       Feature::AvailabilityResult::kInvalidSessionType,
       mojom::FeatureSessionType::kUnknown,
       {mojom::FeatureSessionType::kRegular}},
      {"non kiosk feature in initial session",
       Feature::AvailabilityResult::kInvalidSessionType,
       mojom::FeatureSessionType::kInitial,
       {mojom::FeatureSessionType::kRegular}},
      {"session agnostic feature in kiosk session",
       Feature::AvailabilityResult::kIsAvailable,
       mojom::FeatureSessionType::kKiosk,
       {}},
      {"session agnostic feature in auto-launched kiosk session",
       Feature::AvailabilityResult::kIsAvailable,
       mojom::FeatureSessionType::kAutolaunchedKiosk,
       {}},
      {"session agnostic feature in regular session",
       Feature::AvailabilityResult::kIsAvailable,
       mojom::FeatureSessionType::kRegular,
       {}},
      {"session agnostic feature in unknown session",
       Feature::AvailabilityResult::kIsAvailable,
       mojom::FeatureSessionType::kUnknown,
       {}},
      {"feature with multiple session types",
       Feature::AvailabilityResult::kIsAvailable,
       mojom::FeatureSessionType::kRegular,
       {mojom::FeatureSessionType::kRegular,
        mojom::FeatureSessionType::kKiosk}},
      {"feature with multiple session types in unknown session",
       Feature::AvailabilityResult::kInvalidSessionType,
       mojom::FeatureSessionType::kUnknown,
       {mojom::FeatureSessionType::kRegular,
        mojom::FeatureSessionType::kKiosk}},
      {"feature with multiple session types in initial session",
       Feature::AvailabilityResult::kInvalidSessionType,
       mojom::FeatureSessionType::kInitial,
       {mojom::FeatureSessionType::kRegular,
        mojom::FeatureSessionType::kKiosk}},
      {"feature with auto-launched kiosk session type in regular session",
       Feature::AvailabilityResult::kInvalidSessionType,
       mojom::FeatureSessionType::kAutolaunchedKiosk,
       {mojom::FeatureSessionType::kRegular}},
      {"feature with auto-launched kiosk session type in auto-launched kiosk",
       Feature::AvailabilityResult::kIsAvailable,
       mojom::FeatureSessionType::kAutolaunchedKiosk,
       {mojom::FeatureSessionType::kAutolaunchedKiosk}},
      {"feature with kiosk session type in auto-launched kiosk session",
       Feature::AvailabilityResult::kIsAvailable,
       mojom::FeatureSessionType::kAutolaunchedKiosk,
       {mojom::FeatureSessionType::kKiosk}},
  });

  for (const auto& entry : kTestData) {
    std::unique_ptr<base::AutoReset<mojom::FeatureSessionType>> current_session(
        ScopedCurrentFeatureSessionType(entry.current_session_type));

    SimpleFeature feature;
    feature.set_session_types(entry.feature_session_types);

    EXPECT_EQ(entry.expected_availability,
              feature
                  .IsAvailableToContext(
                      extension.get(), mojom::ContextType::kPrivilegedExtension,
                      Feature::CHROMEOS_PLATFORM, kUnspecifiedContextId,
                      TestContextData())
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
  }
}

TEST_F(SimpleFeatureTest, Location) {
  // Component extensions can access any location.
  EXPECT_TRUE(LocationIsAvailable(SimpleFeature::COMPONENT_LOCATION,
                                  ManifestLocation::kComponent));
  EXPECT_TRUE(LocationIsAvailable(SimpleFeature::EXTERNAL_COMPONENT_LOCATION,
                                  ManifestLocation::kComponent));
  EXPECT_TRUE(LocationIsAvailable(SimpleFeature::POLICY_LOCATION,
                                  ManifestLocation::kComponent));
  EXPECT_TRUE(LocationIsAvailable(SimpleFeature::UNPACKED_LOCATION,
                                  ManifestLocation::kComponent));

  // Only component extensions can access the "component" location.
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::COMPONENT_LOCATION,
                                   ManifestLocation::kInvalidLocation));
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::COMPONENT_LOCATION,
                                   ManifestLocation::kUnpacked));
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::COMPONENT_LOCATION,
                                   ManifestLocation::kExternalComponent));
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::COMPONENT_LOCATION,
                                   ManifestLocation::kExternalPrefDownload));
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::COMPONENT_LOCATION,
                                   ManifestLocation::kExternalPolicy));
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::COMPONENT_LOCATION,
                                   ManifestLocation::kExternalPolicyDownload));

  // Policy extensions can access the "policy" location.
  EXPECT_TRUE(LocationIsAvailable(SimpleFeature::POLICY_LOCATION,
                                  ManifestLocation::kExternalPolicy));
  EXPECT_TRUE(LocationIsAvailable(SimpleFeature::POLICY_LOCATION,
                                  ManifestLocation::kExternalPolicyDownload));

  // Non-policy (except component) extensions cannot access policy.
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::POLICY_LOCATION,
                                   ManifestLocation::kExternalComponent));
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::POLICY_LOCATION,
                                   ManifestLocation::kInvalidLocation));
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::POLICY_LOCATION,
                                   ManifestLocation::kUnpacked));
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::POLICY_LOCATION,
                                   ManifestLocation::kExternalPrefDownload));

  // External component extensions can access the "external_component"
  // location.
  EXPECT_TRUE(LocationIsAvailable(SimpleFeature::EXTERNAL_COMPONENT_LOCATION,
                                  ManifestLocation::kExternalComponent));

  // Only unpacked and command line extensions can access the "unpacked"
  // location.
  EXPECT_TRUE(LocationIsAvailable(SimpleFeature::UNPACKED_LOCATION,
                                  ManifestLocation::kUnpacked));
  EXPECT_TRUE(LocationIsAvailable(SimpleFeature::UNPACKED_LOCATION,
                                  ManifestLocation::kCommandLine));
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::UNPACKED_LOCATION,
                                   ManifestLocation::kInternal));
}

TEST_F(SimpleFeatureTest, Platform) {
  SimpleFeature feature;
  feature.set_platforms({Feature::CHROMEOS_PLATFORM});
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
  SimpleFeature feature;
  feature.set_min_manifest_version(5);

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

  feature.set_max_manifest_version(8);

  EXPECT_EQ(Feature::AvailabilityResult::kInvalidMaxManifestVersion,
            feature
                .IsAvailableToManifest(
                    HashedExtensionId(), Manifest::Type::kUnknown,
                    ManifestLocation::kInvalidLocation, 10,
                    Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                .result());
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            feature
                .IsAvailableToManifest(
                    HashedExtensionId(), Manifest::Type::kUnknown,
                    ManifestLocation::kInvalidLocation, 8,
                    Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                .result());
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            feature
                .IsAvailableToManifest(
                    HashedExtensionId(), Manifest::Type::kUnknown,
                    ManifestLocation::kInvalidLocation, 7,
                    Feature::UNSPECIFIED_PLATFORM, kUnspecifiedContextId)
                .result());
}

TEST_F(SimpleFeatureTest, CommandLineSwitch) {
  SimpleFeature feature;
  feature.set_command_line_switch("laser-beams");
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
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitch("laser-beams=1");
    EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
              feature.IsAvailableToEnvironment(kUnspecifiedContextId).result());
  }
  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitch("laser-beams=0");
    EXPECT_EQ(Feature::AvailabilityResult::kMissingCommandLineSwitch,
              feature.IsAvailableToEnvironment(kUnspecifiedContextId).result());
  }
}

TEST_F(SimpleFeatureTest, FeatureFlags) {
  static BASE_FEATURE(kStubFeature1, "StubFeature1",
                      base::FEATURE_ENABLED_BY_DEFAULT);
  static BASE_FEATURE(kStubFeature2, "StubFeature2",
                      base::FEATURE_DISABLED_BY_DEFAULT);
  const base::Feature* kOverriddenFeatures[] = {&kStubFeature1, &kStubFeature2};
  auto scoped_feature_override =
      CreateScopedFeatureFlagsOverrideForTesting(kOverriddenFeatures);

  SimpleFeature simple_feature_1;
  simple_feature_1.set_feature_flag(kStubFeature1.name);
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            simple_feature_1.IsAvailableToEnvironment(kUnspecifiedContextId)
                .result());

  SimpleFeature simple_feature_2;
  simple_feature_2.set_feature_flag(kStubFeature2.name);
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
  std::unique_ptr<ComplexFeature> complex_feature;
  {
    auto feature1 = std::make_unique<SimpleFeature>();
    feature1->set_channel(Channel::BETA);
    feature1->set_extension_types({Manifest::Type::kExtension});
    auto feature2 = std::make_unique<SimpleFeature>();
    feature2->set_channel(Channel::BETA);
    feature2->set_extension_types({Manifest::Type::kLegacyPackagedApp});
    std::vector<Feature*> list;
    list.push_back(feature1.release());
    list.push_back(feature2.release());
    complex_feature = std::make_unique<ComplexFeature>(&list);
  }

  Feature* feature = static_cast<Feature*>(complex_feature.get());
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
  std::unique_ptr<ComplexFeature> complex_feature;
  {
    // Rule: "extension", channel trunk.
    auto feature1 = std::make_unique<SimpleFeature>();
    feature1->set_channel(Channel::UNKNOWN);
    feature1->set_extension_types({Manifest::Type::kExtension});
    auto feature2 = std::make_unique<SimpleFeature>();
    // Rule: "legacy_packaged_app", channel stable.
    feature2->set_channel(Channel::STABLE);
    feature2->set_extension_types({Manifest::Type::kLegacyPackagedApp});
    std::vector<Feature*> list;
    list.push_back(feature1.release());
    list.push_back(feature2.release());
    complex_feature = std::make_unique<ComplexFeature>(&list);
  }

  const HashedExtensionId kId1(std::string(32, 'a'));
  const HashedExtensionId kId2(std::string(32, 'b'));
  Feature* feature = static_cast<Feature*>(complex_feature.get());
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
  std::string expected_feature_name = "DisallowedFeature";
  uint32_t delegated_availability_check_call_count = 0;
  auto delegated_availability_check = base::BindLambdaForTesting(
      [&](const std::string& api_full_name, const Extension* extension,
          mojom::ContextType context, const GURL& url,
          Feature::Platform platform, int context_id, bool check_developer_mode,
          const ContextData& context_data) {
        ++delegated_availability_check_call_count;
        EXPECT_EQ(expected_feature_name, api_full_name);
        return api_full_name == "AllowedFeature";
      });

  SimpleFeature feature;
  feature.set_requires_delegated_availability_check(true);
  feature.set_contexts({mojom::ContextType::kWebPage});

  const GURL kTestPage = GURL("https://www.example.com");
  feature.set_matches({kTestPage.spec().c_str()});
  {
    // Test a feature that requires a delegated availability check but is
    // missing the check handler.
    EXPECT_EQ(Feature::AvailabilityResult::kMissingDelegatedAvailabilityCheck,
              feature
                  .IsAvailableToContext(
                      /*extension=*/nullptr, mojom::ContextType::kWebPage,
                      kTestPage, kUnspecifiedContextId, TestContextData())
                  .result());
  }

  feature.SetDelegatedAvailabilityCheckHandler(delegated_availability_check);
  feature.set_name(expected_feature_name);
  {
    // Test a feature that requires a delegated availability check and the check
    // is not successful.
    EXPECT_EQ(Feature::AvailabilityResult::kFailedDelegatedAvailabilityCheck,
              feature
                  .IsAvailableToContext(
                      /*extension=*/nullptr, mojom::ContextType::kWebPage,
                      kTestPage, kUnspecifiedContextId, TestContextData())
                  .result());
    EXPECT_EQ(1u, delegated_availability_check_call_count);
  }

  expected_feature_name = "AllowedFeature";
  feature.set_name(expected_feature_name);
  {
    // Test a feature that requires a delegated availability check and the check
    // is successful.
    EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
              feature
                  .IsAvailableToContext(
                      /*extension=*/nullptr, mojom::ContextType::kWebPage,
                      kTestPage, kUnspecifiedContextId, TestContextData())
                  .result());
    EXPECT_EQ(2u, delegated_availability_check_call_count);
  }

  feature.set_channel(version_info::Channel::DEV);
  {
    // Test a feature that requires a delegated availability check and the check
    // would be successful, but actually isn't called since the environment
    // check fails.
    ScopedCurrentChannel current_channel(Channel::STABLE);
    EXPECT_EQ(Feature::AvailabilityResult::kUnsupportedChannel,
              feature
                  .IsAvailableToContext(
                      /*extension=*/nullptr, mojom::ContextType::kWebPage,
                      kTestPage, kUnspecifiedContextId, TestContextData())
                  .result());
    EXPECT_EQ(2u, delegated_availability_check_call_count);
  }
  feature.set_channel(version_info::Channel::STABLE);
  {
    // Test a feature that requires a delegated availability check and the check
    // would be successful, then confirm the check is called because the
    // environment check passes.
    ScopedCurrentChannel current_channel(Channel::STABLE);
    EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
              feature
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
              feature
                  .IsAvailableToContext(
                      /*extension=*/nullptr, mojom::ContextType::kWebPage,
                      kTestPageNotInMatchList, kUnspecifiedContextId,
                      TestContextData())
                  .result());
    EXPECT_EQ(4u, delegated_availability_check_call_count);
  }
}

TEST(SimpleFeatureUnitTest, TestChannelsWithoutExtension) {
  // Create a webui feature available on trunk.
  SimpleFeature feature;
  feature.set_contexts({mojom::ContextType::kWebUi});
  feature.set_matches({content::GetWebUIURLString("settings/*").c_str()});
  feature.set_channel(version_info::Channel::UNKNOWN);

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

TEST(SimpleFeatureUnitTest, TestAvailableToEnvironment) {
  {
    // Test with no environment restrictions, but with other restrictions. The
    // result should always be available.
    SimpleFeature feature;
    feature.set_min_manifest_version(2);
    feature.set_extension_types({Manifest::Type::kExtension});
    feature.set_contexts({mojom::ContextType::kPrivilegedExtension});
    EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
              feature.IsAvailableToEnvironment(kUnspecifiedContextId).result());
  }

  {
    // Test with channel restrictions.
    SimpleFeature feature;
    feature.set_channel(Channel::BETA);
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
    const char kFakeSwitch[] = "some-fake-switch";
    SimpleFeature feature;
    feature.set_command_line_switch(kFakeSwitch);

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

TEST(SimpleFeatureUnitTest, TestExperimentalExtensionApisSwitch) {
  ScopedCurrentChannel current_channel(Channel::STABLE);

  auto test_feature = []() {
    SimpleFeature feature;
    feature.set_channel(version_info::Channel::UNKNOWN);
    return feature.IsAvailableToEnvironment(kUnspecifiedContextId).result();
  };

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
}

TEST_F(SimpleFeatureTest, RestrictDeveloperModeAPIs) {
  constexpr int kContextId1 = 1;
  constexpr int kContextId2 = 2;
  SimpleFeature dev_mode_only_feature;
  dev_mode_only_feature.set_developer_mode_only(true);
  SimpleFeature other_feature;

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
  SimpleFeature feature;
  feature.set_name("somefeature");
  feature.set_contexts({mojom::ContextType::kPrivilegedExtension});
  feature.set_extension_types({Manifest::Type::kExtension});

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

  // Disable the feature for service workers. The feature should be disallowed.
  feature.set_disallow_for_service_workers(true);
  EXPECT_EQ(Feature::AvailabilityResult::kInvalidContext,
            feature
                .IsAvailableToContext(
                    extension.get(), mojom::ContextType::kPrivilegedExtension,
                    extension->GetResourceURL(
                        ExtensionBuilder::kServiceWorkerScriptFile),
                    Feature::CHROMEOS_PLATFORM, kUnspecifiedContextId,
                    TestContextData())
                .result());
}

}  // namespace extensions
