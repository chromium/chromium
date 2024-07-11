// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/common/manifest_test.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"

using extensions::mojom::ManifestLocation;

namespace extensions {
namespace {

std::string GetNameFromManifest(const base::Value::Dict& manifest) {
  const std::string* name = manifest.FindString(manifest_keys::kName);
  return name ? *name : std::string();
}

// |manifest_path| is an absolute path to a manifest file.
std::optional<base::Value::Dict> LoadManifestFile(
    const base::FilePath& manifest_path,
    std::string* error) {
  base::FilePath extension_path = manifest_path.DirName();

  EXPECT_TRUE(base::PathExists(manifest_path)) <<
      "Couldn't find " << manifest_path.value();

  JSONFileValueDeserializer deserializer(manifest_path);
  std::unique_ptr<base::Value> manifest =
      deserializer.Deserialize(nullptr, error);

  if (!manifest || !manifest->is_dict()) {
    return std::nullopt;
  }

  // Most unit tests don't need localization, and they'll fail if we try to
  // localize them, since their manifests don't have a default_locale key.
  // Only localize manifests that indicate they want to be localized.
  // Calling LocalizeExtension at this point mirrors file_util::LoadExtension.
  if (base::Contains(manifest_path.value(), FILE_PATH_LITERAL("localized"))) {
    extension_l10n_util::LocalizeExtension(
        extension_path, manifest->GetIfDict(),
        extension_l10n_util::GzippedMessagesPermission::kDisallow, error);
  }

  return std::move(*manifest).TakeDict();
}

}  // namespace

ManifestTest::ManifestTest()
    : enable_apps_(true) {
}

ManifestTest::~ManifestTest() = default;

// Helper class that simplifies creating methods that take either a filename
// to a manifest or the manifest itself.
ManifestTest::ManifestData::ManifestData(std::string_view name) : name_(name) {}

ManifestTest::ManifestData::ManifestData(base::Value::Dict manifest,
                                         std::string_view name)
    : name_(name), manifest_(std::move(manifest)) {}

ManifestTest::ManifestData::ManifestData(base::Value::Dict manifest)
    : name_(GetNameFromManifest(manifest)), manifest_(std::move(manifest)) {}

ManifestTest::ManifestData::ManifestData(ManifestData&& other) = default;
ManifestTest::ManifestData::~ManifestData() = default;

const std::optional<base::Value::Dict>& ManifestTest::ManifestData::GetManifest(
    const base::FilePath& test_data_dir,
    std::string* error) const {
  if (!manifest_) {
    base::FilePath manifest_path = test_data_dir.AppendASCII(name_);
    manifest_ = LoadManifestFile(manifest_path, error);
  }
  return manifest_;
}

// static
ManifestTest::ManifestData ManifestTest::ManifestData::FromJSON(
    std::string_view json) {
  // ParseJsonDict() will ADD_FAILURE() if `json` is not a valid dict.
  base::Value::Dict manifest_dict = base::test::ParseJsonDict(json);
  return ManifestData(std::move(manifest_dict));
}

std::string ManifestTest::GetTestExtensionID() const {
  return std::string();
}

base::FilePath ManifestTest::GetTestDataDir() {
  base::FilePath path;
  base::PathService::Get(DIR_TEST_DATA, &path);
  return path.AppendASCII("manifest_tests");
}

std::optional<base::Value::Dict> ManifestTest::LoadManifest(
    char const* manifest_name,
    std::string* error) {
  base::FilePath manifest_path = GetTestDataDir().AppendASCII(manifest_name);
  return LoadManifestFile(manifest_path, error);
}

scoped_refptr<Extension> ManifestTest::LoadExtension(
    const ManifestData& manifest,
    std::string* error,
    ManifestLocation location,
    int flags) {
  base::FilePath test_data_dir = GetTestDataDir();
  const std::optional<base::Value::Dict>& dict =
      manifest.GetManifest(test_data_dir, error);
  if (!dict) {
    return nullptr;
  }
  return Extension::Create(test_data_dir.DirName(), location, *dict, flags,
                           GetTestExtensionID(), error);
}

scoped_refptr<Extension> ManifestTest::LoadAndExpectSuccess(
    const ManifestData& manifest,
    ManifestLocation location,
    int flags) {
  std::string error;
  scoped_refptr<Extension> extension =
      LoadExtension(manifest, &error, location, flags);
  EXPECT_TRUE(extension.get()) << manifest.name();
  EXPECT_EQ(std::string(), error) << manifest.name();
  return extension;
}

scoped_refptr<Extension> ManifestTest::LoadAndExpectSuccess(
    char const* manifest_name,
    ManifestLocation location,
    int flags) {
  return LoadAndExpectSuccess(ManifestData(manifest_name), location, flags);
}

scoped_refptr<Extension> ManifestTest::LoadAndExpectWarning(
    const ManifestData& manifest,
    const std::string& expected_warning,
    ManifestLocation location,
    int flags) {
  std::string error;
  scoped_refptr<Extension> extension =
      LoadExtension(manifest, &error, location, flags);
  EXPECT_TRUE(extension.get()) << manifest.name();
  EXPECT_EQ(std::string(), error) << manifest.name();
  EXPECT_EQ(1u, extension->install_warnings().size());
  if (extension->install_warnings().size() == 1) {
    EXPECT_EQ(expected_warning, extension->install_warnings()[0].message);
  }
  return extension;
}

scoped_refptr<Extension> ManifestTest::LoadAndExpectWarning(
    char const* manifest_name,
    const std::string& expected_warning,
    ManifestLocation location,
    int flags) {
  return LoadAndExpectWarning(
      ManifestData(manifest_name), expected_warning, location, flags);
}

scoped_refptr<Extension> ManifestTest::LoadAndExpectWarnings(
    const ManifestData& manifest,
    const std::vector<std::string>& expected_warnings,
    ManifestLocation location,
    int flags) {
  std::string error;
  scoped_refptr<Extension> extension =
      LoadExtension(manifest, &error, location, flags);
  EXPECT_TRUE(extension) << manifest.name();
  EXPECT_EQ(std::string(), error) << manifest.name();
  EXPECT_EQ(expected_warnings.size(), extension->install_warnings().size());

  std::vector<std::string> warning_messages;
  warning_messages.reserve(extension->install_warnings().size());
  for (const auto& warning : extension->install_warnings()) {
    warning_messages.push_back(warning.message);
  }

  EXPECT_THAT(warning_messages,
              testing::UnorderedElementsAreArray(expected_warnings));
  return extension;
}

scoped_refptr<Extension> ManifestTest::LoadAndExpectWarnings(
    char const* manifest_name,
    const std::vector<std::string>& expected_warnings,
    ManifestLocation location,
    int flags) {
  std::string error;
  scoped_refptr<Extension> extension =
      LoadExtension(ManifestData(manifest_name), &error, location, flags);
  EXPECT_TRUE(extension.get()) << manifest_name;
  EXPECT_EQ(std::string(), error) << manifest_name;

  std::vector<std::string> warning_messages;
  warning_messages.reserve(extension->install_warnings().size());
  for (const auto& warning : extension->install_warnings()) {
    warning_messages.push_back(warning.message);
  }

  EXPECT_THAT(warning_messages,
              testing::UnorderedElementsAreArray(expected_warnings));
  return extension;
}

void ManifestTest::VerifyExpectedError(
    Extension* extension,
    const std::string& name,
    const std::string& error,
    const std::string& expected_error) {
  EXPECT_FALSE(extension) <<
      "Expected failure loading extension '" << name <<
      "', but didn't get one.";
  EXPECT_TRUE(base::MatchPattern(error, expected_error))
      << name << " expected '" << expected_error << "' but got '" << error
      << "'";
}

void ManifestTest::LoadAndExpectError(const ManifestData& manifest,
                                      const std::string& expected_error,
                                      ManifestLocation location,
                                      int flags) {
  std::string error;
  scoped_refptr<Extension> extension(
      LoadExtension(manifest, &error, location, flags));
  VerifyExpectedError(extension.get(), manifest.name(), error,
                      expected_error);
}

void ManifestTest::LoadAndExpectError(const ManifestData& manifest,
                                      const std::u16string& expected_error,
                                      ManifestLocation location,
                                      int flags) {
  return LoadAndExpectError(manifest, base::UTF16ToUTF8(expected_error),
                            location, flags);
}

void ManifestTest::LoadAndExpectError(char const* manifest_name,
                                      const std::string& expected_error,
                                      ManifestLocation location,
                                      int flags) {
  return LoadAndExpectError(
      ManifestData(manifest_name), expected_error, location, flags);
}

void ManifestTest::LoadAndExpectError(char const* manifest_name,
                                      const std::u16string& expected_error,
                                      ManifestLocation location,
                                      int flags) {
  return LoadAndExpectError(ManifestData(manifest_name),
                            base::UTF16ToUTF8(expected_error), location, flags);
}

void ManifestTest::AddPattern(extensions::URLPatternSet* extent,
                              const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  extent->AddPattern(URLPattern(schemes, pattern));
}

ManifestTest::Testcase::Testcase(const std::string& manifest_filename,
                                 const std::string& expected_error,
                                 ManifestLocation location,
                                 int flags)
    : manifest_filename_(manifest_filename),
      expected_error_(expected_error),
      location_(location),
      flags_(flags) {}

ManifestTest::Testcase::Testcase(const std::string& manifest_filename,
                                 const std::u16string& expected_error,
                                 ManifestLocation location,
                                 int flags)
    : Testcase(manifest_filename,
               base::UTF16ToUTF8(expected_error),
               location,
               flags) {}

ManifestTest::Testcase::Testcase(const std::string& manifest_filename,
                                 const std::string& expected_error)
    : manifest_filename_(manifest_filename),
      expected_error_(expected_error),
      location_(ManifestLocation::kInternal),
      flags_(Extension::NO_FLAGS) {}

ManifestTest::Testcase::Testcase(const std::string& manifest_filename,
                                 const std::u16string& expected_error)
    : Testcase(manifest_filename, base::UTF16ToUTF8(expected_error)) {}

ManifestTest::Testcase::Testcase(const std::string& manifest_filename)
    : manifest_filename_(manifest_filename),
      location_(ManifestLocation::kInternal),
      flags_(Extension::NO_FLAGS) {}

ManifestTest::Testcase::Testcase(const std::string& manifest_filename,
                                 ManifestLocation location,
                                 int flags)
    : manifest_filename_(manifest_filename),
      location_(location),
      flags_(flags) {}

void ManifestTest::RunTestcases(const Testcase* testcases,
                                size_t num_testcases,
                                ExpectType type) {
  for (size_t i = 0; i < num_testcases; ++i) {
    RunTestcase(testcases[i], type);
  }
}

void ManifestTest::RunTestcase(const Testcase& testcase, ExpectType type) {
  SCOPED_TRACE(base::StringPrintf("Testing file '%s'",
                                  testcase.manifest_filename_.c_str()));

  switch (type) {
    case EXPECT_TYPE_ERROR:
      LoadAndExpectError(testcase.manifest_filename_.c_str(),
                         testcase.expected_error_,
                         testcase.location_,
                         testcase.flags_);
      break;
    case EXPECT_TYPE_WARNING:
      LoadAndExpectWarning(testcase.manifest_filename_.c_str(),
                           testcase.expected_error_,
                           testcase.location_,
                           testcase.flags_);
      break;
    case EXPECT_TYPE_SUCCESS:
      LoadAndExpectSuccess(testcase.manifest_filename_.c_str(),
                           testcase.location_,
                           testcase.flags_);
      break;
   }
}

}  // namespace extensions
