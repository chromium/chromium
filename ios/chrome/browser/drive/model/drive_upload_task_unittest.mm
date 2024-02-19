// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/drive_upload_task.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/download/model/download_mimetype_util.h"
#import "ios/chrome/browser/drive/model/drive_metrics.h"
#import "ios/chrome/browser/drive/model/test_drive_file_uploader.h"
#import "ios/chrome/browser/drive/model/test_upload_task_observer.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "net/base/url_util.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using State = UploadTask::State;

// DriveUploadTask unit tests.
class DriveUploadTaskTest : public PlatformTest {
 protected:
  void SetUp() final {
    PlatformTest::SetUp();
    uploading_identity_ = [FakeSystemIdentity fakeIdentity1];
    auto uploader =
        std::make_unique<TestDriveFileUploader>(uploading_identity_);
    uploader_ = uploader.get();
    task_ = std::make_unique<DriveUploadTask>(std::move(uploader));
    observer_ = std::make_unique<TestUploadTaskObserver>();
    task_->AddObserver(observer_.get());
  }

  using TaskEnvironment = base::test::TaskEnvironment;
  TaskEnvironment task_environment_{TaskEnvironment::TimeSource::MOCK_TIME};
  id<SystemIdentity> uploading_identity_;
  raw_ptr<TestDriveFileUploader> uploader_;
  std::unique_ptr<TestUploadTaskObserver> observer_;
  std::unique_ptr<DriveUploadTask> task_;
};

// Tests that upon first starting and later cancelling an upload task, the state
// of the task is correctly updated.
TEST_F(DriveUploadTaskTest, TaskCanBeStartedAndCancelled) {
  EXPECT_EQ(0, task_->GetProgress());
  EXPECT_EQ(State::kNotStarted, task_->GetState());
  EXPECT_EQ(nil, task_->GetError());
  EXPECT_EQ(std::nullopt, task_->GetResponseLink());
  EXPECT_FALSE(task_->IsDone());
  // Starting the task.
  EXPECT_EQ(nullptr, observer_->GetUpdatedUpload());
  task_->Start();
  EXPECT_EQ(task_.get(), observer_->GetUpdatedUpload());
  observer_->ResetUpdatedUpload();
  EXPECT_EQ(State::kInProgress, task_->GetState());
  // Cancelling the task.
  EXPECT_EQ(nullptr, observer_->GetUpdatedUpload());
  task_->Cancel();
  EXPECT_EQ(task_.get(), observer_->GetUpdatedUpload());
  EXPECT_EQ(State::kCancelled, task_->GetState());
  EXPECT_TRUE(task_->IsDone());
}

