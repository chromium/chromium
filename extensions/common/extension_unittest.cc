// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension.h"

#include <optional>
#include <string_view>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::mojom::ManifestLocation;

namespace extensions {
namespace {

std::string GetVersionTooHighWarning(int max_version, int supplied_version) {
  return ErrorUtils::FormatErrorMessage(
      manifest_errors::kManifestVersionTooHighWarning,
      base::NumberToString(max_version),
      base::NumberToString(supplied_version));
}

testing::AssertionResult RunManifestVersionSuccess(
    base::DictValue manifest,
    Manifest::Type expected_type,
    int expected_manifest_version,
    std::string_view expected_warning = "",
    Extension::InitFromValueFlags custom_flag = Extension::NO_FLAGS,
    ManifestLocation manifest_location = ManifestLocation::kInternal) {
  std::u16string error;
  scoped_refptr<const Extension> extension = Extension::Create(
      base::FilePath(), manifest_location, manifest, custom_flag, &error);
  if (!extension) {
    return testing::AssertionFailure()
           << "Extension creation failed: " << error;
  }

  if (extension->GetType() != expected_type) {
    return testing::AssertionFailure()
           << "Wrong type: " << extension->GetType();
  }

  if (extension->manifest_version() != expected_manifest_version) {
    return testing::AssertionFailure()
           << "Wrong manifest version: " << extension->manifest_version();
  }

  std::string manifest_version_warning;
  for (const auto& warning : extension->install_warnings()) {
    if (warning.key == manifest_keys::kManifestVersion) {
      manifest_version_warning = warning.message;
      break;
    }
  }

  if (expected_warning != manifest_version_warning) {
    return testing::AssertionFailure()
           << "Expected warning: '" << expected_warning << "', Found Warning: '"
           << manifest_version_warning << "'";
  }

  return testing::AssertionSuccess();
}

testing::AssertionResult RunManifestVersionFailure(
    base::DictValue manifest,
    Extension::InitFromValueFlags custom_flag = Extension::NO_FLAGS) {
  std::u16string error;
  scoped_refptr<const Extension> extension =
      Extension::Create(base::FilePath(), ManifestLocation::kInternal, manifest,
                        custom_flag, &error);
  if (extension)
    return testing::AssertionFailure() << "Extension creation succeeded.";

  return testing::AssertionSuccess();
}

testing::AssertionResult RunCreationWithFlags(
    const base::DictValue& manifest,
    mojom::ManifestLocation location,
    Manifest::Type expected_type,
    Extension::InitFromValueFlags custom_flag = Extension::NO_FLAGS) {
  std::u16string error;
  scoped_refptr<const Extension> extension = Extension::Create(
      base::FilePath(), location, manifest, custom_flag, &error);
  if (!extension) {
    return testing::AssertionFailure()
           << "Extension creation failed: " << error;
  }

  if (extension->GetType() != expected_type) {
    return testing::AssertionFailure()
           << "Wrong type: " << extension->GetType();
  }
  return testing::AssertionSuccess();
}

// Tests successful parsing of extension manifest "version" attribute and
// validate that the version is serialized into a correct canonical string. For
// example, for manifest "version" value "1.01" the canonical serialization
// would be "1.1". If version string is parsed and re-serialized to the same
// string, then expected_version can be omitted.
testing::AssertionResult RunVersionSuccess(
    std::string_view input_version,
    std::optional<std::string_view> expected_version = std::nullopt) {
  base::DictValue manifest = base::DictValue()
                                 .Set(manifest_keys::kName, "My Extension")
                                 .Set(manifest_keys::kManifestVersion, 3)
                                 .Set(manifest_keys::kVersion, input_version);

  std::u16string error;
  scoped_refptr<const Extension> extension =
      Extension::Create(base::FilePath(), ManifestLocation::kInternal, manifest,
                        Extension::NO_FLAGS, &error);
  if (!extension) {
    return testing::AssertionFailure()
           << "Extension creation failed: " << error;
  }

  if (extension->VersionString().empty()) {
    return testing::AssertionFailure() << "Extension version can not be empty";
  }

  if (!expected_version || input_version == *expected_version) {
    // Parsed version should be the same as input string.
    if (extension->VersionString() != input_version) {
      return testing::AssertionFailure()
             << "Extension version should match the manifest value: expected "
             << input_version << " but got " << extension->VersionString();
    }
    if (!extension->install_warnings().empty()) {
      return testing::AssertionFailure() << "Received an unexpected warning";
    }
  } else {
    // Version string should be successflly parsed but reformatted,
    // with an appropriate warning being logged.
    auto expected_warning =
        base::StringPrintf(manifest_errors::kVersionFormatting,
                           extension->VersionString().c_str());
    if (extension->VersionString() != *expected_version) {
      return testing::AssertionFailure() << "Extension version does not match "
                                            "the expected value: expected '"
                                         << *expected_version << "' but got '"
                                         << extension->VersionString() << "'";
    }
    if (extension->install_warnings().empty()) {
      return testing::AssertionFailure()
             << "Extension should have received a warning about re-formatted "
                "version string";
    } else if (extension->install_warnings().size() > 1) {
      return testing::AssertionFailure()
             << "Extension should have received exactly one warning about "
                "re-formatted version string";
    } else if (extension->install_warnings()[0].key !=
                   manifest_keys::kVersion ||
               extension->install_warnings()[0].message != expected_warning) {
      testing::AssertionFailure()
          << "Expected Warning: Key: '" << manifest_keys::kVersion
          << "' Message: '" << expected_warning << "', Found Warning: Key: '"
          << extension->install_warnings()[0].key << "' Message: '"
          << extension->install_warnings()[0].message << "'";
    }
    // warnings
  }
  return testing::AssertionSuccess();
}

// Validate failure to install extensions with invalid `version` manifest
// attribute. If input is `std::nullopt`, then test validates installation failure
// for extension with `version` omitted entirely.
testing::AssertionResult RunVersionFailure(
    std::optional<std::string_view> version) {
  base::DictValue manifest = base::DictValue()
                                 .Set(manifest_keys::kName, "My Extension")
                                 .Set(manifest_keys::kManifestVersion, 3);
  if (version) {
    manifest.Set(manifest_keys::kVersion, *version);
  }

  std::u16string error;
  scoped_refptr<const Extension> extension =
      Extension::Create(base::FilePath(), ManifestLocation::kInternal, manifest,
                        Extension::NO_FLAGS, &error);

  if (extension) {
    return testing::AssertionFailure() << "Extension creation succeeded.";
  }
  if (error != manifest_errors::kInvalidVersion) {
    return testing::AssertionFailure()
           << "Expected Error: '" << manifest_errors::kInvalidVersion
           << "', Found Error: '" << error << "'";
  }
  return testing::AssertionSuccess();
}

}  // namespace

// TODO(devlin): Move tests from chrome/common/extensions/extension_unittest.cc
// that don't depend on //chrome into here.

TEST(ExtensionTest, ExtensionManifestVersions) {
  auto get_manifest = [](std::optional<int> manifest_version) {
    auto manifest = base::DictValue()
                        .Set("name", "My Extension")
                        .Set("version", "0.1")
                        .Set("description", "An awesome extension");
    if (manifest_version)
      manifest.Set("manifest_version", *manifest_version);
    return manifest;
  };

  const Manifest::Type kType = Manifest::Type::kExtension;
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(2), kType, 2));
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(3), kType, 3));
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(4), kType, 4,
                                        GetVersionTooHighWarning(3, 4)));

  // Loading an unpacked MV2 extension should emit a warning.
  EXPECT_TRUE(RunManifestVersionSuccess(
      get_manifest(2), kType, 2,
      manifest_errors::kManifestV2IsDeprecatedWarning, Extension::NO_FLAGS,
      ManifestLocation::kUnpacked));

  // Manifest v1 is deprecated, and should not load.
  EXPECT_TRUE(RunManifestVersionFailure(get_manifest(1)));
  // Omitting the key defaults to v1 for extensions.
  EXPECT_TRUE(RunManifestVersionFailure(get_manifest(std::nullopt)));

  // '0' and '-1' are invalid values.
  EXPECT_TRUE(RunManifestVersionFailure(get_manifest(0)));
  EXPECT_TRUE(RunManifestVersionFailure(get_manifest(-1)));

  {
    // Manifest v1 should only load if a command line switch is used.
    base::test::ScopedCommandLine command_line;
    command_line.GetProcessCommandLine()->AppendSwitch(
        switches::kAllowLegacyExtensionManifests);
    EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(1), kType, 1));
    EXPECT_TRUE(
        RunManifestVersionSuccess(get_manifest(std::nullopt), kType, 1));
  }
}

