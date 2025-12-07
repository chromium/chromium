// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/extension_action_handler.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/version_info/channel.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/api/extension_action/action_info_test_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/file_util.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_test.h"
#include "extensions/common/warnings_test_util.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

base::FilePath GetTestDataDir() {
  base::FilePath path;
  base::PathService::Get(DIR_TEST_DATA, &path);
  return path.AppendASCII("manifest_handlers");
}

}  // namespace

// Tests that an unpacked extension with an invisible browser action
// default icon fails as expected.
TEST(ExtensionActionHandlerTest, LoadInvisibleBrowserActionIconUnpacked) {
  base::FilePath extension_dir =
      GetTestDataDir().AppendASCII("browser_action_invisible_icon");
  // Set the flag that enables the error.
  file_util::SetReportErrorForInvisibleIconForTesting(true);
  std::u16string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      extension_dir, mojom::ManifestLocation::kUnpacked, Extension::NO_FLAGS,
      &error));
  file_util::SetReportErrorForInvisibleIconForTesting(false);
  EXPECT_FALSE(extension);
  EXPECT_EQ(
      u"Icon 'invisible_icon.png' specified in 'browser_action' is not "
      u"sufficiently visible.",
      error);
}

// Tests that an unpacked extension with an invisible page action
// default icon fails as expected.
TEST(ExtensionActionHandlerTest, LoadInvisiblePageActionIconUnpacked) {
  base::FilePath extension_dir =
      GetTestDataDir().AppendASCII("page_action_invisible_icon");
  // Set the flag that enables the error.
  file_util::SetReportErrorForInvisibleIconForTesting(true);
  std::u16string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      extension_dir, mojom::ManifestLocation::kUnpacked, Extension::NO_FLAGS,
      &error));
  file_util::SetReportErrorForInvisibleIconForTesting(false);
  EXPECT_FALSE(extension);
  EXPECT_EQ(
      u"Icon 'invisible_icon.png' specified in 'page_action' is not "
      u"sufficiently visible.",
      error);
}

// Tests that an action is always validated in manifest V3.
TEST(ExtensionActionHandlerTest, InvalidActionIcon_ManifestV3) {
  base::FilePath extension_dir =
      GetTestDataDir().AppendASCII("action_invalid_icon");
  std::u16string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      extension_dir, mojom::ManifestLocation::kUnpacked, Extension::NO_FLAGS,
      &error));
  EXPECT_FALSE(extension);
  EXPECT_EQ(
      u"Could not load icon 'nonexistent_icon.png' specified in 'action'.",
      error);
}

using ExtensionActionHandlerManifestTest = ManifestTest;

// An extension using the "action" key correctly does not have any warnings.
TEST_F(ExtensionActionHandlerManifestTest, NoWarnings) {
  ManifestData manifest_data = ManifestData::FromJSON(
      R"({
        "name": "Test",
        "version": "1",
        "manifest_version": 3,
        "action": {}
      })");
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess(manifest_data));
  EXPECT_TRUE(extension->install_warnings().empty());
}

// Don't enable the icon variants feature. Load a valid "action.icon_variants"
// value. Warn if the key is used, but don't create an error.
TEST_F(ExtensionActionHandlerManifestTest, IconVariantsNotEnabled) {
  ManifestData manifest_data = ManifestData::FromJSON(
      R"({
        "name": "Test",
        "version": "1",
        "manifest_version": 3,
        "action": {"icon_variants": [{
          "16": "icon_variants.16.png"
        }]}
      })");
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess(manifest_data));
  warnings_test_util::HasInstallWarning(extension,
                                        "'icon_variants' not enabled.");
}

TEST_F(ExtensionActionHandlerManifestTest, NoActionSpecified_ManifestV2) {
  constexpr char kManifest[] =
      R"({
           "name": "Test",
           "manifest_version": 2,
           "version": "0.1"
         })";

  scoped_refptr<const Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  ASSERT_TRUE(extension);

  const ActionInfo* action_info =
      GetActionInfoOfType(*extension, ActionInfo::Type::kPage);
  ASSERT_TRUE(action_info);
}

TEST_F(ExtensionActionHandlerManifestTest, NoActionSpecified_ManifestV3) {
  constexpr char kManifest[] =
      R"({
           "name": "Test",
           "manifest_version": 3,
           "version": "0.1"
         })";

  scoped_refptr<const Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  ASSERT_TRUE(extension);

  const ActionInfo* action_info =
      GetActionInfoOfType(*extension, ActionInfo::Type::kAction);
  ASSERT_TRUE(action_info);
  EXPECT_EQ(ActionInfo::DefaultState::kDisabled, action_info->default_state);
}

