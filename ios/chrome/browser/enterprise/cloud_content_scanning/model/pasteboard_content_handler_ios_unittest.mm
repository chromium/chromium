// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/pasteboard_content_handler_ios.h"

#import <memory>
#import <string>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/test/test_future.h"
#import "components/enterprise/common/proto/connectors.pb.h"
#import "components/enterprise/connectors/core/analysis_settings.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/clipboard_request_handler.h"
#import "components/enterprise/connectors/core/common.h"
#import "components/policy/core/browser/browser_policy_connector.h"
#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/ios_cloud_binary_upload_service_factory.h"
#import "ios/chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_reporting_event_router_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace enterprise_connectors {

namespace {

constexpr char kTestUrl[] = "https://example.com";
constexpr char kTestText[] = "test pasteboard text";
constexpr char kTestImage[] = "test pasteboard image";

class FakeClipboardRequestHandler : public ClipboardRequestHandler {
 public:
  FakeClipboardRequestHandler(
      ContentAnalysisInfoBase* content_analysis_info,
      BinaryUploadService* upload_service,
      ReportingEventRouter* router,
      GURL url,
      Type type,
      DeepScanAccessPoint access_point,
      ContentMetaData::CopiedTextSource clipboard_source,
      std::string source_content_area_email,
      std::string content_transfer_method,
      std::string data,
      CompletionCallback callback,
      BinaryUploadRequest::BrowserPolicyConnectorGetter policy_getter,
      RequestHandlerResult result)
      : ClipboardRequestHandler(content_analysis_info,
                                upload_service,
                                router,
                                std::move(url),
                                type,
                                access_point,
                                std::move(clipboard_source),
                                std::move(source_content_area_email),
                                std::move(content_transfer_method),
                                std::move(data),
                                base::DoNothing(),
                                std::move(policy_getter)),
        callback_(std::move(callback)),
        result_(std::move(result)) {}

  bool UploadDataImpl() override {
    if (callback_) {
      std::move(callback_).Run(result_);
    }
    return true;
  }

  void ReportWarningBypass(
      std::optional<std::u16string> user_justification) override {
    warning_bypass_reported_ = true;
  }

  bool warning_bypass_reported() { return warning_bypass_reported_; }

 private:
  CompletionCallback callback_;
  RequestHandlerResult result_;
  bool warning_bypass_reported_ = false;
};

}  // namespace

class PasteboardContentHandlerIOSTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(profile_.get());
    web_state_->SetCurrentURL(GURL(kTestUrl));

    ClipboardRequestHandler::SetFactoryForTesting(base::BindRepeating(
        &PasteboardContentHandlerIOSTest::CreateFakeClipboardRequestHandler,
        base::Unretained(this)));
  }

  void TearDown() override {
    ClipboardRequestHandler::ResetFactoryForTesting();
    PlatformTest::TearDown();
  }

  // Needs to be called before the end of each test to avoid dangling pointer.
  void ResetHandlers() {
    text_request_handler_ = nullptr;
    image_request_handler_ = nullptr;
  }

  std::unique_ptr<ClipboardRequestHandler> CreateFakeClipboardRequestHandler(
      ContentAnalysisInfoBase* content_analysis_info,
      BinaryUploadService* upload_service,
      ReportingEventRouter* router,
      GURL url,
      ClipboardRequestHandler::Type type,
      DeepScanAccessPoint access_point,
      ContentMetaData::CopiedTextSource clipboard_source,
      std::string source_content_area_email,
      std::string content_transfer_method,
      std::string data,
      ClipboardRequestHandler::CompletionCallback callback,
      BinaryUploadRequest::BrowserPolicyConnectorGetter policy_getter) {
    auto handler = std::make_unique<FakeClipboardRequestHandler>(
        content_analysis_info, upload_service, router, std::move(url), type,
        access_point, std::move(clipboard_source),
        std::move(source_content_area_email),
        std::move(content_transfer_method), std::move(data),
        std::move(callback), std::move(policy_getter),
        std::move(type == ClipboardRequestHandler::Type::kText
                      ? text_result_
                      : image_result_));

    if (type == ClipboardRequestHandler::Type::kText) {
      text_request_handler_ = handler.get();
    } else {
      image_request_handler_ = handler.get();
    }
    return handler;
  }

  std::unique_ptr<ContentAnalysisInfo> CreateContentAnalysisInfo() {
    AnalysisSettings settings;
    return std::make_unique<ContentAnalysisInfo>(
        GURL(kTestUrl), std::move(settings),
        ContentAnalysisRequest::CLIPBOARD_PASTE, *web_state_);
  }

  BinaryUploadService* upload_service() {
    return IOSCloudBinaryUploadServiceFactory::GetForProfile(profile_.get());
  }

  ReportingEventRouter* router() {
    return IOSReportingEventRouterFactory::GetForProfile(profile_.get());
  }

  raw_ptr<FakeClipboardRequestHandler> text_request_handler_;
  raw_ptr<FakeClipboardRequestHandler> image_request_handler_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_;
  RequestHandlerResult text_result_;
  RequestHandlerResult image_result_;
};

