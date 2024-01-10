// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/csp_info.h"

#include <string_view>

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "components/version_info/channel.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_test.h"

namespace extensions {

namespace {

namespace errors = manifest_errors;
namespace keys = manifest_keys;

std::string GetInvalidManifestKeyError(std::string_view key) {
  return ErrorUtils::FormatErrorMessage(errors::kInvalidManifestKey, key);
}

const char kDefaultSandboxedPageCSP[] =
    "sandbox allow-scripts allow-forms allow-popups allow-modals; "
    "script-src 'self' 'unsafe-inline' 'unsafe-eval'; child-src 'self';";
const char kDefaultExtensionPagesCSP[] =
    "script-src 'self' blob: filesystem:; "
    "object-src 'self' blob: filesystem:;";
const char kDefaultSecureCSP[] = "script-src 'self';";

}  // namespace

using CSPInfoUnitTest = ManifestTest;

TEST_F(CSPInfoUnitTest, SandboxedPages) {
  // Sandboxed pages specified, no custom CSP value.
  scoped_refptr<Extension> extension1(
      LoadAndExpectSuccess("sandboxed_pages_valid_1.json"));

  // No sandboxed pages.
  scoped_refptr<Extension> extension2(
      LoadAndExpectSuccess("sandboxed_pages_valid_2.json"));

  // Sandboxed pages specified with a custom CSP value.
  scoped_refptr<Extension> extension3(
      LoadAndExpectSuccess("sandboxed_pages_valid_3.json"));

  // Sandboxed pages specified with wildcard, no custom CSP value.
  scoped_refptr<Extension> extension4(
      LoadAndExpectSuccess("sandboxed_pages_valid_4.json"));

  // Sandboxed pages specified with filename wildcard, no custom CSP value.
  scoped_refptr<Extension> extension5(
      LoadAndExpectSuccess("sandboxed_pages_valid_5.json"));

  // Sandboxed pages specified for a platform app with a custom CSP.
  scoped_refptr<Extension> extension6(
      LoadAndExpectSuccess("sandboxed_pages_valid_6.json"));

  // Sandboxed pages specified for a platform app with no custom CSP.
  scoped_refptr<Extension> extension7(
      LoadAndExpectSuccess("sandboxed_pages_valid_7.json"));

  const char kCustomSandboxedCSP[] =
      "sandbox; script-src 'self'; child-src 'self';";

  EXPECT_EQ(kDefaultSandboxedPageCSP, CSPInfo::GetResourceContentSecurityPolicy(
                                          extension1.get(), "/test"));
  EXPECT_EQ(
      kDefaultExtensionPagesCSP,
      CSPInfo::GetResourceContentSecurityPolicy(extension1.get(), "/none"));
  EXPECT_EQ(
      kDefaultExtensionPagesCSP,
      CSPInfo::GetResourceContentSecurityPolicy(extension2.get(), "/test"));
  EXPECT_EQ(kCustomSandboxedCSP, CSPInfo::GetResourceContentSecurityPolicy(
                                     extension3.get(), "/test"));
  EXPECT_EQ(
      kDefaultExtensionPagesCSP,
      CSPInfo::GetResourceContentSecurityPolicy(extension3.get(), "/none"));
  EXPECT_EQ(kDefaultSandboxedPageCSP, CSPInfo::GetResourceContentSecurityPolicy(
                                          extension4.get(), "/test"));
  EXPECT_EQ(kDefaultSandboxedPageCSP, CSPInfo::GetResourceContentSecurityPolicy(
                                          extension5.get(), "/path/test.ext"));
  EXPECT_EQ(
      kDefaultExtensionPagesCSP,
      CSPInfo::GetResourceContentSecurityPolicy(extension5.get(), "/test"));
  EXPECT_EQ(kCustomSandboxedCSP, CSPInfo::GetResourceContentSecurityPolicy(
                                     extension6.get(), "/test"));
  EXPECT_EQ(kDefaultSandboxedPageCSP, CSPInfo::GetResourceContentSecurityPolicy(
                                          extension7.get(), "/test"));

  Testcase testcases[] = {
      Testcase("sandboxed_pages_invalid_1.json",
               errors::kInvalidSandboxedPagesList),
      Testcase("sandboxed_pages_invalid_2.json", errors::kInvalidSandboxedPage),
      Testcase("sandboxed_pages_invalid_3.json",
               GetInvalidManifestKeyError(keys::kSandboxedPagesCSP)),
      Testcase("sandboxed_pages_invalid_4.json",
               GetInvalidManifestKeyError(keys::kSandboxedPagesCSP)),
      Testcase("sandboxed_pages_invalid_5.json",
               GetInvalidManifestKeyError(keys::kSandboxedPagesCSP))};
  RunTestcases(testcases, std::size(testcases), EXPECT_TYPE_ERROR);
}

TEST_F(CSPInfoUnitTest, CSPStringKey) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("csp_string_valid.json");
  ASSERT_TRUE(extension);
  EXPECT_EQ("script-src 'self'; default-src 'none';",
            CSPInfo::GetExtensionPagesCSP(extension.get()));

