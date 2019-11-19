// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/file_util.h"

#include <stddef.h>

#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_string_value_serializer.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace extensions {

namespace {

const char manifest_content[] =
    "{\n"
    "  \"name\": \"Underscore folder test\",\n"
    "  \"version\": \"1.0\",\n"
    "  \"manifest_version\": 2\n"
    "}\n";

scoped_refptr<Extension> LoadExtensionManifest(
    const base::DictionaryValue& manifest,
    const base::FilePath& manifest_dir,
    Manifest::Location location,
    int extra_flags,
    std::string* error) {
  scoped_refptr<Extension> extension =
      Extension::Create(manifest_dir, location, manifest, extra_flags, error);
  return extension;
}

scoped_refptr<Extension> LoadExtensionManifest(
    const std::string& manifest_value,
    const base::FilePath& manifest_dir,
    Manifest::Location location,
    int extra_flags,
    std::string* error) {
  JSONStringValueDeserializer deserializer(manifest_value);
  std::unique_ptr<base::Value> result =
      deserializer.Deserialize(nullptr, error);
  if (!result.get())
    return nullptr;
  CHECK_EQ(base::Value::Type::DICTIONARY, result->type());
  return LoadExtensionManifest(*base::DictionaryValue::From(std::move(result)),
                               manifest_dir, location, extra_flags, error);
}

void RunUnderscoreDirectoriesTest(
    const std::vector<std::string>& underscore_directories) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath ext_path = temp.GetPath();
  ASSERT_TRUE(base::CreateDirectory(ext_path));

  for (const auto& dir : underscore_directories)
    ASSERT_TRUE(base::CreateDirectory(ext_path.AppendASCII(dir)));

  ASSERT_EQ(static_cast<int>(strlen(manifest_content)),
            base::WriteFile(ext_path.AppendASCII("manifest.json"),
                            manifest_content, strlen(manifest_content)));

  std::string error;
  scoped_refptr<Extension> extension = file_util::LoadExtension(
      ext_path, Manifest::UNPACKED, Extension::NO_FLAGS, &error);
  ASSERT_TRUE(extension) << error;
  EXPECT_TRUE(error.empty());

  const std::vector<InstallWarning>& warnings = extension->install_warnings();
  ASSERT_EQ(1u, warnings.size());

  // The warning should report any one of the illegal underscore directories.
  bool warning_matched = false;
  for (const auto& dir : underscore_directories) {
    std::string expected_warning = base::StringPrintf(
        "Cannot load extension with file or directory name %s. Filenames "
        "starting with \"_\" are reserved for use by the system.",
        dir.c_str());
    if (expected_warning == warnings[0].message)
      warning_matched = true;
  }

  EXPECT_TRUE(warning_matched)
      << "Correct warning not generated for an unpacked extension with "
      << base::JoinString(underscore_directories, ",") << " directories.";
}

}  // namespace

typedef testing::Test FileUtilTest;

