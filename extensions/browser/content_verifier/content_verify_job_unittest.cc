// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "base/version.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/content_verifier/content_verifier.h"
#include "extensions/browser/content_verifier/test_utils.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/file_util.h"
#include "extensions/test/test_extension_dir.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/zip.h"

namespace extensions {

namespace {

// Specifies how test ContentVerifyJob's asynchronous steps to read hash and
// read contents are ordered.
// Note that:
// OnHashesReady: is called when hash reading is complete.
// BytesRead + DoneReading: are called when content reading is complete.
enum ContentVerifyJobAsyncRunMode {
  // None - Let hash reading and content reading continue as is asynchronously.
  kNone,
  // Hashes become available after the contents become available.
  kContentReadBeforeHashesReady,
  // The contents become available before the hashes are ready.
  kHashesReadyBeforeContentRead,
};

std::string GetVerifiedContents(const Extension& extension) {
  std::string verified_contents;
  EXPECT_TRUE(base::ReadFileToString(
      file_util::GetVerifiedContentsPath(extension.path()),
      &verified_contents));
  return verified_contents;
}

void WriteManifest(TestExtensionDir* dir) {
  dir->WriteManifest(base::Value::Dict()
                         .Set("manifest_version", 2)
                         .Set("name", "Test extension")
                         .Set("version", "1.0"));
}

void WriteComputedHashes(
    const base::FilePath& extension_root,
    const std::map<base::FilePath, std::string>& contents) {
  int block_size = extension_misc::kContentVerificationDefaultBlockSize;
  ComputedHashes::Data computed_hashes_data;

  for (const auto& resource : contents) {
    std::vector<std::string> hashes =
        ComputedHashes::GetHashesForContent(resource.second, block_size);
    computed_hashes_data.Add(resource.first, block_size, std::move(hashes));
  }

  base::CreateDirectory(extension_root.Append(kMetadataFolder));
  ASSERT_TRUE(
      ComputedHashes(std::move(computed_hashes_data))
          .WriteToFile(file_util::GetComputedHashesPath(extension_root)));
}

}  // namespace

class ContentVerifyJobUnittest : public ExtensionsTest {
 public:
  ContentVerifyJobUnittest() {}

  ContentVerifyJobUnittest(const ContentVerifyJobUnittest&) = delete;
  ContentVerifyJobUnittest& operator=(const ContentVerifyJobUnittest&) = delete;

  ~ContentVerifyJobUnittest() override {}

  // Helper to get files from our subdirectory in the general extensions test
  // data dir.
  base::FilePath GetTestPath(const std::string& relative_path) {
    base::FilePath base_path;
    EXPECT_TRUE(base::PathService::Get(DIR_TEST_DATA, &base_path));
    return base_path.AppendASCII("content_hash_fetcher")
        .AppendASCII(relative_path);
  }

  void SetUp() override {
    ExtensionsTest::SetUp();

    auto delegate = std::make_unique<MockContentVerifierDelegate>();
    content_verifier_delegate_ = delegate.get();
    content_verifier_ = base::MakeRefCounted<ContentVerifier>(
        &testing_context_, std::move(delegate));
  }

  void TearDown() override {
    content_verifier_->Shutdown();
    content_verifier_delegate_ = nullptr;

    ExtensionsTest::TearDown();
  }

  scoped_refptr<ContentVerifier> content_verifier() {
    return content_verifier_;
  }

 protected:
  ContentVerifyJob::FailureReason RunContentVerifyJob(
      const Extension& extension,
      const base::FilePath& resource_path,
      std::optional<std::string_view> resource_contents,
      ContentVerifyJobAsyncRunMode run_mode) {
    TestContentVerifySingleJobObserver observer(extension.id(), resource_path);
    auto verify_job = base::MakeRefCounted<ContentVerifyJob>(
        extension.id(), extension.path(), resource_path);

    auto run_content_read_step = base::BindRepeating(
        [](std::optional<std::string_view> resource_contents,
           ContentVerifyJob* verify_job) {
          // Simulate serving |resource_contents| from |resource_path|.
          auto read_data = resource_contents.value_or(std::string_view{});
          MojoResult read_result =
              resource_contents ? MOJO_RESULT_OK : MOJO_RESULT_NOT_FOUND;
          verify_job->BytesRead(read_data.data(), read_data.size(),
                                read_result);
          verify_job->DoneReading();
        },
        std::move(resource_contents));

    switch (run_mode) {
      case kNone:
        // Read hashes asynchronously.
        StartJob(verify_job, extension.version(), extension.manifest_version(),
                 base::DoNothing());
        run_content_read_step.Run(verify_job.get());
        break;
      case kContentReadBeforeHashesReady:
        run_content_read_step.Run(verify_job.get());
        // Read hashes asynchronously.
        StartJob(verify_job, extension.version(), extension.manifest_version(),
                 base::DoNothing());
        break;
      case kHashesReadyBeforeContentRead:
        StartJob(verify_job, extension.version(), extension.manifest_version(),
                 base::DoNothing());
        // Wait for hashes to become ready.
        observer.WaitForOnHashesReady();
        run_content_read_step.Run(verify_job.get());
        break;
    }
    return observer.WaitForJobFinished();
  }