  // Manifest V2 extensions bypass the main world CSP in their isolated worlds.
  const std::string* isolated_world_csp =
      CSPInfo::GetIsolatedWorldCSP(*extension);
  ASSERT_TRUE(isolated_world_csp);
  EXPECT_TRUE(isolated_world_csp->empty());

  RunTestcase(Testcase("csp_invalid_1.json", GetInvalidManifestKeyError(
                                                 keys::kContentSecurityPolicy)),
              EXPECT_TYPE_ERROR);
}

TEST_F(CSPInfoUnitTest, CSPDictionary_ExtensionPages) {
  struct {
    const char* file_name;
    const char* csp;
  } cases[] = {{"csp_dictionary_valid_1.json", "default-src 'none'"},
               {"csp_dictionary_valid_2.json",
                "worker-src 'self'; script-src; default-src 'self'"},
               {"csp_empty_dictionary_valid.json", kDefaultSecureCSP}};

  for (const auto& test_case : cases) {
    SCOPED_TRACE(base::StringPrintf("Testing %s.", test_case.file_name));
    scoped_refptr<Extension> extension =
        LoadAndExpectSuccess(test_case.file_name);
    ASSERT_TRUE(extension.get());
    EXPECT_EQ(test_case.csp, CSPInfo::GetExtensionPagesCSP(extension.get()));
  }

  Testcase testcases[] = {
      Testcase("csp_invalid_2.json",
               GetInvalidManifestKeyError(
                   keys::kContentSecurityPolicy_ExtensionPagesPath)),
      Testcase("csp_invalid_3.json",
               GetInvalidManifestKeyError(
                   keys::kContentSecurityPolicy_ExtensionPagesPath)),
      Testcase(
          "csp_missing_src.json",
          ErrorUtils::FormatErrorMessage(
              errors::kInvalidCSPMissingSecureSrc,
              keys::kContentSecurityPolicy_ExtensionPagesPath, "script-src")),
      Testcase("csp_insecure_src.json",
               ErrorUtils::FormatErrorMessage(
                   errors::kInvalidCSPInsecureValueError,
                   keys::kContentSecurityPolicy_ExtensionPagesPath,
                   "'unsafe-eval'", "worker-src")),
  };
  RunTestcases(testcases, std::size(testcases), EXPECT_TYPE_ERROR);
}

