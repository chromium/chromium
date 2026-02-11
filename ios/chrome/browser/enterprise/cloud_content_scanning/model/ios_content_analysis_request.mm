// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/ios_content_analysis_request.h"

#import <memory>

#import "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"

namespace enterprise_connectors {

IOSContentAnalysisRequest::IOSContentAnalysisRequest(
    const enterprise_connectors::AnalysisSettings& analysis_settings,
    base::FilePath path,
    std::string mime_type,
    BinaryUploadRequest::ContentAnalysisCallback callback)
    : BinaryUploadRequest(
          std::move(callback),
          analysis_settings.cloud_or_local_settings,
          base::BindRepeating([]() -> policy::BrowserPolicyConnector* {
            return GetApplicationContext()->GetBrowserPolicyConnector();
          })) {
  DCHECK(!path.empty());
  data_.path = path;
  data_.mime_type = mime_type;
  result_ = ScanRequestUploadResult::kSuccess;
  set_filename(path.AsUTF8Unsafe());
}

IOSContentAnalysisRequest::IOSContentAnalysisRequest(
    const enterprise_connectors::AnalysisSettings& analysis_settings,
    std::string mime_type,
    std::string data,
    BinaryUploadRequest::ContentAnalysisCallback callback)
    : BinaryUploadRequest(
          std::move(callback),
          analysis_settings.cloud_or_local_settings,
          base::BindRepeating([]() -> policy::BrowserPolicyConnector* {
            return GetApplicationContext()->GetBrowserPolicyConnector();
          })) {
  DCHECK_GT(data.size(), 0u);
  data_.size = data.size();
  data_.mime_type = std::move(mime_type);

  if (data.size() < BinaryUploadService::kMaxUploadSizeBytes) {
    // TODO(crbug.com/482050524): Add something to signal this is a download
    // request.
    data_.contents = std::move(data);
    result_ = ScanRequestUploadResult::kSuccess;
  } else {
    result_ = ScanRequestUploadResult::kFileTooLarge;
  }
}

IOSContentAnalysisRequest::~IOSContentAnalysisRequest() = default;

void IOSContentAnalysisRequest::GetRequestData(DataCallback callback) {
  std::move(callback).Run(result_, data_);
}

}  // namespace enterprise_connectors