  ContentVerifyJob::FailureReason RunContentVerifyJob(
      const Extension& extension,
      const base::FilePath& resource_path,
      std::optional<std::string_view> resource_contents) {
    return RunContentVerifyJob(extension, resource_path, resource_contents,
                               kNone);
  }

  ContentVerifyJob::FailureReason RunContentVerifyJobWithMultipleReadErrors(
      const Extension& extension,
      const base::FilePath& resource_path,
      base::span<MojoResult> read_errors) {
    TestContentVerifySingleJobObserver observer(extension.id(), resource_path);
    auto verify_job = base::MakeRefCounted<ContentVerifyJob>(
        extension.id(), extension.path(), resource_path);

    // Read hashes asynchronously.
    StartJob(verify_job, extension.version(), extension.manifest_version(),
             base::DoNothing());

    for (const MojoResult read_error : read_errors) {
      verify_job->BytesRead(nullptr, 0, read_error);
    }
    verify_job->DoneReading();

    return observer.WaitForJobFinished();
  }

  void StartContentVerifyJob(const Extension& extension,
                             const base::FilePath& resource_path) {
    auto verify_job = base::MakeRefCounted<ContentVerifyJob>(
        extension.id(), extension.path(), resource_path);
    StartJob(verify_job, extension.version(), extension.manifest_version(),
             base::DoNothing());
  }

  // Returns an extension after extracting and loading it from a .zip file.
  // The extension may be expected to have verified_contents.json in it.
  scoped_refptr<Extension> LoadTestExtensionFromZipPathToTempDir(
      TestExtensionDir* temp_dir,
      const std::string& zip_directory_name,
      const std::string& zip_filename) {
    base::FilePath unzipped_path = temp_dir->UnpackedPath();
    base::FilePath test_dir_base = GetTestPath(zip_directory_name);
    scoped_refptr<Extension> extension =
        content_verifier_test_utils::UnzipToDirAndLoadExtension(
            test_dir_base.AppendASCII(zip_filename), unzipped_path);
    // If needed, make sure there is a verified_contents.json file there as this
    // test cannot fetch it.
    EXPECT_TRUE(extension);
    if (content_verifier_delegate()->GetVerifierSourceType(*extension) ==
            ContentVerifierDelegate::VerifierSourceType::SIGNED_HASHES &&
        !base::PathExists(
            file_util::GetVerifiedContentsPath(extension->path()))) {
      ADD_FAILURE() << "verified_contents.json not found.";
      return nullptr;
    }
    content_verifier_->OnExtensionLoaded(&testing_context_, extension.get());
    return extension;
  }

  // Returns an extension after creating it from scratch with help of
  // |create_callback|. This callback is expected to create all required
  // extension resources in |extension_path|, including manifest.json.
  scoped_refptr<Extension> CreateAndLoadTestExtensionToTempDir(
      TestExtensionDir* temp_dir,
      std::optional<std::map<base::FilePath, std::string>>
          resources_for_hashes) {
    WriteManifest(temp_dir);

    if (resources_for_hashes) {
      WriteComputedHashes(temp_dir->UnpackedPath(),
                          resources_for_hashes.value());
    }

    std::string error;
    scoped_refptr<Extension> extension = file_util::LoadExtension(
        temp_dir->UnpackedPath(), mojom::ManifestLocation::kInternal,
        Extension::InitFromValueFlags::NO_FLAGS, &error);
    EXPECT_NE(nullptr, extension.get()) << " error:'" << error << "'";

    content_verifier_->OnExtensionLoaded(&testing_context_, extension.get());
    return extension;
  }

  MockContentVerifierDelegate* content_verifier_delegate() {
    DCHECK(content_verifier_);
    DCHECK(content_verifier_delegate_);
    return content_verifier_delegate_;
  }

 private:
  void StartJob(scoped_refptr<ContentVerifyJob> job,
                const base::Version& extension_version,
                int manifest_version,
                ContentVerifyJob::FailureCallback failure_callback) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&ContentVerifyJob::Start, job,
                                  base::Unretained(content_verifier_.get()),
                                  extension_version, manifest_version,
                                  std::move(failure_callback)));
  }

  scoped_refptr<ContentVerifier> content_verifier_;
  raw_ptr<MockContentVerifierDelegate> content_verifier_delegate_ =
      nullptr;  // Owned by |content_verifier_|.
  content::TestBrowserContext testing_context_;
};

