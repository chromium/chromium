// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_license_storage_host.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/pass_key.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/media/cdm_file_impl.h"
#include "content/browser/media/media_license_database.h"
#include "content/browser/media/media_license_manager.h"
#include "media/cdm/cdm_type.h"
#include "media/mojo/mojom/cdm_storage.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "storage/browser/file_system/file_system_context.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

namespace {
constexpr uint64_t kBytesPerKB = 1024;
constexpr int kMinDatabaseSizeKB = 0;
// Used for histogram reporting, the max size of the database we expect in KB.
constexpr uint64_t kMaxDatabaseSizeKB = 512000 * 10;
constexpr int kSizeKBBuckets = 1000;
}  // namespace

// static
void MediaLicenseStorageHost::ReportDatabaseOpenError(
    MediaLicenseStorageHostOpenError error,
    bool is_incognito) {
  DCHECK_NE(error, MediaLicenseStorageHostOpenError::kOk);
  const std::string kDatabaseOpenErrorUmaName =
      "Media.EME.MediaLicenseStorageHostOpenError";
  base::UmaHistogramEnumeration(kDatabaseOpenErrorUmaName, error);

  if (is_incognito) {
    base::UmaHistogramEnumeration(kDatabaseOpenErrorUmaName + ".Incognito",
                                  error);
  } else {
    base::UmaHistogramEnumeration(kDatabaseOpenErrorUmaName + ".NotIncognito",
                                  error);
  }
}

MediaLicenseStorageHost::MediaLicenseStorageHost(
    MediaLicenseManager* manager,
    const storage::BucketLocator& bucket_locator)
    : manager_(manager),
      bucket_locator_(bucket_locator),
      db_(manager_->db_runner(), manager_->GetDatabasePath(bucket_locator_)) {
  DCHECK(manager_);

  // base::Unretained is safe here because this MediaLicenseStorageHost owns
  // `receivers_`. So, the unretained MediaLicenseStorageHost is guaranteed to
  // outlive `receivers_` and the closure that it uses.
  receivers_.set_disconnect_handler(base::BindRepeating(
      &MediaLicenseStorageHost::OnReceiverDisconnect, base::Unretained(this)));
}

MediaLicenseStorageHost::~MediaLicenseStorageHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MediaLicenseStorageHost::Open(const std::string& file_name,
                                   OpenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (bucket_locator_.id.is_null()) {
    DVLOG(1) << "Could not retrieve valid bucket.";
    ReportDatabaseOpenError(MediaLicenseStorageHostOpenError::kInvalidBucket,
                            in_memory());
    std::move(callback).Run(Status::kFailure, mojo::NullAssociatedRemote());
    return;
  }

  if (file_name.empty()) {
    DVLOG(1) << "No file specified.";
    ReportDatabaseOpenError(MediaLicenseStorageHostOpenError::kNoFileSpecified,
                            in_memory());
    std::move(callback).Run(Status::kFailure, mojo::NullAssociatedRemote());
    return;
  }

  if (!CdmFileImpl::IsValidName(file_name)) {
    ReportDatabaseOpenError(MediaLicenseStorageHostOpenError::kInvalidFileName,
                            in_memory());
    std::move(callback).Run(Status::kFailure, mojo::NullAssociatedRemote());
    return;
  }

  const CdmStorageBindingContext& binding_context =
      receivers_.current_context();
  db_.AsyncCall(&MediaLicenseDatabase::OpenFile)
      .WithArgs(binding_context.cdm_type, file_name)
      .Then(base::BindOnce(&MediaLicenseStorageHost::DidOpenFile,
                           weak_factory_.GetWeakPtr(), file_name,
                           binding_context, std::move(callback)));
}

void MediaLicenseStorageHost::BindReceiver(
    const CdmStorageBindingContext& binding_context,
    mojo::PendingReceiver<media::mojom::CdmStorage> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(binding_context.storage_key, bucket_locator_.storage_key);

  receivers_.Add(this, std::move(receiver), binding_context);
}

void MediaLicenseStorageHost::DidOpenFile(
    const std::string& file_name,
    CdmStorageBindingContext binding_context,
    OpenCallback callback,
    MediaLicenseStorageHostOpenError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error != MediaLicenseStorageHostOpenError::kOk) {
    ReportDatabaseOpenError(error, in_memory());
    std::move(callback).Run(Status::kFailure, mojo::NullAssociatedRemote());
    return;
  }

  // Check whether this CDM file is in-use.
  CdmFileId id(file_name, binding_context.cdm_type);
  if (base::Contains(cdm_files_, id)) {
    std::move(callback).Run(Status::kInUse, mojo::NullAssociatedRemote());
    return;
  }

  // File was opened successfully, so create the binding and return success.
  mojo::PendingAssociatedRemote<media::mojom::CdmFile> cdm_file;

  // `this` is safe here since `cdm_file_impl` is owned by this instance.
  cdm_files_.emplace(id, std::make_unique<CdmFileImpl>(
                             this, binding_context.cdm_type, file_name,
                             cdm_file.InitWithNewEndpointAndPassReceiver()));

  // We don't actually touch the database here, but notify the quota system
  // anyways since conceptually we're creating an empty file.
  manager_->quota_manager_proxy()->NotifyBucketModified(
      storage::QuotaClientType::kMediaLicense, bucket_locator_, /*delta=*/0,
      /*modification_time=*/base::Time::Now(),
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(std::move(callback), Status::kSuccess,
                     std::move(cdm_file)));
}