// A parameterized test suite to test each different extension action key
// ("page_action", "browser_action", "action").
class ExtensionActionManifestTest
    : public ManifestTest,
      public testing::WithParamInterface<ActionInfo::Type> {
 public:
  ExtensionActionManifestTest() = default;

  ExtensionActionManifestTest(const ExtensionActionManifestTest&) = delete;
  ExtensionActionManifestTest& operator=(const ExtensionActionManifestTest&) =
      delete;

  ~ExtensionActionManifestTest() override = default;

  // Constructs and returns a ManifestData object with the provided
  // |action_spec|.
  ManifestData GetManifestData(const char* action_spec, int manifest_version) {
    constexpr char kManifestStub[] =
        R"({
             "name": "Test",
             "manifest_version": %d,
             "version": "0.1",
             "%s": %s
           })";

    const char* action_key =
        ActionInfo::GetManifestKeyForActionType(GetParam());

    return ManifestData::FromJSON(base::StringPrintf(
        kManifestStub, manifest_version, action_key, action_spec));
  }

  scoped_refptr<Extension> LoadExtensionWithDefaultPopup(
      const char* popup_file_name,
      int manifest_version,
      TestExtensionDir* test_extension_dir,
      std::u16string* error) {
    const char* action_key =
        ActionInfo::GetManifestKeyForActionType(GetParam());

    test_extension_dir->WriteManifest(base::StringPrintf(
        R"({
             "name": "Test",
             "manifest_version": %d,
             "version": "0.1",
             "%s": { "default_popup": "%s" }
           })",
        manifest_version, action_key, popup_file_name));
    test_extension_dir->WriteFile(FILE_PATH_LITERAL("popup.html"), "");

    scoped_refptr<Extension> extension(file_util::LoadExtension(
        test_extension_dir->UnpackedPath(), mojom::ManifestLocation::kUnpacked,
        Extension::NO_FLAGS, error));
    return extension;
  }
};

// Tests that parsing an action succeeds and properly populates the given
// fields.
TEST_P(ExtensionActionManifestTest, Basic) {
  constexpr char kValidAllFields[] =
      R"({
           "default_popup": "popup.html",
           "default_title": "Title",
           "default_icon": "icon.png"
         })";
  int manifest_version = GetManifestVersionForActionType(GetParam());
  scoped_refptr<const Extension> extension =
      LoadAndExpectSuccess(GetManifestData(kValidAllFields, manifest_version));
  ASSERT_TRUE(extension);
  const ActionInfo* action_info = GetActionInfoOfType(*extension, GetParam());
  ASSERT_TRUE(action_info);

  EXPECT_EQ(extension->GetResourceURL("popup.html"),
            action_info->default_popup_url);
  EXPECT_EQ("Title", action_info->default_title);
  // Make a copy of the map since [] is more readable than find() for comparing
  // values.
  ExtensionIconSet::IconMap icons = action_info->default_icon.map();
  EXPECT_EQ(1u, icons.size());
  EXPECT_EQ(icons[extension_misc::EXTENSION_ICON_GIGANTOR], "icon.png");
}

// Tests that specifying an empty action (e.g., "action": {}) works correctly,
// with empty defaults.
TEST_P(ExtensionActionManifestTest, TestEmptyAction) {
  constexpr char kValidNoFields[] = "{}";
  int manifest_version = GetManifestVersionForActionType(GetParam());
  scoped_refptr<const Extension> extension =
      LoadAndExpectSuccess(GetManifestData(kValidNoFields, manifest_version));
  ASSERT_TRUE(extension);
  const ActionInfo* action_info = GetActionInfoOfType(*extension, GetParam());
  ASSERT_TRUE(action_info);

  EXPECT_EQ(GURL(), action_info->default_popup_url);
  EXPECT_EQ(std::string(), action_info->default_title);
  const ExtensionIconSet::IconMap& icons = action_info->default_icon.map();
  EXPECT_TRUE(icons.empty());
}

