// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm_storage_host.h"

#include "base/notreached.h"
#include "base/types/pass_key.h"
#include "content/browser/media/cdm_file_impl.h"
#include "content/browser/media/cdm_storage_manager.h"
#include "media/cdm/cdm_type.h"

namespace content {

// static
void CdmStorageHost::ReportDatabaseOpenError(CdmStorageHostOpenError error,
                                             bool in_memory) {}

CdmStorageHost::CdmStorageHost(CdmStorageManager* manager,
                               blink::StorageKey storage_key)
    : manager_(manager), storage_key_(storage_key) {}

CdmStorageHost::~CdmStorageHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTIMPLEMENTED();
}

// media::mojom::CdmStorage implementation.
void CdmStorageHost::Open(const std::string& file_name, OpenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTIMPLEMENTED();
}

void CdmStorageHost::BindReceiver(
    const CdmStorageBindingContext& binding_context,
    mojo::PendingReceiver<media::mojom::CdmStorage> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTIMPLEMENTED();
}

// CDM file operations.
void CdmStorageHost::ReadFile(const media::CdmType& cdm_type,
                              const std::string& file_name,
                              ReadFileCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTIMPLEMENTED();
}

void CdmStorageHost::WriteFile(const media::CdmType& cdm_type,
                               const std::string& file_name,
                               const std::vector<uint8_t>& data,
                               WriteFileCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTIMPLEMENTED();
}

void CdmStorageHost::DeleteFile(const media::CdmType& cdm_type,
                                const std::string& file_name,
                                DeleteFileCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTIMPLEMENTED();
}

void CdmStorageHost::DeleteDataForStorageKey() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTIMPLEMENTED();
}

void CdmStorageHost::OnFileReceiverDisconnect(
    const std::string& name,
    const media::CdmType& cdm_type,
    base::PassKey<CdmFileImpl> pass_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTIMPLEMENTED();
}

void CdmStorageHost::OnReceiverDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTIMPLEMENTED();

  // May delete `this`.
  manager_->OnHostReceiverDisconnect(this, base::PassKey<CdmStorageHost>());
}

}  // namespace content
