// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_IOS_CONTENT_ANALYSIS_REQUEST_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_IOS_CONTENT_ANALYSIS_REQUEST_H_

#import "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_request.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/common.h"

namespace enterprise_connectors {

// A BinaryUploadRequest implementation that gets the data to scan from a file
// or a string corresponding to the image data.
//
// TODO(crbug.com/482050524): Implement this class and switch to inherit from
// file_analysis_request_base.
class IOSContentAnalysisRequest : public BinaryUploadRequest {
 public:
  // Creates a IOSContentAnalysisRequest from a file located on disk and the
  // file path is available.
  IOSContentAnalysisRequest(
      const enterprise_connectors::AnalysisSettings& analysis_settings,
      base::FilePath path,
      std::string mime_type,
      BinaryUploadRequest::ContentAnalysisCallback callback);

  // Creates a IOSContentAnalysisRequest from data already in memory.
  IOSContentAnalysisRequest(
      const enterprise_connectors::AnalysisSettings& analysis_settings,
      std::string mime_type,
      std::string data,
      BinaryUploadRequest::ContentAnalysisCallback callback);

  IOSContentAnalysisRequest(const IOSContentAnalysisRequest&) = delete;
  IOSContentAnalysisRequest& operator=(const IOSContentAnalysisRequest&) =
      delete;
  ~IOSContentAnalysisRequest() override;

  // BinaryUploadRequest override.
  void GetRequestData(DataCallback callback) override;

 private:
  Data data_;
  ScanRequestUploadResult result_;
};

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_IOS_CONTENT_ANALYSIS_REQUEST_H_