// Tests when both pasteboard text and image are empty, no requests are created,
// and the result is SUCCESS.
TEST_F(PasteboardContentHandlerIOSTest, EmptyTextAndImage) {
  base::test::TestFuture<RequestHandlerResult> future;
  ContentMetaData::CopiedTextSource copied_source;

  PasteboardInfo pasteboard_info{
      .text = "",
      .image = "",
      .destination_url = GURL(kTestUrl),
  };

  auto handler = std::make_unique<PasteboardContentHandlerIOS>(
      std::move(pasteboard_info), upload_service(), router(), copied_source,
      CreateContentAnalysisInfo(),
      /*policy_callback=*/
      base::BindRepeating(
          []() -> policy::BrowserPolicyConnector* { return nullptr; }),
      future.GetCallback());

  handler->StartContentAnalysisRequest();

  ASSERT_FALSE(text_request_handler_);
  ASSERT_FALSE(image_request_handler_);

  EXPECT_TRUE(future.IsReady());
  RequestHandlerResult result = future.Take();
  EXPECT_EQ(result.final_result, FinalContentAnalysisResult::SUCCESS);

  ResetHandlers();
}

// Tests when only text is provided, only create handler for text.
TEST_F(PasteboardContentHandlerIOSTest, OnlyText) {
  base::test::TestFuture<RequestHandlerResult> future;
  ContentMetaData::CopiedTextSource copied_source;

  text_result_.final_result = FinalContentAnalysisResult::SUCCESS;
  text_result_.complies = true;

  PasteboardInfo pasteboard_info{
      .text = kTestText,
      .image = "",
      .destination_url = GURL(kTestUrl),
  };

  auto handler = std::make_unique<PasteboardContentHandlerIOS>(
      std::move(pasteboard_info), upload_service(), router(), copied_source,
      CreateContentAnalysisInfo(),
      /*policy_callback=*/
      base::BindRepeating(
          []() -> policy::BrowserPolicyConnector* { return nullptr; }),
      future.GetCallback());

  handler->StartContentAnalysisRequest();

  ASSERT_TRUE(text_request_handler_);
  ASSERT_FALSE(image_request_handler_);

  EXPECT_TRUE(future.IsReady());
  RequestHandlerResult final_result = future.Take();
  EXPECT_EQ(final_result.final_result, FinalContentAnalysisResult::SUCCESS);
  EXPECT_TRUE(final_result.complies);

  ResetHandlers();
}

