// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/ios_content_analysis_request.h"

#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/functional/callback_helpers.h"
#import "base/test/test_future.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace enterprise_connectors {

class IOSContentAnalysisRequestTest : public PlatformTest {
 protected:
  std::unique_ptr<IOSContentAnalysisRequest> MakeRequest(
      base::FilePath path,
      base::FilePath file_name,
      std::string mime_type,
      bool delay_opening_file) {
    AnalysisSettings settings;
    return std::make_unique<IOSContentAnalysisRequest>(
        settings, path, file_name, mime_type, delay_opening_file,
        base::DoNothing(), base::DoNothing());
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
};

// Tests that a file-based request correctly populates the upload data using the
// provided path.
TEST_F(IOSContentAnalysisRequestTest, FileRequestSuccess) {
  base::FilePath path(FILE_PATH_LITERAL("/path/to/file"));
  base::FilePath file_name(FILE_PATH_LITERAL("file.txt"));

  auto request = MakeRequest(path, file_name, "text/plain",
                             /*delay_opening_file=*/false);

  EXPECT_EQ(request->filename(), path.AsUTF8Unsafe());
}

// Tests that a non-existent file returns an unknown result.
TEST_F(IOSContentAnalysisRequestTest, InvalidFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path = temp_dir.GetPath().AppendASCII("non_existent.txt");

  auto request = MakeRequest(path, path.BaseName(), "text/plain",
                             /*delay_opening_file=*/false);

  base::test::TestFuture<ScanRequestUploadResult, BinaryUploadRequest::Data>
      future;
  request->GetRequestData(future.GetCallback());

  auto [result, data] = future.Take();
  EXPECT_EQ(result, ScanRequestUploadResult::kUnknown);
  EXPECT_EQ(data.size, 0u);
}

// Tests that an empty file returns success.
TEST_F(IOSContentAnalysisRequestTest, EmptyFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path = temp_dir.GetPath().AppendASCII("empty.txt");
  ASSERT_TRUE(base::WriteFile(path, ""));

  auto request = MakeRequest(path, path.BaseName(), "text/plain",
                             /*delay_opening_file=*/false);

  base::test::TestFuture<ScanRequestUploadResult, BinaryUploadRequest::Data>
      future;
  request->GetRequestData(future.GetCallback());

  auto [result, data] = future.Take();
  EXPECT_EQ(result, ScanRequestUploadResult::kSuccess);
  EXPECT_EQ(data.size, 0u);
}

// Tests that a normal file returns success and correctly populates data.
TEST_F(IOSContentAnalysisRequestTest, NormalFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path = temp_dir.GetPath().AppendASCII("normal.txt");
  std::string content = "Normal file contents";
  ASSERT_TRUE(base::WriteFile(path, content));

  auto request = MakeRequest(path, path.BaseName(), "text/plain",
                             /*delay_opening_file=*/false);

  base::test::TestFuture<ScanRequestUploadResult, BinaryUploadRequest::Data>
      future;
  request->GetRequestData(future.GetCallback());

  auto [result, data] = future.Take();
  EXPECT_EQ(result, ScanRequestUploadResult::kSuccess);
  EXPECT_EQ(data.size, content.size());
  EXPECT_EQ(data.hash,
            "29644C10BD036866FCFD2BDACFF340DB5DE47A90002D6AB0C42DE6A22C26158B");
  EXPECT_EQ(request->digest(), data.hash);
}

// Tests that delayed opening correctly populates data after OpenFile is called.
TEST_F(IOSContentAnalysisRequestTest, DelayedOpening) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path = temp_dir.GetPath().AppendASCII("delayed.txt");
  std::string content = "Delayed content";
  ASSERT_TRUE(base::WriteFile(path, content));

  auto request = MakeRequest(path, path.BaseName(), "text/plain",
                             /*delay_opening_file=*/true);

  base::test::TestFuture<ScanRequestUploadResult, BinaryUploadRequest::Data>
      future;
  request->GetRequestData(future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  request->OpenFile();

  auto [result, data] = future.Take();
  EXPECT_EQ(result, ScanRequestUploadResult::kSuccess);
  EXPECT_EQ(data.size, content.size());
}

// Tests that results are cached for subsequent requests.
TEST_F(IOSContentAnalysisRequestTest, CachesResults) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path = temp_dir.GetPath().AppendASCII("cached.txt");
  std::string content = "Cached content";
  ASSERT_TRUE(base::WriteFile(path, content));

  auto request = MakeRequest(path, path.BaseName(), "text/plain",
                             /*delay_opening_file=*/false);

  base::test::TestFuture<ScanRequestUploadResult, BinaryUploadRequest::Data>
      future1;
  request->GetRequestData(future1.GetCallback());
  auto [result1, data1] = future1.Take();

  base::test::TestFuture<ScanRequestUploadResult, BinaryUploadRequest::Data>
      future2;
  request->GetRequestData(future2.GetCallback());
  auto [result2, data2] = future2.Take();

  EXPECT_EQ(result1, result2);
  EXPECT_EQ(data1.hash, data2.hash);
  EXPECT_EQ(data1.size, data2.size);
}

// Tests that a file larger than the limit returns a FileTooLarge result.
TEST_F(IOSContentAnalysisRequestTest, FileTooLarge) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path = temp_dir.GetPath().AppendASCII("too_large.txt");
  std::string content(BinaryUploadService::kMaxUploadSizeBytes + 1, 'a');
  ASSERT_TRUE(base::WriteFile(path, content));

  auto request = MakeRequest(path, path.BaseName(), "text/plain",
                             /*delay_opening_file=*/false);

  base::test::TestFuture<ScanRequestUploadResult, BinaryUploadRequest::Data>
      future;
  request->GetRequestData(future.GetCallback());

  auto [result, data] = future.Take();
  EXPECT_EQ(result, ScanRequestUploadResult::kFileTooLarge);
  EXPECT_EQ(data.size, content.size());
}

}  // namespace enterprise_connectors