// Tests that if the destination folder does *NOT* exist, the task will create
// it and upload the file as expected.
TEST_F(DriveUploadTaskTest, CreatesFolderIfNotFound) {
  base::HistogramTester histogram_tester;
  // Set up the uploader to simulate empty search results, a successful folder
  // creation and file upload progress and successful completion.
  uploader_->SetFolderSearchResult({.folder_identifier = nil, .error = nil});
  uploader_->SetFolderCreationResult(
      {.folder_identifier = @"test_folder_identifier", .error = nil});
  const std::vector<DriveFileUploadProgress> progress_elements{
      {0, 100}, {10, 100}, {25, 100}, {50, 100}, {99, 100}, {100, 100},
  };
  uploader_->SetFileUploadProgressElements(progress_elements);
  const char* response_link_str = "https://test_response.file_link";
  const GURL response_link(response_link_str);
  const GURL response_link_with_identifier = net::AppendOrReplaceQueryParameter(
      response_link, "huid",
      base::SysNSStringToUTF8(uploader_->GetIdentity().hashedGaiaID));
  uploader_->SetFileUploadResult(
      { .file_link = @(response_link_str), .error = nil });
  // Set up the task with the name of the parent folder as well as the path,
  // suggested name and MIME type of the file to upload.
  task_->SetDestinationFolderName("test_folder_name");
  const base::FilePath file_path_to_upload{"/test/path/of/file/to/upload"};
  const base::FilePath file_suggested_name{"test_uploaded_file_name"};
  task_->SetFileToUpload(file_path_to_upload, file_suggested_name,
                         "test_mime_type", 42);
  // Test that a folder with appropriate name is searched.
  uploader_->SetSearchFolderQuitClosure(task_environment_.QuitClosure());
  EXPECT_EQ(State::kNotStarted, task_->GetState());
  task_->Start();
  EXPECT_EQ(State::kInProgress, task_->GetState());
  task_environment_.RunUntilQuit();
  EXPECT_NSEQ(@"test_folder_name", uploader_->GetSearchedFolderName());
  // Test that a folder with appropriate name is created.
  uploader_->SetCreateFolderQuitClosure(task_environment_.QuitClosure());
  task_environment_.RunUntilQuit();
  EXPECT_NSEQ(@"test_folder_name", uploader_->GetCreatedFolderName());
  // Test that the file parameters provided earlier are forwarded to the
  // uploader.
  EXPECT_NSEQ(base::apple::FilePathToNSURL(file_path_to_upload),
              uploader_->GetUploadedFileUrl());
  EXPECT_NSEQ(base::apple::FilePathToNSString(file_suggested_name),
              uploader_->GetUploadedFileName());
  EXPECT_NSEQ(@"test_mime_type", uploader_->GetUploadedFileMimeType());
  // Test that the parent folder identifier is the one returned by the uploader.
  EXPECT_NSEQ(@"test_folder_identifier",
              uploader_->GetUploadedFileFolderIdentifier());
  // Test that progress is reported as expected.
  for (const DriveFileUploadProgress& progress : progress_elements) {
    uploader_->SetUploadFileProgressQuitClosure(
        task_environment_.QuitClosure());
    observer_->ResetUpdatedUpload();
    task_environment_.RunUntilQuit();
    EXPECT_EQ(task_.get(), observer_->GetUpdatedUpload());
    const float progress_float =
        static_cast<float>(progress.total_bytes_uploaded) /
        progress.total_bytes_expected_to_upload;
    EXPECT_EQ(progress_float, task_->GetProgress());
  }
  // Test that the result is reported as expected.
  uploader_->SetUploadFileCompletionQuitClosure(
      task_environment_.QuitClosure());
  observer_->ResetUpdatedUpload();
  task_environment_.RunUntilQuit();
  EXPECT_EQ(task_.get(), observer_->GetUpdatedUpload());
  EXPECT_EQ(response_link,
            task_->GetResponseLink(/* add_user_identifier= */ false));
  EXPECT_EQ(response_link_with_identifier,
            task_->GetResponseLink(/* add_user_identifier= */ true));
  EXPECT_NSEQ(nil, task_->GetError());
  EXPECT_EQ(State::kComplete, task_->GetState());
  // Test that expected histograms were recorded.
  histogram_tester.ExpectUniqueSample(kDriveSearchFolderResultSuccessful, true,
                                      1);
  histogram_tester.ExpectUniqueSample(kDriveCreateFolderResultSuccessful, true,
                                      1);
  histogram_tester.ExpectUniqueSample(kDriveFileUploadResultSuccessful, true,
                                      1);
}