// Tests that deleted legitimate files trigger content verification failure.
// Also tests that non-existent file request does not trigger content
// verification failure.
TEST_F(ContentVerifyJobUnittest, DeletedAndMissingFiles) {
  TestExtensionDir temp_dir;
  scoped_refptr<Extension> extension = LoadTestExtensionFromZipPathToTempDir(
      &temp_dir, "with_verified_contents", "source_all.zip");
  ASSERT_TRUE(extension.get());
  base::FilePath unzipped_path = temp_dir.UnpackedPath();

  const base::FilePath::CharType kExistentResource[] =
      FILE_PATH_LITERAL("background.js");
  base::FilePath existent_resource_path(kExistentResource);
  {
    // Make sure background.js passes verification correctly.
    std::string contents;
    base::ReadFileToString(unzipped_path.Append(existent_resource_path),
                           &contents);
    EXPECT_EQ(ContentVerifyJob::NONE,
              RunContentVerifyJob(*extension.get(), existent_resource_path,
                                  contents));
  }

  {
    // Once background.js is deleted, verification will result in HASH_MISMATCH.
    // Delete the existent file first.
    EXPECT_TRUE(base::DeleteFile(unzipped_path.Append(existent_resource_path)));

    // Deleted file will result in a read error.
    EXPECT_EQ(ContentVerifyJob::HASH_MISMATCH,
              RunContentVerifyJob(*extension.get(), existent_resource_path,
                                  std::nullopt));
  }

  {
    // Now ask for a non-existent resource non-existent.js. Verification should
    // skip this file as it is not listed in our verified_contents.json file.
    const base::FilePath::CharType kNonExistentResource[] =
        FILE_PATH_LITERAL("non-existent.js");
    base::FilePath non_existent_resource_path(kNonExistentResource);
    // Non-existent file will result in a read error.
    EXPECT_EQ(ContentVerifyJob::NONE,
              RunContentVerifyJob(*extension.get(), non_existent_resource_path,
                                  std::nullopt));
  }

  {
    // Now create a resource foo.js which exists on disk but is not in the
    // extension's verified_contents.json. Verification should result in
    // NO_HASHES_FOR_FILE since the extension is trying to load a file the
    // extension should not have.
    const base::FilePath::CharType kUnexpectedResource[] =
        FILE_PATH_LITERAL("foo.js");
    base::FilePath unexpected_resource_path(kUnexpectedResource);

    base::FilePath full_path = unzipped_path.Append(unexpected_resource_path);
    EXPECT_TRUE(base::WriteFile(full_path, "42"));

    std::string contents;
    base::ReadFileToString(full_path, &contents);
    EXPECT_EQ(ContentVerifyJob::NO_HASHES_FOR_FILE,
              RunContentVerifyJob(*extension.get(), unexpected_resource_path,
                                  contents));
  }

  {
    // Ask for the root path of the extension (i.e., chrome-extension://<id>/).
    // Verification should skip this request as if the resource were
    // non-existent. See https://crbug.com/791929.
    base::FilePath empty_path_resource_path(FILE_PATH_LITERAL(""));
    EXPECT_EQ(ContentVerifyJob::NONE,
              RunContentVerifyJob(*extension.get(), empty_path_resource_path,
                                  std::nullopt));
  }

  {
    // Ask for the path of one of the extension's folders which exists on disk.
    // Verification of the folder should skip the request as if the folder
    // was non-existent. See https://crbug.com/791929.
    const base::FilePath::CharType kUnexpectedFolder[] =
        FILE_PATH_LITERAL("bar/");
    base::FilePath unexpected_folder_path(kUnexpectedFolder);

    base::CreateDirectory(unzipped_path.Append(unexpected_folder_path));
    EXPECT_EQ(ContentVerifyJob::NONE,
              RunContentVerifyJob(*extension.get(), unexpected_folder_path,
                                  std::nullopt));
  }
}

// Tests that multiple read errors are handled correctly.
TEST_F(ContentVerifyJobUnittest, MultipleReadErrors) {
  TestExtensionDir temp_dir;
  scoped_refptr<Extension> extension = LoadTestExtensionFromZipPathToTempDir(
      &temp_dir, "with_verified_contents", "source_all.zip");
  ASSERT_TRUE(extension.get());
  base::FilePath unzipped_path = temp_dir.UnpackedPath();

  const base::FilePath::CharType kExistentResource[] =
      FILE_PATH_LITERAL("background.js");
  base::FilePath existent_resource_path(kExistentResource);

  MojoResult read_errors[] = {
      MOJO_RESULT_DATA_LOSS,
      MOJO_RESULT_ABORTED,
  };
  EXPECT_EQ(ContentVerifyJob::HASH_MISMATCH,
            RunContentVerifyJobWithMultipleReadErrors(
                *extension.get(), existent_resource_path, read_errors));
}