TEST_F(FileUtilTest, InstallUninstallGarbageCollect) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  // Create a source extension.
  std::string extension_id("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  std::string version("1.0");
  base::FilePath src = temp.GetPath().AppendASCII(extension_id);
  ASSERT_TRUE(base::CreateDirectory(src));

  base::FilePath extension_content;
  base::CreateTemporaryFileInDir(src, &extension_content);
  ASSERT_TRUE(base::PathExists(extension_content));

  // Create a extensions tree.
  base::FilePath all_extensions = temp.GetPath().AppendASCII("extensions");
  ASSERT_TRUE(base::CreateDirectory(all_extensions));

  // Install in empty directory. Should create parent directories as needed.
  base::FilePath version_1 =
      file_util::InstallExtension(src, extension_id, version, all_extensions);
  ASSERT_EQ(
      version_1.value(),
      all_extensions.AppendASCII(extension_id).AppendASCII("1.0_0").value());
  ASSERT_TRUE(base::DirectoryExists(version_1));
  ASSERT_TRUE(base::PathExists(version_1.Append(extension_content.BaseName())));

  // Should have moved the source.
  ASSERT_FALSE(base::DirectoryExists(src));

  // Install again. Should create a new one with different name.
  ASSERT_TRUE(base::CreateDirectory(src));
  base::FilePath version_2 =
      file_util::InstallExtension(src, extension_id, version, all_extensions);
  ASSERT_EQ(
      version_2.value(),
      all_extensions.AppendASCII(extension_id).AppendASCII("1.0_1").value());
  ASSERT_TRUE(base::DirectoryExists(version_2));

  // Should have moved the source.
  ASSERT_FALSE(base::DirectoryExists(src));

  // Install yet again. Should create a new one with a different name.
  ASSERT_TRUE(base::CreateDirectory(src));
  base::FilePath version_3 =
      file_util::InstallExtension(src, extension_id, version, all_extensions);
  ASSERT_EQ(
      version_3.value(),
      all_extensions.AppendASCII(extension_id).AppendASCII("1.0_2").value());
  ASSERT_TRUE(base::DirectoryExists(version_3));

  // Uninstall. Should remove entire extension subtree.
  file_util::UninstallExtension(all_extensions, extension_id);
  ASSERT_FALSE(base::DirectoryExists(version_1.DirName()));
  ASSERT_FALSE(base::DirectoryExists(version_2.DirName()));
  ASSERT_FALSE(base::DirectoryExists(version_3.DirName()));
  ASSERT_TRUE(base::DirectoryExists(all_extensions));
}

TEST_F(FileUtilTest, LoadExtensionWithMetadataFolder) {
  RunUnderscoreDirectoriesTest({"_metadata"});
}

TEST_F(FileUtilTest, LoadExtensionWithUnderscoreFolder) {
  RunUnderscoreDirectoriesTest({"_badfolder"});
}

TEST_F(FileUtilTest, LoadExtensionWithUnderscoreAndMetadataFolder) {
  RunUnderscoreDirectoriesTest({"_metadata", "_badfolder"});
}

TEST_F(FileUtilTest, LoadExtensionWithValidLocales) {
  base::FilePath install_dir;
  ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &install_dir));
  install_dir = install_dir.AppendASCII("extension_with_locales");

  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      install_dir, Manifest::UNPACKED, Extension::NO_FLAGS, &error));
  ASSERT_TRUE(extension.get() != nullptr);
  EXPECT_EQ("The first extension that I made.", extension->description());
}

TEST_F(FileUtilTest, LoadExtensionWithoutLocalesFolder) {
  base::FilePath install_dir;
  ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &install_dir));
  install_dir = install_dir.AppendASCII("extension_without_locales");

  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      install_dir, Manifest::UNPACKED, Extension::NO_FLAGS, &error));
  ASSERT_FALSE(extension.get() == nullptr);
  EXPECT_TRUE(error.empty());
}

TEST_F(FileUtilTest, CheckIllegalFilenamesNoUnderscores) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath src_path = temp.GetPath().AppendASCII("some_dir");
  ASSERT_TRUE(base::CreateDirectory(src_path));

  std::string data = "{ \"name\": { \"message\": \"foobar\" } }";
  ASSERT_EQ(static_cast<int>(data.length()),
            base::WriteFile(src_path.AppendASCII("some_file.txt"), data.c_str(),
                            data.length()));
  std::string error;
  EXPECT_TRUE(file_util::CheckForIllegalFilenames(temp.GetPath(), &error));
}

TEST_F(FileUtilTest, CheckIllegalFilenamesOnlyReserved) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  static const base::FilePath::CharType* const folders[] = {
      kLocaleFolder, kPlatformSpecificFolder};

  for (size_t i = 0; i < base::size(folders); i++) {
    base::FilePath src_path = temp.GetPath().Append(folders[i]);
    ASSERT_TRUE(base::CreateDirectory(src_path));
  }

  std::string error;
  EXPECT_TRUE(file_util::CheckForIllegalFilenames(temp.GetPath(), &error));
}

