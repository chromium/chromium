// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/background_cloud_scanner_manager.h"

#import <sys/clonefile.h>

#import <string>
#import <string_view>

#import "base/check.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/no_destructor.h"
#import "base/sequence_checker.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/uuid.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/files_request_handler_base.h"
#import "components/enterprise/connectors/core/reporting_event_router.h"
#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/download_protection_metrics.h"
#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/files_request_handler_ios.h"
#import "ios/chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service.h"
#import "url/gurl.h"

namespace enterprise_connectors {

BASE_FEATURE(kEnableDLPClonefileScanUploadIOS,
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

// Name of the temporary directory used for enterprise download scans.
constexpr std::string_view kEnterpriseScanDirName = "enterprise_scan";

// Info of the file to scan.
struct ClonedFileInfo {
  // The temporary directory path for storing the cloned file. Empty if cloning
  // was skipped or failed.
  base::FilePath temp_dir;

  // The cloned file with the same filename and extension of the original file,
  // or the original file path if cloning was skipped or failed.
  base::FilePath file_name;
};

// Returns a UUID for the current app session.
const std::string& GetEnterpriseScanSession() {
  static const base::NoDestructor<std::string> session_id(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  return *session_id;
}

// Fills `directory_path` with the FilePath to the temporary enterprise scan
// directory for the current session. Returns true if this is successful.
bool GetSessionEnterpriseScanDirectory(base::FilePath* directory_path) {
  if (!GetTempEnterpriseScanDirectory(directory_path)) {
    return false;
  }
  *directory_path = directory_path->Append(GetEnterpriseScanSession());
  return true;
}

// Synchronously deletes temporary enterprise scan directories from previous
// sessions, skipping the current session's directory. Runs on a background
// thread.
void DeleteTempEnterpriseScanDirectorySync() {
  base::FilePath scan_directory;
  if (!GetTempEnterpriseScanDirectory(&scan_directory) ||
      !base::DirectoryExists(scan_directory)) {
    return;
  }
  const std::string& session_directory = GetEnterpriseScanSession();
  base::FileEnumerator enumerator(scan_directory, /*recursive=*/false,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    if (path.BaseName().value() == session_directory) {
      continue;
    }
    base::DeletePathRecursively(path);
  }
}

// Deletes the temporary directory at `temp_dir`. Runs on a background thread.
void DeleteTemporaryPath(const base::FilePath& temp_dir) {
  bool delete_success = base::DeletePathRecursively(temp_dir);
  base::UmaHistogramBoolean(
      kIOSDownloadProtectionScanTriggeredTempDirectoryDeleteSuccessHistogram,
      delete_success);
}

// Clones the file at `source` to a new isolated temporary directory using
// APFS Copy-on-Write (clonefile) to avoid duplicating bytes while preserving
// the original file name and extension. Falls back to `source` if the feature
// is disabled or cloning fails. Runs on a background thread.
ClonedFileInfo CloneToTemporaryDirectory(const base::FilePath& source) {
  if (!base::FeatureList::IsEnabled(kEnableDLPClonefileScanUploadIOS)) {
    return ClonedFileInfo{.file_name = source};
  }

  base::FilePath session_scan_dir;
  if (!GetSessionEnterpriseScanDirectory(&session_scan_dir)) {
    return ClonedFileInfo{.file_name = source};
  }
  base::FilePath temp_dir = session_scan_dir.Append(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  if (!base::CreateDirectory(temp_dir)) {
    return ClonedFileInfo{.file_name = source};
  }
  base::FilePath clone_path = temp_dir.Append(source.BaseName());

  bool clone_success =
      clonefile(source.value().c_str(), clone_path.value().c_str(), 0) == 0;

  base::UmaHistogramBoolean(
      kIOSDownloadProtectionScanTriggeredClonefileSuccessHistogram,
      clone_success);

  // If `clonefile` fails, clean up the temporary directory and fall back to
  // scanning `source` directly.
  if (!clone_success) {
    DeleteTemporaryPath(temp_dir);
    return ClonedFileInfo{.file_name = source};
  }

  return ClonedFileInfo{.temp_dir = temp_dir, .file_name = clone_path};
}

// Called on the UI thread when the file clone task completes.
void OnTemporaryFileCloned(base::WeakPtr<BackgroundCloudScannerManager> manager,
                           std::unique_ptr<ContentAnalysisInfo> info,
                           const GURL& url,
                           base::OnceCallback<void(bool)> download_proceed,
                           ClonedFileInfo file_info) {
  // Always allow the download to proceed once the clone is ready. This
  // guarantees the clone is ready before the original downloaded file is moved.
  if (download_proceed) {
    std::move(download_proceed).Run(true);
  }

  // If the manager is still alive, initiate the scan with `file_info.file_name`
  // (either the cloned file or the original file on fallback). If the manager
  // has been destroyed or shut down and a temporary directory was created,
  // clean up the temporary directory.
  if (manager && !file_info.file_name.empty()) {
    manager->StartScannerWithClonedFile(
        std::move(info), url, file_info.file_name, file_info.temp_dir);
  } else if (!file_info.temp_dir.empty()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&DeleteTemporaryPath, file_info.temp_dir));
  }
}

}  // namespace

bool GetTempEnterpriseScanDirectory(base::FilePath* directory_path) {
  if (!base::GetTempDir(directory_path)) {
    return false;
  }
  *directory_path = directory_path->Append(kEnterpriseScanDirName);
  return true;
}

void DeleteTempEnterpriseScanDirectory() {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&DeleteTempEnterpriseScanDirectorySync));
}

BackgroundCloudScannerManager::BackgroundCloudScanner::BackgroundCloudScanner(
    std::unique_ptr<ContentAnalysisInfo> info,
    ConnectorsService* connectors_service,
    ReportingEventRouter* router,
    BinaryUploadService* upload_service,
    const GURL& url,
    const base::FilePath& file_path,
    const base::FilePath& temp_dir,
    OnCompleteCallback on_complete_callback)
    : info_(std::move(info)),
      on_complete_callback_(std::move(on_complete_callback)),
      temp_dir_(temp_dir) {
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
    ~BackgroundCloudScanner() {
  if (!temp_dir_.empty()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&DeleteTemporaryPath, std::move(temp_dir_)));
  }
}

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
    const base::FilePath& file_path,
    base::OnceCallback<void(bool)> download_proceed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Scans can't be started once the profile is being torn down.
  if (is_shutdown_) {
    if (download_proceed) {
      // Ensure `download_proceed` is run as an asynchronous task.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(download_proceed), true));
    }
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&CloneToTemporaryDirectory, file_path),
      base::BindOnce(&OnTemporaryFileCloned, GetWeakPtr(), std::move(info), url,
                     std::move(download_proceed)));
}

void BackgroundCloudScannerManager::StartScannerWithClonedFile(
    std::unique_ptr<ContentAnalysisInfo> info,
    const GURL& url,
    const base::FilePath& clone_path,
    const base::FilePath& temp_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_shutdown_) {
    if (!temp_dir.empty()) {
      base::ThreadPool::PostTask(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
          base::BindOnce(&DeleteTemporaryPath, temp_dir));
    }
    return;
  }
  auto scanner = base::WrapUnique(new BackgroundCloudScanner(
      std::move(info), connectors_service_, router_, upload_service_, url,
      clone_path, temp_dir,
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
