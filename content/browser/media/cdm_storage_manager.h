// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CDM_STORAGE_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_CDM_STORAGE_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/sequence_bound.h"
#include "base/types/pass_key.h"
#include "content/browser/media/cdm_file_impl.h"
#include "content/browser/media/cdm_storage_common.h"
#include "content/browser/media/cdm_storage_database.h"
#include "content/common/content_export.h"
#include "media/cdm/cdm_type.h"
#include "media/mojo/mojom/cdm_storage.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

class CONTENT_EXPORT CdmStorageManager : public media::mojom::CdmStorage {
 public:
  explicit CdmStorageManager(const base::FilePath& path);
  CdmStorageManager(const CdmStorageManager&) = delete;
  CdmStorageManager& operator=(const CdmStorageManager&) = delete;
  ~CdmStorageManager() override;

  // media::mojom::CdmStorage implementation.
  void Open(const std::string& file_name, OpenCallback callback) final;

  void OpenCdmStorage(const CdmStorageBindingContext& binding_context,
                      mojo::PendingReceiver<media::mojom::CdmStorage> receiver);

  void ReadFile(
      const blink::StorageKey& storage_key,
      const media::CdmType& cdm_type,
      const std::string& file_name,
      base::OnceCallback<void(absl::optional<std::vector<uint8_t>>)> callback);

  void WriteFile(const blink::StorageKey& storage_key,
                 const media::CdmType& cdm_type,
                 const std::string& file_name,
                 const std::vector<uint8_t>& data,
                 base::OnceCallback<void(bool)> callback);

  void DeleteFile(const blink::StorageKey& storage_key,
                  const media::CdmType& cdm_type,
                  const std::string& file_name,
                  base::OnceCallback<void(bool)> callback);

  void DeleteDataForStorageKey(const blink::StorageKey& storage_key,
                               const media::CdmType& cdm_type,
                               base::OnceCallback<void(bool)> callback);

  void DeleteDatabase();

  void OnFileReceiverDisconnect(const std::string& name,
                                const media::CdmType& cdm_type,
                                const blink::StorageKey& storage_key,
                                base::PassKey<CdmFileImpl> pass_key);

  bool has_empty_receiver_set() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return receivers_.empty();
  }

  CdmStorageBindingContext current_context() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return receivers_.current_context();
  }

  bool in_memory() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return path_.empty();
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  void DidOpenFile(const blink::StorageKey& storage_key,
                   const media::CdmType& cdm_type,
                   const std::string& file_name,
                   OpenCallback callback,
                   CdmStorageOpenError error);

  void DidReadFile(
      base::OnceCallback<void(absl::optional<std::vector<uint8_t>>)> callback,
      absl::optional<std::vector<uint8_t>> data);

  void DidWriteFile(base::OnceCallback<void(bool)> callback, bool success);

  void DidDeleteFile(base::OnceCallback<void(bool)> callback, bool success);

  void DidDeleteForStorageKey(base::OnceCallback<void(bool)> callback,
                              bool success);

  void DidDeleteDatabase(bool success);

  void ReportDatabaseOpenError(CdmStorageOpenError error);

  std::string GetHistogramName(const char operation[]);

  const base::FilePath path_;

  // All file operations are run through this member.
  base::SequenceBound<CdmStorageDatabase> db_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // All receivers for frames and workers whose storage key is `storage_key()`.
  mojo::ReceiverSet<media::mojom::CdmStorage, CdmStorageBindingContext>
      receivers_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Keep track of all media::mojom::CdmFile receivers, as each CdmFileImpl
  // object keeps a reference to `this`. If `this` goes away unexpectedly,
  // all remaining CdmFile receivers will be closed.
  std::map<CdmFileIdTwo, std::unique_ptr<CdmFileImpl>> cdm_files_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<CdmStorageManager> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CDM_STORAGE_MANAGER_H_
