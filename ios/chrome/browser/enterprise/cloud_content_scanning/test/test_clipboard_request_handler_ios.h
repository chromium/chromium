// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_TEST_TEST_CLIPBOARD_REQUEST_HANDLER_IOS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_TEST_TEST_CLIPBOARD_REQUEST_HANDLER_IOS_H_

#import <memory>
#import <string>

#import "components/enterprise/connectors/core/cloud_content_scanning/clipboard_request_handler.h"
#import "url/gurl.h"

namespace enterprise_connectors {

class TestClipboardRequestHandlerIOS : public ClipboardRequestHandler {
 public:
  TestClipboardRequestHandlerIOS(
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
      BinaryUploadRequest::BrowserPolicyConnectorGetter policy_getter);

  void UploadForDeepScanning(
      std::unique_ptr<ClipboardAnalysisRequest> request) override;

 private:
  TriggeredRule::Action action_;
};

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
    BinaryUploadRequest::BrowserPolicyConnectorGetter policy_getter);

// This method is used to inject a `closure_callback` in between getting the
// `RequestHandlerResult` from scanning and before providing the result to the
// tab helper. It can be used to inject code for verification or simulate change
// of "Pasting state" to invalidate a paste event.
void SetMockClipboardRequestHandlerWithClosureForTesting(
    TriggeredRule::Action action,
    base::RepeatingClosure closure_callback);

// This method is used to simulate a scan that never completes by dropping the
// completion callback without executing it, preventing the tab helper from
// receiving a result. The `closure_callback` is executed to allow the test to
// wait for the background request to be triggered.
void SetMockClipboardRequestHandlerWithClosureAndNoResultForTesting(
    TriggeredRule::Action action,
    base::RepeatingClosure closure_callback);

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_TEST_TEST_CLIPBOARD_REQUEST_HANDLER_IOS_H_