TEST(ExtensionTest, PlatformAppManifestVersions) {
  auto get_manifest = [](std::optional<int> manifest_version) {
    base::DictValue background;
    background.Set("scripts", base::ListValue().Append("background.js"));
    auto manifest = base::DictValue()
                        .Set("name", "My Platform App")
                        .Set("version", "0.1")
                        .Set("description", "A platform app")
                        .Set("app", base::DictValue().Set(
                                        "background", std::move(background)));
    if (manifest_version)
      manifest.Set("manifest_version", *manifest_version);
    return manifest;
  };

  const Manifest::Type kType = Manifest::Type::kPlatformApp;
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(2), kType, 2));
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(3), kType, 3));
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(4), kType, 4,
                                        GetVersionTooHighWarning(3, 4)));

  // Omitting the key defaults to v2 for platform apps.
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(std::nullopt), kType, 2));

  // Manifest v1 is deprecated, and should not load.
  EXPECT_TRUE(RunManifestVersionFailure(get_manifest(1)));

  // '0' and '-1' are invalid values.
  EXPECT_TRUE(RunManifestVersionFailure(get_manifest(0)));
  EXPECT_TRUE(RunManifestVersionFailure(get_manifest(-1)));

  {
    // Manifest v1 should not load for platform apps, even with the command line
    // switch.
    base::test::ScopedCommandLine command_line;
    command_line.GetProcessCommandLine()->AppendSwitch(
        switches::kAllowLegacyExtensionManifests);
    EXPECT_TRUE(RunManifestVersionFailure(get_manifest(1)));
  }
}

