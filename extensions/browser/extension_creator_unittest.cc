// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_creator.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/values.h"
#include "crypto/rsa_private_key.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_paths.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {
namespace {

base::FilePath GetTestFile(const char* test_file) {
  base::FilePath path;
  base::PathService::Get(DIR_TEST_DATA, &path);
  return path.AppendASCII("extension_creator/").AppendASCII(test_file);
}
}  // namespace

class ExtensionCreatorTest : public testing::Test {
 public:
  ExtensionCreatorTest() = default;

  ExtensionCreatorTest(const ExtensionCreatorTest&) = delete;
  ExtensionCreatorTest& operator=(const ExtensionCreatorTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    test_path_ = temp_dir_.GetPath();

    extension_creator_ = std::make_unique<ExtensionCreator>();
  }

  // Helper functions to call private methods of ExtensionCreator.
  std::unique_ptr<crypto::RSAPrivateKey> ReadInputKey(
      const base::FilePath& private_key_path) {
    return extension_creator_->ReadInputKey(private_key_path);
  }

  bool ValidateExtension(const base::FilePath& dir, int flags) {
    return extension_creator_->ValidateExtension(dir, flags);
  }

  ExtensionCreator* extension_creator() const {
    return extension_creator_.get();
  }

  base::FilePath CreateTestPath() const { return test_path_; }

 private:
  base::ScopedTempDir temp_dir_;
  base::FilePath test_path_;
  std::unique_ptr<ExtensionCreator> extension_creator_;
};

TEST_F(ExtensionCreatorTest, ReadInputKeyPathNonExistent) {
  const base::FilePath file_path =
      CreateTestPath().Append(FILE_PATH_LITERAL("non_existent.pem"));
  EXPECT_EQ(nullptr, ReadInputKey(file_path));
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_EXTENSION_PRIVATE_KEY_NO_EXISTS),
            extension_creator()->error_message());
}

TEST_F(ExtensionCreatorTest, ReadInputKeyDangerousPath) {
  const base::FilePath file_path =
      CreateTestPath().Append(FILE_PATH_LITERAL("foo/bar"));
  ASSERT_TRUE(base::CreateDirectory(file_path));
  const base::FilePath file_path_dangerous =
      file_path.Append(FILE_PATH_LITERAL(".."))
          .Append(FILE_PATH_LITERAL("dangerous_path_test.pem"));
  ASSERT_TRUE(file_path_dangerous.ReferencesParent());

  const char kTestData[] = "0123";
  ASSERT_TRUE(base::WriteFile(file_path_dangerous, kTestData));

  // If a path includes parent reference `..`, reading the path must fail.
  EXPECT_EQ(nullptr, ReadInputKey(file_path_dangerous));
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_EXTENSION_PRIVATE_KEY_FAILED_TO_READ),
            extension_creator()->error_message());
}

TEST_F(ExtensionCreatorTest, ReadInputKeyInvalidPEMFormat) {
  const base::FilePath file_path =
      CreateTestPath().Append(FILE_PATH_LITERAL("invalid_format.pem"));

  // Creates a file that starts with `-----BEGIN`. but it doesn't end with
  // `KEY-----`.
  const char kTestData[] = "-----BEGIN foo";
  ASSERT_TRUE(base::WriteFile(file_path, kTestData));

  EXPECT_EQ(nullptr, ReadInputKey(file_path));
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_EXTENSION_PRIVATE_KEY_INVALID),
            extension_creator()->error_message());
}

TEST_F(ExtensionCreatorTest, ReadInputKeyNotPKCSFormat) {
  EXPECT_EQ(nullptr, ReadInputKey(GetTestFile("not_pkcs.pem")));
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_EXTENSION_PRIVATE_KEY_INVALID_FORMAT),
            extension_creator()->error_message());
}

TEST_F(ExtensionCreatorTest, ReadInputKeyPKCSFormat) {
  EXPECT_NE(nullptr, ReadInputKey(GetTestFile("pkcs8.pem")));
  EXPECT_TRUE(extension_creator()->error_message().empty());
}

TEST_F(ExtensionCreatorTest, ValidateExtension) {
  const base::FilePath src_path = CreateTestPath();
  ASSERT_TRUE(base::CreateDirectory(src_path));

  EXPECT_FALSE(ValidateExtension(src_path, 0));
  EXPECT_EQ("Manifest file is missing or unreadable",
            extension_creator()->error_message());

  // Add partial manifest file.
  auto manifest_file = src_path.Append(FILE_PATH_LITERAL("manifest.json"));
  ASSERT_TRUE(base::WriteFile(manifest_file, R"({ "manifest_version": 3)"));

  EXPECT_FALSE(ValidateExtension(src_path, 0));
  EXPECT_TRUE(extension_creator()->error_message().starts_with(
      "Manifest is not valid JSON."));

  // Replace partial manifest with correct minimum file.
  ASSERT_TRUE(base::WriteFile(manifest_file,
      R"({ "manifest_version": 3, "name": "test", "version": "1" })"));

  EXPECT_TRUE(ValidateExtension(src_path, 0));
  // TODO(crbug.com/40278707) Adjust GetDefaultLocaleFromManifest method.
  // EXPECT_TRUE(extension_creator()->error_message().empty());

  // Replace manifest specifying default_locale without adding folder yet.
  ASSERT_TRUE(base::WriteFile(manifest_file, R"({ "manifest_version": 3,
      "name": "test", "version": "1", "default_locale": "en" })"));

  EXPECT_FALSE(ValidateExtension(src_path, 0));
  EXPECT_EQ("Default locale was specified, but _locales subtree is missing.",
            extension_creator()->error_message());

  // Add localization folder.
  const auto locale_path = src_path.Append(kLocaleFolder);
  base::FilePath en_locale = locale_path.AppendASCII("en");
  ASSERT_TRUE(base::CreateDirectory(en_locale));

  EXPECT_FALSE(ValidateExtension(src_path, 0));
  EXPECT_EQ("Catalog file is missing for locale en.",
            extension_creator()->error_message());

  // Add valid default localization file.
  base::FilePath en_messages_file = en_locale.Append(kMessagesFilename);
  const std::string en_data = R"({ "name": { "message": "default" } })";
  ASSERT_TRUE(base::WriteFile(en_messages_file, en_data));

  EXPECT_TRUE(ValidateExtension(src_path, 0));
  EXPECT_TRUE(extension_creator()->error_message().empty());

  // Add additional localization file with undefined variable.
  base::FilePath de_locale = locale_path.AppendASCII("de");
  ASSERT_TRUE(base::CreateDirectory(de_locale));
  base::FilePath de_messages_file = de_locale.Append(kMessagesFilename);
  const std::string de_data = R"({ "name": { "message": "with $VAR$" } })";
  ASSERT_TRUE(base::WriteFile(de_messages_file, de_data));

  EXPECT_FALSE(ValidateExtension(src_path, 0));
  EXPECT_THAT(extension_creator()->error_message(),
              testing::HasSubstr("Variable $VAR$ used but not defined."));
}

}  // namespace extensions