// Tests when only image is provided, only create handler for image
TEST_F(PasteboardContentHandlerIOSTest, OnlyImage) {
  base::test::TestFuture<RequestHandlerResult> future;
  ContentMetaData::CopiedTextSource copied_source;

  image_result_.final_result = FinalContentAnalysisResult::SUCCESS;
  image_result_.complies = true;

  PasteboardInfo pasteboard_info{
      .text = "",
      .image = kTestImage,
      .destination_url = GURL(kTestUrl),
  };

  auto handler = std::make_unique<PasteboardContentHandlerIOS>(
      std::move(pasteboard_info), upload_service(), router(), copied_source,
      CreateContentAnalysisInfo(),
      /*policy_callback=*/
      base::BindRepeating(
          []() -> policy::BrowserPolicyConnector* { return nullptr; }),
      future.GetCallback());

  handler->StartContentAnalysisRequest();

  ASSERT_FALSE(text_request_handler_);
  ASSERT_TRUE(image_request_handler_);

  EXPECT_TRUE(future.IsReady());
  RequestHandlerResult final_result = future.Take();
  EXPECT_EQ(final_result.final_result, FinalContentAnalysisResult::SUCCESS);
  EXPECT_TRUE(final_result.complies);

  ResetHandlers();
}

// Tests when text has higher action level (kBlock) than image (kAudit), the
// correct Result will be provided to the callback.
TEST_F(PasteboardContentHandlerIOSTest, TextActionLevelHigherThanImage) {
  base::test::TestFuture<RequestHandlerResult> future;
  ContentMetaData::CopiedTextSource copied_source;

  text_result_.final_result = FinalContentAnalysisResult::FAILURE;
  text_result_.complies = false;

  image_result_.final_result = FinalContentAnalysisResult::SUCCESS;
  image_result_.complies = true;

  PasteboardInfo pasteboard_info{
      .text = kTestText,
      .image = kTestImage,
      .destination_url = GURL(kTestUrl),
  };

  auto handler = std::make_unique<PasteboardContentHandlerIOS>(
      std::move(pasteboard_info), upload_service(), router(), copied_source,
      CreateContentAnalysisInfo(),
      /*policy_callback=*/
      base::BindRepeating(
          []() -> policy::BrowserPolicyConnector* { return nullptr; }),
      future.GetCallback());

  handler->StartContentAnalysisRequest();

  ASSERT_TRUE(text_request_handler_);
  ASSERT_TRUE(image_request_handler_);

  EXPECT_TRUE(future.IsReady());
  RequestHandlerResult final_result = future.Take();
  EXPECT_EQ(final_result.final_result, FinalContentAnalysisResult::FAILURE);

  ResetHandlers();
}

// Tests when image has higher action level (kBlock) than text (kWarn), the
// correct Result will be provided to the callback.
TEST_F(PasteboardContentHandlerIOSTest, ImageActionLevelHigherThanText) {
  base::test::TestFuture<RequestHandlerResult> future;
  ContentMetaData::CopiedTextSource copied_source;

  text_result_.final_result = FinalContentAnalysisResult::WARNING;
  text_result_.complies = false;

  image_result_.final_result = FinalContentAnalysisResult::FAILURE;
  image_result_.complies = false;

  PasteboardInfo pasteboard_info{
      .text = kTestText,
      .image = kTestImage,
      .destination_url = GURL(kTestUrl),
  };

  auto handler = std::make_unique<PasteboardContentHandlerIOS>(
      std::move(pasteboard_info), upload_service(), router(), copied_source,
      CreateContentAnalysisInfo(),
      /*policy_callback=*/
      base::BindRepeating(
          []() -> policy::BrowserPolicyConnector* { return nullptr; }),
      future.GetCallback());

  handler->StartContentAnalysisRequest();

  ASSERT_TRUE(text_request_handler_);
  ASSERT_TRUE(image_request_handler_);

  EXPECT_TRUE(future.IsReady());
  RequestHandlerResult final_result = future.Take();
  EXPECT_EQ(final_result.final_result, FinalContentAnalysisResult::FAILURE);

  ResetHandlers();
}