// Tests that if the destination folder *DOES* exist, the task will use it as-is
// and upload the file as expected.
TEST_F(DriveUploadTaskTest, UsesExistingFolderIfFound) {
  base::HistogramTester histogram_tester;
  // Set up the uploader to simulate non-empty search result, and file upload
  // progress and successful completion.
  uploader_->SetFolderSearchResult(
      {.folder_identifier = @"test_folder_identifier", .error = nil});
  const std::vector<DriveFileUploadProgress> progress_elements{
      {0, 100}, {10, 100}, {25, 100}, {50, 100}, {99, 100}, {100, 100},
  };
  uploader_->SetFileUploadProgressElements(progress_elements);
  const char* response_link_str = "https://test_response.file_link";
  const GURL response_link(response_link_str);
  const GURL response_link_with_identifier = net::AppendOrReplaceQueryParameter(
      response_link, "huid",
      base::SysNSStringToUTF8(uploader_->GetIdentity().hashedGaiaID));
  uploader_->SetFileUploadResult(
      { .file_link = @(response_link_str), .error = nil });
  // Set up the task with the name of the parent folder as well as the path,
  // suggested name and MIME type of the file to upload.
  task_->SetDestinationFolderName("test_folder_name");
  const base::FilePath file_path_to_upload{"/test/path/of/file/to/upload"};
  const base::FilePath file_suggested_name{"test_uploaded_file_name"};
  task_->SetFileToUpload(file_path_to_upload, file_suggested_name,
                         "test_mime_type", 42);
  // Test that a folder with appropriate name is searched.
  uploader_->SetSearchFolderQuitClosure(task_environment_.QuitClosure());
  EXPECT_EQ(State::kNotStarted, task_->GetState());
  task_->Start();
  EXPECT_EQ(State::kInProgress, task_->GetState());
  task_environment_.RunUntilQuit();
  EXPECT_NSEQ(@"test_folder_name", uploader_->GetSearchedFolderName());
  // Test that the file parameters provided earlier are forwarded to the
  // uploader.
  EXPECT_NSEQ(base::apple::FilePathToNSURL(file_path_to_upload),
              uploader_->GetUploadedFileUrl());
  EXPECT_NSEQ(base::apple::FilePathToNSString(file_suggested_name),
              uploader_->GetUploadedFileName());
  EXPECT_NSEQ(@"test_mime_type", uploader_->GetUploadedFileMimeType());
  // Test that the parent folder identifier is the one returned by the uploader.
  EXPECT_NSEQ(@"test_folder_identifier",
              uploader_->GetUploadedFileFolderIdentifier());
  // Test that progress is reported as expected.
  for (const DriveFileUploadProgress& progress : progress_elements) {
    uploader_->SetUploadFileProgressQuitClosure(
        task_environment_.QuitClosure());
    observer_->ResetUpdatedUpload();
    task_environment_.RunUntilQuit();
    EXPECT_EQ(task_.get(), observer_->GetUpdatedUpload());
    const float progress_float =
        static_cast<float>(progress.total_bytes_uploaded) /
        progress.total_bytes_expected_to_upload;
    EXPECT_EQ(progress_float, task_->GetProgress());
  }
  // Test that the result is reported as expected.
  uploader_->SetUploadFileCompletionQuitClosure(
      task_environment_.QuitClosure());
  observer_->ResetUpdatedUpload();
  task_environment_.RunUntilQuit();
  EXPECT_EQ(task_.get(), observer_->GetUpdatedUpload());
  EXPECT_EQ(response_link,
            task_->GetResponseLink(/* add_user_identifier= */ false));
  EXPECT_EQ(response_link_with_identifier,
            task_->GetResponseLink(/* add_user_identifier= */ true));
  EXPECT_NSEQ(nil, task_->GetError());
  EXPECT_EQ(State::kComplete, task_->GetState());
  // Test that expected histograms were recorded.
  histogram_tester.ExpectUniqueSample(kDriveSearchFolderResultSuccessful, true,
                                      1);
  histogram_tester.ExpectUniqueSample(kDriveCreateFolderResultSuccessful, true,
                                      0);
  histogram_tester.ExpectUniqueSample(kDriveFileUploadResultSuccessful, true,
                                      1);
}

