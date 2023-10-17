// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm_storage_manager.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "base/types/pass_key.h"
#include "content/browser/media/cdm_storage_common.h"
#include "content/public/common/content_features.h"
#include "media/cdm/cdm_type.h"
#include "media/mojo/mojom/cdm_storage.mojom.h"

namespace content {

namespace {

const char kUmaPrefix[] = "Media.EME.CdmStorageManager.";

const char kIncognito[] = "Incognito";
const char kNonIncognito[] = "NonIncognito";

const char kMigration[] = ".Migration";

const char kDeleteDatabaseError[] = "DeleteDatabaseError.";
const char kDeleteForStorageKeyError[] = "DeleteForStorageKeyError.";
const char kDeleteFileError[] = "DeleteFileError.";
const char kWriteFileError[] = "WriteFileError.";
const char kReadFileError[] = "ReadFileError.";
const char kDatabaseOpenErrorNoPeriod[] = "DatabaseOpenError";
const char kDatabaseOpenError[] = "DatabaseOpenError.";

// Creates a task runner suitable for running SQLite database operations.
scoped_refptr<base::SequencedTaskRunner> CreateDatabaseTaskRunner() {
  // We use a SequencedTaskRunner so that there is a global ordering to a
  // storage key's directory operations.
  return base::ThreadPool::CreateSequencedTaskRunner({
      // Needed for file I/O.
      base::MayBlock(),

      // Reasonable compromise, given that a few database operations are
      // blocking, while most operations are not. We should be able to do better
      // when we get scheduling APIs on the Web Platform.
      base::TaskPriority::USER_VISIBLE,

      // Needed to allow for clearing site data on shutdown.
      base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
  });
}

}  // namespace

CdmStorageManager::CdmStorageManager(const base::FilePath& path)
    : path_(path), db_(CreateDatabaseTaskRunner(), path_) {}

CdmStorageManager::~CdmStorageManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CdmStorageManager::Open(const std::string& file_name,
                             OpenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (file_name.empty()) {
    DVLOG(1) << "No file specified.";
    ReportDatabaseOpenError(CdmStorageOpenError::kNoFileSpecified);
    std::move(callback).Run(Status::kFailure, mojo::NullAssociatedRemote());
    return;
  }

  if (!CdmFileImpl::IsValidName(file_name)) {
    DVLOG(1) << "Invalid name of file.";
    ReportDatabaseOpenError(CdmStorageOpenError::kInvalidFileName);
    std::move(callback).Run(Status::kFailure, mojo::NullAssociatedRemote());
    return;
  }

  db_.AsyncCall(&CdmStorageDatabase::EnsureOpen)
      .Then(base::BindOnce(&CdmStorageManager::DidOpenFile,
                           weak_factory_.GetWeakPtr(),
                           receivers_.current_context().storage_key,
                           receivers_.current_context().cdm_type, file_name,
                           std::move(callback)));
}

void CdmStorageManager::OpenCdmStorage(
    const CdmStorageBindingContext& binding_context,
    mojo::PendingReceiver<media::mojom::CdmStorage> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  receivers_.Add(this, std::move(receiver), binding_context);
}

void CdmStorageManager::ReadFile(
    const blink::StorageKey& storage_key,
    const media::CdmType& cdm_type,
    const std::string& file_name,
    base::OnceCallback<void(absl::optional<std::vector<uint8_t>>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  db_.AsyncCall(&CdmStorageDatabase::ReadFile)
      .WithArgs(storage_key, cdm_type, file_name)
      .Then(base::BindOnce(&CdmStorageManager::DidReadFile,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CdmStorageManager::WriteFile(const blink::StorageKey& storage_key,
                                  const media::CdmType& cdm_type,
                                  const std::string& file_name,
                                  const std::vector<uint8_t>& data,
                                  base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  db_.AsyncCall(&CdmStorageDatabase::WriteFile)
      .WithArgs(storage_key, cdm_type, file_name, data)
      .Then(base::BindOnce(&CdmStorageManager::DidWriteFile,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CdmStorageManager::DeleteFile(const blink::StorageKey& storage_key,
                                   const media::CdmType& cdm_type,
                                   const std::string& file_name,
                                   base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  db_.AsyncCall(&CdmStorageDatabase::DeleteFile)
      .WithArgs(storage_key, cdm_type, file_name)
      .Then(base::BindOnce(&CdmStorageManager::DidDeleteFile,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CdmStorageManager::DeleteDataForStorageKey(
    const blink::StorageKey& storage_key,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  db_.AsyncCall(&CdmStorageDatabase::DeleteDataForStorageKey)
      .WithArgs(storage_key)
      .Then(base::BindOnce(&CdmStorageManager::DidDeleteForStorageKey,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CdmStorageManager::DeleteDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  db_.AsyncCall(&CdmStorageDatabase::ClearDatabase)
      .Then(base::BindOnce(&CdmStorageManager::DidDeleteDatabase,
                           weak_factory_.GetWeakPtr()));
}

void CdmStorageManager::OnFileReceiverDisconnect(
    const std::string& name,
    const media::CdmType& cdm_type,
    const blink::StorageKey& storage_key,
    base::PassKey<CdmFileImpl> pass_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto count = cdm_files_.erase(CdmFileIdTwo(name, cdm_type, storage_key));
  DCHECK_GT(count, 0u);
}

void CdmStorageManager::DidOpenFile(const blink::StorageKey& storage_key,
                                    const media::CdmType& cdm_type,
                                    const std::string& file_name,
                                    OpenCallback callback,
                                    CdmStorageOpenError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error != CdmStorageOpenError::kOk) {
    ReportDatabaseOpenError(error);
    std::move(callback).Run(Status::kFailure, mojo::NullAssociatedRemote());
    return;
  }

  // Check whether this CDM file is in-use.
  CdmFileIdTwo id(file_name, cdm_type, storage_key);
  if (base::Contains(cdm_files_, id)) {
    std::move(callback).Run(Status::kInUse, mojo::NullAssociatedRemote());
    return;
  }

  // File was opened successfully, so create the binding and return success.
  mojo::PendingAssociatedRemote<media::mojom::CdmFile> cdm_file;

  cdm_files_.emplace(id, std::make_unique<CdmFileImpl>(
                             this, storage_key, cdm_type, file_name,
                             cdm_file.InitWithNewEndpointAndPassReceiver()));

  std::move(callback).Run(Status::kSuccess, std::move(cdm_file));
}

void CdmStorageManager::DidReadFile(
    base::OnceCallback<void(absl::optional<std::vector<uint8_t>>)> callback,
    absl::optional<std::vector<uint8_t>> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramBoolean(GetHistogramName(kReadFileError),
                            data == absl::nullopt);

  std::move(callback).Run(data);
}

void CdmStorageManager::DidWriteFile(base::OnceCallback<void(bool)> callback,
                                     bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramBoolean(GetHistogramName(kWriteFileError), !success);

  std::move(callback).Run(success);
}

void CdmStorageManager::DidDeleteFile(base::OnceCallback<void(bool)> callback,
                                      bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramBoolean(GetHistogramName(kDeleteFileError), !success);

  std::move(callback).Run(success);
}

void CdmStorageManager::DidDeleteForStorageKey(
    base::OnceCallback<void(bool)> callback,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramBoolean(GetHistogramName(kDeleteForStorageKeyError),
                            !success);

  std::move(callback).Run(success);
}

// TODO(crbug.com/1454512) Investigate if we can propagate the SQL errors.
// Investigate adding delete functionality to 'MojoCdmHelper::CloseCdmFileIO' to
// close database on CdmFileIO closure.
void CdmStorageManager::DidDeleteDatabase(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramBoolean(GetHistogramName(kDeleteDatabaseError), !success);
}

void CdmStorageManager::ReportDatabaseOpenError(CdmStorageOpenError error) {
  // General Errors without distinguishing incognito or not.
  base::UmaHistogramEnumeration(
      std::string{kUmaPrefix} + std::string{kDatabaseOpenErrorNoPeriod}, error);

  // Histogram split by incognito and non-incognito.
  base::UmaHistogramEnumeration(GetHistogramName(kDatabaseOpenError), error);
}

std::string CdmStorageManager::GetHistogramName(const char operation[]) {
  // If the 'kCdmStorageDatabaseMigration' flag is enabled, we should mark the
  // UMA with the fact that this error came during the migration.

  auto histogram_name =
      std::string{kUmaPrefix} + std::string{operation} +
      (in_memory() ? std::string{kIncognito} : std::string{kNonIncognito});

  if (base::FeatureList::IsEnabled(features::kCdmStorageDatabase) &&
      base::FeatureList::IsEnabled(features::kCdmStorageDatabaseMigration)) {
    histogram_name += std::string{kMigration};
  }

  return histogram_name;
}
}  // namespace content
