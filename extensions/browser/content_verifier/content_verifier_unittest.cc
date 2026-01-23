// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_verifier/content_verifier.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/icu_test_util.h"
#include "base/test/with_feature_override.h"
#include "base/values.h"
#include "extensions/browser/content_verifier/content_verifier_utils.h"
#include "extensions/browser/content_verifier/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/api/content_scripts.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
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

base::FilePath kBackgroundScriptPath(FILE_PATH_LITERAL("foo/bg.js"));
base::FilePath kContentScriptPath(FILE_PATH_LITERAL("foo/content.js"));
base::FilePath kBackgroundPagePath(FILE_PATH_LITERAL("foo/page.html"));
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
  // This is necessary when the test is using both the UI and IO threads.
  base::Lock lock_;
  std::set<base::FilePath> browser_images_paths_;
};

std::set<base::FilePath> TestContentVerifierDelegate::GetBrowserImagePaths(
    const extensions::Extension* extension) {
  base::AutoLock lock(lock_);
  return std::set<base::FilePath>(browser_images_paths_);
}

void TestContentVerifierDelegate::SetBrowserImagePaths(
    std::set<base::FilePath> paths) {
  base::AutoLock lock(lock_);
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
  }

  base::FilePath original_path;

  // Case variants of |original_path| that are *not* equal to |original_path|.
  std::set<base::FilePath> case_variants;
};

class TestContentHashWaiter : public ContentVerifier::TestObserver {
 public:
  TestContentHashWaiter() { ContentVerifier::SetObserverForTests(this); }

  TestContentHashWaiter(const TestContentHashWaiter&) = delete;
  TestContentHashWaiter& operator=(const TestContentHashWaiter&) = delete;

  ~TestContentHashWaiter() { ContentVerifier::SetObserverForTests(nullptr); }

  void WaitForHash() { run_loop_.Run(); }

  void OnFetchComplete(const scoped_refptr<const ContentHash>& content_hash,
                       bool did_hash_mismatch) override {
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
};

}  // namespace

class ContentVerifierTest : public ExtensionsTest {
 public:
  ContentVerifierTest() = default;