TEST(ExtensionTest, HostedAppManifestVersions) {
  auto get_manifest = [](std::optional<int> manifest_version) {
    base::DictValue app;
    app.Set("urls", base::ListValue().Append("http://example.com"));
    auto manifest = base::DictValue()
                        .Set("name", "My Hosted App")
                        .Set("version", "0.1")
                        .Set("description", "A hosted app")
                        .Set("app", std::move(app));
    if (manifest_version)
      manifest.Set("manifest_version", *manifest_version);
    return manifest;
  };

  const Manifest::Type kType = Manifest::Type::kHostedApp;
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(2), kType, 2));
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(3), kType, 3));
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(4), kType, 4,
                                        GetVersionTooHighWarning(3, 4)));

  // Manifest v1 is deprecated, but should still load for hosted apps.
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(1), kType, 1));
  // Omitting the key defaults to v1 for hosted apps, and v1 is still allowed.
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(std::nullopt), kType, 1));

  // Requiring the modern manifest version should make hosted apps require v2.
  EXPECT_TRUE(RunManifestVersionFailure(
      get_manifest(1), Extension::REQUIRE_MODERN_MANIFEST_VERSION));
}

TEST(ExtensionTest, UserScriptManifestVersions) {
  auto get_manifest = [](std::optional<int> manifest_version) {
    auto manifest = base::DictValue()
                        .Set("name", "My Extension")
                        .Set("version", "0.1")
                        .Set("description", "An awesome extension")
                        .Set("converted_from_user_script", true);
    if (manifest_version)
      manifest.Set("manifest_version", *manifest_version);
    return manifest;
  };

  const Manifest::Type kType = Manifest::Type::kUserScript;
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(2), kType, 2));
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(3), kType, 3));
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(4), kType, 4,
                                        GetVersionTooHighWarning(3, 4)));

  // Manifest v1 is deprecated, but should still load for user scripts.
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(1), kType, 1));
  // Omitting the key defaults to v1 for user scripts, but v1 is still allowed.
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(std::nullopt), kType, 1));

  // Requiring the modern manifest version should make user scripts require v2.
  EXPECT_TRUE(RunManifestVersionFailure(
      get_manifest(1), Extension::REQUIRE_MODERN_MANIFEST_VERSION));
}

TEST(ExtensionTest, LoginScreenFlag) {
  auto manifest = base::DictValue()
                      .Set("name", "My Extension")
                      .Set("version", "0.1")
                      .Set("description", "An awesome extension")
                      .Set("manifest_version", 2);

  EXPECT_TRUE(RunCreationWithFlags(manifest, ManifestLocation::kExternalPolicy,
                                   Manifest::Type::kExtension,
                                   Extension::NO_FLAGS));
  EXPECT_TRUE(RunCreationWithFlags(manifest, ManifestLocation::kExternalPolicy,
                                   Manifest::Type::kLoginScreenExtension,
                                   Extension::FOR_LOGIN_SCREEN));
}

TEST(ExtensionTest, ExtensionVersionFormat) {
  // Extension "version" is accepted without transformations.
  EXPECT_TRUE(RunVersionSuccess("1"));
  EXPECT_TRUE(RunVersionSuccess("1.0"));
  EXPECT_TRUE(RunVersionSuccess("0.1"));
  EXPECT_TRUE(RunVersionSuccess("0.0.0"));

  // Extension "version" is parseable, but has a non-canonical form,
  // e.g., leading zeros.
  EXPECT_TRUE(RunVersionSuccess("0.0.01", "0.0.1"));
  EXPECT_TRUE(RunVersionSuccess("1.002.3", "1.2.3"));

  // Extension "version" can not be parsed and causes installation failure.
  EXPECT_TRUE(RunVersionFailure(std::nullopt));
  EXPECT_TRUE(RunVersionFailure("01"));
  EXPECT_TRUE(RunVersionFailure("00.1"));
  EXPECT_TRUE(RunVersionFailure("-1.0"));
  EXPECT_TRUE(RunVersionFailure("1.-1"));
  EXPECT_TRUE(RunVersionFailure("-0.0"));
}

}  // namespace extensions