// Tests specifying an icon dictionary (with different pixel sizes) in the
// action.
TEST_P(ExtensionActionManifestTest, ValidIconDictionary) {
  constexpr char kValidIconDictionary[] =
      R"({
           "default_icon": {
             "24": "icon24.png",
             "48": "icon48.png",
             "79": "icon79.png"
           }
         })";

  int manifest_version = GetManifestVersionForActionType(GetParam());
  scoped_refptr<const Extension> extension = LoadAndExpectSuccess(
      GetManifestData(kValidIconDictionary, manifest_version));
  ASSERT_TRUE(extension);
  const ActionInfo* action_info = GetActionInfoOfType(*extension, GetParam());
  ASSERT_TRUE(action_info);

  EXPECT_EQ(GURL(), action_info->default_popup_url);
  EXPECT_EQ(std::string(), action_info->default_title);
  // Make a copy of the map since [] is more readable than find() for comparing
  // values.
  ExtensionIconSet::IconMap icons = action_info->default_icon.map();
  EXPECT_EQ(3u, icons.size());
  EXPECT_EQ("icon24.png", icons[24]);
  EXPECT_EQ("icon48.png", icons[48]);
  EXPECT_EQ("icon79.png", icons[79]);
}

// Tests some invalid cases.
TEST_P(ExtensionActionManifestTest, Invalid) {
  constexpr char kInvalidTopLevel1[] = "[]";
  constexpr char kInvalidTopLevel2[] = "\"foo\"";
  constexpr char kInvalidTopLevel3[] = "17";

  const char* expected_error = nullptr;
  switch (GetParam()) {
    case ActionInfo::Type::kBrowser:
      expected_error = manifest_errors::kInvalidBrowserAction;
      break;
    case ActionInfo::Type::kPage:
      expected_error = manifest_errors::kInvalidPageAction;
      break;
    case ActionInfo::Type::kAction:
      expected_error = manifest_errors::kInvalidAction;
      break;
  }

  int manifest_version = GetManifestVersionForActionType(GetParam());
  for (const char* spec :
       {kInvalidTopLevel1, kInvalidTopLevel2, kInvalidTopLevel3}) {
    LoadAndExpectError(GetManifestData(spec, manifest_version), expected_error);
  }

  constexpr char kInvalidPopup[] = R"({ "default_popup": {} })";
  LoadAndExpectError(GetManifestData(kInvalidPopup, manifest_version),
                     manifest_errors::kInvalidActionDefaultPopup);
  constexpr char kInvalidTitle[] = R"({ "default_title": {} })";
  LoadAndExpectError(GetManifestData(kInvalidTitle, manifest_version),
                     manifest_errors::kInvalidActionDefaultTitle);
  constexpr char kInvalidIcon[] = R"({ "default_icon": [] })";
  LoadAndExpectError(GetManifestData(kInvalidIcon, manifest_version),
                     manifest_errors::kInvalidActionDefaultIcon);
}

// Tests success when default_popup is valid.
TEST_P(ExtensionActionManifestTest, ValidDefaultPopup) {
  constexpr char valid_popup_file_name[] = "popup.html";
  TestExtensionDir test_extension_dir = TestExtensionDir();
  int manifest_version = GetManifestVersionForActionType(GetParam());
  std::u16string error;
  scoped_refptr<Extension> test_extension = LoadExtensionWithDefaultPopup(
      valid_popup_file_name, manifest_version, &test_extension_dir, &error);
  ASSERT_TRUE(test_extension) << error;

  std::vector<InstallWarning> warnings;
  if (GetParam() == ActionInfo::Type::kBrowser) {
    warnings.emplace_back("Unrecognized manifest key 'browser_action'.");
  }
  if (manifest_version == 2) {
    warnings.emplace_back(manifest_errors::kManifestV2IsDeprecatedWarning);
  }
  EXPECT_EQ(warnings, test_extension->install_warnings());

  const ActionInfo* action_info =
      GetActionInfoOfType(*test_extension, GetParam());
  ASSERT_TRUE(action_info);
  EXPECT_EQ(test_extension->GetResourceURL("popup.html"),
            action_info->default_popup_url);
}

// Tests success when default_popup is empty.
TEST_P(ExtensionActionManifestTest, EmptyDefaultPopup) {
  constexpr char empty_popup_file_name[] = "";
  TestExtensionDir test_extension_dir = TestExtensionDir();
  int manifest_version = GetManifestVersionForActionType(GetParam());
  std::u16string error;
  scoped_refptr<Extension> test_extension = LoadExtensionWithDefaultPopup(
      empty_popup_file_name, manifest_version, &test_extension_dir, &error);
  ASSERT_TRUE(test_extension) << error;

  std::vector<InstallWarning> warnings;
  if (GetParam() == ActionInfo::Type::kBrowser) {
    warnings.emplace_back("Unrecognized manifest key 'browser_action'.");
  }
  if (manifest_version == 2) {
    warnings.emplace_back(manifest_errors::kManifestV2IsDeprecatedWarning);
  }
  EXPECT_EQ(warnings, test_extension->install_warnings());

  const ActionInfo* action_info =
      GetActionInfoOfType(*test_extension, GetParam());
  ASSERT_TRUE(action_info);
  EXPECT_TRUE(action_info->default_popup_url.is_empty());
}