namespace {

void WriteIncorrectComputedHashes(const base::FilePath& extension_path,
                                  const base::FilePath& resource_path) {
  // It is important that correct computed_hashes.json already exists, because
  // we don't want to modify it while it is being created. "source_all.zip"
  // ensures we already have it.
  ASSERT_TRUE(
      base::PathExists(file_util::GetComputedHashesPath(extension_path)));

  base::DeleteFile(file_util::GetComputedHashesPath(extension_path));

  int block_size = extension_misc::kContentVerificationDefaultBlockSize;
  ComputedHashes::Data incorrect_computed_hashes_data;

  // Write a valid computed_hashes.json with incorrect hash for |resource_path|.
  const std::string kFakeContents = "fake contents";
  std::vector<std::string> hashes =
      ComputedHashes::GetHashesForContent(kFakeContents, block_size);
  incorrect_computed_hashes_data.Add(resource_path, block_size,
                                     std::move(hashes));

  ASSERT_TRUE(
      ComputedHashes(std::move(incorrect_computed_hashes_data))
          .WriteToFile(file_util::GetComputedHashesPath(extension_path)));
}

void WriteEmptyComputedHashes(const base::FilePath& extension_path) {
  // It is important that correct computed_hashes.json already exists, because
  // we don't want to modify it while it is being created. "source_all.zip"
  // ensures we already have it.
  ASSERT_TRUE(
      base::PathExists(file_util::GetComputedHashesPath(extension_path)));

  base::DeleteFile(file_util::GetComputedHashesPath(extension_path));

  ComputedHashes::Data incorrect_computed_hashes_data;

  ASSERT_TRUE(
      ComputedHashes(std::move(incorrect_computed_hashes_data))
          .WriteToFile(file_util::GetComputedHashesPath(extension_path)));
}

}  // namespace

// Tests that deletion of an extension resource and invalid hash for it in
// computed_hashes.json won't result in bypassing corruption check.
TEST_F(ContentVerifyJobUnittest, DeletedResourceAndCorruptedComputedHashes) {
  TestExtensionDir temp_dir;

  const base::FilePath::CharType kResource[] =
      FILE_PATH_LITERAL("background.js");
  base::FilePath resource_path(kResource);

  scoped_refptr<Extension> extension = LoadTestExtensionFromZipPathToTempDir(
      &temp_dir, "with_verified_contents", "source_all.zip");
  ASSERT_TRUE(extension.get());

  // Tamper the extension: remove resource and place wrong hash for its entry in
  // computed_hashes.json. Reload content verifier's cache after that because
  // content verifier may read computed_hashes.json with old values upon
  // extension loading.
  base::FilePath unzipped_path = temp_dir.UnpackedPath();
  WriteIncorrectComputedHashes(unzipped_path, resource_path);
  EXPECT_TRUE(
      base::DeleteFile(unzipped_path.Append(base::FilePath(kResource))));
  content_verifier()->ClearCacheForTesting();

  EXPECT_EQ(ContentVerifyJob::CORRUPTED_HASHES,
            RunContentVerifyJob(*extension.get(), resource_path, std::nullopt));
}

// Tests that deletion of an extension resource and removing its entry from
// computed_hashes.json won't result in bypassing corruption check.
TEST_F(ContentVerifyJobUnittest, DeletedResourceAndCleanedComputedHashes) {
  TestExtensionDir temp_dir;

  const base::FilePath::CharType kResource[] =
      FILE_PATH_LITERAL("background.js");
  base::FilePath resource_path(kResource);

  scoped_refptr<Extension> extension = LoadTestExtensionFromZipPathToTempDir(
      &temp_dir, "with_verified_contents", "source_all.zip");
  ASSERT_TRUE(extension.get());

  // Tamper the extension: remove resource and remove its entry from
  // computed_hashes.json. Reload content verifier's cache after that because
  // content verifier may read computed_hashes.json with old values upon
  // extension loading.
  base::FilePath unzipped_path = temp_dir.UnpackedPath();
  WriteEmptyComputedHashes(unzipped_path);
  EXPECT_TRUE(
      base::DeleteFile(unzipped_path.Append(base::FilePath(kResource))));
  content_verifier()->ClearCacheForTesting();

  EXPECT_EQ(ContentVerifyJob::CORRUPTED_HASHES,
            RunContentVerifyJob(*extension.get(), resource_path, std::nullopt));
}

// Tests that extension resources that are originally 0 byte behave correctly
// with content verification.
TEST_F(ContentVerifyJobUnittest, LegitimateZeroByteFile) {
  TestExtensionDir temp_dir;
  // |extension| has a 0 byte background.js file in it.
  scoped_refptr<Extension> extension = LoadTestExtensionFromZipPathToTempDir(
      &temp_dir, "zero_byte_file", "source.zip");
  ASSERT_TRUE(extension.get());
  base::FilePath unzipped_path = temp_dir.UnpackedPath();

  const base::FilePath::CharType kResource[] =
      FILE_PATH_LITERAL("background.js");
  base::FilePath resource_path(kResource);
  {
    // Make sure 0 byte background.js passes content verification.
    std::string contents;
    base::ReadFileToString(unzipped_path.Append(resource_path), &contents);
    EXPECT_EQ(ContentVerifyJob::NONE,
              RunContentVerifyJob(*extension.get(), resource_path, contents));
  }

  {
    // Make sure non-empty background.js fails content verification.
    std::string modified_contents = "console.log('non empty');";
    EXPECT_EQ(ContentVerifyJob::HASH_MISMATCH,
              RunContentVerifyJob(*extension.get(), resource_path,
                                  modified_contents));
  }
}

