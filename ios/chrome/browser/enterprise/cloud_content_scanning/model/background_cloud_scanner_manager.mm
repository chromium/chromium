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
#import "components/enterprise/connectors/core/reporting_event_router.h"
#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/files_request_handler_ios.h"
#import "ios/chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service.h"
#import "url/gurl.h"

namespace enterprise_connectors {

BackgroundCloudScannerManager::BackgroundCloudScanner::BackgroundCloudScanner(
    std::unique_ptr<ContentAnalysisInfo> info,
    ConnectorsService* connectors_service,
    ReportingEventRouter* router,
    BinaryUploadService* upload_service,
    const GURL& url,
    const base::FilePath& file_path,
    OnCompleteCallback on_complete_callback)
    : info_(std::move(info)),
      on_complete_callback_(std::move(on_complete_callback)) {
  auto delegate = std::make_unique<FilesRequestHandlerIOS>(
      connectors_service, router, file_path,
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
    ConnectorsService* connectors_service,
    ReportingEventRouter* router,
    BinaryUploadService* upload_service)
    : upload_service_(upload_service),
      connectors_service_(connectors_service),
      router_(router) {}

BackgroundCloudScannerManager::~BackgroundCloudScannerManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The scanners must have been destroyed by Shutdown(), see the comment there.
  DCHECK(scanners_.empty());
  DCHECK(scanners_pending_deletion_.empty());
}

void BackgroundCloudScannerManager::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_shutdown_ = true;

  // Destroy the pending scans here rather than in the destructor. Destroying a
  // scan cancels its in-flight request, which synchronously reports an
  // "unscanned file" audit event and thus needs to access ConnectorsService and
  // ReportingEventRouter through their factories. KeyedServices are only
  // deleted after the profile has been marked as destroyed, so doing this in
  // the destructor would hit a NOTREACHED() in DependencyManager. At Shutdown()
  // time, the profile is still alive and the services this manager depends on
  // have not been shut down yet (they are shut down after their dependents).
  scanners_.clear();
  scanners_pending_deletion_.clear();

  upload_service_ = nullptr;
  connectors_service_ = nullptr;
  router_ = nullptr;
}

void BackgroundCloudScannerManager::StartScanner(
    std::unique_ptr<ContentAnalysisInfo> info,
    const GURL& url,
    const base::FilePath& file_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Scans can't be started once the profile is being torn down.
  if (is_shutdown_) {
    return;
  }
  auto scanner = base::WrapUnique(new BackgroundCloudScanner(
      std::move(info), connectors_service_, router_, upload_service_, url,
      file_path,
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
  if (it == scanners_.end()) {
    return;
  }

  // The scanner can't be deleted synchronously since it is still running the
  // callback this is called from. Park it in `scanners_pending_deletion_`
  // instead of handing it over to DeleteSoon() so that it stays owned by this
  // manager: if the profile is destroyed before the task below runs, Shutdown()
  // destroys the scanner while the services it reports to are still alive.
  scanners_pending_deletion_.push_back(std::move(*it));
  scanners_.erase(it);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&BackgroundCloudScannerManager::DeletePendingScanners,
                     GetWeakPtr()));
}

void BackgroundCloudScannerManager::DeletePendingScanners() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scanners_pending_deletion_.clear();
}

base::WeakPtr<BackgroundCloudScannerManager>
BackgroundCloudScannerManager::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

size_t BackgroundCloudScannerManager::GetScannerCountForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return scanners_.size();
}

size_t BackgroundCloudScannerManager::GetPendingDeletionCountForTesting()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return scanners_pending_deletion_.size();
}

}  // namespace enterprise_connectors