// Tests warning when the default_popup seems to be for another extension.
TEST_P(ExtensionActionManifestTest, OtherExtensionSpecifiedDefaultPopup) {
  constexpr char other_extension_specified_popup_file_name[] =
      "chrome-extension://abcdefghijklmnopqrstuvwxyzabcdef/popup.html";
  TestExtensionDir test_extension_dir = TestExtensionDir();
  int manifest_version = GetManifestVersionForActionType(GetParam());
  std::u16string error;
  scoped_refptr<Extension> test_extension = LoadExtensionWithDefaultPopup(
      other_extension_specified_popup_file_name, manifest_version,
      &test_extension_dir, &error);
  ASSERT_FALSE(test_extension);
  ASSERT_EQ(manifest_errors::kInvalidActionDefaultPopup, error);
}

// Tests warning when the default_popup doesn't exist on file system.
TEST_P(ExtensionActionManifestTest, NonexistentDefaultPopup) {
  constexpr char nonexistent_popup_file_name[] = "nonexistent_popup.html";
  TestExtensionDir test_extension_dir = TestExtensionDir();
  int manifest_version = GetManifestVersionForActionType(GetParam());
  std::u16string error;
  scoped_refptr<Extension> test_extension = LoadExtensionWithDefaultPopup(
      nonexistent_popup_file_name, manifest_version, &test_extension_dir,
      &error);
  ASSERT_TRUE(test_extension) << error;

  std::vector<InstallWarning> warnings;
  if (GetParam() == ActionInfo::Type::kBrowser) {
    warnings.emplace_back("Unrecognized manifest key 'browser_action'.");
  }
  if (manifest_version == 2) {
    warnings.emplace_back(manifest_errors::kManifestV2IsDeprecatedWarning);
  }
  warnings.emplace_back(manifest_errors::kNonexistentDefaultPopup);
  EXPECT_EQ(warnings, test_extension->install_warnings());

  const ActionInfo* action_info =
      GetActionInfoOfType(*test_extension, GetParam());
  ASSERT_TRUE(action_info);
  EXPECT_EQ(test_extension->GetResourceURL("nonexistent_popup.html"),
            action_info->default_popup_url);
}

// Test the handling of the default_state key.
TEST_P(ExtensionActionManifestTest, DefaultState) {
  constexpr char kDefaultStateDisabled[] = R"({"default_state": "disabled"})";
  constexpr char kDefaultStateEnabled[] = R"({"default_state": "enabled"})";
  constexpr char kDefaultStateInvalid[] = R"({"default_state": "foo"})";

  // default_state is only valid for "action" types.
  const bool default_state_allowed = GetParam() == ActionInfo::Type::kAction;
  const std::string key_disallowed_error =
      base::UTF16ToUTF8(manifest_errors::kDefaultStateShouldNotBeSet);
  const std::string invalid_action_error =
      base::UTF16ToUTF8(manifest_errors::kInvalidActionDefaultState);

  struct {
    // The manifest definition of the action key.
    const char* spec;
    // The expected error, if parsing was unsuccessful.
    const char* expected_error;
    // The expected state, if parsing was successful.
    std::optional<ActionInfo::DefaultState> expected_state;
  } test_cases[] = {
      {kDefaultStateDisabled,
       default_state_allowed ? nullptr : key_disallowed_error.c_str(),
       default_state_allowed
           ? std::make_optional(ActionInfo::DefaultState::kDisabled)
           : std::nullopt},
      {kDefaultStateEnabled,
       default_state_allowed ? nullptr : key_disallowed_error.c_str(),
       default_state_allowed
           ? std::make_optional(ActionInfo::DefaultState::kEnabled)
           : std::nullopt},
      {kDefaultStateInvalid,
       default_state_allowed ? invalid_action_error.c_str()
                             : key_disallowed_error.c_str(),
       std::nullopt},
  };

  int manifest_version = GetManifestVersionForActionType(GetParam());
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.spec);

    if (test_case.expected_error == nullptr) {
      ASSERT_TRUE(test_case.expected_state);
      scoped_refptr<const Extension> extension = LoadAndExpectSuccess(
          GetManifestData(test_case.spec, manifest_version));
      ASSERT_TRUE(extension);
      const ActionInfo* action_info =
          GetActionInfoOfType(*extension, GetParam());
      ASSERT_TRUE(action_info);
      EXPECT_EQ(*test_case.expected_state, action_info->default_state);
    } else {
      ASSERT_FALSE(test_case.expected_state);
      LoadAndExpectError(GetManifestData(test_case.spec, manifest_version),
                         test_case.expected_error);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExtensionActionManifestTest,
                         testing::Values(ActionInfo::Type::kBrowser,
                                         ActionInfo::Type::kPage,
                                         ActionInfo::Type::kAction));

