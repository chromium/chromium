// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "base/version.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/browser/content_verifier/test_utils.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/file_util.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_util.h"
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

}  // namespace

class ContentVerifyJobUnittest : public ExtensionsTest {
 public:
  ContentVerifyJobUnittest()
      // The TestBrowserThreadBundle is needed for ContentVerifyJob::Start().
      : resource_context_(&test_url_request_context_) {}
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

    extension_info_map_ = new InfoMap();
    net::URLRequestContext* request_context =
        resource_context_.GetRequestContext();
    old_factory_ = request_context->job_factory();
    content_verifier_ = new ContentVerifier(
        &testing_context_, std::make_unique<MockContentVerifierDelegate>());
    extension_info_map_->SetContentVerifier(content_verifier_.get());
  }

  void TearDown() override {
    net::URLRequestContext* request_context =
        resource_context_.GetRequestContext();
    request_context->set_job_factory(old_factory_);
    content_verifier_->Shutdown();

    ExtensionsTest::TearDown();
  }

  ContentVerifier* content_verifier() { return content_verifier_.get(); }

 protected:
  ContentVerifyJob::FailureReason RunContentVerifyJob(
      const Extension& extension,
      const base::FilePath& resource_path,
      std::string& resource_contents,
      ContentVerifyJobAsyncRunMode run_mode) {
    TestContentVerifySingleJobObserver observer(extension.id(), resource_path);
    scoped_refptr<ContentVerifyJob> verify_job = new ContentVerifyJob(
        extension.id(), extension.version(), extension.path(), resource_path,
        base::DoNothing());

    auto run_content_read_step = [](ContentVerifyJob* verify_job,
                                    std::string* resource_contents) {
      // Simulate serving |resource_contents| from |resource_path|.
      verify_job->BytesRead(resource_contents->size(),
                            base::data(*resource_contents));
      verify_job->DoneReading();
    };

    switch (run_mode) {
      case kNone:
        StartJob(verify_job);  // Read hashes asynchronously.
        run_content_read_step(verify_job.get(), &resource_contents);
        break;
      case kContentReadBeforeHashesReady:
        run_content_read_step(verify_job.get(), &resource_contents);
        StartJob(verify_job);  // Read hashes asynchronously.
        break;
      case kHashesReadyBeforeContentRead:
        StartJob(verify_job);
        // Wait for hashes to become ready.
        observer.WaitForOnHashesReady();
        run_content_read_step(verify_job.get(), &resource_contents);
        break;
    };
    return observer.WaitForJobFinished();
  }

  ContentVerifyJob::FailureReason RunContentVerifyJob(
      const Extension& extension,
      const base::FilePath& resource_path,
      std::string& resource_contents) {
    return RunContentVerifyJob(extension, resource_path, resource_contents,
                               kNone);
  }

  // Returns an extension after extracting and loading it from a .zip file.
  // The extension is expected to have verified_contents.json in it.
  scoped_refptr<Extension> LoadTestExtensionFromZipPathToTempDir(
      base::ScopedTempDir* temp_dir,
      const std::string& zip_directory_name,
      const std::string& zip_filename) {
    if (!temp_dir->CreateUniqueTempDir()) {
      ADD_FAILURE() << "Failed to create temp dir.";
      return nullptr;
    }
    base::FilePath unzipped_path = temp_dir->GetPath();
    base::FilePath test_dir_base = GetTestPath(zip_directory_name);
    scoped_refptr<Extension> extension =
        content_verifier_test_utils::UnzipToDirAndLoadExtension(
            test_dir_base.AppendASCII(zip_filename), unzipped_path);
    // Make sure there is a verified_contents.json file there as this test
    // cannot fetch it.
    if (extension && !base::PathExists(file_util::GetVerifiedContentsPath(
                         extension->path()))) {
      ADD_FAILURE() << "verified_contents.json not found.";
      return nullptr;
    }
    return extension;
  }

 private:
  void StartJob(scoped_refptr<ContentVerifyJob> job) {
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(&ContentVerifyJob::Start, job,
                       base::Unretained(content_verifier_.get())));
  }

  scoped_refptr<InfoMap> extension_info_map_;
  net::URLRequestJobFactoryImpl job_factory_;
  const net::URLRequestJobFactory* old_factory_;
  net::TestURLRequestContext test_url_request_context_;
  content::MockResourceContext resource_context_;
  scoped_refptr<ContentVerifier> content_verifier_;
  content::TestBrowserContext testing_context_;

  DISALLOW_COPY_AND_ASSIGN(ContentVerifyJobUnittest);
};

