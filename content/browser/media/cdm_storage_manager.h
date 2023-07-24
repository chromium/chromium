// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CDM_STORAGE_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_CDM_STORAGE_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/threading/sequence_bound.h"
#include "base/types/pass_key.h"
#include "content/browser/media/cdm_storage_common.h"
#include "content/browser/media/cdm_storage_database.h"
#include "content/browser/media/cdm_storage_host.h"
#include "content/common/content_export.h"
#include "media/cdm/cdm_type.h"
#include "media/mojo/mojom/cdm_storage.mojom.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

// CdmStorageHost uses CdmStorageManager to direct database operations to the
// CdmStorageDatabase. Ownership stays with CdmStorageManager, but a pointer is
// passed on so that the CdmStorageHost can call CdmStorageManager methods.
class CONTENT_EXPORT CdmStorageManager {
 public:
  explicit CdmStorageManager(bool in_memory);
  CdmStorageManager(const CdmStorageManager&) = delete;
  CdmStorageManager& operator=(const CdmStorageManager&) = delete;
  ~CdmStorageManager();

  void OpenCdmStorage(const CdmStorageBindingContext& binding_context,
                      mojo::PendingReceiver<media::mojom::CdmStorage> receiver);

  void ReadFile(const media::CdmType& cdm_type,
                const std::string& file_name,
                CdmStorageHost::ReadFileCallback callback);

  void WriteFile(const media::CdmType& cdm_type,
                 const std::string& file_name,
                 const std::vector<uint8_t>& data,
                 CdmStorageHost::WriteFileCallback callback);

  void DeleteFile(const media::CdmType& cdm_type,
                  const std::string& file_name,
                  CdmStorageHost::DeleteFileCallback callback);

  // Called when a receiver is disconnected from a CdmStorageHost.
  //
  // `host` must be owned by this manager. `host` may be deleted.
  void OnHostReceiverDisconnect(CdmStorageHost* host,
                                base::PassKey<CdmStorageHost> pass_key);

  bool in_memory() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return in_memory_;
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // All file operations are run through this member.
  base::SequenceBound<CdmStorageDatabase> db_
      GUARDED_BY_CONTEXT(sequence_checker_);

  const bool in_memory_;

  base::flat_map<blink::StorageKey, std::unique_ptr<CdmStorageHost>> hosts_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<CdmStorageManager> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CDM_STORAGE_MANAGER_H_