// Enable the icon variants feature.
class ExtensionActionIconVariantsTest : public ManifestTest {
 public:
  ExtensionActionIconVariantsTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionIconVariants);
  }

 private:
  const ScopedCurrentChannel current_channel_{version_info::Channel::CANARY};
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ExtensionActionIconVariantsTest, All) {
  // Warn, don't error, if manifest.json has an empty action.icon_variants.
  {
    ManifestData manifest_data = ManifestData::FromJSON(
        R"({
          "name": "Test",
          "version": "1",
          "manifest_version": 3,
          "action": {"icon_variants": {}}
        })");
    scoped_refptr<extensions::Extension> extension(
        LoadAndExpectSuccess(manifest_data));
    warnings_test_util::HasInstallWarning(extension,
                                          "'icon_variants' must be a list.");
  }

  // Warn, don't error, if manifest.json has an icon with an invalid mime type.
  {
    ManifestData manifest_data = ManifestData::FromJSON(
        R"({
          "name": "Test",
          "version": "1",
          "manifest_version": 3,
          "action": {"icon_variants": [{
            "16": "icon_variants.16.txt"
          }]}
        })");
    scoped_refptr<extensions::Extension> extension(
        LoadAndExpectSuccess(manifest_data));
    warnings_test_util::HasInstallWarning(
        extension, "'icon_variants' file path unsupported mime type.");

    const ActionInfo* action_info =
        GetActionInfoOfType(*extension, ActionInfo::Type::kAction);
    ASSERT_TRUE(action_info);
    // TODO(crbug.com/344639840): Get() using filters to avoid manual retrieval.
    const std::vector<ExtensionIconVariant>& icon_variants =
        action_info->icon_variants->GetList();
    EXPECT_TRUE(icon_variants.empty());
  }

  // Warn, don't error, if manifest.json has an icon with an invalid path.
  {
    ManifestData manifest_data = ManifestData::FromJSON(
        R"({
          "name": "Test",
          "version": "1",
          "manifest_version": 3,
          "action": {"icon_variants": [{
            "16": "C:\\icon_variants.16.png"
          }]}
        })");
    scoped_refptr<extensions::Extension> extension(
        LoadAndExpectSuccess(manifest_data));
    warnings_test_util::HasInstallWarning(extension,
                                          "'icon_variants' invalid file path.");

    const ActionInfo* action_info =
        GetActionInfoOfType(*extension, ActionInfo::Type::kAction);
    ASSERT_TRUE(action_info);
    // TODO(crbug.com/344639840): Get() using filters to avoid manual retrieval.
    const std::vector<ExtensionIconVariant>& icon_variants =
        action_info->icon_variants->GetList();
    EXPECT_TRUE(icon_variants.empty());
  }

  // Valid "action.icon_variants" value.
  {
    ManifestData manifest_data = ManifestData::FromJSON(
        R"({
          "name": "Test",
          "version": "1",
          "manifest_version": 3,
          "action": {"icon_variants": [{
            "16": "icon_variants.16.png"
          }]}
        })");
    scoped_refptr<extensions::Extension> extension(
        LoadAndExpectSuccess(manifest_data));

    const ActionInfo* action_info =
        GetActionInfoOfType(*extension, ActionInfo::Type::kAction);
    ASSERT_TRUE(action_info);
    // TODO(crbug.com/344639840): Get() using filters to avoid manual retrieval.
    const std::vector<ExtensionIconVariant>& icon_variants =
        action_info->icon_variants->GetList();
    EXPECT_EQ(1u, icon_variants.size());
    EXPECT_EQ("icon_variants.16.png", icon_variants[0]
                                          .GetSizes()
                                          .find(16)
                                          ->second.relative_path()
                                          .AsUTF8Unsafe());
  }
}

}  // namespace extensions
