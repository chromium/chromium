// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm_storage_manager.h"

#include "base/notreached.h"
#include "base/types/pass_key.h"
#include "content/browser/media/cdm_storage_common.h"
#include "media/cdm/cdm_type.h"
#include "media/mojo/mojom/cdm_storage.mojom.h"

namespace content {

CdmStorageManager::CdmStorageManager(bool in_memory) : in_memory_(in_memory) {}
CdmStorageManager::~CdmStorageManager() = default;

void CdmStorageManager::OpenCdmStorage(
    const CdmStorageBindingContext& binding_context,
    mojo::PendingReceiver<media::mojom::CdmStorage> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTIMPLEMENTED();
}

void CdmStorageManager::ReadFile(const media::CdmType& cdm_type,
                                 const std::string& file_name,
                                 CdmStorageHost::ReadFileCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTIMPLEMENTED();
}

void CdmStorageManager::WriteFile(const media::CdmType& cdm_type,
                                  const std::string& file_name,
                                  const std::vector<uint8_t>& data,
                                  CdmStorageHost::WriteFileCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTIMPLEMENTED();
}

void CdmStorageManager::DeleteFile(
    const media::CdmType& cdm_type,
    const std::string& file_name,
    CdmStorageHost::DeleteFileCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTIMPLEMENTED();
}

void CdmStorageManager::OnHostReceiverDisconnect(
    CdmStorageHost* host,
    base::PassKey<CdmStorageHost> pass_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTIMPLEMENTED();
}

}  // namespace content