// Tests that if the file upload fails, failure is correctly reported.
TEST_F(DriveUploadTaskTest, ReportsFileUploadFailure) {
  base::HistogramTester histogram_tester;
  // Set up the uploader to simulate non-empty search result, and file upload
  // progress and unsuccessful completion.
  uploader_->SetFolderSearchResult(
      {.folder_identifier = @"test_folder_identifier", .error = nil});
  const std::vector<DriveFileUploadProgress> progress_elements{
      {0, 100},
      {10, 100},
      {25, 100},
      {50, 100},
  };
  uploader_->SetFileUploadProgressElements(progress_elements);
  NSError* file_upload_error = [NSError errorWithDomain:@"test_domain"
                                                   code:400
                                               userInfo:nil];
  uploader_->SetFileUploadResult(
      {.file_link = nil, .error = file_upload_error});
  // Set up the task with the name of the parent folder as well as the path,
  // suggested name and MIME type of the file to upload.
  task_->SetDestinationFolderName("test_folder_name");
  const base::FilePath file_path_to_upload{"/test/path/of/file/to/upload"};
  const base::FilePath file_suggested_name{"test_uploaded_file_name"};
  task_->SetFileToUpload(file_path_to_upload, file_suggested_name,
                         "test_mime_type", 42);
  // Test that a folder with appropriate name is searched.
  uploader_->SetSearchFolderQuitClosure(task_environment_.QuitClosure());
  EXPECT_EQ(State::kNotStarted, task_->GetState());
  task_->Start();
  EXPECT_EQ(State::kInProgress, task_->GetState());
  task_environment_.RunUntilQuit();
  EXPECT_NSEQ(@"test_folder_name", uploader_->GetSearchedFolderName());
  // Test that the file parameters provided earlier are forwarded to the
  // uploader.
  EXPECT_NSEQ(base::apple::FilePathToNSURL(file_path_to_upload),
              uploader_->GetUploadedFileUrl());
  EXPECT_NSEQ(base::apple::FilePathToNSString(file_suggested_name),
              uploader_->GetUploadedFileName());
  EXPECT_NSEQ(@"test_mime_type", uploader_->GetUploadedFileMimeType());
  // Test that the parent folder identifier is the one returned by the uploader.
  EXPECT_NSEQ(@"test_folder_identifier",
              uploader_->GetUploadedFileFolderIdentifier());
  // Test that progress is reported as expected.
  for (const DriveFileUploadProgress& progress : progress_elements) {
    uploader_->SetUploadFileProgressQuitClosure(
        task_environment_.QuitClosure());
    observer_->ResetUpdatedUpload();
    task_environment_.RunUntilQuit();
    EXPECT_EQ(task_.get(), observer_->GetUpdatedUpload());
    const float progress_float =
        static_cast<float>(progress.total_bytes_uploaded) /
        progress.total_bytes_expected_to_upload;
    EXPECT_EQ(progress_float, task_->GetProgress());
  }
  // Test that the result is reported as expected.
  uploader_->SetUploadFileCompletionQuitClosure(
      task_environment_.QuitClosure());
  observer_->ResetUpdatedUpload();
  task_environment_.RunUntilQuit();
  EXPECT_EQ(task_.get(), observer_->GetUpdatedUpload());
  EXPECT_EQ(std::nullopt, task_->GetResponseLink());
  EXPECT_NSEQ(file_upload_error, task_->GetError());
  EXPECT_EQ(State::kFailed, task_->GetState());
  // Test that expected histograms were recorded.
  histogram_tester.ExpectUniqueSample(kDriveSearchFolderResultSuccessful, true,
                                      1);
  histogram_tester.ExpectUniqueSample(kDriveCreateFolderResultSuccessful, true,
                                      0);
  histogram_tester.ExpectUniqueSample(kDriveFileUploadResultSuccessful, false,
                                      1);
  histogram_tester.ExpectUniqueSample(kDriveFileUploadResultErrorCode, 400, 1);
}

// Tests that a task that is destroyed before being started records the expected
// histograms.
TEST_F(DriveUploadTaskTest, TaskNotStartedDestructorRecordsMetrics) {
  base::HistogramTester histogram_tester;
  // Give file to task.
  task_->SetFileToUpload(base::FilePath("file_name.txt"),
                         base::FilePath("file_name.txt"), "text/plain",
                         500 * 1024);  // 500 KB
  // Destroy the upload task.
  task_.reset();
  // Check that all expected histograms are recorded.
  histogram_tester.ExpectUniqueSample(kDriveUploadTaskFinalState,
                                      UploadTaskStateHistogram::kNotStarted, 1);
  histogram_tester.ExpectUniqueSample(
      std::string(kDriveUploadTaskNumberOfAttempts) + ".NotStarted", 0, 1);
  histogram_tester.ExpectUniqueSample(
      std::string(kDriveUploadTaskMimeType) + ".NotStarted",
      DownloadMimeTypeResult::Text, 1);
  histogram_tester.ExpectUniqueSample(
      std::string(kDriveUploadTaskFileSize) + ".NotStarted", 0 /* MB */, 1);
}

// Tests that a task that is destroyed before being started records the expected
// histograms.
TEST_F(DriveUploadTaskTest, TaskInProgressDestructorRecordsMetrics) {
  base::HistogramTester histogram_tester;
  // Give file to task.
  task_->SetFileToUpload(base::FilePath("file_name.pdf"),
                         base::FilePath("file_name.pdf"), "application/pdf",
                         1 * 1024 * 1024);  // 1 MB
  // Start the task.
  task_->Start();
  // Destroy the upload task.
  task_.reset();
  // Check that all expected histograms are recorded.
  histogram_tester.ExpectUniqueSample(kDriveUploadTaskFinalState,
                                      UploadTaskStateHistogram::kCancelled, 1);
  histogram_tester.ExpectUniqueSample(
      std::string(kDriveUploadTaskNumberOfAttempts) + ".Cancelled", 1, 1);
  histogram_tester.ExpectUniqueSample(
      std::string(kDriveUploadTaskMimeType) + ".Cancelled",
      DownloadMimeTypeResult::AdobePortableDocumentFormat, 1);
  histogram_tester.ExpectUniqueSample(
      std::string(kDriveUploadTaskFileSize) + ".Cancelled", 1 /* MB */, 1);
}

