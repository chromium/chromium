// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_LICENSE_STORAGE_HOST_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_LICENSE_STORAGE_HOST_H_

#include "base/containers/unique_ptr_adapters.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/types/pass_key.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "content/browser/media/cdm_storage_common.h"
#include "content/browser/media/media_license_manager.h"
#include "content/common/content_export.h"
#include "media/mojo/mojom/cdm_storage.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {
class CdmFileImpl;
class MediaLicenseDatabase;

// Per-storage-key backend for media license (CDM) files. MediaLicenseManager
// owns an instance of this class for each storage key that is actively using
// CDM files. Each instance owns all CdmStorage receivers for the corresponding
// storage key.
class CONTENT_EXPORT MediaLicenseStorageHost : public media::mojom::CdmStorage {
 public:
  using ReadFileCallback =
      base::OnceCallback<void(std::optional<std::vector<uint8_t>>)>;
  using WriteFileCallback = base::OnceCallback<void(bool)>;
  using DeleteFileCallback = base::OnceCallback<void(bool)>;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class MediaLicenseStorageHostOpenError {
    kOk = -1,
    kInvalidBucket = 0,      // The database's path could not be determined
                             // because the default storage bucket for the
                             // StorageKey could not be retrieved.
    kNoFileSpecified = 1,    // No file was specified.
    kInvalidFileName = 2,    // File name specified was invalid.
    kDatabaseOpenError = 3,  // Error occurred at the Database level.
    kBucketNotFound = 4,     // If the default Storage Bucket for the StorageKey
                             // is not found.
    kDatabaseRazeError = 5,  // The database was in an invalid state and failed
                             // to be razed.
    kSQLExecutionError = 6,  // Error executing the SQL statement.
    kBucketLocatorError = 7,  // Error with the bucket locator. This error was
                              // introduced after the previous errors so that
                              // we can drill down deeper on the source of the
                              // errors.
    kMaxValue = kBucketLocatorError
  };

  static void ReportDatabaseOpenError(MediaLicenseStorageHostOpenError error,
                                      bool in_memory);

  MediaLicenseStorageHost(MediaLicenseManager* manager,
                          const storage::BucketLocator& bucket_locator);
  ~MediaLicenseStorageHost() override;

  // media::mojom::CdmStorage implementation.
  void Open(const std::string& file_name, OpenCallback callback) final;

  void BindReceiver(const CdmStorageBindingContext& binding_context,
                    mojo::PendingReceiver<media::mojom::CdmStorage> receiver);

  // CDM file operations.
  void ReadFile(const media::CdmType& cdm_type,
                const std::string& file_name,
                ReadFileCallback callback);
  void WriteFile(const media::CdmType& cdm_type,
                 const std::string& file_name,
                 const std::vector<uint8_t>& data,
                 WriteFileCallback callback);
  void DeleteFile(const media::CdmType& cdm_type,
                  const std::string& file_name,
                  DeleteFileCallback callback);

  void DeleteBucketData(base::OnceCallback<void(bool)> callback);

  void OnFileReceiverDisconnect(const std::string& name,
                                const media::CdmType& cdm_type,
                                base::PassKey<CdmFileImpl> pass_key);

  // True if there are no receivers connected to this host.
  //
  // The MediaLicenseManagerImpl that owns this host is expected to destroy the
  // host when it isn't serving any receivers.
  bool has_empty_receiver_set() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return receivers_.empty();
  }

  const blink::StorageKey& storage_key() { return bucket_locator_.storage_key; }

  bool in_memory() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return manager_->in_memory();
  }

 private:
  void OnReceiverDisconnect();

  void DidOpenFile(const std::string& file_name,
                   CdmStorageBindingContext binding_context,
                   OpenCallback callback,
                   MediaLicenseStorageHostOpenError error);
  void DidGetDatabaseSize(const uint64_t size);
  void DidReadFile(const media::CdmType& cdm_type,
                   const std::string& file_name,
                   ReadFileCallback callback,
                   std::optional<std::vector<uint8_t>> data);
  void DidWriteFile(WriteFileCallback callback, bool success);

  SEQUENCE_CHECKER(sequence_checker_);

  // Track MediaLicenseDatabaseSize
  bool database_size_reported_ = false;

  // MediaLicenseManager instance which owns this object.
  const raw_ptr<MediaLicenseManager> manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Media licenses are only supported from the default bucket.
  // `bucket_locator_` corresponds to the default bucket for the StorageKey this
  // host represents.
  const storage::BucketLocator bucket_locator_;

  // This keeps track of the 'CdmFileIdTwo' values that have been migrated to
  // the CdmStorageDatabase after the first read, so that when the Cdm goes to
  // read during the migration, the second time and onwards, we read from the
  // CdmStorageDatabase instead of the MediaLicenseDatabase. Note that this is
  // not a permanent storage, so it has to be repopulated when user restarts
  // Chrome.
  std::vector<CdmFileIdTwo> files_migrated_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // All file operations are run through this member.
  base::SequenceBound<MediaLicenseDatabase> db_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // All receivers for frames and workers whose storage key is `storage_key()`.
  mojo::ReceiverSet<media::mojom::CdmStorage, CdmStorageBindingContext>
      receivers_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Keep track of all media::mojom::CdmFile receivers, as each CdmFileImpl
  // object keeps a reference to |this|. If |this| goes away unexpectedly,
  // all remaining CdmFile receivers will be closed.
  std::map<CdmFileId, std::unique_ptr<CdmFileImpl>> cdm_files_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<MediaLicenseStorageHost> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_LICENSE_STORAGE_HOST_H_