TEST_F(FileUtilTest, CheckIllegalFilenamesReservedAndIllegal) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath src_path = temp.GetPath().Append(kLocaleFolder);
  ASSERT_TRUE(base::CreateDirectory(src_path));

  src_path = temp.GetPath().AppendASCII("_some_dir");
  ASSERT_TRUE(base::CreateDirectory(src_path));

  std::string error;
  EXPECT_FALSE(file_util::CheckForIllegalFilenames(temp.GetPath(), &error));
}

// These tests do not work on Windows, because it is illegal to create a
// file/directory with a Windows reserved name. Because we cannot create a
// file that will cause the test to fail, let's skip the test.
#if !defined(OS_WIN)
TEST_F(FileUtilTest, CheckIllegalFilenamesDirectoryWindowsReserved) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath src_path = temp.GetPath().AppendASCII("aux");
  ASSERT_TRUE(base::CreateDirectory(src_path));

  std::string error;
  EXPECT_FALSE(
      file_util::CheckForWindowsReservedFilenames(temp.GetPath(), &error));
}

TEST_F(FileUtilTest,
       CheckIllegalFilenamesWindowsReservedFilenameWithExtension) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath src_path = temp.GetPath().AppendASCII("some_dir");
  ASSERT_TRUE(base::CreateDirectory(src_path));

  std::string data = "{ \"name\": { \"message\": \"foobar\" } }";
  ASSERT_EQ(static_cast<int>(data.length()),
            base::WriteFile(src_path.AppendASCII("lpt1.txt"), data.c_str(),
                            data.length()));

  std::string error;
  EXPECT_FALSE(
      file_util::CheckForWindowsReservedFilenames(temp.GetPath(), &error));
}
#endif

TEST_F(FileUtilTest, LoadExtensionGivesHelpfullErrorOnMissingManifest) {
  base::FilePath install_dir;
  ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &install_dir));
  install_dir =
      install_dir.AppendASCII("file_util").AppendASCII("missing_manifest");

  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      install_dir, Manifest::UNPACKED, Extension::NO_FLAGS, &error));
  ASSERT_TRUE(extension.get() == nullptr);
  ASSERT_FALSE(error.empty());
  ASSERT_EQ(manifest_errors::kManifestUnreadable, error);
}

TEST_F(FileUtilTest, LoadExtensionGivesHelpfullErrorOnBadManifest) {
  base::FilePath install_dir;
  ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &install_dir));
  install_dir =
      install_dir.AppendASCII("file_util").AppendASCII("bad_manifest");

  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      install_dir, Manifest::UNPACKED, Extension::NO_FLAGS, &error));
  ASSERT_TRUE(extension.get() == nullptr);
  ASSERT_FALSE(error.empty());
  ASSERT_EQ(manifest_errors::kManifestParseError +
                std::string("  Line: 2, column: 16, Syntax error."),
            error);
}

TEST_F(FileUtilTest, ValidateThemeUTF8) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  // "aeo" with accents. Use http://0xcc.net/jsescape/ to decode them.
  std::string non_ascii_file = "\xC3\xA0\xC3\xA8\xC3\xB2.png";
  base::FilePath non_ascii_path =
      temp.GetPath().Append(base::FilePath::FromUTF8Unsafe(non_ascii_file));
  base::WriteFile(non_ascii_path, "", 0);

  std::string kManifest = base::StringPrintf(
      "{ \"name\": \"Test\", \"version\": \"1.0\", "
      "  \"theme\": { \"images\": { \"theme_frame\": \"%s\" } }"
      "}",
      non_ascii_file.c_str());
  std::string error;
  scoped_refptr<Extension> extension = LoadExtensionManifest(
      kManifest, temp.GetPath(), Manifest::UNPACKED, 0, &error);
  ASSERT_TRUE(extension.get()) << error;

  std::vector<InstallWarning> warnings;
  EXPECT_TRUE(file_util::ValidateExtension(extension.get(), &error, &warnings))
      << error;
  EXPECT_EQ(0U, warnings.size());
}

