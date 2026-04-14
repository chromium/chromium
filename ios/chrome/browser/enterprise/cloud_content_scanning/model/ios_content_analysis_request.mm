// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/ios_content_analysis_request.h"

#import <memory>

#import "base/task/sequenced_task_runner.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

namespace enterprise_connectors {

IOSContentAnalysisRequest::IOSContentAnalysisRequest(
    const enterprise_connectors::AnalysisSettings& analysis_settings,
    base::FilePath path,
    base::FilePath file_name,
    std::string mime_type,
    bool delay_opening_file,
    BinaryUploadRequest::ContentAnalysisCallback callback,
    BinaryUploadRequest::RequestStartCallback start_callback)
    : FileAnalysisRequestBase(
          analysis_settings,
          std::move(path),
          std::move(file_name),
          std::move(mime_type),
          delay_opening_file,
          std::move(callback),
          base::BindRepeating([]() -> policy::BrowserPolicyConnector* {
            return GetApplicationContext()->GetBrowserPolicyConnector();
          }),
          web::GetUIThreadTaskRunner({}),
          std::move(start_callback)) {}

IOSContentAnalysisRequest::~IOSContentAnalysisRequest() = default;

}  // namespace enterprise_connectors
