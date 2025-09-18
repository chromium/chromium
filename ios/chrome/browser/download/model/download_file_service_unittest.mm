// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_file_service.h"

#import <memory>
#import <string>

#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/functional/bind.h"
#import "base/run_loop.h"
#import "base/test/bind.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/download/model/download_directory_util.h"
#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/chrome/browser/download/model/download_record_service.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using testing::_;
using testing::StrictMock;

namespace {

const std::string kTestDownloadId = "test_download_id";
const char kTestFileName[] = "test_file.pdf";
const char kTestFileContent[] = "test file content";
const char kConflictFileName[] = "conflict_file.txt";

// Mock implementation of DownloadRecordService for testing.
class MockDownloadRecordService : public DownloadRecordService {
 public:
  MockDownloadRecordService() = default;
  ~MockDownloadRecordService() override = default;

  // Method that will be actually tested.
  MOCK_METHOD(void,
              UpdateDownloadFilePathAsync,
              (const std::string& download_id,
               const base::FilePath& file_path,
               CompletionCallback callback),
              (override));

  // Mock all other methods but don't set expectations for them.
  MOCK_METHOD(void, RecordDownload, (web::DownloadTask * task), (override));
  MOCK_METHOD(void,
              GetAllDownloadsAsync,
              (DownloadRecordsCallback callback),
              (override));
  MOCK_METHOD(void,
              GetDownloadByIdAsync,
              (const std::string& download_id, DownloadRecordCallback callback),
              (override));
  MOCK_METHOD(void,
              RemoveDownloadByIdAsync,
              (const std::string& download_id, CompletionCallback callback),
              (override));
  MOCK_METHOD(web::DownloadTask*,
              GetDownloadTaskById,
              (std::string_view download_id),
              (const, override));
  MOCK_METHOD(void,
              AddObserver,
              (DownloadRecordObserver * observer),
              (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (DownloadRecordObserver * observer),
              (override));
};

}  // namespace

class DownloadFileServiceTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    feature_list_.InitAndEnableFeature(kDownloadList);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Create strict mock download record service (fails on unexpected calls).
    mock_download_record_service_ =
        std::make_unique<StrictMock<MockDownloadRecordService>>();

    // Create service with mock dependency.
    service_ = std::make_unique<DownloadFileService>(
        mock_download_record_service_.get());

    // Create test directories.
    source_dir_ = temp_dir_.GetPath().AppendASCII("source");
    dest_dir_ = temp_dir_.GetPath().AppendASCII("destination");
    ASSERT_TRUE(base::CreateDirectory(source_dir_));
    ASSERT_TRUE(base::CreateDirectory(dest_dir_));

    // Set up test downloads directory override.
    test::SetDownloadsDirectoryForTesting(&dest_dir_);
  }

  void TearDown() override {
    // Reset downloads directory override.
    test::SetDownloadsDirectoryForTesting(nullptr);

    service_.reset();
    mock_download_record_service_.reset();
    PlatformTest::TearDown();
  }

  // Helper method to create a test file with given content.
  base::FilePath CreateTestFile(const base::FilePath& dir,
                                const std::string& filename,
                                const std::string& content) {
    base::FilePath file_path = dir.AppendASCII(filename);
    EXPECT_TRUE(base::WriteFile(file_path, content));
    EXPECT_TRUE(base::PathExists(file_path));
    return file_path;
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
  base::FilePath source_dir_;
  base::FilePath dest_dir_;
  std::unique_ptr<StrictMock<MockDownloadRecordService>>
      mock_download_record_service_;
  std::unique_ptr<DownloadFileService> service_;
};