// Tests that extension resources of different interesting sizes work properly.
// Regression test for https://crbug.com/720597, where content verification
// always failed for sizes multiple of content hash's block size (4096 bytes).
TEST_F(ContentVerifyJobUnittest, DifferentSizedFiles) {
  TestExtensionDir temp_dir;
  scoped_refptr<Extension> extension = LoadTestExtensionFromZipPathToTempDir(
      &temp_dir, "different_sized_files", "source.zip");
  ASSERT_TRUE(extension.get());
  base::FilePath unzipped_path = temp_dir.UnpackedPath();

  const struct {
    const char* name;
    size_t byte_size;
  } kFilesToTest[] = {
      {"1024.js", 1024}, {"4096.js", 4096}, {"8192.js", 8192},
      {"8191.js", 8191}, {"8193.js", 8193},
  };
  for (const auto& file_to_test : kFilesToTest) {
    base::FilePath resource_path = base::FilePath::FromASCII(file_to_test.name);
    std::string contents;
    base::ReadFileToString(unzipped_path.AppendASCII(file_to_test.name),
                           &contents);
    EXPECT_EQ(file_to_test.byte_size, contents.size());
    EXPECT_EQ(ContentVerifyJob::NONE,
              RunContentVerifyJob(*extension.get(), resource_path, contents));
  }
}

// Tests that if both file contents and hash are modified, corruption will still
// be detected.
TEST_F(ContentVerifyJobUnittest, ModifiedComputedHashes) {
  TestExtensionDir temp_dir;
  scoped_refptr<Extension> extension = LoadTestExtensionFromZipPathToTempDir(
      &temp_dir, "with_verified_contents_corrupted", "source_all.zip");
  ASSERT_TRUE(extension.get());
  base::FilePath unzipped_path = temp_dir.UnpackedPath();

  const base::FilePath::CharType kExistentResource[] =
      FILE_PATH_LITERAL("background.js");
  base::FilePath existent_resource_path(kExistentResource);
  {
    std::string contents;
    base::ReadFileToString(unzipped_path.Append(existent_resource_path),
                           &contents);
    EXPECT_EQ(ContentVerifyJob::CORRUPTED_HASHES,
              RunContentVerifyJob(*extension.get(), existent_resource_path,
                                  contents));
  }
}

using ContentVerifyJobWithoutSignedHashesUnittest = ContentVerifyJobUnittest;
// Mark tests with extensions which intentionally don't contain
// verified_contents.json. Typically these are self-hosted extension, since
// there is no possibility for them to use private Chrome Web Store key to sign
// hashes.

// Tests that without verified_contents.json file computes_hashes.json file is
// loaded correctly and appropriate error is reported when load fails.
TEST_F(ContentVerifyJobWithoutSignedHashesUnittest, ComputedHashesLoad) {
  TestExtensionDir temp_dir;
  content_verifier_delegate()->SetVerifierSourceType(
      ContentVerifierDelegate::VerifierSourceType::UNSIGNED_HASHES);

  // Simple resource to trigger content verify job start and hashes load.
  const base::FilePath kResourcePath(FILE_PATH_LITERAL("script.js"));
  const std::string kResourceContents = "console.log('Nothing special');";
  std::map<base::FilePath, std::string> resource_map = {
      {kResourcePath, kResourceContents}};

  // Contents of corrupted computed_hashes.json file.
  const std::string kCorruptedContents = "not a json";

  scoped_refptr<Extension> extension =
      CreateAndLoadTestExtensionToTempDir(&temp_dir, std::move(resource_map));
  ASSERT_TRUE(extension);
  base::FilePath unzipped_path = temp_dir.UnpackedPath();

  {
    // Case where computed_hashes.json is on its place and correct.
    TestContentVerifySingleJobObserver observer(extension->id(), kResourcePath);
    content_verifier()->ClearCacheForTesting();
    StartContentVerifyJob(*extension, kResourcePath);
    ContentHashReader::InitStatus hashes_status =
        observer.WaitForOnHashesReady();
    EXPECT_EQ(ContentHashReader::InitStatus::SUCCESS, hashes_status);
  }

  {
    // Case where computed_hashes.json is corrupted.
    ASSERT_TRUE(base::WriteFile(file_util::GetComputedHashesPath(unzipped_path),
                                kCorruptedContents));

    TestContentVerifySingleJobObserver observer(extension->id(), kResourcePath);
    content_verifier()->ClearCacheForTesting();
    StartContentVerifyJob(*extension, kResourcePath);
    ContentHashReader::InitStatus hashes_status =
        observer.WaitForOnHashesReady();
    EXPECT_EQ(ContentHashReader::InitStatus::HASHES_DAMAGED, hashes_status);
  }

  {
    // Case where computed_hashes.json doesn't exist.
    base::DeleteFile(file_util::GetComputedHashesPath(unzipped_path));

    TestContentVerifySingleJobObserver observer(extension->id(), kResourcePath);
    content_verifier()->ClearCacheForTesting();
    StartContentVerifyJob(*extension, kResourcePath);
    ContentHashReader::InitStatus hashes_status =
        observer.WaitForOnHashesReady();
    EXPECT_EQ(ContentHashReader::InitStatus::HASHES_MISSING, hashes_status);
  }
}

