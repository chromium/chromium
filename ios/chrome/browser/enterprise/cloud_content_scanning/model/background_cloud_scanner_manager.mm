// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/background_cloud_scanner_manager.h"

#import "base/check.h"
#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/memory/ptr_util.h"
#import "base/sequence_checker.h"
#import "base/task/sequenced_task_runner.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/files_request_handler_base.h"
#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/files_request_handler_ios.h"
#import "ios/chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "url/gurl.h"

namespace enterprise_connectors {

BackgroundCloudScannerManager::BackgroundCloudScanner::BackgroundCloudScanner(
    std::unique_ptr<ContentAnalysisInfo> info,
    ProfileIOS* profile,
    BinaryUploadService* upload_service,
    const GURL& url,
    const base::FilePath& file_path,
    OnCompleteCallback on_complete_callback)
    : info_(std::move(info)),
      on_complete_callback_(std::move(on_complete_callback)) {
  auto delegate = std::make_unique<FilesRequestHandlerIOS>(
      profile, file_path,
      base::BindOnce(&BackgroundCloudScannerManager::BackgroundCloudScanner::
                         OnScanComplete,
                     weak_ptr_factory_.GetWeakPtr()));
  handler_ = std::make_unique<FilesRequestHandlerBase>(
      info_.get(), upload_service, url, "", DeepScanAccessPoint::DOWNLOAD,
      std::move(delegate));
}

void BackgroundCloudScannerManager::BackgroundCloudScanner::Start() {
  handler_->UploadData();
}

BackgroundCloudScannerManager::BackgroundCloudScanner::
    ~BackgroundCloudScanner() = default;

void BackgroundCloudScannerManager::BackgroundCloudScanner::OnScanComplete(
    RequestHandlerResult result) {
  std::move(on_complete_callback_).Run(this);
}

BackgroundCloudScannerManager::BackgroundCloudScannerManager(
    ProfileIOS* profile,
    BinaryUploadService* upload_service)
    : profile_(profile), upload_service_(upload_service) {}

BackgroundCloudScannerManager::~BackgroundCloudScannerManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BackgroundCloudScannerManager::StartScanner(
    std::unique_ptr<ContentAnalysisInfo> info,
    const GURL& url,
    const base::FilePath& file_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto scanner = base::WrapUnique(new BackgroundCloudScanner(
      std::move(info), profile_, upload_service_, url, file_path,
      base::BindOnce(&BackgroundCloudScannerManager::RemoveScanner,
                     GetWeakPtr())));
  BackgroundCloudScanner* scanner_ptr = scanner.get();
  AddScanner(std::move(scanner));
  scanner_ptr->Start();
}

void BackgroundCloudScannerManager::AddScanner(
    std::unique_ptr<BackgroundCloudScanner> scanner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scanners_.push_back(std::move(scanner));
}

void BackgroundCloudScannerManager::RemoveScanner(
    BackgroundCloudScanner* scanner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it =
      std::find_if(scanners_.begin(), scanners_.end(),
                   [scanner](const std::unique_ptr<BackgroundCloudScanner>& s) {
                     return s.get() == scanner;
                   });
  if (it != scanners_.end()) {
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                               std::move(*it));
    scanners_.erase(it);
  }
}

base::WeakPtr<BackgroundCloudScannerManager>
BackgroundCloudScannerManager::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace enterprise_connectors
