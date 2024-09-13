// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/content_verifier/content_verifier.h"
#include "extensions/browser/content_verifier/content_verifier_utils.h"
#include "extensions/browser/content_verifier/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/api/content_scripts.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/scoped_testing_manifest_handler_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

enum class BackgroundManifestType {
  kNone,
  kBackgroundScript,
  kBackgroundPage,
};

const std::string kDotSpaceSuffixList[] = {
    ".", ". ", " .", "..", ".. ", " ..", " . ",
};

base::FilePath kBackgroundScriptPath(FILE_PATH_LITERAL("foo/bg.txt"));
base::FilePath kContentScriptPath(FILE_PATH_LITERAL("foo/content.txt"));
base::FilePath kBackgroundPagePath(FILE_PATH_LITERAL("foo/page.txt"));
base::FilePath kScriptFilePath(FILE_PATH_LITERAL("bar/code.js"));
base::FilePath kUnknownTypeFilePath(FILE_PATH_LITERAL("bar/code.txt"));
base::FilePath kHTMLFilePath(FILE_PATH_LITERAL("bar/page.html"));
base::FilePath kHTMFilePath(FILE_PATH_LITERAL("bar/page.htm"));
base::FilePath kIconPath(FILE_PATH_LITERAL("bar/16.png"));

base::FilePath ToUppercasePath(const base::FilePath& path) {
  return base::FilePath(base::ToUpperASCII(path.value()));
}
base::FilePath ToFirstLetterUppercasePath(const base::FilePath& path) {
  base::FilePath::StringType path_copy = path.value();
  // Note: if there are no lowercase letters in |path|, this method returns
  // |path|.
  for (auto& c : path_copy) {
    const auto upper_c = base::ToUpperASCII(c);
    if (upper_c != c) {
      c = upper_c;
      break;
    }
  }
  return base::FilePath(path_copy);
}
base::FilePath AppendSuffix(const base::FilePath& path,
                            const std::string& suffix) {
  return base::FilePath::FromUTF8Unsafe(path.AsUTF8Unsafe().append(suffix));
}

class TestContentVerifierDelegate : public MockContentVerifierDelegate {
 public:
  TestContentVerifierDelegate() = default;

  TestContentVerifierDelegate(const TestContentVerifierDelegate&) = delete;
  TestContentVerifierDelegate& operator=(const TestContentVerifierDelegate&) =
      delete;

  ~TestContentVerifierDelegate() override = default;

  std::set<base::FilePath> GetBrowserImagePaths(
      const extensions::Extension* extension) override;

  void SetBrowserImagePaths(std::set<base::FilePath> paths);

 private:
  std::set<base::FilePath> browser_images_paths_;
};

std::set<base::FilePath> TestContentVerifierDelegate::GetBrowserImagePaths(
    const extensions::Extension* extension) {
  return std::set<base::FilePath>(browser_images_paths_);
}

void TestContentVerifierDelegate::SetBrowserImagePaths(
    std::set<base::FilePath> paths) {
  browser_images_paths_ = paths;
}

// Generated variants of a base::FilePath that are interesting for
// content-verification tests.
struct FilePathVariants {
  explicit FilePathVariants(const base::FilePath& path) : original_path(path) {
    auto insert_if_non_empty_and_different =
        [&path](std::set<base::FilePath>* container, base::FilePath new_path) {
          if (!new_path.empty() && new_path != path) {
            container->insert(new_path);
          }
        };

    // 1. Case variant 1/2: All uppercase.
    insert_if_non_empty_and_different(&case_variants, ToUppercasePath(path));
    // 2. Case variant 2/2: First letter uppercase.
    insert_if_non_empty_and_different(&case_variants,
                                      ToFirstLetterUppercasePath(path));
    // 3. Dot-space suffix variants:
    for (const auto& dot_space_suffix : kDotSpaceSuffixList) {
      insert_if_non_empty_and_different(&dot_space_suffix_variants,
                                        AppendSuffix(path, dot_space_suffix));
    }
    // 4. Case variants that also have dot-space suffix:
    for (const auto& case_variant : case_variants) {
      for (const auto& suffix : kDotSpaceSuffixList) {
        insert_if_non_empty_and_different(&case_and_dot_space_suffix_variants,
                                          AppendSuffix(case_variant, suffix));
      }
    }
  }

  base::FilePath original_path;

  // Case variants of |original_path| that are *not* equal to |original_path|.
  std::set<base::FilePath> case_variants;

  // Dot space suffix added variants of |original_path| that are *not* equal to
  // |original_path|.
  std::set<base::FilePath> dot_space_suffix_variants;