  template <typename... Args>
  explicit ContentVerifierTest(Args... args) : ExtensionsTest(args...) {}

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
    OnContentVerifierReady();
  }

  virtual void OnContentVerifierReady() {
    content_verifier_->ResetIODataForTesting(extension_.get());
  }

  void TearDown() override {
    content_verifier_->Shutdown();
    ExtensionsTest::TearDown();
  }

  void UpdateBrowserImagePaths(const std::set<base::FilePath>& paths) {
    content_verifier_delegate_raw_->SetBrowserImagePaths(paths);
    OnContentVerifierReady();
  }

  bool ShouldVerifySinglePath(const base::FilePath& path) {
    return content_verifier_->ShouldVerifyAnyPathsForTesting(
        extension_->id(), extension_->path(), {path});
  }

  BackgroundManifestType GetBackgroundManifestType() {
    return background_manifest_type_;
  }

  void TestPathsForCaseInsensitiveHandling(std::string_view lower,
                                           std::string_view upper) {
    const auto lower_path = base::FilePath::FromUTF8Unsafe(lower);
    const auto upper_path = base::FilePath::FromUTF8Unsafe(upper);
    UpdateBrowserImagePaths({});

    // The path would be treated as unclassified resource, so it gets verified.
    EXPECT_TRUE(ShouldVerifySinglePath(lower_path))
        << "for lower_path " << lower_path;
    EXPECT_TRUE(ShouldVerifySinglePath(upper_path))
        << "for upper_path " << upper_path;

    // If the path is specified as browser image, it doesn't get verified.
    UpdateBrowserImagePaths({lower_path});
    EXPECT_FALSE(ShouldVerifySinglePath(lower_path))
        << "for lower_path " << lower_path;
    EXPECT_FALSE(ShouldVerifySinglePath(upper_path))
        << "for upper_path " << upper_path;

    // The case of the image path shouldn't matter.
    UpdateBrowserImagePaths({upper_path});
    EXPECT_FALSE(ShouldVerifySinglePath(lower_path))
        << "for lower_path " << lower_path;
    EXPECT_FALSE(ShouldVerifySinglePath(upper_path))
        << "for upper_path " << upper_path;

    UpdateBrowserImagePaths({});
  }

  scoped_refptr<ContentVerifier> content_verifier() {
    return content_verifier_;
  }
  scoped_refptr<Extension> extension() { return extension_; }
  raw_ptr<TestContentVerifierDelegate> content_verifier_delegate_raw() {
    return content_verifier_delegate_raw_;
  }

 protected:
  BackgroundManifestType background_manifest_type_ =
      BackgroundManifestType::kNone;

 private:
  // Create a test extension with a content script and possibly a background
  // page or background script.
  scoped_refptr<Extension> CreateTestExtension() {
    auto manifest = base::DictValue()
                        .Set("name", "Dummy Extension")
                        .Set("version", "1")
                        .Set("manifest_version", 2);

    if (background_manifest_type_ ==
        BackgroundManifestType::kBackgroundScript) {
      base::ListValue background_scripts;
      background_scripts.Append(kBackgroundScriptPath.AsUTF8Unsafe());
      manifest.SetByDottedPath(manifest_keys::kBackgroundScripts,
                               std::move(background_scripts));
    } else if (background_manifest_type_ ==
               BackgroundManifestType::kBackgroundPage) {
      manifest.SetByDottedPath(manifest_keys::kBackgroundPage,
                               kBackgroundPagePath.AsUTF8Unsafe());
    }

    base::ListValue content_scripts;
    base::DictValue content_script;
    base::ListValue js_files;
    base::ListValue matches;
    js_files.Append("foo/content.txt");
    content_script.Set("js", std::move(js_files));
    matches.Append("http://*/*");
    content_script.Set("matches", std::move(matches));
    content_scripts.Append(std::move(content_script));
    manifest.Set(api::content_scripts::ManifestKeys::kContentScripts,
                 std::move(content_scripts));

    base::FilePath path;
    EXPECT_TRUE(base::PathService::Get(DIR_TEST_DATA, &path));

    std::u16string error;
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
      kContentScriptPath, kScriptFilePath,       kHTMLFilePath,
      kHTMFilePath,       kBackgroundScriptPath, kBackgroundPagePath};
  std::vector<base::FilePath> files_not_to_be_verified{kIconPath,
                                                       kUnknownTypeFilePath};

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
    base::FilePath::StringViewType input;
    base::FilePath::StringViewType expected;
  } test_cases[] = {
      {FPL("foo/bar"), FPL("foo/bar")},
      {FPL("foo//bar"), FPL("foo/bar")},
      {FPL("foo/bar/"), FPL("foo/bar/")},
      {FPL("foo/bar//"), FPL("foo/bar/")},
      {FPL("foo/options.html/"), FPL("foo/options.html/")},
      {FPL("foo/./bar"), FPL("foo/bar")},
      {FPL("foo/../bar"), FPL("bar")},
      {FPL("foo/../.."), FPL("")},
      {FPL("./foo"), FPL("foo")},
      {FPL("../foo"), FPL("foo")},
      {FPL("foo/../../bar"), FPL("bar")},
  };
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
  std::vector<std::string> exts_lowercase = {
      // Common extensions.
      "js",
      "html",
      "htm",
      // Less common extensions.
      "mjs",
      "shtml",
      "shtm",
  };

  for (const auto& ext_lowercase : exts_lowercase) {
    auto ext_uppercase = base::ToUpperASCII(ext_lowercase);
    auto ext_capitalized = ext_lowercase;
    ext_capitalized[0] = base::ToUpperASCII(ext_capitalized[0]);
    for (const auto& ext : {ext_lowercase, ext_uppercase, ext_capitalized}) {
      const auto path =
          base::FilePath(FILE_PATH_LITERAL("x")).AddExtensionASCII(ext);
      UpdateBrowserImagePaths({});
      // |path| would be treated as unclassified resource, so it gets verified.
      EXPECT_TRUE(ShouldVerifySinglePath(path)) << "for path " << path;
      // Even if |path| was specified as browser image, as |path| is JS/html
      // (sensitive) resource, it would still get verified.
      UpdateBrowserImagePaths({path});
      EXPECT_TRUE(ShouldVerifySinglePath(path)) << "for path " << path;
    }
  }
}

TEST_F(ContentVerifierTest, CaseInsensitivePaths) {
  if (content_verifier_utils::IsFileAccessCaseSensitive()) {
    return;
  }

  std::vector<std::pair<std::string, std::string>> lower_upper = {
      {"a.png", "A.png"},
      {"ä.png", "Ä.png"},
      {"æ.png", "Æ.png"},
      {"ф.png", "Ф.png"},
  };

  for (const auto& [lower, upper] : lower_upper) {
    TestPathsForCaseInsensitiveHandling(lower, upper);
  }
}