// Tests moving a download file successfully.
TEST_F(DownloadFileServiceTest, MoveDownloadFileSuccess) {
  // Create source file.
  base::FilePath source_path =
      CreateTestFile(source_dir_, kTestFileName, kTestFileContent);
  base::FilePath dest_path = dest_dir_.AppendASCII(kTestFileName);

  // Expect UpdateDownloadFilePathAsync to be called.
  base::FilePath expected_relative_path =
      ConvertToRelativeDownloadPath(dest_path);
  EXPECT_CALL(
      *mock_download_record_service_,
      UpdateDownloadFilePathAsync(kTestDownloadId, expected_relative_path, _))
      .Times(1);

  base::RunLoop run_loop;
  bool callback_success = false;
  std::string callback_download_id;
  base::FilePath callback_source_path;
  base::FilePath callback_final_path;

  service_->MoveDownloadFile(
      kTestDownloadId, source_path, dest_path,
      base::BindLambdaForTesting([&](bool success,
                                     const std::string& download_id,
                                     const base::FilePath& actual_source_path,
                                     const base::FilePath& actual_final_path) {
        callback_success = success;
        callback_download_id = download_id;
        callback_source_path = actual_source_path;
        callback_final_path = actual_final_path;
        run_loop.Quit();
      }));

  run_loop.Run();

  // Verify callback results.
  EXPECT_TRUE(callback_success);
  EXPECT_EQ(kTestDownloadId, callback_download_id);
  EXPECT_EQ(source_path, callback_source_path);
  EXPECT_EQ(dest_path, callback_final_path);

  // Verify file operations.
  EXPECT_FALSE(base::PathExists(source_path));
  EXPECT_TRUE(base::PathExists(dest_path));

  // Verify file content.
  std::string content;
  EXPECT_TRUE(base::ReadFileToString(dest_path, &content));
  EXPECT_EQ(kTestFileContent, content);
}

// Tests moving a non-existent source file.
TEST_F(DownloadFileServiceTest, MoveDownloadFileNonExistentSource) {
  base::FilePath source_path = source_dir_.AppendASCII("non_existent.txt");
  base::FilePath dest_path = dest_dir_.AppendASCII(kTestFileName);

  // Should not call UpdateDownloadFilePathAsync on failure.
  EXPECT_CALL(*mock_download_record_service_,
              UpdateDownloadFilePathAsync(_, _, _))
      .Times(0);

  base::RunLoop run_loop;
  bool callback_success =
      true;  // Initialize to true to verify it becomes false

  service_->MoveDownloadFile(
      kTestDownloadId, source_path, dest_path,
      base::BindLambdaForTesting([&](bool success,
                                     const std::string& download_id,
                                     const base::FilePath& actual_source_path,
                                     const base::FilePath& actual_final_path) {
        callback_success = success;
        run_loop.Quit();
      }));

  run_loop.Run();

  EXPECT_FALSE(callback_success);
  EXPECT_FALSE(base::PathExists(dest_path));
}

// Tests moving a download file with empty download id.
TEST_F(DownloadFileServiceTest, MoveDownloadFileEmptyParameters) {
  base::RunLoop run_loop;
  bool callback_success = true;

  service_->MoveDownloadFile(
      "", source_dir_.AppendASCII("test.txt"),
      dest_dir_.AppendASCII("test.txt"),
      base::BindLambdaForTesting([&](bool success,
                                     const std::string& download_id,
                                     const base::FilePath& actual_source_path,
                                     const base::FilePath& actual_final_path) {
        callback_success = success;
        run_loop.Quit();
      }));

  run_loop.Run();
  EXPECT_FALSE(callback_success);
}

// Tests moving a download file without DownloadRecordService.
TEST_F(DownloadFileServiceTest, MoveDownloadFileWithoutRecordService) {
  // Create service without DownloadRecordService.
  auto service_without_record = std::make_unique<DownloadFileService>(nullptr);

  base::FilePath source_path =
      CreateTestFile(source_dir_, kTestFileName, kTestFileContent);
  base::FilePath dest_path = dest_dir_.AppendASCII(kTestFileName);

  base::RunLoop run_loop;
  bool callback_success = false;

  service_without_record->MoveDownloadFile(
      kTestDownloadId, source_path, dest_path,
      base::BindLambdaForTesting([&](bool success,
                                     const std::string& download_id,
                                     const base::FilePath& actual_source_path,
                                     const base::FilePath& actual_final_path) {
        callback_success = success;
        run_loop.Quit();
      }));

  run_loop.Run();

  // Should still succeed even without record service.
  EXPECT_TRUE(callback_success);
  EXPECT_TRUE(base::PathExists(dest_path));
}