  // Case variants appended with dot space suffix to |original_path| that are
  // *not* equal to |original_path|.
  std::set<base::FilePath> case_and_dot_space_suffix_variants;
};

}  // namespace

class ContentVerifierTest : public ExtensionsTest {
 public:
  ContentVerifierTest() = default;

  ContentVerifierTest(const ContentVerifierTest&) = delete;
  ContentVerifierTest& operator=(const ContentVerifierTest&) = delete;

  void SetUp() override {
    ExtensionsTest::SetUp();

    // Manually register handlers since the |ContentScriptsHandler| is not
    // usually registered in extensions_unittests.
    ScopedTestingManifestHandlerRegistry scoped_registry;
    {
      ManifestHandlerRegistry* registry = ManifestHandlerRegistry::Get();
      registry->RegisterHandler(std::make_unique<BackgroundManifestHandler>());
      registry->RegisterHandler(std::make_unique<ContentScriptsHandler>());
      ManifestHandler::FinalizeRegistration();
    }

    extension_ = CreateTestExtension();
    ExtensionRegistry::Get(browser_context())->AddEnabled(extension_);

    auto content_verifier_delegate =
        std::make_unique<TestContentVerifierDelegate>();
    content_verifier_delegate_raw_ = content_verifier_delegate.get();

    content_verifier_ = new ContentVerifier(
        browser_context(), std::move(content_verifier_delegate));
    // |ContentVerifier::ShouldVerifyAnyPaths| always returns false if the
    // Content Verifier does not have |ContentVerifierIOData::ExtensionData|
    // for the extension.
    content_verifier_->ResetIODataForTesting(extension_.get());
  }

  void TearDown() override {
    content_verifier_->Shutdown();
    ExtensionsTest::TearDown();
  }

  void UpdateBrowserImagePaths(const std::set<base::FilePath>& paths) {
    content_verifier_delegate_raw_->SetBrowserImagePaths(paths);
    content_verifier_->ResetIODataForTesting(extension_.get());
  }

  bool ShouldVerifySinglePath(const base::FilePath& path) {
    return content_verifier_->ShouldVerifyAnyPathsForTesting(
        extension_->id(), extension_->path(), {path});
  }

  BackgroundManifestType GetBackgroundManifestType() {
    return background_manifest_type_;
  }

 protected:
  BackgroundManifestType background_manifest_type_ =
      BackgroundManifestType::kNone;

 private:
  // Create a test extension with a content script and possibly a background
  // page or background script.
  scoped_refptr<Extension> CreateTestExtension() {
    auto manifest = base::Value::Dict()
                        .Set("name", "Dummy Extension")
                        .Set("version", "1")
                        .Set("manifest_version", 2);

    if (background_manifest_type_ ==
        BackgroundManifestType::kBackgroundScript) {
      base::Value::List background_scripts;
      background_scripts.Append("foo/bg.txt");
      manifest.SetByDottedPath(manifest_keys::kBackgroundScripts,
                               std::move(background_scripts));
    } else if (background_manifest_type_ ==
               BackgroundManifestType::kBackgroundPage) {
      manifest.SetByDottedPath(manifest_keys::kBackgroundPage, "foo/page.txt");
    }

    base::Value::List content_scripts;
    base::Value::Dict content_script;
    base::Value::List js_files;
    base::Value::List matches;
    js_files.Append("foo/content.txt");
    content_script.Set("js", std::move(js_files));
    matches.Append("http://*/*");
    content_script.Set("matches", std::move(matches));
    content_scripts.Append(std::move(content_script));
    manifest.Set(api::content_scripts::ManifestKeys::kContentScripts,
                 std::move(content_scripts));

    base::FilePath path;
    EXPECT_TRUE(base::PathService::Get(DIR_TEST_DATA, &path));

    std::string error;
    scoped_refptr<Extension> extension(
        Extension::Create(path, mojom::ManifestLocation::kInternal, manifest,
                          Extension::NO_FLAGS, &error));
    EXPECT_TRUE(extension.get()) << error;
    return extension;
  }

  scoped_refptr<ContentVerifier> content_verifier_;
  scoped_refptr<Extension> extension_;
  raw_ptr<TestContentVerifierDelegate> content_verifier_delegate_raw_;
};

class ContentVerifierTestWithBackgroundType
    : public ContentVerifierTest,
      public testing::WithParamInterface<BackgroundManifestType> {
 public:
  ContentVerifierTestWithBackgroundType() {
    background_manifest_type_ = GetParam();
  }

  ContentVerifierTestWithBackgroundType(
      const ContentVerifierTestWithBackgroundType&) = delete;
  ContentVerifierTestWithBackgroundType& operator=(
      const ContentVerifierTestWithBackgroundType&) = delete;
};

