// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/files_request_handler_ios.h"

#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/json/json_reader.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "components/enterprise/connectors/core/analysis_settings.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/files_request_handler_base.h"
#import "components/enterprise/connectors/core/connectors_prefs.h"
#import "components/enterprise/connectors/core/content_analysis_info_base.h"
#import "components/enterprise/connectors/core/reporting_event_router.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/enterprise/common/test/mock_reporting_event_router.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service_factory.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_reporting_event_router_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/components/enterprise/analysis/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace enterprise_connectors {

namespace {

// Mock implementation of ContentAnalysisInfoBase to control the settings and
// metadata provided to the request handler.
class MockContentAnalysisInfoBase : public ContentAnalysisInfoBase {
 public:
  MOCK_METHOD(void,
              InitializeRequest,
              (BinaryUploadRequest * request,
               bool include_enterprise_only_fields),
              (override));
  MOCK_METHOD(const AnalysisSettings&, settings, (), (const, override));
  MOCK_METHOD(signin::IdentityManager*,
              identity_manager,
              (),
              (const, override));
  MOCK_METHOD(int, user_action_requests_count, (), (const, override));
  MOCK_METHOD(std::string, tab_title, (), (const, override));
  MOCK_METHOD(std::string, user_action_id, (), (const, override));
  MOCK_METHOD(std::string, email, (), (const, override));
  MOCK_METHOD(const GURL&, url, (), (const, override));
  MOCK_METHOD(const GURL&, tab_url, (), (const, override));
  MOCK_METHOD(ContentAnalysisRequest::Reason, reason, (), (const, override));
  MOCK_METHOD(
      (google::protobuf::RepeatedPtrField<::safe_browsing::ReferrerChainEntry>),
      referrer_chain,
      (),
      (const, override));
  MOCK_METHOD(google::protobuf::RepeatedPtrField<std::string>,
              frame_url_chain,
              (),
              (const, override));
  MOCK_METHOD(std::string, GetContentAreaAccountEmail, (), (const, override));
};

// Mock implementation of BinaryUploadService to verify that upload requests
// are correctly initiated.
class MockBinaryUploadService : public BinaryUploadService {
 public:
  MOCK_METHOD(void,
              MaybeUploadForDeepScanning,
              (std::unique_ptr<BinaryUploadRequest> request),
              (override));
  MOCK_METHOD(void,
              MaybeAcknowledge,
              (std::unique_ptr<BinaryUploadAck> ack),
              (override));
  MOCK_METHOD(void,
              MaybeCancelRequests,
              (std::unique_ptr<BinaryUploadCancelRequests> cancel),
              (override));
  base::WeakPtr<BinaryUploadService> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockBinaryUploadService> weak_ptr_factory_{this};
};

// Default analysis settings JSON for testing.
constexpr char kWildcardAnalysisSettingsPref[] = R"([
  {
    "service_provider": "google",
    "enable": [
      {"url_list": ["*"], "tags": ["dlp", "malware"]}
    ],
    "block_until_verdict": 1
  }
])";

}  // namespace

// Test fixture for FilesRequestHandlerIOS, which handles the iOS-specific
// implementation of file deep scanning requests.
class FilesRequestHandlerIOSTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IOSReportingEventRouterFactory::GetInstance(),
        base::BindOnce(
            &MockReportingEventRouter::BuildMockReportingEventRouter));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    reporting_router_ = static_cast<MockReportingEventRouter*>(
        IOSReportingEventRouterFactory::GetForProfile(profile_.get()));

    scoped_feature_list_.InitAndEnableFeature(kEnableFileDownloadConnectorIOS);
  }

  // Enables the file download connector by setting the appropriate pref.
  void EnableConnector() {
    profile_->GetTestingPrefService()->Set(
        AnalysisConnectorPref(AnalysisConnector::FILE_DOWNLOADED),
        *base::JSONReader::Read(kWildcardAnalysisSettingsPref,
                                base::JSON_PARSE_RFC));
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  raw_ptr<MockReportingEventRouter> reporting_router_;
  MockContentAnalysisInfoBase content_analysis_info_;
  MockBinaryUploadService upload_service_;
  AnalysisSettings settings_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that UploadDataImpl returns false and calls the callback when the path
// is empty, as there is nothing to scan.
TEST_F(FilesRequestHandlerIOSTest, UploadDataImpl_NoPath) {
  base::test::TestFuture<RequestHandlerResult> future;
  auto delegate = std::make_unique<FilesRequestHandlerIOS>(
      profile_.get(), base::FilePath(), future.GetCallback());

  EXPECT_FALSE(delegate->UploadDataImpl());

  RequestHandlerResult result = future.Take();
  EXPECT_EQ(result.final_result, FinalContentAnalysisResult::SUCCESS);
}

// Tests that UploadDataImpl returns false and calls the callback when the
// connector is disabled, allowing the operation to proceed without scanning.
TEST_F(FilesRequestHandlerIOSTest, UploadDataImpl_ConnectorDisabled) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path = temp_dir.GetPath().AppendASCII("test.txt");
  ASSERT_TRUE(base::WriteFile(path, "test content"));

  base::test::TestFuture<RequestHandlerResult> future;
  auto delegate = std::make_unique<FilesRequestHandlerIOS>(
      profile_.get(), path, future.GetCallback());

  EXPECT_FALSE(delegate->UploadDataImpl());

  RequestHandlerResult result = future.Take();
  EXPECT_EQ(result.final_result, FinalContentAnalysisResult::SUCCESS);
}

