// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/ios_content_analysis_request.h"

#import "base/files/file_path.h"
#import "base/functional/callback_helpers.h"
#import "base/test/test_future.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace enterprise_connectors {

class IOSContentAnalysisRequestTest : public PlatformTest {
 protected:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
};

// Tests that a valid data-based request correctly populates the upload data.
TEST_F(IOSContentAnalysisRequestTest, StringRequestSuccess) {
  AnalysisSettings settings;
  std::string data = "test data";
  base::test::TestFuture<ScanRequestUploadResult, BinaryUploadRequest::Data>
      future;

  auto request = std::make_unique<IOSContentAnalysisRequest>(
      settings, "text/plain", data, base::DoNothing());

  request->GetRequestData(future.GetCallback());
  auto [result, request_data] = future.Take();

  EXPECT_EQ(result, ScanRequestUploadResult::kSuccess);
  EXPECT_EQ(request_data.contents, data);
  EXPECT_EQ(request_data.size, data.size());
  EXPECT_EQ(request_data.mime_type, "text/plain");
}

// Tests that data-based requests exceeding the size limit are correctly
// flagged.
TEST_F(IOSContentAnalysisRequestTest, StringRequestTooLarge) {
  AnalysisSettings settings;
  std::string data(BinaryUploadService::kMaxUploadSizeBytes + 1, 'a');
  base::test::TestFuture<ScanRequestUploadResult, BinaryUploadRequest::Data>
      future;

  auto request = std::make_unique<IOSContentAnalysisRequest>(
      settings, "text/plain", data, base::DoNothing());

  request->GetRequestData(future.GetCallback());
  auto [result, request_data] = future.Take();

  EXPECT_EQ(result, ScanRequestUploadResult::kFileTooLarge);
  EXPECT_EQ(request_data.size, data.size());
  EXPECT_TRUE(request_data.contents.empty());
  EXPECT_EQ(request_data.mime_type, "text/plain");
}

// Tests that a file-based request correctly populates the upload data using the
// provided path.
TEST_F(IOSContentAnalysisRequestTest, FileRequestSuccess) {
  AnalysisSettings settings;
  base::FilePath path(FILE_PATH_LITERAL("/path/to/file"));
  base::test::TestFuture<ScanRequestUploadResult, BinaryUploadRequest::Data>
      future;

  auto request = std::make_unique<IOSContentAnalysisRequest>(
      settings, path, "text/plain", base::DoNothing());

  request->GetRequestData(future.GetCallback());
  auto [result, request_data] = future.Take();

  EXPECT_EQ(result, ScanRequestUploadResult::kSuccess);
  EXPECT_EQ(request_data.path, path);
  EXPECT_EQ(request_data.mime_type, "text/plain");
  EXPECT_EQ(request->filename(), path.AsUTF8Unsafe());
}

}  // namespace enterprise_connectors
