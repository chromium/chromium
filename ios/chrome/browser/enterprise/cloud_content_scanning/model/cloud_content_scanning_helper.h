// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_CLOUD_CONTENT_SCANNING_HELPER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_CLOUD_CONTENT_SCANNING_HELPER_H_

#import <Foundation/Foundation.h>

#import <memory>

#import "base/functional/callback.h"
#import "base/memory/weak_ptr.h"
#import "components/enterprise/connectors/core/common.h"

namespace base {
class FilePath;
}

namespace web {
class WebState;
}

class GURL;

namespace enterprise_connectors {

class ContentAnalysisInfo;
class FilesRequestHandlerBase;

// Represents the download type that triggers the file download.
enum TriggerType { kSavePrompt, kShareSheet };

// Holds the resources required for a file download scanning request,
// maintaining their lifetime while the scanning process is active.
struct FileDownloadScanningResources {
  FileDownloadScanningResources();
  ~FileDownloadScanningResources();
  FileDownloadScanningResources(FileDownloadScanningResources&&);
  FileDownloadScanningResources& operator=(FileDownloadScanningResources&&);

  std::unique_ptr<ContentAnalysisInfo> content_analysis_info;
  std::unique_ptr<FilesRequestHandlerBase> files_request_handler;
};

// Handles the scan decision and shows the Warning Dialog or Snackbar Message to
// the user. A callback `download_proceed` will run at the end to indicate if
// the download should proceed.
void HandleScanDecision(base::WeakPtr<web::WebState> web_state,
                        TriggerType trigger_type,
                        base::OnceCallback<void(bool)> download_proceed,
                        RequestHandlerResult result);

// Starts the file download scanning process for a file.
// Returns a `FileDownloadScanningResources` object that must be kept alive by
// the caller while the scanning is in progress.
FileDownloadScanningResources StartCloudContentScanning(
    web::WebState* web_state,
    const GURL& url,
    const base::FilePath& file_path,
    TriggerType trigger_type,
    base::OnceCallback<void(bool)> download_proceed);

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_CLOUD_CONTENT_SCANNING_HELPER_H_
