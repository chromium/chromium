// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace errors = manifest_errors;
namespace dnr_api = api::declarative_net_request;

namespace declarative_net_request {
namespace {

std::string GetRuleResourcesPath() {
  return base::JoinString({dnr_api::ManifestKeys::kDeclarativeNetRequest,
                           dnr_api::DNRInfo::kRuleResources},
                          ".");
}

// Fixture testing the kDeclarativeNetRequestKey manifest key.
class DNRManifestTest : public testing::Test {
 public:
  DNRManifestTest() = default;

  DNRManifestTest(const DNRManifestTest&) = delete;
  DNRManifestTest& operator=(const DNRManifestTest&) = delete;

 protected:
  // Loads the extension and verifies the |expected_error|.
  void LoadAndExpectError(const std::string& expected_error) {
    std::string error;
    scoped_refptr<Extension> extension = file_util::LoadExtension(
        temp_dir_.GetPath(), mojom::ManifestLocation::kUnpacked,
        Extension::NO_FLAGS, &error);
    EXPECT_FALSE(extension);
    EXPECT_EQ(expected_error, error);
  }

  // Loads the extension and verifies that the manifest info is correctly set
  // up without any warnings or errors.
  void LoadAndExpectSuccess(const std::vector<TestRulesetInfo>& info) {
    std::vector<InstallWarning> warnings;
    warnings.emplace_back("Unrecognized manifest key 'browser_action'.");
    warnings.emplace_back(errors::kManifestV2IsDeprecatedWarning);
    LoadAndExpectWarning(info, warnings);
  }

  // Loads the extension and verifies that the manifest info is correctly set
  // up, has no errors, but has provided warning.
  void LoadAndExpectWarning(
      const std::vector<TestRulesetInfo>& info,
      const std::vector<InstallWarning>& expected_warnings) {
    std::string error;
    scoped_refptr<Extension> extension = file_util::LoadExtension(
        temp_dir_.GetPath(), mojom::ManifestLocation::kUnpacked,
        Extension::NO_FLAGS, &error);
    ASSERT_TRUE(extension) << error;
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(expected_warnings, extension.get()->install_warnings());

    const std::vector<DNRManifestData::RulesetInfo>& rulesets =
        DNRManifestData::GetRulesets(*extension);
    ASSERT_EQ(info.size(), rulesets.size());
    for (size_t i = 0; i < rulesets.size(); ++i) {
      EXPECT_EQ(RulesetID(i + kMinValidStaticRulesetID.value()),
                rulesets[i].id);

      EXPECT_EQ(info[i].manifest_id, rulesets[i].manifest_id);

      // Compare normalized FilePaths instead of their string representations
      // since different platforms represent path separators differently.
      EXPECT_EQ(base::FilePath()
                    .AppendASCII(info[i].relative_file_path)
                    .NormalizePathSeparators(),
                rulesets[i].relative_path);

      EXPECT_EQ(info[i].enabled, rulesets[i].enabled);

      EXPECT_EQ(&rulesets[i],
                &DNRManifestData::GetRuleset(*extension, rulesets[i].id));
    }
  }

  void WriteManifestAndRuleset(const base::Value::Dict& manifest,
                               const std::vector<TestRulesetInfo>& info) {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    for (const auto& ruleset : info) {
      base::FilePath rules_path =
          temp_dir_.GetPath().AppendASCII(ruleset.relative_file_path);

      // Create parent directory of |rules_path| if it doesn't exist.
      EXPECT_TRUE(base::CreateDirectory(rules_path.DirName()));

      // Persist an empty ruleset file.
      EXPECT_TRUE(base::WriteFile(rules_path, std::string_view()));
    }

    // Persist manifest file.
    JSONFileValueSerializer(temp_dir_.GetPath().Append(kManifestFilename))
        .Serialize(manifest);
  }