TEST_F(FileUtilTest, BackgroundScriptsMustExist) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->SetString("name", "test");
  value->SetString("version", "1");
  value->SetInteger("manifest_version", 2);

  base::ListValue* scripts =
      value->SetList("background.scripts", std::make_unique<base::ListValue>());
  scripts->AppendString("foo.js");

  std::string error;
  std::vector<InstallWarning> warnings;
  scoped_refptr<Extension> extension = LoadExtensionManifest(
      *value, temp.GetPath(), Manifest::UNPACKED, 0, &error);
  ASSERT_TRUE(extension.get()) << error;

  EXPECT_FALSE(
      file_util::ValidateExtension(extension.get(), &error, &warnings));
  EXPECT_EQ(
      l10n_util::GetStringFUTF8(IDS_EXTENSION_LOAD_BACKGROUND_SCRIPT_FAILED,
                                base::ASCIIToUTF16("foo.js")),
      error);
  EXPECT_EQ(0U, warnings.size());

  scripts->Clear();
  scripts->AppendString("http://google.com/foo.js");

  extension = LoadExtensionManifest(*value, temp.GetPath(), Manifest::UNPACKED,
                                    0, &error);
  ASSERT_TRUE(extension.get()) << error;

  warnings.clear();
  EXPECT_FALSE(
      file_util::ValidateExtension(extension.get(), &error, &warnings));
  EXPECT_EQ(
      l10n_util::GetStringFUTF8(IDS_EXTENSION_LOAD_BACKGROUND_SCRIPT_FAILED,
                                base::ASCIIToUTF16("http://google.com/foo.js")),
      error);
  EXPECT_EQ(0U, warnings.size());
}

// Private key, generated by Chrome specifically for this test, and
// never used elsewhere.
const char private_key[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIICdgIBADANBgkqhkiG9w0BAQEFAASCAmAwggJcAgEAAoGBAKt02SR0FYaYy6fpW\n"
    "MAA+kU1BgK3d+OmmWfdr+JATIjhRkyeSF4lTd/71JQsyKqPzYkQPi3EeROWM+goTv\n"
    "EhJqq07q63BolpsFmlV+S4ny+sBA2B4aWwRYXlBWikdrQSA0mJMzvEHc6nKzBgXik\n"
    "QSVbyyBNAsxlDB9WaCxRVOpK3AgMBAAECgYBGvSPlrVtAOAQ2V8j9FqorKZA8SLPX\n"
    "IeJC/yzU3RB2nPMjI17aMOvrUHxJUhzMeh4jwabVvSzzDtKFozPGupW3xaI8sQdi2\n"
    "WWMTQIk/Q9HHDWoQ9qA6SwX2qWCc5SyjCKqVp78ye+000kqTJYjBsDgXeAlzKcx2B\n"
    "4GAAeWonDdkQJBANNb8wrqNWFn7DqyQTfELzcRTRnqQ/r1pdeJo6obzbnwGnlqe3t\n"
    "KhLjtJNIGrQg5iC0OVLWFuvPJs0t3z62A1ckCQQDPq2JZuwTwu5Pl4DJ0r9O1FdqN\n"
    "JgqPZyMptokCDQ3khLLGakIu+TqB9YtrzI69rJMSG2Egb+6McaDX+dh3XmR/AkB9t\n"
    "xJf6qDnmA2td/tMtTc0NOk8Qdg/fD8xbZ/YfYMnVoYYs9pQoilBaWRePDRNURMLYZ\n"
    "vHAI0Llmw7tj7jv17pAkEAz44uXRpjRKtllUIvi5pUENAHwDz+HvdpGH68jpU3hmb\n"
    "uOwrmnQYxaMReFV68Z2w9DcLZn07f7/R9Wn72z89CxwJAFsDoNaDes4h48bX7plct\n"
    "s9ACjmTwcCigZjN2K7AGv7ntCLF3DnV5dK0dTHNaAdD3SbY3jl29Rk2CwiURSX6Ee\n"
    "g==\n"
    "-----END PRIVATE KEY-----\n";

TEST_F(FileUtilTest, FindPrivateKeyFiles) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath src_path = temp.GetPath().AppendASCII("some_dir");
  ASSERT_TRUE(base::CreateDirectory(src_path));

  ASSERT_EQ(static_cast<int>(base::size(private_key)),
            base::WriteFile(src_path.AppendASCII("a_key.pem"), private_key,
                            base::size(private_key)));
  ASSERT_EQ(static_cast<int>(base::size(private_key)),
            base::WriteFile(src_path.AppendASCII("second_key.pem"), private_key,
                            base::size(private_key)));
  // Shouldn't find a key with a different extension.
  ASSERT_EQ(static_cast<int>(base::size(private_key)),
            base::WriteFile(src_path.AppendASCII("key.diff_ext"), private_key,
                            base::size(private_key)));
  // Shouldn't find a key that isn't parsable.
  ASSERT_EQ(static_cast<int>(base::size(private_key)) - 30,
            base::WriteFile(src_path.AppendASCII("unparsable_key.pem"),
                            private_key, base::size(private_key) - 30));
  std::vector<base::FilePath> private_keys =
      file_util::FindPrivateKeyFiles(temp.GetPath());
  EXPECT_EQ(2U, private_keys.size());
  EXPECT_THAT(private_keys,
              testing::Contains(src_path.AppendASCII("a_key.pem")));
  EXPECT_THAT(private_keys,
              testing::Contains(src_path.AppendASCII("second_key.pem")));
}

TEST_F(FileUtilTest, WarnOnPrivateKey) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath ext_path = temp.GetPath().AppendASCII("ext_root");
  ASSERT_TRUE(base::CreateDirectory(ext_path));

  const char manifest[] =
      "{\n"
      "  \"name\": \"Test Extension\",\n"
      "  \"version\": \"1.0\",\n"
      "  \"manifest_version\": 2,\n"
      "  \"description\": \"The first extension that I made.\"\n"
      "}\n";
  ASSERT_EQ(static_cast<int>(strlen(manifest)),
            base::WriteFile(ext_path.AppendASCII("manifest.json"), manifest,
                            strlen(manifest)));
  ASSERT_EQ(static_cast<int>(strlen(private_key)),
            base::WriteFile(ext_path.AppendASCII("a_key.pem"), private_key,
                            strlen(private_key)));

  std::string error;
  scoped_refptr<Extension> extension(
      file_util::LoadExtension(ext_path,
                               "the_id",
                               Manifest::EXTERNAL_PREF,
                               Extension::NO_FLAGS,
                               &error));
  ASSERT_TRUE(extension.get()) << error;
  ASSERT_EQ(1u, extension->install_warnings().size());
  EXPECT_THAT(extension->install_warnings(),
              testing::ElementsAre(testing::Field(
                  &InstallWarning::message,
                  testing::ContainsRegex(
                      "extension includes the key file.*ext_root.a_key.pem"))));

  // Turn the warning into an error with ERROR_ON_PRIVATE_KEY.
  extension = file_util::LoadExtension(ext_path,
                                       "the_id",
                                       Manifest::EXTERNAL_PREF,
                                       Extension::ERROR_ON_PRIVATE_KEY,
                                       &error);
  EXPECT_FALSE(extension.get());
  EXPECT_THAT(error,
              testing::ContainsRegex(
                  "extension includes the key file.*ext_root.a_key.pem"));
}

// Try to install an extension with a zero-length icon file.
TEST_F(FileUtilTest, CheckZeroLengthAndMissingIconFile) {
  base::FilePath install_dir;
  ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &install_dir));

  base::FilePath ext_dir =
      install_dir.AppendASCII("file_util").AppendASCII("bad_icon");

  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      ext_dir, Manifest::INTERNAL, Extension::NO_FLAGS, &error));
  ASSERT_FALSE(extension);
}

// Try to install an unpacked extension with a zero-length icon file.
TEST_F(FileUtilTest, CheckZeroLengthAndMissingIconFileUnpacked) {
  base::FilePath install_dir;
  ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &install_dir));

  base::FilePath ext_dir =
      install_dir.AppendASCII("file_util").AppendASCII("bad_icon");

  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      ext_dir, Manifest::UNPACKED, Extension::NO_FLAGS, &error));
  EXPECT_FALSE(extension);
  EXPECT_EQ("Could not load extension icon 'missing-icon.png'.", error);
}

// Try to install an unpacked extension with an invisible icon. This
// should fail.
TEST_F(FileUtilTest, CheckInvisibleIconFileUnpacked) {
  base::FilePath install_dir;
  ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &install_dir));

  base::FilePath ext_dir =
      install_dir.AppendASCII("file_util").AppendASCII("invisible_icon");

  // Set the flag that enables the error.
  file_util::SetReportErrorForInvisibleIconForTesting(true);
  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      ext_dir, Manifest::UNPACKED, Extension::NO_FLAGS, &error));
  file_util::SetReportErrorForInvisibleIconForTesting(false);
  EXPECT_FALSE(extension);
  EXPECT_EQ("The icon is not sufficiently visible 'invisible_icon.png'.",
            error);
}

// Try to install a packed extension with an invisible icon. This should
// succeed.
TEST_F(FileUtilTest, CheckInvisibleIconFilePacked) {
  base::FilePath install_dir;
  ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &install_dir));

  base::FilePath ext_dir =
      install_dir.AppendASCII("file_util").AppendASCII("invisible_icon");

  // Set the flag that enables the error.
  file_util::SetReportErrorForInvisibleIconForTesting(true);
  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      ext_dir, Manifest::INTERNAL, Extension::NO_FLAGS, &error));
  file_util::SetReportErrorForInvisibleIconForTesting(false);
  EXPECT_TRUE(extension);
  EXPECT_TRUE(error.empty());
}

TEST_F(FileUtilTest, ExtensionURLToRelativeFilePath) {
#define URL_PREFIX "chrome-extension://extension-id/"
  struct TestCase {
    const char* url;
    const char* expected_relative_path;
  } test_cases[] = {
    {URL_PREFIX "simple.html", "simple.html"},
    {URL_PREFIX "directory/to/file.html", "directory/to/file.html"},
    {URL_PREFIX "escape%20spaces.html", "escape spaces.html"},
    {URL_PREFIX "%C3%9Cber.html",
     "\xC3\x9C"
     "ber.html"},
#if defined(OS_WIN)
    {URL_PREFIX "C%3A/simple.html", ""},
#endif
    {URL_PREFIX "////simple.html", "simple.html"},
    {URL_PREFIX "/simple.html", "simple.html"},
    {URL_PREFIX "\\simple.html", "simple.html"},
    {URL_PREFIX "\\\\foo\\simple.html", "foo/simple.html"},
    // Escaped file paths result in failure.
    {URL_PREFIX "..%2f..%2fsimple.html", ""},
    // Escaped things that look like escaped file paths, on the other hand,
    // should work.
    {URL_PREFIX "..%252f..%252fsimple.html", "..%2f..%2fsimple.html"},
    // This is a UTF-8 lock icon, which is unsafe to display in the omnibox, but
    // is a valid, if unusual, file name.
    {URL_PREFIX "%F0%9F%94%93.html", "\xF0\x9F\x94\x93.html"},
  };
#undef URL_PREFIX

  for (size_t i = 0; i < base::size(test_cases); ++i) {
    GURL url(test_cases[i].url);
    base::FilePath expected_path =
        base::FilePath::FromUTF8Unsafe(test_cases[i].expected_relative_path);
    base::FilePath actual_path = file_util::ExtensionURLToRelativeFilePath(url);
    EXPECT_FALSE(actual_path.IsAbsolute()) <<
      " For the path " << actual_path.value();
    EXPECT_EQ(expected_path.value(), actual_path.value()) <<
      " For the path " << url;
  }
}

}  // namespace extensions
