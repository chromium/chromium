// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_IOS_ANALYSIS_REQUEST_HANDLER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_IOS_ANALYSIS_REQUEST_HANDLER_H_

#import "base/files/file_path.h"
#import "base/functional/callback.h"
#import "components/enterprise/connectors/core/common.h"
#import "ios/chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"

class ProfileIOS;

namespace enterprise_connectors {

// Handles content scanning requests for a given file specified by `path`, on
// completion of the scan, `callback_` is called with the scanning results.
//
// TODO(crbug.com/482051070): Implemment this class.
class IOSAnalysisRequestHandler {
 public:
  // Callback that informs caller of scanning verdicts for the file.
  using CompletionCallback = base::OnceCallback<void(RequestHandlerResult)>;

  // Constructor for IOSAnalysisRequestHandler.
  //
  // - `content_analysis_info`: provides analysis settings and content analysis
  // context.
  // - `profile`: the profile associated with the analysis request.
  // - `content_transfer_method`: string indicating the content transfer method
  // for the action.
  // - `access_point`: enum indicating the context in which the scan was
  // triggered.
  // - `path`: the file path of the content to be analyzed.
  // primarily accessed via `path`.
  // - `callback`: the callback to be run once the analysis is complete,
  // informing the caller of the result.
  IOSAnalysisRequestHandler(ContentAnalysisInfo* content_analysis_info,
                            ProfileIOS* profile,
                            const std::string& content_transfer_method,
                            DeepScanAccessPoint access_point,
                            const base::FilePath path,
                            CompletionCallback callback);

  IOSAnalysisRequestHandler(const IOSAnalysisRequestHandler&) = delete;
  IOSAnalysisRequestHandler& operator=(const IOSAnalysisRequestHandler&) =
      delete;
  ~IOSAnalysisRequestHandler();

  //  Prepares an upload request for the file the content scanning.
  void PrepareContentAnalysisRequest();

 private:
  CompletionCallback callback_;
};

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_IOS_ANALYSIS_REQUEST_HANDLER_H_