// Tests that extension without verified_contents.json is checked properly.
TEST_F(ContentVerifyJobWithoutSignedHashesUnittest, UnverifiedExtension) {
  TestExtensionDir temp_dir;
  content_verifier_delegate()->SetVerifierSourceType(
      ContentVerifierDelegate::VerifierSourceType::UNSIGNED_HASHES);

  const base::FilePath kResourceOkPath(FILE_PATH_LITERAL("script-ok.js"));
  const base::FilePath kResourceCorruptedPath(
      FILE_PATH_LITERAL("script-corrupted.js"));
  const base::FilePath kResourceMissingPath(
      FILE_PATH_LITERAL("script-missing.js"));
  const base::FilePath kResourceUnexpectedPath(
      FILE_PATH_LITERAL("script-unexpected.js"));

  const std::string kOkContents = "console.log('Nothing special');";
  const std::string kCorruptedContents = "alert('Evil corrupted script');";

  std::map<base::FilePath, std::string> resource_map = {
      {kResourceOkPath, kOkContents}, {kResourceCorruptedPath, kOkContents}};
  scoped_refptr<Extension> extension =
      CreateAndLoadTestExtensionToTempDir(&temp_dir, std::move(resource_map));
  ASSERT_TRUE(extension);
  base::FilePath unzipped_path = temp_dir.UnpackedPath();

  ASSERT_TRUE(
      base::WriteFile(unzipped_path.Append(kResourceOkPath), kOkContents));
  ASSERT_TRUE(base::WriteFile(unzipped_path.Append(kResourceCorruptedPath),
                              kCorruptedContents));
  ASSERT_TRUE(base::WriteFile(unzipped_path.Append(kResourceUnexpectedPath),
                              kOkContents));

  {
    // Sanity check that an unmodified file passes content verification.
    std::string contents;
    base::ReadFileToString(unzipped_path.Append(kResourceOkPath), &contents);
    EXPECT_EQ(ContentVerifyJob::NONE,
              RunContentVerifyJob(*extension.get(), kResourceOkPath, contents));
  }
  {
    // Make sure a file with incorrect content (eg. corrupted one) fails content
    // verification.
    std::string contents;
    base::ReadFileToString(unzipped_path.Append(kResourceCorruptedPath),
                           &contents);
    EXPECT_EQ(ContentVerifyJob::HASH_MISMATCH,
              RunContentVerifyJob(*extension.get(), kResourceCorruptedPath,
                                  contents));
  }
  {
    // Make sure non-existing file doesn't fail content verification.
    EXPECT_EQ(ContentVerifyJob::NONE,
              RunContentVerifyJob(*extension.get(), kResourceMissingPath,
                                  std::nullopt));
  }
  {
    // Make sure existing file fail content verification if there is no entry
    // for it in computed_hashes.json.
    std::string contents;
    base::ReadFileToString(unzipped_path.Append(kResourceUnexpectedPath),
                           &contents);
    EXPECT_EQ(ContentVerifyJob::NO_HASHES_FOR_FILE,
              RunContentVerifyJob(*extension.get(), kResourceUnexpectedPath,
                                  contents));
  }
}