void MediaLicenseStorageHost::ReadFile(const media::CdmType& cdm_type,
                                       const std::string& file_name,
                                       ReadFileCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  manager_->quota_manager_proxy()->NotifyBucketAccessed(
      bucket_locator_,
      /*access_time=*/base::Time::Now());

  // This is to read from the cdm_storage database when the migration is active
  // and we've already migrated the data from the cdm_storage database to the
  // media_license database.
  if (manager_->cdm_storage_manager() &&
      base::Contains(files_migrated_,
                     CdmFileIdTwo{file_name, cdm_type, storage_key()})) {
    manager_->cdm_storage_manager()->ReadFile(storage_key(), cdm_type,
                                              file_name, std::move(callback));
    return;
  }

  db_.AsyncCall(&MediaLicenseDatabase::ReadFile)
      .WithArgs(cdm_type, file_name)
      .Then(base::BindOnce(&MediaLicenseStorageHost::DidReadFile,
                           weak_factory_.GetWeakPtr(), cdm_type, file_name,
                           std::move(callback)));
}

void MediaLicenseStorageHost::DidReadFile(
    const media::CdmType& cdm_type,
    const std::string& file_name,
    ReadFileCallback callback,
    std::optional<std::vector<uint8_t>> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The code only reaches this callback during the migration when this is our
  // first time reading this specific storage_key, cdm_type, and file name. If
  // the data has value, then we write it to the cdm_storage database and from
  // then on, read from the cdm_storage database through the media_license code
  // when the migration is active.
  if (data.has_value() && manager_->cdm_storage_manager()) {
    manager_->cdm_storage_manager()->WriteFile(
        storage_key(), cdm_type, file_name, data.value(), base::DoNothing());
    files_migrated_.emplace_back(file_name, cdm_type, storage_key());
  }

  if (!database_size_reported_) {
    db_.AsyncCall(&MediaLicenseDatabase::GetDatabaseSize)
        .Then(base::BindOnce(&MediaLicenseStorageHost::DidGetDatabaseSize,
                             weak_factory_.GetWeakPtr()));
  }

  std::move(callback).Run(data);
}

void MediaLicenseStorageHost::DidGetDatabaseSize(const uint64_t size) {
  // One time report DatabaseSize.

  base::UmaHistogramCustomCounts(
      "Media.EME.MediaLicenseStorageHost.CurrentDatabaseUsageKB",
      size / kBytesPerKB, kMinDatabaseSizeKB, kMaxDatabaseSizeKB,
      kSizeKBBuckets);

  database_size_reported_ = true;
}

void MediaLicenseStorageHost::WriteFile(const media::CdmType& cdm_type,
                                        const std::string& file_name,
                                        const std::vector<uint8_t>& data,
                                        WriteFileCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (manager_->cdm_storage_manager()) {
    // We don't populate the callback because we want the
    // `MediaLicenseStorageHost` to still maintain control. We just call in to
    // the `CdmStorageManager` object to be able to update the
    // `CdmStorageDatabase` to keep it in line with `MediaLicenseDatabase`.
    manager_->cdm_storage_manager()->WriteFile(
        storage_key(), cdm_type, file_name, data, base::DoNothing());
  }

  db_.AsyncCall(&MediaLicenseDatabase::WriteFile)
      .WithArgs(cdm_type, file_name, data)
      .Then(base::BindOnce(&MediaLicenseStorageHost::DidWriteFile,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
}

void MediaLicenseStorageHost::DidWriteFile(WriteFileCallback callback,
                                           bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!success) {
    manager_->quota_manager_proxy()->OnClientWriteFailed(storage_key());
    std::move(callback).Run(false);
    return;
  }

  // Pass `delta`=0 since media license data does not count against quota.
  // TODO(crbug.com/40218094): Consider counting this data against quota.
  manager_->quota_manager_proxy()->NotifyBucketModified(
      storage::QuotaClientType::kMediaLicense, bucket_locator_, /*delta=*/0,
      /*modification_time=*/base::Time::Now(),
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(std::move(callback), success));
}

void MediaLicenseStorageHost::DeleteFile(const media::CdmType& cdm_type,
                                         const std::string& file_name,
                                         DeleteFileCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (manager_->cdm_storage_manager()) {
    // We don't populate the callback because we want the
    // `MediaLicenseStorageHost` to still maintain control. We just call in to
    // the `CdmStorageManager` object to be able to update the
    // `CdmStorageDatabase` to keep it in line with `MediaLicenseDatabase`.
    // TODO(crbug.com/40272342): Create UMA to track failures from the
    // MediaLicense* path, as we choose to fail silently to not affect the
    // current code-path's behavior and affect CDM operations.
    manager_->cdm_storage_manager()->DeleteFile(storage_key(), cdm_type,
                                                file_name, base::DoNothing());
  }

  db_.AsyncCall(&MediaLicenseDatabase::DeleteFile)
      .WithArgs(cdm_type, file_name)
      .Then(base::BindOnce(&MediaLicenseStorageHost::DidWriteFile,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
}

void MediaLicenseStorageHost::DeleteBucketData(
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  db_.AsyncCall(&MediaLicenseDatabase::ClearDatabase).Then(std::move(callback));
}

void MediaLicenseStorageHost::OnReceiverDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // May delete `this`.
  manager_->OnHostReceiverDisconnect(this,
                                     base::PassKey<MediaLicenseStorageHost>());
}

void MediaLicenseStorageHost::OnFileReceiverDisconnect(
    const std::string& name,
    const media::CdmType& cdm_type,
    base::PassKey<CdmFileImpl> /*pass_key*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto count = cdm_files_.erase(CdmFileId(name, cdm_type));
  DCHECK_GT(count, 0u);
}

}  // namespace content
