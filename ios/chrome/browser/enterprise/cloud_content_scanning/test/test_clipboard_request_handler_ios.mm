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

void SetMockClipboardRequestHandlerWithClosureForTesting(
    TriggeredRule::Action action,
    base::RepeatingClosure closure_callback) {
  enterprise_connectors::ClipboardRequestHandler::SetFactoryForTesting(
      base::BindRepeating(
          [](TriggeredRule::Action action,
             base::RepeatingClosure closure_callback,
             enterprise_connectors::ContentAnalysisInfoBase*
                 content_analysis_info,
             enterprise_connectors::BinaryUploadService* upload_service,
             enterprise_connectors::ReportingEventRouter* router, GURL url,
             enterprise_connectors::ClipboardRequestHandler::Type type,
             enterprise_connectors::DeepScanAccessPoint access_point,
             enterprise_connectors::ContentMetaData::CopiedTextSource
                 clipboard_source,
             std::string source_content_area_email,
             std::string content_transfer_method, std::string data,
             enterprise_connectors::ClipboardRequestHandler::CompletionCallback
                 callback,
             enterprise_connectors::BinaryUploadRequest::
                 BrowserPolicyConnectorGetter policy_getter) {
            auto wrapped_callback = base::BindOnce(
                [](base::RepeatingClosure closure_callback,
                   enterprise_connectors::ClipboardRequestHandler::
                       CompletionCallback original_callback,
                   enterprise_connectors::RequestHandlerResult result) {
                  closure_callback.Run();
                  std::move(original_callback).Run(result);
                },
                closure_callback, std::move(callback));

            return enterprise_connectors::CreateTestClipboardRequestHandlerIOS(
                action, content_analysis_info, upload_service, router,
                std::move(url), type, access_point, std::move(clipboard_source),
                std::move(source_content_area_email),
                std::move(content_transfer_method), std::move(data),
                std::move(wrapped_callback), std::move(policy_getter));
          },
          action, closure_callback));
}

void SetMockClipboardRequestHandlerWithClosureAndNoResultForTesting(
    TriggeredRule::Action action,
    base::RepeatingClosure closure_callback) {
  enterprise_connectors::ClipboardRequestHandler::SetFactoryForTesting(
      base::BindRepeating(
          [](TriggeredRule::Action action,
             base::RepeatingClosure closure_callback,
             enterprise_connectors::ContentAnalysisInfoBase*
                 content_analysis_info,
             enterprise_connectors::BinaryUploadService* upload_service,
             enterprise_connectors::ReportingEventRouter* router, GURL url,
             enterprise_connectors::ClipboardRequestHandler::Type type,
             enterprise_connectors::DeepScanAccessPoint access_point,
             enterprise_connectors::ContentMetaData::CopiedTextSource
                 clipboard_source,
             std::string source_content_area_email,
             std::string content_transfer_method, std::string data,
             enterprise_connectors::ClipboardRequestHandler::CompletionCallback
                 callback,
             enterprise_connectors::BinaryUploadRequest::
                 BrowserPolicyConnectorGetter policy_getter) {
            auto dropped_callback = base::BindOnce(
                [](base::RepeatingClosure closure_callback,
                   enterprise_connectors::ClipboardRequestHandler::
                       CompletionCallback /*original_callback*/,
                   enterprise_connectors::RequestHandlerResult result) {
                  closure_callback.Run();
                  // Do nothing else, effectively dropping the callback.
                },
                closure_callback, std::move(callback));

            return enterprise_connectors::CreateTestClipboardRequestHandlerIOS(
                action, content_analysis_info, upload_service, router,
                std::move(url), type, access_point, std::move(clipboard_source),
                std::move(source_content_area_email),
                std::move(content_transfer_method), std::move(data),
                std::move(dropped_callback), std::move(policy_getter));
          },
          action, closure_callback));
}

}  // namespace enterprise_connectors
