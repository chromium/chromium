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

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_TEST_TEST_CLIPBOARD_REQUEST_HANDLER_IOS_H_