// Tests the requirements for object-src specifications.
TEST_F(CSPInfoUnitTest, ObjectSrcRequirements) {
  enum class ManifestVersion {
    kMV2,
    kMV3,
  };

  static constexpr char kManifestV3Template[] =
      R"({
           "name": "Test Extension",
           "manifest_version": 3,
           "version": "0.1",
           "content_security_policy": {
             "extension_pages": "%s"
           }
         })";

  static constexpr char kManifestV2Template[] =
      R"({
           "name": "Test Extension",
           "manifest_version": 2,
           "version": "0.1",
           "content_security_policy": "%s"
         })";

  auto get_manifest = [](ManifestVersion version, const char* input) {
    return base::test::ParseJsonDict(
        version == ManifestVersion::kMV2
            ? base::StringPrintf(kManifestV2Template, input)
            : base::StringPrintf(kManifestV3Template, input));
  };

  static constexpr struct {
    ManifestVersion version;
    const char* csp;
  } kPassingTestcases[] = {
      // object-src doesn't need to be explicitly specified in manifest V3.
      {ManifestVersion::kMV3, "script-src 'self'"},
      {ManifestVersion::kMV3, "default-src 'self'"},
      // Secure object-src specifications are allowed.
      {ManifestVersion::kMV3, "script-src 'self'; object-src 'self'"},
      {ManifestVersion::kMV3, "script-src 'self'; object-src 'none'"},
      {ManifestVersion::kMV3,
       ("script-src 'self'; object-src 'self'; frame-src 'self'; default-src "
        "https://google.com")},
      // Even though the object-src in the example below is effectively
      // https://google.com (because it falls back to the default-src), we
      // still allow it so that developers don't need to explicitly specify an
      // object-src just because they specified a default-src. The minimum CSP
      // (which includes `object-src 'self'`) still kicks in and prevents any
      // insecure use.
      {ManifestVersion::kMV3,
       "script-src 'self'; default-src https://google.com"},

      // In Manifest V2, object-src must be specified (if it's omitted, we add
      // it; see `kWarningTestcases` below).
      // Note: in MV2, our parsing will implicitly also add a trailing semicolon
      // if one isn't provided, so we always add one here so that the final CSP
      // matches.
      {ManifestVersion::kMV2, "script-src 'self'; object-src 'self';"},
      {ManifestVersion::kMV2, "script-src 'self'; object-src 'none';"},
      {ManifestVersion::kMV2,
       ("script-src 'self'; object-src 'self'; frame-src 'self'; default-src "
        "https://google.com;")},
      // Manifest V2 allows (secure) remote object-src specifications.
      {ManifestVersion::kMV2,
       "script-src 'self'; object-src https://google.com;"},
      {ManifestVersion::kMV2,
       "script-src 'self'; default-src https://google.com;"},
  };

  for (const auto& testcase : kPassingTestcases) {
    SCOPED_TRACE(testcase.csp);
    ManifestData manifest_data(get_manifest(testcase.version, testcase.csp));
    scoped_refptr<const Extension> extension =
        LoadAndExpectSuccess(manifest_data);
    ASSERT_TRUE(extension);
    EXPECT_EQ(testcase.csp, CSPInfo::GetExtensionPagesCSP(extension.get()));
  }

  static constexpr struct {
    ManifestVersion version;
    const char* csp;
    const char* expected_error;
  } kFailingTestcases[] = {
      // If an object-src *is* specified, it must be secure and must not allow
      // remotely-hosted code (in MV3).
      {ManifestVersion::kMV3,
       "script-src 'self'; object-src https://google.com",
       "*Insecure CSP value \"https://google.com\" in directive 'object-src'."},
  };

  for (const auto& testcase : kFailingTestcases) {
    SCOPED_TRACE(testcase.csp);
    ManifestData manifest_data(get_manifest(testcase.version, testcase.csp));
    LoadAndExpectError(manifest_data, testcase.expected_error);
  }

  static constexpr struct {
    ManifestVersion version;
    const char* csp;
    const char* expected_warning;
    const char* effective_csp;
  } kWarningTestcases[] = {
      // In MV2, if an object-src is not provided, we will warn and synthesize
      // one.
      {ManifestVersion::kMV2, "script-src 'self'",
       ("'content_security_policy': CSP directive 'object-src' must be "
        "specified (either explicitly, or implicitly via 'default-src') "
        "and must allowlist only secure resources."),
       "script-src 'self'; object-src 'self';"},
      // Similarly, if an insecure (e.g. http) object-src is provided, we simply
      // ignore it.
      {ManifestVersion::kMV2, "script-src 'self'; object-src http://google.com",
       ("'content_security_policy': Ignored insecure CSP value "
        "\"http://google.com\" in directive 'object-src'."),
       "script-src 'self'; object-src;"}};

  for (const auto& testcase : kWarningTestcases) {
    // Special case: In MV2, if the developer doesn't provide an object-src, we
    // insert one ('self') and emit a warning.
    ManifestData manifest_data(get_manifest(testcase.version, testcase.csp));
    scoped_refptr<const Extension> extension =
        LoadAndExpectWarning(manifest_data, testcase.expected_warning);
    ASSERT_TRUE(extension);
    EXPECT_EQ(testcase.effective_csp,
              CSPInfo::GetExtensionPagesCSP(extension.get()));
  }
}

TEST_F(CSPInfoUnitTest, AllowWasmInMV3) {
  struct {
    const char* file_name;
    const char* csp;
  } cases[] = {{"csp_dictionary_with_wasm.json",
                "worker-src 'self' 'wasm-unsafe-eval'; default-src 'self'"},
               {"csp_dictionary_with_unsafe_wasm.json",
                "worker-src 'self' 'wasm-unsafe-eval'; default-src 'self'"},
               {"csp_dictionary_empty_v3.json", "script-src 'self';"},
               {"csp_dictionary_valid_1.json", "default-src 'none'"},
               {"csp_omitted_mv2.json", kDefaultExtensionPagesCSP}};

  for (const auto& test_case : cases) {
    SCOPED_TRACE(base::StringPrintf("Testing %s.", test_case.file_name));
    scoped_refptr<Extension> extension =
        LoadAndExpectSuccess(test_case.file_name);
    ASSERT_TRUE(extension.get());
    EXPECT_EQ(test_case.csp, CSPInfo::GetExtensionPagesCSP(extension.get()));
  }
}

