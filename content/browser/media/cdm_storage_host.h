// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CDM_STORAGE_HOST_H_
#define CONTENT_BROWSER_MEDIA_CDM_STORAGE_HOST_H_

#include "base/containers/unique_ptr_adapters.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/types/pass_key.h"
#include "content/browser/media/cdm_storage_common.h"
#include "content/common/content_export.h"
#include "media/cdm/cdm_type.h"
#include "media/mojo/mojom/cdm_storage.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {
class CdmFileImpl;
class CdmStorageManager;

// Per-storage-key backend for (CDM) files. CdmStorageManager owns an instance
// of this class for each storage key that is actively using CDM files. Each
// instance owns all CdmStorage receivers for the corresponding storage key.
class CONTENT_EXPORT CdmStorageHost : public media::mojom::CdmStorage {
 public:
  using ReadFileCallback =
      base::OnceCallback<void(absl::optional<std::vector<uint8_t>>)>;
  using WriteFileCallback = base::OnceCallback<void(bool)>;
  using DeleteFileCallback = base::OnceCallback<void(bool)>;
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CdmStorageHostOpenError {
    kOk = -1,
    kNoFileSpecified = 0,    // No file was specified.
    kInvalidFileName = 1,    // File name specified was invalid.
    kDatabaseOpenError = 2,  // Error occurred at the Database level.
    kDatabaseRazeError = 3,  // The database was in an invalid state and failed
                             // to be razed.
    kSQLExecutionError = 4,  // Error executing the SQL statement.
    kMaxValue = kSQLExecutionError
  };

  static void ReportDatabaseOpenError(CdmStorageHostOpenError error,
                                      bool in_memory);

  CdmStorageHost(CdmStorageManager* manager, blink::StorageKey storage_key);
  ~CdmStorageHost() override;

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

  void DeleteDataForStorageKey();

  void OnFileReceiverDisconnect(const std::string& name,
                                const media::CdmType& cdm_type,
                                base::PassKey<CdmFileImpl> pass_key);

  // True if there are no receivers connected to this host.
  //
  // The CdmStorageManager that owns this host is expected to destroy the
  // host when it isn't serving any receivers.
  bool has_empty_receiver_set() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return receivers_.empty();
  }

  raw_ref<blink::StorageKey> storage_key() { return storage_key_; }

 private:
  void OnReceiverDisconnect();

  SEQUENCE_CHECKER(sequence_checker_);

  // MediaLicenseManager instance which owns this object.
  const raw_ptr<CdmStorageManager> manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  raw_ref<blink::StorageKey> storage_key_;

  // All receivers for frames and workers whose storage key is `storage_key()`.
  mojo::ReceiverSet<media::mojom::CdmStorage, CdmStorageBindingContext>
      receivers_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Keep track of all media::mojom::CdmFile receivers, as each CdmFileImpl
  // object keeps a reference to `this`. If `this` goes away unexpectedly,
  // all remaining CdmFile receivers will be closed.
  std::map<CdmFileId, std::unique_ptr<CdmFileImpl>> cdm_files_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<CdmStorageHost> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CDM_STORAGE_HOST_H_