// Tests that UploadDataImpl returns true when the connector is enabled and a
// valid file path is provided, indicating that scanning has started.
TEST_F(FilesRequestHandlerIOSTest, UploadDataImpl_Success) {
  EnableConnector();

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path = temp_dir.GetPath().AppendASCII("test.txt");
  ASSERT_TRUE(base::WriteFile(path, "test content"));

  base::test::TestFuture<RequestHandlerResult> future;
  auto delegate_ptr = std::make_unique<FilesRequestHandlerIOS>(
      profile_.get(), path, future.GetCallback());
  auto* delegate = delegate_ptr.get();

  FilesRequestHandlerBase handler(
      &content_analysis_info_, &upload_service_, GURL("https://example.com"),
      "method", DeepScanAccessPoint::DOWNLOAD, std::move(delegate_ptr));

  EXPECT_CALL(content_analysis_info_, settings())
      .WillRepeatedly(testing::ReturnRef(settings_));

  EXPECT_TRUE(delegate->UploadDataImpl());
}

// Tests that path and source/destination getters return the expected values,
// and that file information (hash, size, mime type) can be updated.
TEST_F(FilesRequestHandlerIOSTest, GettersAndSetters) {
  base::FilePath path(FILE_PATH_LITERAL("/path/to/test.txt"));
  auto delegate = std::make_unique<FilesRequestHandlerIOS>(profile_.get(), path,
                                                           base::DoNothing());

  EXPECT_EQ(delegate->GetPath(0), path);
  EXPECT_EQ(delegate->GetSource(), "");
  EXPECT_EQ(delegate->GetDestination(), "");

  FilesRequestHandlerBase::FileInfo& file_info =
      delegate->GetMutableFileInfo(0);
  file_info.sha256_or_cb = "test_hash";
  file_info.size = 1234;
  file_info.mime_type = "text/plain";

  const FilesRequestHandlerBase::FileInfo& file_info_const =
      delegate->GetFileInfo(0);
  EXPECT_EQ(std::get<std::string>(file_info_const.sha256_or_cb), "test_hash");
  EXPECT_EQ(file_info_const.size, 1234u);
  EXPECT_EQ(file_info_const.mime_type, "text/plain");
}

// Tests that UpdateRequestHandlerResult correctly stores the scan result and
// server response, which are then returned via the completion callback.
TEST_F(FilesRequestHandlerIOSTest, UpdateRequestHandlerResult) {
  base::test::TestFuture<RequestHandlerResult> future;
  auto delegate = std::make_unique<FilesRequestHandlerIOS>(
      profile_.get(), base::FilePath(), future.GetCallback());

  RequestHandlerResult result;
  result.complies = true;
  result.final_result = FinalContentAnalysisResult::SUCCESS;
  result.tag = "dlp";

  ContentAnalysisResponse response;
  response.set_request_token("test_token");

  delegate->UpdateRequestHandlerResult(0, result, response);

  delegate->MaybeCompleteScanRequest();

  RequestHandlerResult final_result = future.Take();
  EXPECT_EQ(final_result.complies, result.complies);
  EXPECT_EQ(final_result.final_result, result.final_result);
  EXPECT_EQ(final_result.tag, result.tag);
}

// Tests that ReportWarningBypass and MaybeReportDangerousDownload correctly
// triggers a sensitive data event via the reporting event router when called.
TEST_F(FilesRequestHandlerIOSTest, ReportWarningBypass) {
  base::test::TestFuture<RequestHandlerResult> future;
  auto delegate_ptr = std::make_unique<FilesRequestHandlerIOS>(
      profile_.get(), base::FilePath(), future.GetCallback());
  auto* delegate = delegate_ptr.get();

  FilesRequestHandlerBase handler(
      &content_analysis_info_, &upload_service_, GURL("https://example.com"),
      "method", DeepScanAccessPoint::DOWNLOAD, std::move(delegate_ptr));

  ContentAnalysisResponse response;
  auto* result = response.add_results();
  result->set_tag("dlp");
  result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  result->add_triggered_rules()->set_action(TriggeredRule::WARN);

  RequestHandlerResult request_result;
  request_result.final_result = FinalContentAnalysisResult::WARNING;

  google::protobuf::RepeatedPtrField<::safe_browsing::ReferrerChainEntry>
      referrer_chain;
  google::protobuf::RepeatedPtrField<std::string> frame_url_chain;

  EXPECT_CALL(content_analysis_info_, url())
      .WillRepeatedly(testing::ReturnRef(GURL::EmptyGURL()));
  EXPECT_CALL(content_analysis_info_, tab_url())
      .WillRepeatedly(testing::ReturnRef(GURL::EmptyGURL()));
  EXPECT_CALL(content_analysis_info_, GetContentAreaAccountEmail())
      .WillRepeatedly(testing::Return(""));
  EXPECT_CALL(content_analysis_info_, referrer_chain())
      .WillRepeatedly(testing::Return(referrer_chain));
  EXPECT_CALL(content_analysis_info_, frame_url_chain())
      .WillRepeatedly(testing::Return(frame_url_chain));

  EXPECT_CALL(*reporting_router_, OnSensitiveDataEvent(testing::_)).Times(1);

  delegate->UpdateRequestHandlerResult(0, request_result, response);
  handler.ReportWarningBypass(u"justification");
}

}  // namespace enterprise_connectors