// Tests resolving available file path with no conflict.
TEST_F(DownloadFileServiceTest, ResolveAvailableFilePathNoConflict) {
  base::FilePath suggested_filename =
      base::FilePath::FromUTF8Unsafe(kTestFileName);

  base::RunLoop run_loop;
  base::FilePath resolved_path;

  service_->ResolveAvailableFilePath(
      dest_dir_, suggested_filename,
      base::BindLambdaForTesting([&](base::FilePath path) {
        resolved_path = path;
        run_loop.Quit();
      }));

  run_loop.Run();

  base::FilePath expected_path = dest_dir_.AppendASCII(kTestFileName);
  EXPECT_EQ(expected_path, resolved_path);
}

// Tests resolving available file path with conflicts.
TEST_F(DownloadFileServiceTest, ResolveAvailableFilePathWithConflicts) {
  // Create conflicting files.
  CreateTestFile(dest_dir_, kConflictFileName, kTestFileContent);
  CreateTestFile(dest_dir_, "conflict_file (1).txt", kTestFileContent);

  base::FilePath suggested_filename =
      base::FilePath::FromUTF8Unsafe(kConflictFileName);

  base::RunLoop run_loop;
  base::FilePath resolved_path;

  service_->ResolveAvailableFilePath(
      dest_dir_, suggested_filename,
      base::BindLambdaForTesting([&](base::FilePath path) {
        resolved_path = path;
        run_loop.Quit();
      }));

  run_loop.Run();

  base::FilePath expected_path = dest_dir_.AppendASCII("conflict_file (2).txt");
  EXPECT_EQ(expected_path, resolved_path);
}

// Check if file exists.
TEST_F(DownloadFileServiceTest, CheckFileExists) {
  base::FilePath test_file =
      CreateTestFile(source_dir_, kTestFileName, kTestFileContent);
  base::FilePath non_existent_file =
      source_dir_.AppendASCII("non_existent.txt");

  // Test existing file.
  {
    base::RunLoop run_loop;
    bool file_exists = false;

    service_->CheckFileExists(test_file,
                              base::BindLambdaForTesting([&](bool exists) {
                                file_exists = exists;
                                run_loop.Quit();
                              }));

    run_loop.Run();
    EXPECT_TRUE(file_exists);
  }

  // Test non-existent file.
  {
    base::RunLoop run_loop;
    bool file_exists = true;

    service_->CheckFileExists(non_existent_file,
                              base::BindLambdaForTesting([&](bool exists) {
                                file_exists = exists;
                                run_loop.Quit();
                              }));

    run_loop.Run();
    EXPECT_FALSE(file_exists);
  }
}

// Tests moving a download file to a non-existent directory.
// The directory should be created and the file moved successfully.
TEST_F(DownloadFileServiceTest, MoveDownloadFileCreateDestinationDirectory) {
  base::FilePath source_path =
      CreateTestFile(source_dir_, kTestFileName, kTestFileContent);
  base::FilePath new_dest_dir = dest_dir_.AppendASCII("new_subdirectory");
  base::FilePath dest_path = new_dest_dir.AppendASCII(kTestFileName);

  // Verify destination directory doesn't exist initially.
  EXPECT_FALSE(base::PathExists(new_dest_dir));

  base::FilePath expected_relative_path =
      ConvertToRelativeDownloadPath(dest_path);
  EXPECT_CALL(
      *mock_download_record_service_,
      UpdateDownloadFilePathAsync(kTestDownloadId, expected_relative_path, _))
      .Times(1);

  base::RunLoop run_loop;
  bool callback_success = false;

  service_->MoveDownloadFile(
      kTestDownloadId, source_path, dest_path,
      base::BindLambdaForTesting([&](bool success,
                                     const std::string& download_id,
                                     const base::FilePath& actual_source_path,
                                     const base::FilePath& actual_final_path) {
        callback_success = success;
        run_loop.Quit();
      }));

  run_loop.Run();

  // Should succeed and create destination directory.
  EXPECT_TRUE(callback_success);
  EXPECT_TRUE(base::PathExists(new_dest_dir));
  EXPECT_TRUE(base::PathExists(dest_path));
}
