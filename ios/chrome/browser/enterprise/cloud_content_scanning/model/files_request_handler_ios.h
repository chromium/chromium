// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_FILES_REQUEST_HANDLER_IOS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_FILES_REQUEST_HANDLER_IOS_H_

#import "base/memory/raw_ptr.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/files_request_handler_base.h"
#import "components/enterprise/connectors/core/common.h"

class ProfileIOS;

namespace enterprise_connectors {

// iOS-specific implementation of the FilesRequestHandlerBase::Delegate. This
// class handles the details of a single file analysis request on iOS, including
// request preparation, uploading, and reporting.
class FilesRequestHandlerIOS : public FilesRequestHandlerBase::Delegate {
 public:
  // Callback that informs caller of scanning verdicts for the deep scanning
  // request.
  using CompletionCallback = base::OnceCallback<void(RequestHandlerResult)>;

  FilesRequestHandlerIOS(ProfileIOS* profile,
                         const base::FilePath& path,
                         CompletionCallback callback);

  FilesRequestHandlerIOS(const FilesRequestHandlerIOS&) = delete;
  FilesRequestHandlerIOS& operator=(const FilesRequestHandlerIOS&) = delete;
  ~FilesRequestHandlerIOS() override;

  // FilesRequestHandlerBase::Delegate overrides:
  std::unique_ptr<FileAnalysisRequestBase> CreateFileRequest(
      size_t index,
      const AnalysisSettings& settings,
      base::OnceCallback<void(ScanRequestUploadResult, ContentAnalysisResponse)>
          callback,
      base::OnceCallback<void(const BinaryUploadRequest&)>
          request_start_callback) override;
  void ReportWarningBypass(std::optional<std::u16string> user_justification,
                           const ContentAnalysisInfoBase& info,
                           const std::string& trigger,
                           const std::string& content_transfer_method) override;
  bool UploadDataImpl() override;
  void UpdateRequestHandlerResult(size_t index,
                                  RequestHandlerResult result,
                                  ContentAnalysisResponse response) override;
  const base::FilePath& GetPath(size_t index) const override;
  const FilesRequestHandlerBase::FileInfo& GetFileInfo(size_t index) override;
  FilesRequestHandlerBase::FileInfo& GetMutableFileInfo(size_t index) override;
  size_t GetFileCount() const override;
  void SetFileScanStartTime(size_t index) override;
  const base::TimeTicks GetFileScanStartTime(size_t index) override;
  ReportingEventRouter* GetReportingEventRouter() override;
  void MaybeCompleteScanRequest() override;
  std::string GetSource() override;
  std::string GetDestination() override;
  void SetHandler(FilesRequestHandlerBase* handler) override;
  void MaybeCancelAndReport() override;
  void MarkFileAsReported(size_t index) override;

 private:
  raw_ptr<FilesRequestHandlerBase> handler_;
  raw_ptr<ProfileIOS> profile_;
  base::FilePath path_;
  FilesRequestHandlerBase::FileInfo file_info_;
  CompletionCallback callback_;

  base::TimeTicks start_time_ = base::TimeTicks::Min();

  RequestHandlerResult result_;
  ContentAnalysisResponse response_;
  bool was_reported_ = false;

  // Whether the scan result is WARNING.
  bool was_warned_ = false;

  base::WeakPtrFactory<FilesRequestHandlerIOS> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_FILES_REQUEST_HANDLER_IOS_H_
