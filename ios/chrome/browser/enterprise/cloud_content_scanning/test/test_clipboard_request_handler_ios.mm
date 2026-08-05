// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/cloud_content_scanning/test/test_clipboard_request_handler_ios.h"

namespace enterprise_connectors {

namespace {
const char kTestToken[] = "test-token";
}  // namespace

TestClipboardRequestHandlerIOS::TestClipboardRequestHandlerIOS(
    TriggeredRule::Action action,
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
    BinaryUploadRequest::BrowserPolicyConnectorGetter policy_getter)
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
                              std::move(callback),
                              std::move(policy_getter)),
      action_(action) {}

void TestClipboardRequestHandlerIOS::UploadForDeepScanning(
    std::unique_ptr<ClipboardAnalysisRequest> request) {
  ContentAnalysisResponse response;
  response.set_request_token(kTestToken);
  ContentAnalysisResponse::Result* result = response.add_results();
  result->set_tag("dlp");
  result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  if (action_ != TriggeredRule::ACTION_UNSPECIFIED) {
    TriggeredRule* rule = result->add_triggered_rules();
    rule->set_action(action_);
    rule->set_rule_name("test-rule");
  }

  request->FinishRequest(ScanRequestUploadResult::kSuccess,
                         std::move(response));
}

std::unique_ptr<ClipboardRequestHandler> CreateTestClipboardRequestHandlerIOS(
    TriggeredRule::Action action,
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
  return std::make_unique<TestClipboardRequestHandlerIOS>(
      action, content_analysis_info, upload_service, router, std::move(url),
      type, access_point, std::move(clipboard_source),
      std::move(source_content_area_email), std::move(content_transfer_method),
      std::move(data), std::move(callback), std::move(policy_getter));
}

}  // namespace enterprise_connectors
