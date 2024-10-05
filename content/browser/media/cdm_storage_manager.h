// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CDM_STORAGE_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_CDM_STORAGE_MANAGER_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/sequence_bound.h"
#include "base/types/pass_key.h"
#include "content/browser/media/cdm_file_impl.h"
#include "content/browser/media/cdm_storage_common.h"
#include "content/browser/media/cdm_storage_database.h"
#include "content/common/content_export.h"
#include "content/public/browser/cdm_storage_data_model.h"
#include "content/public/browser/storage_partition.h"
#include "media/cdm/cdm_type.h"
#include "media/mojo/mojom/cdm_storage.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

class CONTENT_EXPORT CdmStorageManager : public media::mojom::CdmStorage,
                                         public CdmStorageDataModel {
 public:
  explicit CdmStorageManager(const base::FilePath& path);
  CdmStorageManager(const CdmStorageManager&) = delete;
  CdmStorageManager& operator=(const CdmStorageManager&) = delete;
  ~CdmStorageManager() override;

  // media::mojom::CdmStorage implementation.
  void Open(const std::string& file_name, OpenCallback callback) final;

  // CdmStorageDataModel implementation.
  void GetUsagePerAllStorageKeys(
      base::OnceCallback<void(const CdmStorageKeyUsageSize&)> callback,
      base::Time begin,
      base::Time end) final;
  void DeleteDataForStorageKey(const blink::StorageKey& storage_key,
                               base::OnceCallback<void(bool)> callback) final;

  void OpenCdmStorage(const CdmStorageBindingContext& binding_context,
                      mojo::PendingReceiver<media::mojom::CdmStorage> receiver);

  void ReadFile(
      const blink::StorageKey& storage_key,
      const media::CdmType& cdm_type,
      const std::string& file_name,
      base::OnceCallback<void(std::optional<std::vector<uint8_t>>)> callback);

  void WriteFile(const blink::StorageKey& storage_key,
                 const media::CdmType& cdm_type,
                 const std::string& file_name,
                 const std::vector<uint8_t>& data,
                 base::OnceCallback<void(bool)> callback);

  void GetSizeForFile(const blink::StorageKey& storage_key,
                      const media::CdmType& cdm_type,
                      const std::string& file_name,
                      base::OnceCallback<void(uint64_t)> callback);

  void GetSizeForStorageKey(const blink::StorageKey& storage_key,
                            const base::Time begin,
                            const base::Time end,
                            base::OnceCallback<void(uint64_t)> callback);

  void GetSizeForTimeFrame(const base::Time begin,
                           const base::Time end,
                           base::OnceCallback<void(uint64_t)> callback);

  void DeleteFile(const blink::StorageKey& storage_key,
                  const media::CdmType& cdm_type,
                  const std::string& file_name,
                  base::OnceCallback<void(bool)> callback);

  void DeleteData(
      const StoragePartition::StorageKeyMatcherFunction& storage_key_matcher,
      const blink::StorageKey& storage_key,
      const base::Time begin,
      const base::Time end,
      base::OnceCallback<void(bool)> callback);

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
      base::OnceCallback<void(std::optional<std::vector<uint8_t>>)> callback,
      std::optional<std::vector<uint8_t>> data);

  void DidWriteFile(base::OnceCallback<void(bool)> callback, bool success);

  void DidGetDatabaseSize(const uint64_t size);

  void DidGetSize(base::OnceCallback<void(uint64_t)> callback,
                  const std::string& operation,
                  std::optional<uint64_t> size);

  void ReportDatabaseOpenError(CdmStorageOpenError error);

  const base::FilePath path_;

  // Track CdmStorageDatabase size.
  bool database_size_reported_ = false;

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