TEST_F(ContentVerifierTest, CaseInsensitivePathsLocaleIndependent) {
  if (content_verifier_utils::IsFileAccessCaseSensitive()) {
    return;
  }

  std::vector<std::pair<std::string, std::string>> lower_upper = {
      // Locale-depended Turkish conversion may normalize these file names
      // differently, which would be undesirable.
      {"icon.png", "Icon.png"},
  };

  std::vector<std::string> locales = {"en_US", "tr"};

  for (const auto& locale : locales) {
    base::test::ScopedRestoreICUDefaultLocale restore_locale(locale);

    for (const auto& [lower, upper] : lower_upper) {
      TestPathsForCaseInsensitiveHandling(lower, upper);
    }
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
  }
}

class ContentVerifierExtensionRootHashTest
    : public base::test::WithFeatureOverride,
      public ContentVerifierTest {
 public:
  ContentVerifierExtensionRootHashTest()
      : base::test::WithFeatureOverride(
            extensions_features::
                kExtensionContentVerificationUsesExtensionRoot),
        // Necessary to use BrowserTaskEnvironment::RunIOThreadUntilIdle().
        ContentVerifierTest(content::BrowserTaskEnvironment::REAL_IO_THREAD) {}

  void OnContentVerifierReady() override {
    RunTaskOnIOThreadAndWait(
        base::BindOnce(&ContentVerifier::ResetIODataForTesting,
                       content_verifier(), base::RetainedRef(extension())));
  }

  void RunTaskOnIOThreadAndWait(base::OnceClosure task) {
    base::RunLoop run_loop;
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, std::move(task).Then(run_loop.QuitClosure()));
    run_loop.Run();
  }
};

// Tests that when multiple extension roots exist for the same extension version
// we create different `ContentHash`es for them if the extension root feature is
// enabled, otherwise the hashes are the same.
TEST_P(ContentVerifierExtensionRootHashTest,
       ContentHashesForDifferentExtensionRoots) {
  content_verifier_delegate_raw()->SetVerifierSourceType(
      ContentVerifierDelegate::VerifierSourceType::UNSIGNED_HASHES);

  // 1. Setup Extension A (Root A).
  base::FilePath root_a = extension()->path();
  {
    TestContentHashWaiter waiter;
    content_verifier()->OnExtensionLoaded(browser_context(), extension().get());
    waiter.WaitForHash();
  }

  // 2. Setup Extension B (Root B, Same ID, Same Version).
  // We create a copy of the extension but with a different path.
  base::FilePath root_b = root_a.ReplaceExtension(FILE_PATH_LITERAL("_1"));
  scoped_refptr<const Extension> extension_b =
      ExtensionBuilder()
          .SetManifest(base::DictValue()
                           .Set("name", "Dummy Extension")
                           .Set("version", "1")
                           .Set("manifest_version", 3))
          .SetID(extension()->id())
          .SetPath(root_b)
          .Build();
  {
    SCOPED_TRACE(
        "Waiting for the verifier to update with Extension B's data (same ID, "
        "different root)");
    RunTaskOnIOThreadAndWait(
        base::BindOnce(&ContentVerifier::ResetIODataForTesting,
                       content_verifier(), base::RetainedRef(extension_b)));
  }
  {
    TestContentHashWaiter waiter;
    content_verifier()->OnExtensionLoaded(browser_context(), extension_b.get());
    waiter.WaitForHash();
  }

  // 3. Verify that we have separate cache entries for both roots.
  // We need to run this on the IO thread.
  base::RunLoop run_loop;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<ContentVerifier> verifier, ExtensionId id,
             base::Version version, base::FilePath root_a,
             base::FilePath root_b, bool is_feature_enabled,
             base::RepeatingClosure quit_closure) {
            auto hash_a = verifier->GetCachedContentHash(
                id, version, root_a,
                /*force_missing_computed_hashes_creation=*/false);
            auto hash_b = verifier->GetCachedContentHash(
                id, version, root_b,
                /*force_missing_computed_hashes_creation=*/false);

            if (is_feature_enabled) {
              EXPECT_EQ(hash_a->extension_root(), root_a);
              EXPECT_EQ(hash_b->extension_root(), root_b);
              EXPECT_NE(hash_a, hash_b);
            } else {
              EXPECT_EQ(hash_a, hash_b);
              // When the feature is disabled, the cache key ignores the root,
              // so the second load (root_b) overwrites the first (root_a).
              // Thus both lookups return the hash for root_b.
              EXPECT_EQ(hash_a->extension_root(), root_b);
            }
            quit_closure.Run();
          },
          content_verifier(), extension()->id(), extension()->version(), root_a,
          root_b, IsParamFeatureEnabled(), run_loop.QuitClosure()));
  run_loop.Run();
}