// Tests that a task that is destroyed after being started and then cancelled
// records the expected histograms.
TEST_F(DriveUploadTaskTest, TaskCancelledDestructorRecordsMetrics) {
  base::HistogramTester histogram_tester;
  // Give file to task.
  task_->SetFileToUpload(base::FilePath("file_name.pdf"),
                         base::FilePath("file_name.pdf"), "application/pdf",
                         150 * 1024 * 1024);  // 150 MB
  // Start and then cancel the task.
  task_->Start();
  task_->Cancel();
  // Destroy the upload task.
  task_.reset();
  // Check that all expected histograms are recorded.
  histogram_tester.ExpectUniqueSample(kDriveUploadTaskFinalState,
                                      UploadTaskStateHistogram::kCancelled, 1);
  histogram_tester.ExpectUniqueSample(
      std::string(kDriveUploadTaskNumberOfAttempts) + ".Cancelled", 1, 1);
  histogram_tester.ExpectUniqueSample(
      std::string(kDriveUploadTaskMimeType) + ".Cancelled",
      DownloadMimeTypeResult::AdobePortableDocumentFormat, 1);
  histogram_tester.ExpectUniqueSample(
      std::string(kDriveUploadTaskFileSize) + ".Cancelled", 150 /* MB */, 1);
}

TEST_F(DriveUploadTaskTest, TaskCompleteDestructorRecordsMetrics) {
  base::HistogramTester histogram_tester;
  // Give file to task.
  task_->SetFileToUpload(base::FilePath("file_name.pdf"),
                         base::FilePath("file_name.pdf"), "application/pdf",
                         5 * 1024 * 1024 * 1024LL);  // 5 GB
  // Start the task and wait until file upload completion.
  task_->Start();
  uploader_->SetUploadFileCompletionQuitClosure(
      task_environment_.QuitClosure());
  task_environment_.RunUntilQuit();
  // Destroy the upload task.
  task_.reset();
  // Check that all expected histograms are recorded.
  histogram_tester.ExpectUniqueSample(kDriveUploadTaskFinalState,
                                      UploadTaskStateHistogram::kComplete, 1);
  histogram_tester.ExpectUniqueSample(
      std::string(kDriveUploadTaskNumberOfAttempts) + ".Complete", 1, 1);
  histogram_tester.ExpectUniqueSample(
      std::string(kDriveUploadTaskMimeType) + ".Complete",
      DownloadMimeTypeResult::AdobePortableDocumentFormat, 1);
  histogram_tester.ExpectUniqueSample(
      std::string(kDriveUploadTaskFileSize) + ".Complete", 5 * 1024 /* MB */,
      1);
}

TEST_F(DriveUploadTaskTest, TaskFailedDestructorRecordsMetrics) {
  base::HistogramTester histogram_tester;
  // Give file to task.
  task_->SetFileToUpload(base::FilePath("file_name.pdf"),
                         base::FilePath("file_name.pdf"), "application/pdf",
                         1);  // 1 B
  // Start the task and wait until file upload fails.
  task_->Start();
  uploader_->SetUploadFileCompletionQuitClosure(
      task_environment_.QuitClosure());
  NSError* file_upload_error = [NSError errorWithDomain:@"test_domain"
                                                   code:400
                                               userInfo:nil];
  uploader_->SetFileUploadResult(
      {.file_link = nil, .error = file_upload_error});
  task_environment_.RunUntilQuit();
  // Destroy the upload task.
  task_.reset();
  // Check that all expected histograms are recorded.
  histogram_tester.ExpectUniqueSample(kDriveUploadTaskFinalState,
                                      UploadTaskStateHistogram::kFailed, 1);
  histogram_tester.ExpectUniqueSample(
      std::string(kDriveUploadTaskNumberOfAttempts) + ".Failed", 1, 1);
  histogram_tester.ExpectUniqueSample(
      std::string(kDriveUploadTaskMimeType) + ".Failed",
      DownloadMimeTypeResult::AdobePortableDocumentFormat, 1);
  histogram_tester.ExpectUniqueSample(
      std::string(kDriveUploadTaskFileSize) + ".Failed", 0 /* MB */, 1);
}