// Verifies that |ContentVerifier::ShouldVerifyAnyPaths| returns true for
// some file paths even if those paths are specified as browser images.
TEST_P(ContentVerifierTestWithBackgroundType, BrowserImagesShouldBeVerified) {
  std::vector<base::FilePath> files_to_be_verified = {
      kContentScriptPath, kScriptFilePath, kHTMLFilePath, kHTMFilePath};
  std::vector<base::FilePath> files_not_to_be_verified{kIconPath,
                                                       kUnknownTypeFilePath};

  if (GetBackgroundManifestType() ==
      BackgroundManifestType::kBackgroundScript) {
    files_to_be_verified.push_back(kBackgroundScriptPath);
    files_not_to_be_verified.push_back(kBackgroundPagePath);
  } else if (GetBackgroundManifestType() ==
             BackgroundManifestType::kBackgroundPage) {
    files_to_be_verified.push_back(kBackgroundPagePath);
    files_not_to_be_verified.push_back(kBackgroundScriptPath);
  } else {
    files_not_to_be_verified.push_back(kBackgroundScriptPath);
    files_not_to_be_verified.push_back(kBackgroundPagePath);
  }

  auto generate_test_cases = [](const std::vector<base::FilePath>& input) {
    std::set<base::FilePath> output;
    for (const auto& path : input) {
      output.insert(path);
      if (!content_verifier_utils::IsFileAccessCaseSensitive()) {
        // For case insensitive OS, upper casing the FilePaths would be
        // treated in similar fashion.
        output.insert(ToUppercasePath(path));
        // Ditto for only upper casing first character of FilePath.
        output.insert(ToFirstLetterUppercasePath(path));
      }
    }
    return output;
  };

  std::set<base::FilePath> all_files_to_be_verified =
      generate_test_cases(files_to_be_verified);
  for (const base::FilePath& path : all_files_to_be_verified) {
    UpdateBrowserImagePaths({});
    EXPECT_TRUE(ShouldVerifySinglePath(path)) << "for path " << path;
    UpdateBrowserImagePaths(std::set<base::FilePath>{path});
    EXPECT_TRUE(ShouldVerifySinglePath(path)) << "for path " << path;
  }

  std::set<base::FilePath> all_files_not_to_be_verified =
      generate_test_cases(files_not_to_be_verified);
  for (const base::FilePath& path : all_files_not_to_be_verified) {
    UpdateBrowserImagePaths({});
    EXPECT_TRUE(ShouldVerifySinglePath(path)) << "for path " << path;
    UpdateBrowserImagePaths(std::set<base::FilePath>{path});
    EXPECT_FALSE(ShouldVerifySinglePath(path)) << "for path " << path;
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ContentVerifierTestWithBackgroundType,
    testing::Values(BackgroundManifestType::kNone,
                    BackgroundManifestType::kBackgroundScript,
                    BackgroundManifestType::kBackgroundPage));

TEST_F(ContentVerifierTest, NormalizeRelativePath) {
// This macro helps avoid wrapped lines in the test structs.
#define FPL(x) FILE_PATH_LITERAL(x)
  struct TestData {
    base::FilePath::StringPieceType input;
    base::FilePath::StringPieceType expected;
  } test_cases[] = {{FPL("foo/bar"), FPL("foo/bar")},
                    {FPL("foo//bar"), FPL("foo/bar")},
                    {FPL("foo/bar/"), FPL("foo/bar/")},
                    {FPL("foo/bar//"), FPL("foo/bar/")},
                    {FPL("foo/options.html/"), FPL("foo/options.html/")}};
#undef FPL
  for (const auto& test_case : test_cases) {
    base::FilePath input(test_case.input);
    base::FilePath expected(test_case.expected);
    EXPECT_EQ(expected,
              ContentVerifier::NormalizeRelativePathForTesting(input));
  }
}

// Tests that JavaScript and html/htm files are always verified, even if their
// extension case isn't lower cased or even if they are specified as browser
// image paths.
TEST_F(ContentVerifierTest, JSAndHTMLAlwaysVerified) {
  std::vector<std::string> paths = {
      "a.js",  "b.html", "c.htm",  "a.JS",  "b.HTML",
      "c.HTM", "a.Js",   "b.Html", "c.Htm",
  };

  for (const auto& path_str : paths) {
    const base::FilePath path = base::FilePath().AppendASCII(path_str);
    UpdateBrowserImagePaths({});
    // |path| would be treated as unclassified resource, so it gets verified.
    EXPECT_TRUE(ShouldVerifySinglePath(path)) << "for path " << path;
    // Even if |path| was specified as browser image, as |path| is JS/html
    // (sensitive) resource, it would still get verified.
    UpdateBrowserImagePaths({path});
    EXPECT_TRUE(ShouldVerifySinglePath(path)) << "for path " << path;
  }
}

TEST_F(ContentVerifierTest, AlwaysVerifiedPathsWithVariants) {
  FilePathVariants kAlwaysVerifiedTestCases[] = {
      // JS files are always verified.
      FilePathVariants(base::FilePath(FILE_PATH_LITERAL("always.js"))),
      // html files are always verified.
      FilePathVariants(base::FilePath(FILE_PATH_LITERAL("always.html"))),
  };

  for (const auto& test_case : kAlwaysVerifiedTestCases) {
    EXPECT_TRUE(ShouldVerifySinglePath(test_case.original_path))
        << "original_path = " << test_case.original_path;

    // Case changed variants always gets verified in case-insensitive OS.
    // e.g. "ALWAYS.JS" is verified in win/mac. On other OS, they are treated as
    // unclassified resource so also gets verified.
    for (const auto& case_variant : test_case.case_variants) {
      EXPECT_TRUE(ShouldVerifySinglePath({case_variant}))
          << " case_variant = " << case_variant;
    }

    // If OS ignores dot-space suffix, then dot-space suffix added paths would
    // always be verified. Otherwise, they would be treated as unclassified
    // resource, so they also get verified.
    // e.g. "always.js." is always verified on win as it is treated as
    // "always.js". In non-win, it is treated as an arbitrary resource, so it
    // also gets verified. Also note that even if "always.js." is listed as
    // browser image, it's OK.
    for (const auto& dot_space_variant : test_case.dot_space_suffix_variants) {
      EXPECT_TRUE(ShouldVerifySinglePath({dot_space_variant}))
          << "dot_space_variant = " << dot_space_variant;
    }

    // Similar test case with both case variant with dot-space suffix added to
    // them.
    // e.g. "Always.js." is verified in win, and also in other OS. Also note
    // that even if "always.js." is listed as browser image, it's OK.
    for (const auto& path : test_case.case_and_dot_space_suffix_variants) {
      EXPECT_TRUE(ShouldVerifySinglePath({path}))
          << "case_and_dot_space_suffix_variant = " << path;
    }
  }
}

// Tests paths that are never supposed to be verified by content verification.
// Also tests their OS specific equivalents (changing case and appending
// dot-space suffix to them in windows for example).
TEST_F(ContentVerifierTest, NeverVerifiedPaths) {
  FilePathVariants kNeverVerifiedTestCases[] = {
      // manifest.json is never verified.
      FilePathVariants(base::FilePath(FILE_PATH_LITERAL("manifest.json"))),
      // _locales paths are never verified:
      //   - locales with lowercase lang.
      FilePathVariants(
          base::FilePath(FILE_PATH_LITERAL("_locales/en/messages.json"))),
      //   - locales with mixedcase lang.
      FilePathVariants(
          base::FilePath(FILE_PATH_LITERAL("_locales/en_GB/messages.json"))),
  };

  for (const auto& test_case : kNeverVerifiedTestCases) {
    EXPECT_FALSE(ShouldVerifySinglePath(test_case.original_path))
        << test_case.original_path;
    // Case changed variants should only be verified iff the OS
    // is case-sensitive, as they won't be treated as ignorable file path.
    // e.g. "Manifest.json" is not verified in win/mac, but is verified in
    // linux/chromeos.
    for (const auto& case_variant : test_case.case_variants) {
      EXPECT_EQ(content_verifier_utils::IsFileAccessCaseSensitive(),
                ShouldVerifySinglePath({case_variant}))
          << " case_variant = " << case_variant;
    }

    // If OS ignores dot-space suffix, then dot-space suffix added paths would
    // be ignored for verification. Those would verified otherwise.
    // e.g. "manifest.json." is not verified only in win, but is verified in
    // others.
    for (const auto& dot_space_variant : test_case.dot_space_suffix_variants) {
      EXPECT_EQ(!content_verifier_utils::IsDotSpaceFilenameSuffixIgnored(),
                ShouldVerifySinglePath({dot_space_variant}))
          << "dot_space_variant = " << dot_space_variant;
    }

    // Similar test case with both case variant with dot-space suffix added to
    // them.
    // e.g. "Manifest.json." is not verified only in win, but is verified in
    // others.
    for (const auto& path : test_case.case_and_dot_space_suffix_variants) {
      EXPECT_EQ(!content_verifier_utils::IsDotSpaceFilenameSuffixIgnored(),
                ShouldVerifySinglePath({path}))
          << "case_and_dot_space_suffix_variant = " << path;
    }
  }
}

}  // namespace extensions