// Tests that deleted legitimate files trigger content verification failure.
// Also tests that non-existent file request does not trigger content
// verification failure.
TEST_F(ContentVerifyJobUnittest, DeletedAndMissingFiles) {
  base::ScopedTempDir temp_dir;
  scoped_refptr<Extension> extension = LoadTestExtensionFromZipPathToTempDir(
      &temp_dir, "with_verified_contents", "source_all.zip");
  ASSERT_TRUE(extension.get());
  base::FilePath unzipped_path = temp_dir.GetPath();

  const base::FilePath::CharType kExistentResource[] =
      FILE_PATH_LITERAL("background.js");
  base::FilePath existent_resource_path(kExistentResource);
  {
    // Make sure background.js passes verification correctly.
    std::string contents;
    base::ReadFileToString(
        unzipped_path.Append(base::FilePath(kExistentResource)), &contents);
    EXPECT_EQ(ContentVerifyJob::NONE,
              RunContentVerifyJob(*extension.get(), existent_resource_path,
                                  contents));
  }

  {
    // Once background.js is deleted, verification will result in HASH_MISMATCH.
    // Delete the existent file first.
    EXPECT_TRUE(base::DeleteFile(
        unzipped_path.Append(base::FilePath(kExistentResource)), false));

    // Deleted file will serve empty contents.
    std::string empty_contents;
    EXPECT_EQ(ContentVerifyJob::HASH_MISMATCH,
              RunContentVerifyJob(*extension.get(), existent_resource_path,
                                  empty_contents));
  }

  {
    // Now ask for a non-existent resource non-existent.js. Verification should
    // skip this file as it is not listed in our verified_contents.json file.
    const base::FilePath::CharType kNonExistentResource[] =
        FILE_PATH_LITERAL("non-existent.js");
    base::FilePath non_existent_resource_path(kNonExistentResource);
    // Non-existent file will serve empty contents.
    std::string empty_contents;
    EXPECT_EQ(ContentVerifyJob::NONE,
              RunContentVerifyJob(*extension.get(), non_existent_resource_path,
                                  empty_contents));
  }

  {
    // Now create a resource foo.js which exists on disk but is not in the
    // extension's verified_contents.json. Verification should result in
    // NO_HASHES_FOR_FILE since the extension is trying to load a file the
    // extension should not have.
    const base::FilePath::CharType kUnexpectedResource[] =
        FILE_PATH_LITERAL("foo.js");
    base::FilePath unexpected_resource_path(kUnexpectedResource);

    base::FilePath full_path =
        unzipped_path.Append(base::FilePath(unexpected_resource_path));
    const std::string kContent("42");
    EXPECT_EQ(static_cast<int>(kContent.size()),
              base::WriteFile(full_path, kContent.data(), kContent.size()));

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
    std::string empty_contents;
    EXPECT_EQ(ContentVerifyJob::NONE,
              RunContentVerifyJob(*extension.get(), empty_path_resource_path,
                                  empty_contents));
  }

  {
    // Ask for the path of one of the extension's folders which exists on disk.
    // Verification of the folder should skip the request as if the folder
    // was non-existent. See https://crbug.com/791929.
    const base::FilePath::CharType kUnexpectedFolder[] =
        FILE_PATH_LITERAL("bar/");
    base::FilePath unexpected_folder_path(kUnexpectedFolder);

    base::CreateDirectory(unzipped_path.Append(unexpected_folder_path));
    std::string empty_contents;
    EXPECT_EQ(ContentVerifyJob::NONE,
              RunContentVerifyJob(*extension.get(), unexpected_folder_path,
                                  empty_contents));
  }
}