TEST_F(CSPInfoUnitTest, CSPDictionary_Sandbox) {
  ScopedCurrentChannel channel(version_info::Channel::UNKNOWN);

  const char kCustomSandboxedCSP[] =
      "sandbox; script-src https://www.google.com";
  const char kCustomExtensionPagesCSP[] = "script-src; object-src;";

  struct {
    const char* file_name;
    const char* resource_path;
    const char* expected_csp;
  } success_cases[] = {
      {"sandbox_dictionary_1.json", "/test", kCustomSandboxedCSP},
      {"sandbox_dictionary_1.json", "/index", kDefaultSecureCSP},
      {"sandbox_dictionary_2.json", "/test", kDefaultSandboxedPageCSP},
      {"sandbox_dictionary_2.json", "/index", kCustomExtensionPagesCSP},
  };

  for (const auto& test_case : success_cases) {
    SCOPED_TRACE(base::StringPrintf("%s with path %s", test_case.file_name,
                                    test_case.resource_path));
    scoped_refptr<Extension> extension =
        LoadAndExpectSuccess(test_case.file_name);
    ASSERT_TRUE(extension);
    EXPECT_EQ(test_case.expected_csp,
              CSPInfo::GetResourceContentSecurityPolicy(
                  extension.get(), test_case.resource_path));
  }

  Testcase testcases[] = {
      {"sandbox_both_keys.json", errors::kSandboxPagesCSPKeyNotAllowed},
      {"sandbox_csp_with_dictionary.json",
       errors::kSandboxPagesCSPKeyNotAllowed},
      {"sandbox_invalid_type.json",
       GetInvalidManifestKeyError(
           keys::kContentSecurityPolicy_SandboxedPagesPath)},
      {"unsandboxed_csp.json",
       GetInvalidManifestKeyError(
           keys::kContentSecurityPolicy_SandboxedPagesPath)}};
  RunTestcases(testcases, std::size(testcases), EXPECT_TYPE_ERROR);
}

// Ensures that using a dictionary for the keys::kContentSecurityPolicy manifest
// key is mandatory for manifest v3 extensions and that defaults are applied
// correctly.
TEST_F(CSPInfoUnitTest, CSPDictionaryMandatoryForV3) {
  LoadAndExpectError("csp_invalid_type_v3.json",
                     GetInvalidManifestKeyError(keys::kContentSecurityPolicy));

  const char* default_case_filenames[] = {"csp_dictionary_empty_v3.json",
                                          "csp_dictionary_missing_v3.json"};

  // First, run through with loading the extensions as packed extensions.
  for (const char* filename : default_case_filenames) {
    SCOPED_TRACE(filename);
    scoped_refptr<Extension> extension =
        LoadAndExpectSuccess(filename, mojom::ManifestLocation::kInternal);
    ASSERT_TRUE(extension);

    const std::string* isolated_world_csp =
        CSPInfo::GetIsolatedWorldCSP(*extension);
    ASSERT_TRUE(isolated_world_csp);
    EXPECT_EQ(CSPHandler::GetMinimumMV3CSPForTesting(), *isolated_world_csp);

    EXPECT_EQ(kDefaultSandboxedPageCSP,
              CSPInfo::GetSandboxContentSecurityPolicy(extension.get()));
    EXPECT_EQ(kDefaultSecureCSP,
              CSPInfo::GetExtensionPagesCSP(extension.get()));

    EXPECT_EQ(
        CSPHandler::GetMinimumMV3CSPForTesting(),
        *CSPInfo::GetMinimumCSPToAppend(*extension, "not_sandboxed.html"));
  }

  // Repeat the test, loading the extensions as unpacked extensions.
  // The minimum CSP we append (and thus the isolated world CSP) should be
  // different, while the other CSPs should remain the same.
  for (const char* filename : default_case_filenames) {
    SCOPED_TRACE(filename);
    scoped_refptr<Extension> extension =
        LoadAndExpectSuccess(filename, mojom::ManifestLocation::kUnpacked);
    ASSERT_TRUE(extension);

    const std::string* isolated_world_csp =
        CSPInfo::GetIsolatedWorldCSP(*extension);
    ASSERT_TRUE(isolated_world_csp);
    EXPECT_EQ(CSPHandler::GetMinimumUnpackedMV3CSPForTesting(),
              *isolated_world_csp);

    EXPECT_EQ(kDefaultSandboxedPageCSP,
              CSPInfo::GetSandboxContentSecurityPolicy(extension.get()));
    EXPECT_EQ(kDefaultSecureCSP,
              CSPInfo::GetExtensionPagesCSP(extension.get()));

    EXPECT_EQ(
        CSPHandler::GetMinimumUnpackedMV3CSPForTesting(),
        *CSPInfo::GetMinimumCSPToAppend(*extension, "not_sandboxed.html"));
  }
}

// Ensure the CSP dictionary is disallowed for mv2 extensions.
TEST_F(CSPInfoUnitTest, CSPDictionaryDisallowedForV2) {
  LoadAndExpectError("csp_dictionary_mv2.json",
                     GetInvalidManifestKeyError(keys::kContentSecurityPolicy));
}

}  // namespace extensions