// Tests that extension without any hashes (both verified_contents.json and
// computed_hashes.json are missing) is checked properly.
TEST_F(ContentVerifyJobWithoutSignedHashesUnittest, ExtensionWithoutHashes) {
  TestExtensionDir temp_dir;
  content_verifier_delegate()->SetVerifierSourceType(
      ContentVerifierDelegate::VerifierSourceType::UNSIGNED_HASHES);

  const base::FilePath kResourcePath(FILE_PATH_LITERAL("script-ok.js"));

  scoped_refptr<Extension> extension =
      CreateAndLoadTestExtensionToTempDir(&temp_dir, std::nullopt);
  ASSERT_TRUE(extension);
  base::FilePath unzipped_path = temp_dir.UnpackedPath();
  const std::string kContents = "console.log('Nothing special');";
  ASSERT_TRUE(base::WriteFile(unzipped_path.Append(kResourcePath), kContents));

  {
    // Make sure good file passes content verification.
    std::string contents;
    base::ReadFileToString(unzipped_path.Append(kResourcePath), &contents);
    EXPECT_EQ(ContentVerifyJob::MISSING_ALL_HASHES,
              RunContentVerifyJob(*extension.get(), kResourcePath, contents));
    // Make sure that computed_hashes.json was not created. If we create
    // computed_hashes.json at this stage, we may get there hashes of
    // already-corrupted files. We can only computed hashes upon installation,
    // if these hashes are not signed.
    EXPECT_FALSE(
        base::PathExists(file_util::GetComputedHashesPath(extension->path())));
  }
}

class ContentMismatchUnittest
    : public ContentVerifyJobUnittest,
      public testing::WithParamInterface<ContentVerifyJobAsyncRunMode> {
 public:
  ContentMismatchUnittest() {}

  ContentMismatchUnittest(const ContentMismatchUnittest&) = delete;
  ContentMismatchUnittest& operator=(const ContentMismatchUnittest&) = delete;

 protected:
  // Runs test to verify that a modified extension resource (background.js)
  // causes ContentVerifyJob to fail with HASH_MISMATCH. The string
  // |content_to_append_for_mismatch| is appended to the resource for
  // modification. The asynchronous nature of ContentVerifyJob can be controlled
  // by |run_mode|.
  void RunContentMismatchTest(const std::string& content_to_append_for_mismatch,
                              ContentVerifyJobAsyncRunMode run_mode) {
    TestExtensionDir temp_dir;
    scoped_refptr<Extension> extension = LoadTestExtensionFromZipPathToTempDir(
        &temp_dir, "with_verified_contents", "source_all.zip");
    ASSERT_TRUE(extension.get());
    base::FilePath unzipped_path = temp_dir.UnpackedPath();

    const base::FilePath::CharType kResource[] =
        FILE_PATH_LITERAL("background.js");
    base::FilePath existent_resource_path(kResource);
    {
      // Make sure modified background.js fails content verification.
      std::string modified_contents;
      base::ReadFileToString(unzipped_path.Append(existent_resource_path),
                             &modified_contents);
      modified_contents.append(content_to_append_for_mismatch);
      EXPECT_EQ(ContentVerifyJob::HASH_MISMATCH,
                RunContentVerifyJob(*extension.get(), existent_resource_path,
                                    modified_contents, run_mode));
    }
  }
};

INSTANTIATE_TEST_SUITE_P(ContentVerifyJobUnittest,
                         ContentMismatchUnittest,
                         testing::Values(kNone,
                                         kContentReadBeforeHashesReady,
                                         kHashesReadyBeforeContentRead));

// Tests that content modification causes content verification failure.
TEST_P(ContentMismatchUnittest, ContentMismatch) {
  RunContentMismatchTest("console.log('modified');", GetParam());
}

// Similar to ContentMismatch, but uses a file size > 4k.
// Regression test for https://crbug.com/804630.
TEST_P(ContentMismatchUnittest, ContentMismatchWithLargeFile) {
  std::string content_larger_than_block_size(
      extension_misc::kContentVerificationDefaultBlockSize + 1, ';');
  RunContentMismatchTest(content_larger_than_block_size, GetParam());
}

// ContentVerifyJobUnittest with hash fetch interception support.
class ContentVerifyJobWithHashFetchUnittest : public ContentVerifyJobUnittest {
 public:
  ContentVerifyJobWithHashFetchUnittest()
      : hash_fetch_interceptor_(base::BindRepeating(
            &ContentVerifyJobWithHashFetchUnittest::InterceptHashFetch,
            base::Unretained(this))) {}

  ContentVerifyJobWithHashFetchUnittest(
      const ContentVerifyJobWithHashFetchUnittest&) = delete;
  ContentVerifyJobWithHashFetchUnittest& operator=(
      const ContentVerifyJobWithHashFetchUnittest&) = delete;

 protected:
  // Responds to hash fetch request.
  void RespondToClientIfReady() {
    DCHECK(verified_contents_);
    if (!client_ || !ready_to_respond_) {
      return;
    }
    content::URLLoaderInterceptor::WriteResponse(
        std::string(), *verified_contents_, client_.get());
  }