// Tests when text and image have equal action levels, defaulting to text
// result.
TEST_F(PasteboardContentHandlerIOSTest, EqualActionLevelsDefaultToText) {
  base::test::TestFuture<RequestHandlerResult> future;
  ContentMetaData::CopiedTextSource copied_source;

  text_result_.final_result = FinalContentAnalysisResult::WARNING;

  image_result_.final_result = FinalContentAnalysisResult::WARNING;

  PasteboardInfo pasteboard_info{
      .text = kTestText,
      .image = kTestImage,
      .destination_url = GURL(kTestUrl),
  };

  auto handler = std::make_unique<PasteboardContentHandlerIOS>(
      std::move(pasteboard_info), upload_service(), router(), copied_source,
      CreateContentAnalysisInfo(),
      /*policy_callback=*/
      base::BindRepeating(
          []() -> policy::BrowserPolicyConnector* { return nullptr; }),
      future.GetCallback());

  handler->StartContentAnalysisRequest();

  ASSERT_TRUE(text_request_handler_);
  ASSERT_TRUE(image_request_handler_);

  EXPECT_TRUE(future.IsReady());
  RequestHandlerResult final_result = future.Take();
  EXPECT_EQ(final_result.final_result, FinalContentAnalysisResult::WARNING);

  ResetHandlers();
}

// Tests ReportWarningBypass when text scan result is WARNING.
TEST_F(PasteboardContentHandlerIOSTest, ReportWarningBypassText) {
  base::test::TestFuture<RequestHandlerResult> future;
  ContentMetaData::CopiedTextSource copied_source;

  text_result_.final_result = FinalContentAnalysisResult::WARNING;
  image_result_.final_result = FinalContentAnalysisResult::SUCCESS;

  PasteboardInfo pasteboard_info{
      .text = kTestText,
      .image = kTestImage,
      .destination_url = GURL(kTestUrl),
  };

  auto handler = std::make_unique<PasteboardContentHandlerIOS>(
      std::move(pasteboard_info), upload_service(), router(), copied_source,
      CreateContentAnalysisInfo(),
      /*policy_callback=*/
      base::BindRepeating(
          []() -> policy::BrowserPolicyConnector* { return nullptr; }),
      future.GetCallback());

  handler->StartContentAnalysisRequest();

  ASSERT_TRUE(text_request_handler_);
  ASSERT_TRUE(image_request_handler_);

  ASSERT_TRUE(future.IsReady());

  handler->ReportWarningBypass();

  ASSERT_TRUE(text_request_handler_->warning_bypass_reported());
  ASSERT_FALSE(image_request_handler_->warning_bypass_reported());

  ResetHandlers();
}

// Tests ReportWarningBypass when both text and image scan results are WARNING.
TEST_F(PasteboardContentHandlerIOSTest, ReportWarningBypassBoth) {
  base::test::TestFuture<RequestHandlerResult> future;
  ContentMetaData::CopiedTextSource copied_source;

  text_result_.final_result = FinalContentAnalysisResult::WARNING;
  image_result_.final_result = FinalContentAnalysisResult::WARNING;

  PasteboardInfo pasteboard_info{
      .text = kTestText,
      .image = kTestImage,
      .destination_url = GURL(kTestUrl),
  };

  auto handler = std::make_unique<PasteboardContentHandlerIOS>(
      std::move(pasteboard_info), upload_service(), router(), copied_source,
      CreateContentAnalysisInfo(),
      /*policy_callback=*/
      base::BindRepeating(
          []() -> policy::BrowserPolicyConnector* { return nullptr; }),
      future.GetCallback());

  handler->StartContentAnalysisRequest();

  ASSERT_TRUE(text_request_handler_);
  ASSERT_TRUE(image_request_handler_);

  ASSERT_TRUE(future.IsReady());

  handler->ReportWarningBypass();

  ASSERT_TRUE(text_request_handler_->warning_bypass_reported());
  ASSERT_TRUE(image_request_handler_->warning_bypass_reported());

  ResetHandlers();
}

}  // namespace enterprise_connectors
