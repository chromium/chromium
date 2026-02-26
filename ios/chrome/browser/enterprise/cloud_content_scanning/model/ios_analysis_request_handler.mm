// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/ios_analysis_request_handler.h"

#import "base/notimplemented.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/components/enterprise/analysis/features.h"

namespace enterprise_connectors {

IOSAnalysisRequestHandler::IOSAnalysisRequestHandler(
    ContentAnalysisInfo* content_analysis_info,
    ProfileIOS* profile,
    const std::string& content_transfer_method,
    DeepScanAccessPoint access_point,
    const base::FilePath path,
    CompletionCallback callback)
    : callback_(std::move(callback)) {}

void IOSAnalysisRequestHandler::PrepareContentAnalysisRequest() {
  if (!base::FeatureList::IsEnabled(kEnableFileDownloadConnectorIOS)) {
    RequestHandlerResult result;
    result.final_result = FinalContentAnalysisResult::SUCCESS;
    std::move(callback_).Run(result);
    return;
  }

  NOTIMPLEMENTED();
}

IOSAnalysisRequestHandler::~IOSAnalysisRequestHandler() = default;

}  // namespace enterprise_connectors
