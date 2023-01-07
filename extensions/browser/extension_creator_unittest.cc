// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_creator.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "crypto/rsa_private_key.h"
#include "extensions/common/extension_paths.h"
#include "extensions/strings/grit/extensions_strings.h"
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

  // A helper function to call ExtensionCreator::ReadInputKey(...) because it's
  // a private method of ExtensionCreator.
  std::unique_ptr<crypto::RSAPrivateKey> ReadInputKey(
      const base::FilePath& private_key_path) {
    return extension_creator_->ReadInputKey(private_key_path);
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
  ASSERT_EQ(static_cast<int>(strlen(kTestData)),
            base::WriteFile(file_path_dangerous, kTestData, strlen(kTestData)));

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
  ASSERT_EQ(static_cast<int>(strlen(kTestData)),
            base::WriteFile(file_path, kTestData, strlen(kTestData)));

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

}  // namespace extensions