// Tests that a "Stale" ContentVerifyJob (a job created for an old extension
// root that is no longer loaded) is not started if the extension has been
// updated to a new root (even with the same version).
// This prevents a race condition where the old job would create a "Stale"
// ContentHash for the old root, leaking ContentHash and potentially causing
// false positive corruption errors. This race can happen during extension
// corruption repair when the extension goes idle.
TEST_P(ContentVerifierExtensionRootHashTest, StaleJobOnUpdatedExtension) {
  content_verifier_delegate_raw()->SetVerifierSourceType(
      ContentVerifierDelegate::VerifierSourceType::UNSIGNED_HASHES);

  // Load Extension (Root A).
  base::FilePath root_a = extension()->path();
  {
    SCOPED_TRACE("waiting for extension with Root A to create it's hash");
    TestContentHashWaiter waiter;
    content_verifier()->OnExtensionLoaded(browser_context(), extension().get());
    waiter.WaitForHash();
  }

  // "Update" Extension to Root B (same ID and version) which unloads the
  // extension at Root A. This simulates a corruption repair when the
  // extension goes idle.
  content_verifier()->OnExtensionUnloaded(browser_context(), extension().get(),
                                          UnloadedExtensionReason::UPDATE);
  // Load extension with Root B.
  base::FilePath root_b = root_a.ReplaceExtension(FILE_PATH_LITERAL("_1"));
  scoped_refptr<const Extension> extension_b =
      ExtensionBuilder()
          .SetManifest(base::DictValue()
                           .Set("name", "Extension Root B")
                           .Set("version", "1")
                           .Set("manifest_version", 3))
          .SetID(extension()->id())
          .SetPath(root_b)
          .Build();
  {
    SCOPED_TRACE(
        "Waiting for the verifier to update to Root B (simulating a "
        "corruption repair)");
    RunTaskOnIOThreadAndWait(
        base::BindOnce(&ContentVerifier::ResetIODataForTesting,
                       content_verifier(), base::RetainedRef(extension_b)));
  }
  {
    SCOPED_TRACE("waiting for extension with Root B to create it's hash");
    TestContentHashWaiter waiter;
    content_verifier()->OnExtensionLoaded(browser_context(), extension_b.get());
    waiter.WaitForHash();
  }

  // Create and try to start ContentVerifyJob for the old Root A.
  // This simulates a job that was pending or created before the update/reload
  // but started processing afterwards.
  ContentVerifier::CreateAndStartJobFor(
      extension()->id(), root_a, extension()->version(),
      base::FilePath(FILE_PATH_LITERAL("background.js")), content_verifier());

  // Wait for IO tasks to complete.
  {
    SCOPED_TRACE("waiting for content verifier to process content verify jobs");
    task_environment()->RunIOThreadUntilIdle();
  }

  // Verify whether ContentHash is created based on the feature flag.
  bool hash_a_exists = false;
  {
    SCOPED_TRACE("Checking if Root A content hash was created on IO thread");
    RunTaskOnIOThreadAndWait(base::BindOnce(
        [](scoped_refptr<ContentVerifier> verifier, ExtensionId id,
           base::Version version, base::FilePath root_a, bool* hash_exists) {
          auto hash = verifier->GetCachedContentHash(
              id, version, root_a,
              /*force_missing_computed_hashes_creation=*/false);
          *hash_exists = (hash != nullptr);
        },
        content_verifier(), extension()->id(), extension()->version(), root_a,
        &hash_a_exists));
  }

  if (IsParamFeatureEnabled()) {
    EXPECT_FALSE(hash_a_exists) << "Stale ContentHash for Root A should not be "
                                   "created when feature is enabled!";
  } else {
    EXPECT_TRUE(hash_a_exists)
        << "Stale ContentHash for Root A should be created when feature is "
           "disabled!";
  }
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(ContentVerifierExtensionRootHashTest);

}  // namespace extensions