  void ForceHashFetchOnNextResourceLoad(const Extension& extension) {
    // We need to store verified_contents.json's contents so that
    // hash_fetch_interceptor_ can serve its request.
    verified_contents_ = GetVerifiedContents(extension);

    // Delete verified_contents.json.
    EXPECT_TRUE(base::DeletePathRecursively(
        file_util::GetVerifiedContentsPath(extension.path())));

    // Clear cache so that next extension resource load will fetch hashes as
    // we've already deleted verified_contents.json.
    // Use this opportunity to
    base::RunLoop run_loop;
    content::GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(
            [](scoped_refptr<ContentVerifier> content_verifier) {
              content_verifier->ClearCacheForTesting();
            },
            content_verifier()),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  ContentVerifyJob::FailureReason RunContentVerifyJobBeforeHashReady(
      scoped_refptr<Extension> extension,
      const base::FilePath& resource_path,
      MojoResult read_result) {
    // First, make sure that next ContentVerifyJob run requires a hash fetch, so
    // that we can delay its request's response using |hash_fetch_interceptor_|.
    ForceHashFetchOnNextResourceLoad(*extension);

    TestContentVerifySingleJobObserver observer(extension->id(), resource_path);
    {
      // Then ContentVerifyJob gets the read result.
      scoped_refptr<ContentVerifyJob> verify_job =
          base::MakeRefCounted<ContentVerifyJob>(
              extension->id(), extension->path(), resource_path);
      auto do_read_and_done =
          [](scoped_refptr<ContentVerifyJob> job,
             scoped_refptr<ContentVerifier> content_verifier,
             scoped_refptr<Extension> extension, MojoResult read_result,
             base::OnceClosure done_callback) {
            DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
            job->Start(content_verifier.get(), extension->version(),
                       extension->manifest_version(), base::DoNothing());
            job->BytesRead(nullptr, 0u, read_result);
            job->DoneReading();
            std::move(done_callback).Run();
          };

      base::RunLoop run_loop;
      content::GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(do_read_and_done, verify_job, content_verifier(),
                         extension, read_result, run_loop.QuitClosure()));
      run_loop.Run();

      // After read result is seen, finally serve hash to |verify_job|.
      set_ready_to_respond();
      RespondToClientIfReady();
    }
    return observer.WaitForJobFinished();
  }

  void set_ready_to_respond() { ready_to_respond_ = true; }

 private:
  bool InterceptHashFetch(
      content::URLLoaderInterceptor::RequestParams* params) {
    if (params->url_request.url.path_piece() != "/getsignature") {
      return false;
    }

    client_ = std::move(params->client);
    RespondToClientIfReady();

    return true;
  }

  // Used to serve potentially delayed response to verified_contents.json.
  content::URLLoaderInterceptor hash_fetch_interceptor_;
  mojo::Remote<network::mojom::URLLoaderClient> client_;

  // Whether or not |client_| can respond to hash fetch request.
  bool ready_to_respond_ = false;

  // Copy of the contents of verified_contents.json.
  std::optional<std::string> verified_contents_;
};

// Regression test for https://crbug.com/995436.
TEST_F(ContentVerifyJobWithHashFetchUnittest, ReadErrorBeforeHashReady) {
  TestExtensionDir temp_dir;
  scoped_refptr<Extension> extension = LoadTestExtensionFromZipPathToTempDir(
      &temp_dir, "with_verified_contents", "source_all.zip");
  ASSERT_TRUE(extension.get());

  const base::FilePath::CharType kBackgroundJS[] =
      FILE_PATH_LITERAL("background.js");
  base::FilePath resource_path(kBackgroundJS);

  EXPECT_EQ(ContentVerifyJob::NONE,
            RunContentVerifyJobBeforeHashReady(extension, resource_path,
                                               MOJO_RESULT_ABORTED));
}

// Now create a resource foo.js which exists on disk but is not in the
// extension's verified_contents.json. Test the flow of the file being read
// before hashes are ready. Verification should result in NO_HASHES_FOR_FILE
// since the extension is trying to load a file the extension should not have.
TEST_F(ContentVerifyJobWithHashFetchUnittest,
       ReadFileWithoutHashesBeforeHashReady) {
  TestExtensionDir temp_dir;
  scoped_refptr<Extension> extension = LoadTestExtensionFromZipPathToTempDir(
      &temp_dir, "with_verified_contents", "source_all.zip");
  ASSERT_TRUE(extension.get());
  base::FilePath unzipped_path = temp_dir.UnpackedPath();

  const base::FilePath::CharType kUnexpectedResource[] =
      FILE_PATH_LITERAL("foo.js");
  base::FilePath unexpected_resource_path(kUnexpectedResource);

  base::FilePath full_path = unzipped_path.Append(unexpected_resource_path);
  EXPECT_TRUE(base::WriteFile(full_path, "42"));

  EXPECT_EQ(ContentVerifyJob::NO_HASHES_FOR_FILE,
            RunContentVerifyJobBeforeHashReady(
                extension, unexpected_resource_path, MOJO_RESULT_OK));

  // Aborting the read shouldn't result in a verification failure.
  EXPECT_EQ(ContentVerifyJob::NONE,
            RunContentVerifyJobBeforeHashReady(
                extension, unexpected_resource_path, MOJO_RESULT_ABORTED));
}

}  // namespace extensions
