// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/files_request_handler_ios.h"

#import "components/enterprise/connectors/core/cloud_content_scanning/deep_scanning_utils.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/request_handler_base.h"
#import "components/enterprise/connectors/core/reporting_constants.h"
#import "components/enterprise/connectors/core/reporting_event_router.h"
#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/ios_content_analysis_request.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service_factory.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_util.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_reporting_event_router_factory.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/reporting_util.h"

namespace enterprise_connectors {

FilesRequestHandlerIOS::FilesRequestHandlerIOS(ProfileIOS* profile,
                                               const base::FilePath& path,
                                               CompletionCallback callback)
    : profile_(profile), path_(path), callback_(std::move(callback)) {}

FilesRequestHandlerIOS::~FilesRequestHandlerIOS() = default;

void FilesRequestHandlerIOS::SetHandler(FilesRequestHandlerBase* handler) {
  CHECK(handler);
  handler_ = handler;
}

std::unique_ptr<FileAnalysisRequestBase>
FilesRequestHandlerIOS::CreateFileRequest(
    size_t index,
    const AnalysisSettings& settings,
    base::OnceCallback<void(ScanRequestUploadResult, ContentAnalysisResponse)>
        callback,
    base::OnceCallback<void(const BinaryUploadRequest&)>
        request_start_callback) {
  return std::make_unique<IOSContentAnalysisRequest>(
      settings, path_, path_.BaseName(),
      /*mime_type*/ "", /*delay_opening_file*/ false, std::move(callback),
      std::move(request_start_callback));
}

void FilesRequestHandlerIOS::ReportWarningBypass(
    std::optional<std::u16string> user_justification,
    const ContentAnalysisInfoBase& info,
    const std::string& trigger,
    const std::string& content_transfer_method) {
  if (!was_warned_) {
    return;
  }
  ReportAnalysisConnectorWarningBypass(
      GetReportingEventRouter(), &info, GetSource(), GetDestination(),
      path_.AsUTF8Unsafe(), file_info_.sha256_or_cb, file_info_.mime_type,
      trigger, content_transfer_method, file_info_.size, response_,
      user_justification);
}

bool FilesRequestHandlerIOS::UploadDataImpl() {
  // If the DLP download protection is not enabled or no files are passed to the
  // FilesRequestHandlerIOS, we call the callback directly.
  if (path_.empty() || !IsDownloadConnectorEnabled(
                           ConnectorsServiceFactory::GetForProfile(profile_))) {
    result_.final_result = FinalContentAnalysisResult::SUCCESS;
    MaybeCompleteScanRequest();
    return false;
  }

  handler_->PrepareFileRequest(/*index*/ 0);
  return true;
}

void FilesRequestHandlerIOS::UpdateRequestHandlerResult(
    size_t index,
    RequestHandlerResult result,
    ContentAnalysisResponse response) {
  result_ = result;
  was_warned_ = result.final_result == FinalContentAnalysisResult::WARNING;
  response_ = response;

  EventResult event_result = GetEventResult(result);
  if (event_result != EventResult::ALLOWED) {
    MaybeReportDangerousDownloadEvent(*handler_->content_analysis_info(),
                                      response_, path_, file_info_,
                                      event_result, GetReportingEventRouter());
  }
}

const base::FilePath& FilesRequestHandlerIOS::GetPath(size_t index) const {
  return path_;
}

const FilesRequestHandlerBase::FileInfo& FilesRequestHandlerIOS::GetFileInfo(
    size_t index) {
  return file_info_;
}

FilesRequestHandlerBase::FileInfo& FilesRequestHandlerIOS::GetMutableFileInfo(
    size_t index) {
  return file_info_;
}

size_t FilesRequestHandlerIOS::GetFileCount() const {
  return path_.empty() ? 0 : 1;
}

const base::TimeTicks FilesRequestHandlerIOS::GetFileScanStartTime(
    size_t index) {
  return start_time_;
}

ReportingEventRouter* FilesRequestHandlerIOS::GetReportingEventRouter() {
  return IOSReportingEventRouterFactory::GetForProfile(profile_);
}

void FilesRequestHandlerIOS::MaybeCompleteScanRequest() {
  DCHECK(!callback_.is_null());
  std::move(callback_).Run(std::move(result_));
}

void FilesRequestHandlerIOS::MaybeCancelAndReport() {
  if (was_reported_ || path_.empty() || !handler_) {
    return;
  }

  handler_->ReportCanceledFile(/*index=*/0);
}

void FilesRequestHandlerIOS::MarkFileAsReported(size_t index) {
  was_reported_ = true;
}

std::string FilesRequestHandlerIOS::GetSource() {
  // For ios, we don't have the source concept, so we return an empty string
  // here.
  return "";
}

std::string FilesRequestHandlerIOS::GetDestination() {
  // For ios, we don't have the destination concept, so we return an empty
  // string here.
  return "";
}

void FilesRequestHandlerIOS::SetFileScanStartTime(size_t index) {
  start_time_ = base::TimeTicks::Now();
}

}  // namespace enterprise_connectors
