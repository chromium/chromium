// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_IOS_CONTENT_ANALYSIS_REQUEST_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_IOS_CONTENT_ANALYSIS_REQUEST_H_

#import "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/file_analysis_request_base.h"

namespace enterprise_connectors {

// A FileAnalysisRequestBase implementation that gets the data to scan from a
// file.
class IOSContentAnalysisRequest : public FileAnalysisRequestBase {
 public:
  // Creates a IOSContentAnalysisRequest from a file located on disk and the
  // file path is available.
  IOSContentAnalysisRequest(
      const enterprise_connectors::AnalysisSettings& analysis_settings,
      base::FilePath path,
      base::FilePath file_name,
      std::string mime_type,
      bool delay_opening_file,
      BinaryUploadRequest::ContentAnalysisCallback callback,
      BinaryUploadRequest::RequestStartCallback start_callback);

  IOSContentAnalysisRequest(const IOSContentAnalysisRequest&) = delete;
  IOSContentAnalysisRequest& operator=(const IOSContentAnalysisRequest&) =
      delete;
  ~IOSContentAnalysisRequest() override;
};

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_IOS_CONTENT_ANALYSIS_REQUEST_H_