  TestRulesetInfo CreateDefaultRuleset() {
    return TestRulesetInfo("id", "rules_file.json", base::Value::List());
  }

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(DNRManifestTest, EmptyRuleset) {
  std::vector<TestRulesetInfo> rulesets({CreateDefaultRuleset()});
  WriteManifestAndRuleset(CreateManifest(rulesets), rulesets);
  LoadAndExpectSuccess(rulesets);
}

TEST_F(DNRManifestTest, InvalidManifestKey) {
  std::vector<TestRulesetInfo> rulesets({CreateDefaultRuleset()});
  base::Value::Dict manifest = CreateManifest(rulesets);
  manifest.Set(dnr_api::ManifestKeys::kDeclarativeNetRequest, 3);

  WriteManifestAndRuleset(manifest, rulesets);
  LoadAndExpectError(
      "Error at key 'declarative_net_request'. Type is invalid. Expected "
      "dictionary, found integer.");
}

TEST_F(DNRManifestTest, InvalidRulesFileKey) {
  std::vector<TestRulesetInfo> rulesets({CreateDefaultRuleset()});
  base::Value::Dict manifest = CreateManifest(rulesets);
  manifest.SetByDottedPath(GetRuleResourcesPath(), 3);

  WriteManifestAndRuleset(manifest, rulesets);
  LoadAndExpectError(
      "Error at key 'declarative_net_request.rule_resources'. Type is invalid. "
      "Expected list, found integer.");
}

TEST_F(DNRManifestTest, InvalidRulesFileFormat) {
  const char kRulesetFile[] = "file1.json";
  base::Value::Dict manifest = CreateManifest({}).Set(
      dnr_api::ManifestKeys::kDeclarativeNetRequest,
      base::Value::Dict().Set(dnr_api::DNRInfo::kRuleResources,
                              base::Value::List().Append(kRulesetFile)));

  WriteManifestAndRuleset(manifest, {});

  LoadAndExpectError(
      "Error at key 'declarative_net_request.rule_resources'. Parsing array "
      "failed at index 0: expected dictionary, got string");
}

TEST_F(DNRManifestTest, InvalidRulesetPath) {
  TestRulesetInfo ruleset("rules", "sub/../rules.json", base::Value::List());
  WriteManifestAndRuleset(CreateManifest({ruleset}), {});
  LoadAndExpectError(ErrorUtils::FormatErrorMessage(
      errors::kRulesFileIsInvalid,
      dnr_api::ManifestKeys::kDeclarativeNetRequest,
      dnr_api::DNRInfo::kRuleResources, ruleset.relative_file_path));
}

TEST_F(DNRManifestTest, InvalidRulesetPath2) {
  TestRulesetInfo ruleset("rules", "rules.json?param=1", base::Value::List());
  WriteManifestAndRuleset(CreateManifest({ruleset}), {});
  LoadAndExpectError(ErrorUtils::FormatErrorMessage(
      errors::kRulesFileIsInvalid,
      dnr_api::ManifestKeys::kDeclarativeNetRequest,
      dnr_api::DNRInfo::kRuleResources, ruleset.relative_file_path));
}

TEST_F(DNRManifestTest, ZeroRulesets) {
  std::vector<TestRulesetInfo> no_rulesets;
  WriteManifestAndRuleset(CreateManifest(no_rulesets), no_rulesets);
  LoadAndExpectSuccess(no_rulesets);
}

TEST_F(DNRManifestTest, MultipleRulesFileSuccess) {
  TestRulesetInfo ruleset_1("1", "file1.json", base::Value::List(),
                            true /* enabled */);
  TestRulesetInfo ruleset_2("2", "file2.json", base::Value::List(),
                            false /* enabled */);
  TestRulesetInfo ruleset_3("3", "file3.json", base::Value::List(),
                            true /* enabled */);

  std::vector<TestRulesetInfo> rulesets = {ruleset_1, ruleset_2, ruleset_3};
  WriteManifestAndRuleset(CreateManifest(rulesets), rulesets);
  LoadAndExpectSuccess(rulesets);
}

TEST_F(DNRManifestTest, MultipleRulesFileInvalidPath) {
  TestRulesetInfo ruleset_1("1", "file1.json", base::Value::List());
  TestRulesetInfo ruleset_2("2", "file2.json", base::Value::List());

  // Only persist |ruleset_1| on disk but include both in the manifest.
  WriteManifestAndRuleset(CreateManifest({ruleset_1, ruleset_2}), {ruleset_1});

  LoadAndExpectError(ErrorUtils::FormatErrorMessage(
      errors::kRulesFileIsInvalid,
      dnr_api::ManifestKeys::kDeclarativeNetRequest,
      dnr_api::DNRInfo::kRuleResources, ruleset_2.relative_file_path));
}

TEST_F(DNRManifestTest, RulesetCountExceeded) {
  std::vector<TestRulesetInfo> rulesets;
  for (int i = 0; i <= dnr_api::MAX_NUMBER_OF_STATIC_RULESETS; ++i) {
    rulesets.emplace_back(base::NumberToString(i), base::Value::List(),
                          false /* enabled */);
  }

  WriteManifestAndRuleset(CreateManifest(rulesets), rulesets);

  LoadAndExpectError(ErrorUtils::FormatErrorMessage(
      errors::kRulesetCountExceeded,
      dnr_api::ManifestKeys::kDeclarativeNetRequest,
      dnr_api::DNRInfo::kRuleResources,
      base::NumberToString(dnr_api::MAX_NUMBER_OF_STATIC_RULESETS)));
}

TEST_F(DNRManifestTest, EnabledRulesetCountExceeded) {
  std::vector<TestRulesetInfo> rulesets;
  for (int i = 0; i <= dnr_api::MAX_NUMBER_OF_ENABLED_STATIC_RULESETS; ++i) {
    rulesets.emplace_back(base::NumberToString(i), base::Value::List(),
                          true /* enabled */);
  }

  WriteManifestAndRuleset(CreateManifest(rulesets), rulesets);

  LoadAndExpectError(ErrorUtils::FormatErrorMessage(
      errors::kEnabledRulesetCountExceeded,
      dnr_api::ManifestKeys::kDeclarativeNetRequest,
      dnr_api::DNRInfo::kRuleResources,
      base::NumberToString(dnr_api::MAX_NUMBER_OF_ENABLED_STATIC_RULESETS)));
}

TEST_F(DNRManifestTest, NonExistentRulesFile) {
  TestRulesetInfo ruleset("id", "invalid_file.json", base::Value::List());

  base::Value::Dict manifest = CreateManifest({ruleset});

  WriteManifestAndRuleset(manifest, {});

  LoadAndExpectError(ErrorUtils::FormatErrorMessage(
      errors::kRulesFileIsInvalid,
      dnr_api::ManifestKeys::kDeclarativeNetRequest,
      dnr_api::DNRInfo::kRuleResources, ruleset.relative_file_path));
}

TEST_F(DNRManifestTest, NeedsDeclarativeNetRequestPermission) {
  std::vector<TestRulesetInfo> rulesets({CreateDefaultRuleset()});
  base::Value::Dict manifest = CreateManifest(rulesets);
  // Remove "declarativeNetRequest" permission.
  manifest.Remove(manifest_keys::kPermissions);

  WriteManifestAndRuleset(manifest, rulesets);

  LoadAndExpectError(ErrorUtils::FormatErrorMessage(
      errors::kDeclarativeNetRequestPermissionNeeded,
      dnr_api::ManifestKeys::kDeclarativeNetRequest));
}

TEST_F(DNRManifestTest, RulesFileInNestedDirectory) {
  TestRulesetInfo ruleset("id", "dir/rules_file.json", base::Value::List());

  std::vector<TestRulesetInfo> rulesets({ruleset});
  base::Value::Dict manifest = CreateManifest(rulesets);

  WriteManifestAndRuleset(manifest, rulesets);
  LoadAndExpectSuccess(rulesets);
}

TEST_F(DNRManifestTest, EmptyRulesetID) {
  TestRulesetInfo ruleset_1("1", "1.json", base::Value::List());
  TestRulesetInfo ruleset_2("", "2.json", base::Value::List());
  TestRulesetInfo ruleset_3("3", "3.json", base::Value::List());
  std::vector<TestRulesetInfo> rulesets({ruleset_1, ruleset_2, ruleset_3});
  WriteManifestAndRuleset(CreateManifest(rulesets), rulesets);

  LoadAndExpectError(ErrorUtils::FormatErrorMessage(
      errors::kInvalidRulesetID, dnr_api::ManifestKeys::kDeclarativeNetRequest,
      dnr_api::DNRInfo::kRuleResources, "1"));
}

TEST_F(DNRManifestTest, DuplicateRulesetID) {
  TestRulesetInfo ruleset_1("1", "1.json", base::Value::List());
  TestRulesetInfo ruleset_2("2", "2.json", base::Value::List());
  TestRulesetInfo ruleset_3("3", "3.json", base::Value::List());
  TestRulesetInfo ruleset_4("1", "3.json", base::Value::List());
  std::vector<TestRulesetInfo> rulesets(
      {ruleset_1, ruleset_2, ruleset_3, ruleset_4});
  WriteManifestAndRuleset(CreateManifest(rulesets), rulesets);

  LoadAndExpectError(ErrorUtils::FormatErrorMessage(
      errors::kInvalidRulesetID, dnr_api::ManifestKeys::kDeclarativeNetRequest,
      dnr_api::DNRInfo::kRuleResources, "3"));
}

TEST_F(DNRManifestTest, ReservedRulesetID) {
  TestRulesetInfo ruleset_1("foo", "1.json", base::Value::List());
  TestRulesetInfo ruleset_2("_bar", "2.json", base::Value::List());
  TestRulesetInfo ruleset_3("baz", "3.json", base::Value::List());
  std::vector<TestRulesetInfo> rulesets({ruleset_1, ruleset_2, ruleset_3});
  WriteManifestAndRuleset(CreateManifest(rulesets), rulesets);

  LoadAndExpectError(ErrorUtils::FormatErrorMessage(
      errors::kInvalidRulesetID, dnr_api::ManifestKeys::kDeclarativeNetRequest,
      dnr_api::DNRInfo::kRuleResources, "1"));
}

// The webstore installation flow involves creation of a dummy extension with an
// empty extension root path. Ensure the manifest handler for declarative net
// request handles it correctly. Regression test for crbug.com/1087348.
TEST_F(DNRManifestTest, EmptyExtensionRootPath) {
  TestRulesetInfo ruleset("foo", "1.json", base::Value::List());

  std::string error;
  scoped_refptr<Extension> extension = Extension::Create(
      base::FilePath(), mojom::ManifestLocation::kInternal,
      CreateManifest({ruleset}), Extension::FROM_WEBSTORE, &error);

  EXPECT_TRUE(extension);
  EXPECT_TRUE(error.empty()) << error;
}

TEST_F(DNRManifestTest, EmptyRulesetPath1) {
  TestRulesetInfo ruleset("foo", "", base::Value::List());
  WriteManifestAndRuleset(CreateManifest({ruleset}), {});
  LoadAndExpectError(ErrorUtils::FormatErrorMessage(
      errors::kRulesFileIsInvalid,
      dnr_api::ManifestKeys::kDeclarativeNetRequest,
      dnr_api::DNRInfo::kRuleResources, ruleset.relative_file_path));
}

TEST_F(DNRManifestTest, EmptyRulesetPath2) {
  TestRulesetInfo ruleset("foo", ".", base::Value::List());
  WriteManifestAndRuleset(CreateManifest({ruleset}), {});
  LoadAndExpectError(ErrorUtils::FormatErrorMessage(
      errors::kRulesFileIsInvalid,
      dnr_api::ManifestKeys::kDeclarativeNetRequest,
      dnr_api::DNRInfo::kRuleResources, ruleset.relative_file_path));
}

TEST_F(DNRManifestTest, DuplicateRulesetPath) {
  TestRulesetInfo ruleset_1("foo", "rules.json", base::Value::List());
  TestRulesetInfo ruleset_2("bar", "rules.json", base::Value::List());
  std::vector<TestRulesetInfo> rulesets({ruleset_1, ruleset_2});
  WriteManifestAndRuleset(CreateManifest(rulesets), rulesets);
  std::vector<InstallWarning> warnings;
  warnings.emplace_back("Unrecognized manifest key 'browser_action'.");
  warnings.emplace_back(errors::kManifestV2IsDeprecatedWarning);
  warnings.emplace_back(errors::kDeclarativeNetRequestPathDuplicates);
  LoadAndExpectWarning(rulesets, warnings);
}

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions
