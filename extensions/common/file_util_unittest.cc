// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/common/file_util.h"

#include <stddef.h>

#include <optional>
#include <string_view>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/values_test_util.h"
#include "base/types/optional_ref.h"
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

using extensions::mojom::ManifestLocation;

namespace extensions {

namespace {

constexpr char kManifestContent[] =
    R"({
         "name": "Underscore folder test",
         "version": "1.0",
         "manifest_version": 3
       })";
constexpr char kExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

const char kCustomManifest[] = "custom_manifest.json";
const base::FilePath::CharType kCustomManifestFilename[] =
    FILE_PATH_LITERAL("custom_manifest.json");

scoped_refptr<Extension> LoadExtensionManifest(
    const base::Value::Dict& manifest,
    const base::FilePath& manifest_dir,
    ManifestLocation location,
    int extra_flags,
    std::string* error) {
  scoped_refptr<Extension> extension =
      Extension::Create(manifest_dir, location, manifest, extra_flags, error);
  return extension;
}

scoped_refptr<Extension> LoadExtensionManifest(
    const std::string& manifest_value,
    const base::FilePath& manifest_dir,
    ManifestLocation location,
    int extra_flags,
    std::string* error) {
  JSONStringValueDeserializer deserializer(manifest_value);
  std::unique_ptr<base::Value> result =
      deserializer.Deserialize(nullptr, error);
  if (!result.get()) {
    return nullptr;
  }
  CHECK_EQ(base::Value::Type::DICT, result->type());
  return LoadExtensionManifest(std::move(*result).TakeDict(), manifest_dir,
                               location, extra_flags, error);
}

void RunUnderscoreDirectoriesTest(
    const std::vector<std::string>& underscore_directories) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath ext_path = temp.GetPath();
  ASSERT_TRUE(base::CreateDirectory(ext_path));

  for (const auto& dir : underscore_directories) {
    ASSERT_TRUE(base::CreateDirectory(ext_path.AppendASCII(dir)));
  }

  ASSERT_TRUE(
      base::WriteFile(ext_path.AppendASCII("manifest.json"), kManifestContent));

  std::string error;
  scoped_refptr<Extension> extension = file_util::LoadExtension(
      ext_path, ManifestLocation::kUnpacked, Extension::NO_FLAGS, &error);
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
    if (expected_warning == warnings[0].message) {
      warning_matched = true;
    }
  }

  EXPECT_TRUE(warning_matched)
      << "Correct warning not generated for an unpacked extension with "
      << base::JoinString(underscore_directories, ",") << " directories.";
}

struct UninstallTestData {
  std::optional<const base::FilePath> profile_dir;
  std::optional<const base::FilePath> extensions_install_dir;
  std::optional<const base::FilePath> extension_dir_to_delete;
  bool extension_directory_deleted;
};

const std::vector<UninstallTestData>& GetTestData() {
  // TODO(crbug.com/40875193): Condense/enhance with testing::Combine to try all
  // permutations of known bad values.
  static const auto* test_data = new std::vector<UninstallTestData>{
      // Valid directory.
      {/*profile_dir=*/std::nullopt,
       /*extensions_install_dir=*/std::nullopt,
       /*extension_dir_to_delete=*/std::nullopt,
       /*extension_directory_deleted=*/true},

      // Empty profile directory.
      {/*profile_dir=*/base::FilePath(),
       /*extensions_install_dir=*/std::nullopt,
       /*extension_dir_to_delete=*/std::nullopt,
       /*extension_directory_deleted=*/false},
      // Empty extensions directory.
      {/*profile_dir=*/std::nullopt,
       /*extensions_install_dir=*/base::FilePath(),
       /*extension_dir_to_delete=*/std::nullopt,
       /*extension_directory_deleted=*/false},
      // Empty extensions directory to delete.
      {/*profile_dir=*/std::nullopt,
       /*extensions_install_dir=*/std::nullopt, base::FilePath(),
       /*extension_directory_deleted=*/false},

      // Nonabsolute profile directory.
      {/*profile_dir=*/base::FilePath(FILE_PATH_LITERAL("not/absolutepath")),
       /*extensions_install_dir=*/std::nullopt,
       /*extension_dir_to_delete=*/std::nullopt,
       /*extension_directory_deleted=*/false},
      // Nonabsolute extensions directory.
      {/*profile_dir=*/std::nullopt,
       /*extensions_install_dir=*/
       base::FilePath(FILE_PATH_LITERAL("not/absolutepath")),
       /*extension_dir_to_delete=*/std::nullopt,
       /*extension_directory_deleted=*/false},
      // Nonabsolute extensions directory to delete.
      {/*profile_dir=*/std::nullopt,
       /*extensions_install_dir=*/std::nullopt,
       /*extension_dir_to_delete=*/
       base::FilePath(FILE_PATH_LITERAL("not/absolutepath")),
       /*extension_directory_deleted=*/false},

      // Dangerous extensions directory to delete values.
      {/*profile_dir=*/std::nullopt,
       /*extensions_install_dir=*/std::nullopt,
       /*extension_dir_to_delete=*/base::FilePath(FILE_PATH_LITERAL(".")),
       /*extension_directory_deleted=*/false},
      {/*profile_dir=*/std::nullopt,
       /*extensions_install_dir=*/std::nullopt,
       /*extension_dir_to_delete=*/base::FilePath(FILE_PATH_LITERAL("..")),
       /*extension_directory_deleted=*/false},
      {/*profile_dir=*/std::nullopt,
       /*extensions_install_dir=*/std::nullopt,
       /*extension_dir_to_delete=*/base::FilePath(FILE_PATH_LITERAL("/")),
       /*extension_directory_deleted=*/false},

      // Dangerous profile directory values.
      {/*profile_dir=*/base::FilePath(FILE_PATH_LITERAL(".")),
       /*extensions_install_dir=*/std::nullopt,
       /*extension_dir_to_delete=*/std::nullopt,
       /*extension_directory_deleted=*/false},
      {/*profile_dir=*/base::FilePath(FILE_PATH_LITERAL("..")),
       /*extensions_install_dir=*/std::nullopt,
       /*extension_dir_to_delete=*/std::nullopt,
       /*extension_directory_deleted=*/false},
      {/*profile_dir=*/base::FilePath(FILE_PATH_LITERAL("/")),
       /*extensions_install_dir=*/std::nullopt,
       /*extension_dir_to_delete=*/std::nullopt,
       /*extension_directory_deleted=*/false},

      // Dangerous extensions directory values.
      {/*profile_dir=*/std::nullopt,
       /*extensions_install_dir=*/base::FilePath(FILE_PATH_LITERAL(".")),
       /*extension_dir_to_delete=*/std::nullopt,
       /*extension_directory_deleted=*/false},
      {/*profile_dir=*/std::nullopt,
       /*extensions_install_dir=*/base::FilePath(FILE_PATH_LITERAL("..")),
       /*extension_dir_to_delete=*/std::nullopt,
       /*extension_directory_deleted=*/false},
      {/*profile_dir=*/std::nullopt,
       /*extensions_install_dir=*/base::FilePath(FILE_PATH_LITERAL("/")),
       /*extension_dir_to_delete=*/std::nullopt,
       /*extension_directory_deleted=*/false}};

  return *test_data;
}

}  // namespace

using FileUtilTest = testing::Test;

// Tests that packed extensions have all their versions deleted when the
// extension is uninstalled.
TEST_F(FileUtilTest, UninstallRemovesAllPackedExtensionVersions) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  // Create a source extension.
  std::string version("1.0");
  base::FilePath src = temp.GetPath().AppendASCII(kExtensionId);
  ASSERT_TRUE(base::CreateDirectory(src));

  base::FilePath extension_content;
  base::CreateTemporaryFileInDir(src, &extension_content);
  ASSERT_TRUE(base::PathExists(extension_content));

  // Create a extensions tree.
  base::FilePath profile_dir = temp.GetPath().AppendASCII("Default");
  base::FilePath extensions_dir = profile_dir.AppendASCII("TestExtensions");
  ASSERT_TRUE(base::CreateDirectory(extensions_dir));

  base::FilePath extensions_dir_to_delete =
      extensions_dir.AppendASCII(kExtensionId);

  // Install in empty directory. Should create parent directories as needed.
  base::FilePath version_1 =
      file_util::InstallExtension(src, kExtensionId, version, extensions_dir);
  EXPECT_EQ(version_1.value(),
            extensions_dir_to_delete.AppendASCII("1.0_0").value());
  ASSERT_TRUE(base::DirectoryExists(version_1));
  ASSERT_TRUE(base::PathExists(version_1.Append(extension_content.BaseName())));

  // Should have moved the source.
  ASSERT_FALSE(base::DirectoryExists(src));

  // Install again. Should create a new one with different name.
  ASSERT_TRUE(base::CreateDirectory(src));
  base::FilePath version_2 =
      file_util::InstallExtension(src, kExtensionId, version, extensions_dir);
  EXPECT_EQ(version_2.value(),
            extensions_dir_to_delete.AppendASCII("1.0_1").value());
  ASSERT_TRUE(base::DirectoryExists(version_2));

  // Should have moved the source.
  ASSERT_FALSE(base::DirectoryExists(src));

  // Install yet again. Should create a new one with a different name.
  ASSERT_TRUE(base::CreateDirectory(src));
  base::FilePath version_3 =
      file_util::InstallExtension(src, kExtensionId, version, extensions_dir);
  EXPECT_EQ(version_3.value(),
            extensions_dir_to_delete.AppendASCII("1.0_2").value());
  ASSERT_TRUE(base::DirectoryExists(version_3));

  // Uninstall. Should remove entire extension subtree.
  file_util::UninstallExtension(profile_dir, extensions_dir,
                                extensions_dir_to_delete);
  EXPECT_FALSE(base::DirectoryExists(version_1.DirName()));
  EXPECT_FALSE(base::DirectoryExists(version_2.DirName()));
  EXPECT_FALSE(base::DirectoryExists(version_3.DirName()));
  EXPECT_TRUE(base::DirectoryExists(extensions_dir));
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
      install_dir, ManifestLocation::kUnpacked, Extension::NO_FLAGS, &error));
  ASSERT_TRUE(extension.get() != nullptr);
  EXPECT_EQ("The first extension that I made.", extension->description());
}

TEST_F(FileUtilTest, LoadExtensionWithGzippedLocalesAllowed) {
  base::FilePath install_dir;
  ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &install_dir));
  install_dir = install_dir.AppendASCII("extension_with_gzipped_locales");

  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      install_dir, ManifestLocation::kComponent, Extension::NO_FLAGS, &error));
  ASSERT_TRUE(extension.get() != nullptr);
  EXPECT_EQ("The first extension that I made.", extension->description());
  ASSERT_TRUE(error.empty());
}

TEST_F(FileUtilTest, LoadExtensionWithGzippedLocalesNotAllowed) {
  base::FilePath install_dir;
  ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &install_dir));
  install_dir = install_dir.AppendASCII("extension_with_gzipped_locales");

  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      install_dir, ManifestLocation::kUnpacked, Extension::NO_FLAGS, &error));
  ASSERT_TRUE(extension.get() == nullptr);
  EXPECT_EQ("Catalog file is missing for locale en.", error);
}

TEST_F(FileUtilTest, LoadExtensionWithoutLocalesFolder) {
  base::FilePath install_dir;
  ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &install_dir));
  install_dir = install_dir.AppendASCII("extension_without_locales");

  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      install_dir, ManifestLocation::kUnpacked, Extension::NO_FLAGS, &error));
  ASSERT_FALSE(extension.get() == nullptr);
  EXPECT_TRUE(error.empty());
}

TEST_F(FileUtilTest, CheckIllegalFilenamesNoUnderscores) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath src_path = temp.GetPath().AppendASCII("some_dir");
  ASSERT_TRUE(base::CreateDirectory(src_path));

  std::string data = "{ \"name\": { \"message\": \"foobar\" } }";
  ASSERT_TRUE(base::WriteFile(src_path.AppendASCII("some_file.txt"), data));
  std::string error;
  EXPECT_TRUE(file_util::CheckForIllegalFilenames(temp.GetPath(), &error));
}

TEST_F(FileUtilTest, CheckIllegalFilenamesOnlyReserved) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  static const base::FilePath::CharType* const folders[] = {
      kLocaleFolder, kPlatformSpecificFolder};

  for (size_t i = 0; i < std::size(folders); i++) {
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
#if !BUILDFLAG(IS_WIN)
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
  ASSERT_TRUE(base::WriteFile(src_path.AppendASCII("lpt1.txt"), data));

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
      install_dir, ManifestLocation::kUnpacked, Extension::NO_FLAGS, &error));
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
      install_dir, ManifestLocation::kUnpacked, Extension::NO_FLAGS, &error));
  ASSERT_TRUE(extension.get() == nullptr);
  ASSERT_FALSE(error.empty());
  if (base::JSONReader::UsingRust()) {
    ASSERT_NE(
        std::string::npos,
        error.find(manifest_errors::kManifestParseError +
                   std::string("  expected `,` or `}` at line 2 column 16")));
  } else {
    ASSERT_NE(std::string::npos,
              error.find(manifest_errors::kManifestParseError +
                         std::string("  Line: 2, column: 16,")));
  }
}

TEST_F(FileUtilTest, ValidateThemeUTF8) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  // "aeo" with accents. Use http://0xcc.net/jsescape/ to decode them.
  std::string non_ascii_file = "\xC3\xA0\xC3\xA8\xC3\xB2.png";
  base::FilePath non_ascii_path =
      temp.GetPath().Append(base::FilePath::FromUTF8Unsafe(non_ascii_file));
  base::WriteFile(non_ascii_path, "");

  std::string kManifest = base::StringPrintf(
      "{ \"name\": \"Test\", \"version\": \"1.0\", "
      "  \"theme\": { \"images\": { \"theme_frame\": \"%s\" } }"
      "}",
      non_ascii_file.c_str());
  std::string error;
  scoped_refptr<Extension> extension = LoadExtensionManifest(
      kManifest, temp.GetPath(), ManifestLocation::kUnpacked, 0, &error);
  ASSERT_TRUE(extension.get()) << error;

  std::vector<InstallWarning> warnings;
  EXPECT_TRUE(file_util::ValidateExtension(extension.get(), &error, &warnings))
      << error;
  EXPECT_EQ(0U, warnings.size());
}

TEST_F(FileUtilTest, BackgroundScriptsMustExist) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::Value::Dict value;
  value.Set("name", "test");
  value.Set("version", "1");
  value.Set("manifest_version", 2);

  base::Value::List* scripts =
      value.EnsureDict("background")->EnsureList("scripts");
  scripts->Append("foo.js");

  std::string error;
  std::vector<InstallWarning> warnings;
  scoped_refptr<Extension> extension = LoadExtensionManifest(
      value, temp.GetPath(), ManifestLocation::kUnpacked, 0, &error);
  ASSERT_TRUE(extension.get()) << error;

  EXPECT_FALSE(
      file_util::ValidateExtension(extension.get(), &error, &warnings));
  EXPECT_EQ(l10n_util::GetStringFUTF8(
                IDS_EXTENSION_LOAD_BACKGROUND_SCRIPT_FAILED, u"foo.js"),
            error);
  EXPECT_EQ(0U, warnings.size());

  scripts->clear();
  scripts->Append("http://google.com/foo.js");

  extension = LoadExtensionManifest(value, temp.GetPath(),
                                    ManifestLocation::kUnpacked, 0, &error);
  ASSERT_TRUE(extension.get()) << error;

  warnings.clear();
  EXPECT_FALSE(
      file_util::ValidateExtension(extension.get(), &error, &warnings));
  EXPECT_EQ(
      l10n_util::GetStringFUTF8(IDS_EXTENSION_LOAD_BACKGROUND_SCRIPT_FAILED,
                                u"http://google.com/foo.js"),
      error);
  EXPECT_EQ(0U, warnings.size());
}

// Private key, generated by Chrome specifically for this test, and
// never used elsewhere.
constexpr std::string_view private_key =
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

  ASSERT_TRUE(base::WriteFile(src_path.AppendASCII("a_key.pem"), private_key));
  ASSERT_TRUE(
      base::WriteFile(src_path.AppendASCII("second_key.pem"), private_key));
  // Shouldn't find a key with a different extension.
  ASSERT_TRUE(
      base::WriteFile(src_path.AppendASCII("key.diff_ext"), private_key));
  // Shouldn't find a key that isn't parsable.
  std::string_view private_key_substring =
      private_key.substr(0, private_key.size() - 30);
  ASSERT_TRUE(base::WriteFile(src_path.AppendASCII("unparsable_key.pem"),
                              private_key_substring));
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
  ASSERT_TRUE(base::WriteFile(ext_path.AppendASCII("manifest.json"), manifest));
  ASSERT_TRUE(base::WriteFile(ext_path.AppendASCII("a_key.pem"), private_key));

  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      ext_path, "the_id", ManifestLocation::kExternalPref, Extension::NO_FLAGS,
      &error));
  ASSERT_TRUE(extension.get()) << error;
  ASSERT_EQ(1u, extension->install_warnings().size());
  EXPECT_THAT(extension->install_warnings(),
              testing::ElementsAre(testing::Field(
                  &InstallWarning::message,
                  testing::ContainsRegex(
                      "extension includes the key file.*ext_root.a_key.pem"))));

  // Turn the warning into an error with ERROR_ON_PRIVATE_KEY.
  extension = file_util::LoadExtension(ext_path, "the_id",
                                       ManifestLocation::kExternalPref,
                                       Extension::ERROR_ON_PRIVATE_KEY, &error);
  EXPECT_FALSE(extension.get());
  EXPECT_THAT(error,
              testing::ContainsRegex(
                  "extension includes the key file.*ext_root.a_key.pem"));
}

// Specify a file other than manifest.json
TEST_F(FileUtilTest, SpecifyManifestFile) {
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
  ASSERT_TRUE(base::WriteFile(ext_path.AppendASCII(kCustomManifest), manifest));

  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      ext_path, kCustomManifestFilename, "the_id",
      ManifestLocation::kExternalPref, Extension::NO_FLAGS, &error));
  ASSERT_TRUE(extension.get()) << error;
  ASSERT_EQ(0u, extension->install_warnings().size());
}

// Try to install an extension with a zero-length icon file.
TEST_F(FileUtilTest, CheckZeroLengthAndMissingIconFile) {
  base::FilePath install_dir;
  ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &install_dir));

  base::FilePath ext_dir =
      install_dir.AppendASCII("file_util").AppendASCII("bad_icon");

  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      ext_dir, ManifestLocation::kInternal, Extension::NO_FLAGS, &error));
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
      ext_dir, ManifestLocation::kUnpacked, Extension::NO_FLAGS, &error));
  EXPECT_FALSE(extension);
  EXPECT_EQ("Could not load icon 'missing-icon.png' specified in 'icons'.",
            error);
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
      ext_dir, ManifestLocation::kUnpacked, Extension::NO_FLAGS, &error));
  file_util::SetReportErrorForInvisibleIconForTesting(false);
  EXPECT_FALSE(extension);
  EXPECT_EQ(
      "Icon 'invisible_icon.png' specified in 'icons' is not "
      "sufficiently visible.",
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
      ext_dir, ManifestLocation::kInternal, Extension::NO_FLAGS, &error));
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
#if BUILDFLAG(IS_WIN)
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

  for (size_t i = 0; i < std::size(test_cases); ++i) {
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

class UninstallTest : public testing::Test {
 public:
  UninstallTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(SetupExtensionsDirForUninstall());
  }

 protected:
  // Create a directory in a temp dir that has an extension version folder
  // inside it.
  bool SetupExtensionsDirForUninstall();
  bool ExtensionDirectoryDeleted();
  bool ExtensionDirectoryNotDeleted();

  base::FilePath profile_dir_;
  base::FilePath extensions_install_dir_;
  base::FilePath extension_dir_to_delete_;
  base::FilePath extension_id_dir_;
  base::FilePath extension_version_dir_;

 private:
  base::ScopedTempDir temp_dir_;
};

bool UninstallTest::SetupExtensionsDirForUninstall() {
  profile_dir_ = temp_dir_.GetPath().AppendASCII("Default");
  extensions_install_dir_ = profile_dir_.AppendASCII("TestExtensions");
  extension_id_dir_ = extensions_install_dir_.AppendASCII(kExtensionId);
  std::string version("1.0_0");
  extension_version_dir_ = extension_id_dir_.AppendASCII(version);
  base::CreateDirectory(profile_dir_);
  base::CreateDirectory(extensions_install_dir_);
  base::CreateDirectory(extension_id_dir_);
  base::CreateDirectory(extension_version_dir_);
  return base::DirectoryExists(extension_version_dir_);
}

bool UninstallTest::ExtensionDirectoryDeleted() {
  return base::DirectoryExists(extensions_install_dir_) &&
         !base::DirectoryExists(extension_id_dir_);
}

bool UninstallTest::ExtensionDirectoryNotDeleted() {
  return base::DirectoryExists(extension_version_dir_);
}

class UninstallTestParameterized
    : public UninstallTest,
      public testing::WithParamInterface<UninstallTestData> {
 public:
  UninstallTestParameterized() = default;

  void SetUp() override {
    UninstallTest::SetUp();

    // Overrides with parameterized values.
    if (GetParam().profile_dir.has_value()) {
      profile_dir_ = GetParam().profile_dir.value();
    }
    if (GetParam().extensions_install_dir.has_value()) {
      extensions_install_dir_ = GetParam().extensions_install_dir.value();
    }
    if (GetParam().extension_dir_to_delete.has_value()) {
      extension_id_dir_ = GetParam().extension_dir_to_delete.value();
    }
  }
};

// TODO(crbug.com/40875193): Create a custom test name generator that is more
// readable.
// go/gunitadvanced#specifying-names-for-value-parameterized-test-parameters
INSTANTIATE_TEST_SUITE_P(All,
                         UninstallTestParameterized,
                         testing::ValuesIn(GetTestData()));

TEST_P(UninstallTestParameterized, UninstallDirectory) {
  file_util::UninstallExtension(profile_dir_, extensions_install_dir_,
                                /*extension_dir_to_delete=*/extension_id_dir_);
  if (GetParam().extension_directory_deleted) {
    EXPECT_TRUE(ExtensionDirectoryDeleted());
  } else {
    EXPECT_TRUE(ExtensionDirectoryNotDeleted());
  }
}

// Tests when the extensions install directory is outside of the profile
// directory.
TEST_F(UninstallTest,
       UninstallDirectory_ExtensionsInstallDirNotSubdirOfProfileDir) {
  file_util::UninstallExtension(profile_dir_,
                                /*extensions_install_dir=*/
                                profile_dir_.AppendASCII("OutsideProfileDir"),
                                /*extension_dir_to_delete=*/extension_id_dir_);
  EXPECT_TRUE(ExtensionDirectoryNotDeleted());
}

// Tests when the extension directory to delete is outside of the extensions
// install directory.
TEST_F(
    UninstallTest,
    UninstallDirectory_ExtensionsDirToDeleteNotSubdirOfExtensionsInstallDir) {
  file_util::UninstallExtension(
      profile_dir_, extensions_install_dir_,
      /*extension_dir_to_delete=*/
      extensions_install_dir_.AppendASCII("OutsideExtensionsInstallDir"));
  EXPECT_TRUE(ExtensionDirectoryNotDeleted());
}

}  // namespace extensions