// Tests that extension resources that are originally 0 byte behave correctly
// with content verification.
TEST_F(ContentVerifyJobUnittest, LegitimateZeroByteFile) {
  base::ScopedTempDir temp_dir;
  // |extension| has a 0 byte background.js file in it.
  scoped_refptr<Extension> extension = LoadTestExtensionFromZipPathToTempDir(
      &temp_dir, "zero_byte_file", "source.zip");
  ASSERT_TRUE(extension.get());
  base::FilePath unzipped_path = temp_dir.GetPath();

  const base::FilePath::CharType kResource[] =
      FILE_PATH_LITERAL("background.js");
  base::FilePath resource_path(kResource);
  {
    // Make sure 0 byte background.js passes content verification.
    std::string contents;
    base::ReadFileToString(unzipped_path.Append(base::FilePath(kResource)),
                           &contents);
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
  base::ScopedTempDir temp_dir;
  scoped_refptr<Extension> extension = LoadTestExtensionFromZipPathToTempDir(
      &temp_dir, "different_sized_files", "source.zip");
  ASSERT_TRUE(extension.get());
  base::FilePath unzipped_path = temp_dir.GetPath();

  const struct {
    const char* name;
    size_t byte_size;
  } kFilesToTest[] = {
      {"1024.js", 1024}, {"4096.js", 4096}, {"8192.js", 8192},
      {"8191.js", 8191}, {"8193.js", 8193},
  };
  for (const auto& file_to_test : kFilesToTest) {
    base::FilePath resource_path =
        base::FilePath::FromUTF8Unsafe(file_to_test.name);
    std::string contents;
    base::ReadFileToString(unzipped_path.AppendASCII(file_to_test.name),
                           &contents);
    EXPECT_EQ(file_to_test.byte_size, contents.size());
    EXPECT_EQ(ContentVerifyJob::NONE,
              RunContentVerifyJob(*extension.get(), resource_path, contents));
  }
}

class ContentMismatchUnittest
    : public ContentVerifyJobUnittest,
      public testing::WithParamInterface<ContentVerifyJobAsyncRunMode> {
 public:
  ContentMismatchUnittest() {}

 protected:
  // Runs test to verify that a modified extension resource (background.js)
  // causes ContentVerifyJob to fail with HASH_MISMATCH. The string
  // |content_to_append_for_mismatch| is appended to the resource for
  // modification. The asynchronous nature of ContentVerifyJob can be controlled
  // by |run_mode|.
  void RunContentMismatchTest(const std::string& content_to_append_for_mismatch,
                              ContentVerifyJobAsyncRunMode run_mode) {
    base::ScopedTempDir temp_dir;
    scoped_refptr<Extension> extension = LoadTestExtensionFromZipPathToTempDir(
        &temp_dir, "with_verified_contents", "source_all.zip");
    ASSERT_TRUE(extension.get());
    base::FilePath unzipped_path = temp_dir.GetPath();

    const base::FilePath::CharType kResource[] =
        FILE_PATH_LITERAL("background.js");
    base::FilePath existent_resource_path(kResource);
    {
      // Make sure modified background.js fails content verification.
      std::string modified_contents;
      base::ReadFileToString(unzipped_path.Append(base::FilePath(kResource)),
                             &modified_contents);
      modified_contents.append(content_to_append_for_mismatch);
      EXPECT_EQ(ContentVerifyJob::HASH_MISMATCH,
                RunContentVerifyJob(*extension.get(), existent_resource_path,
                                    modified_contents, run_mode));
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentMismatchUnittest);
};

INSTANTIATE_TEST_CASE_P(ContentVerifyJobUnittest,
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

}  // namespace extensions
