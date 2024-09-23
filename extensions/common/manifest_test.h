// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_TEST_H_
#define EXTENSIONS_COMMON_MANIFEST_TEST_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class FilePath;
}

namespace extensions {

// Base class for tests that parse a manifest file.
class ManifestTest : public testing::Test {
 public:
  ManifestTest();

  ManifestTest(const ManifestTest&) = delete;
  ManifestTest& operator=(const ManifestTest&) = delete;

  ~ManifestTest() override;

 protected:
  // Helper class that simplifies creating methods that take either a filename
  // to a manifest or the manifest itself.
  class ManifestData {
   public:
    explicit ManifestData(std::string_view name);
    explicit ManifestData(base::Value::Dict manifest);
    ManifestData(base::Value::Dict manifest, std::string_view name);
    ManifestData(ManifestData&& other);
    ~ManifestData();

    // Constructs a ManifestData object from the given `json` string.
    // Calls ADD_FAILURE() if `json` is not valid JSON.
    static ManifestData FromJSON(std::string_view json);

    const std::string& name() const { return name_; }

    const std::optional<base::Value::Dict>& GetManifest(
        const base::FilePath& manifest_path,
        std::string* error) const;

   private:
    const std::string name_;
    mutable std::optional<base::Value::Dict> manifest_;
  };

  // Allows the test implementation to override a loaded test manifest's
  // extension ID. Useful for testing features behind a allowlist.
  virtual std::string GetTestExtensionID() const;

  // Returns the path in which to find test manifest data files, for example
  // extensions/test/data/manifest_tests.
  virtual base::FilePath GetTestDataDir();

  std::optional<base::Value::Dict> LoadManifest(char const* manifest_name,
                                                std::string* error);

  scoped_refptr<extensions::Extension> LoadExtension(
      const ManifestData& manifest,
      std::string* error,
      mojom::ManifestLocation location = mojom::ManifestLocation::kInternal,
      int flags = extensions::Extension::NO_FLAGS);

  scoped_refptr<extensions::Extension> LoadAndExpectSuccess(
      const ManifestData& manifest,
      mojom::ManifestLocation location = mojom::ManifestLocation::kInternal,
      int flags = extensions::Extension::NO_FLAGS);

  scoped_refptr<extensions::Extension> LoadAndExpectSuccess(
      char const* manifest_name,
      mojom::ManifestLocation location = mojom::ManifestLocation::kInternal,
      int flags = extensions::Extension::NO_FLAGS);

  scoped_refptr<extensions::Extension> LoadAndExpectWarning(
      const ManifestData& manifest,
      const std::string& expected_warning,
      mojom::ManifestLocation location = mojom::ManifestLocation::kInternal,
      int flags = extensions::Extension::NO_FLAGS);

  scoped_refptr<extensions::Extension> LoadAndExpectWarning(
      char const* manifest_name,
      const std::string& expected_warning,
      mojom::ManifestLocation location = mojom::ManifestLocation::kInternal,
      int flags = extensions::Extension::NO_FLAGS);

  scoped_refptr<Extension> LoadAndExpectWarnings(
      const ManifestData& manifest,
      const std::vector<std::string>& expected_warnings,
      mojom::ManifestLocation location = mojom::ManifestLocation::kInternal,
      int flags = extensions::Extension::NO_FLAGS);

  scoped_refptr<extensions::Extension> LoadAndExpectWarnings(
      char const* manifest_name,
      const std::vector<std::string>& expected_warnings,
      mojom::ManifestLocation location = mojom::ManifestLocation::kInternal,
      int flags = extensions::Extension::NO_FLAGS);

  void VerifyExpectedError(extensions::Extension* extension,
                           const std::string& name,
                           const std::string& error,
                           const std::string& expected_error);

  void LoadAndExpectError(
      char const* manifest_name,
      const std::string& expected_error,
      mojom::ManifestLocation location = mojom::ManifestLocation::kInternal,
      int flags = extensions::Extension::NO_FLAGS);

  void LoadAndExpectError(
      char const* manifest_name,
      const std::u16string& expected_error,
      mojom::ManifestLocation location = mojom::ManifestLocation::kInternal,
      int flags = extensions::Extension::NO_FLAGS);

  void LoadAndExpectError(
      const ManifestData& manifest,
      const std::string& expected_error,
      mojom::ManifestLocation location = mojom::ManifestLocation::kInternal,
      int flags = extensions::Extension::NO_FLAGS);

  void LoadAndExpectError(
      const ManifestData& manifest,
      const std::u16string& expected_error,
      mojom::ManifestLocation location = mojom::ManifestLocation::kInternal,
      int flags = extensions::Extension::NO_FLAGS);

  void AddPattern(extensions::URLPatternSet* extent,
                  const std::string& pattern);

  // used to differentiate between calls to LoadAndExpectError,
  // LoadAndExpectWarning and LoadAndExpectSuccess via function RunTestcases.
  enum ExpectType {
    EXPECT_TYPE_ERROR,
    EXPECT_TYPE_WARNING,
    EXPECT_TYPE_SUCCESS
  };

  struct Testcase {
    const std::string manifest_filename_;
    std::string expected_error_;  // only used for ExpectedError tests
    mojom::ManifestLocation location_;
    int flags_;

    Testcase(const std::string& manifest_filename,
             const std::string& expected_error,
             mojom::ManifestLocation location,
             int flags);

    Testcase(const std::string& manifest_filename,
             const std::u16string& expected_error,
             mojom::ManifestLocation location,
             int flags);

    Testcase(const std::string& manifest_filename,
             const std::string& expected_error);

    Testcase(const std::string& manifest_filename,
             const std::u16string& expected_error);

    explicit Testcase(const std::string& manifest_filename);

    Testcase(const std::string& manifest_filename,
             mojom::ManifestLocation location,
             int flags);
  };

  void RunTestcases(const Testcase* testcases,
                    size_t num_testcases,
                    ExpectType type);

  void RunTestcase(const Testcase& testcase, ExpectType type);

  bool enable_apps_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_TEST_H_
