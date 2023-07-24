// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm_storage_database.h"

#include "base/notreached.h"
#include "content/browser/media/cdm_storage_host.h"
#include "media/cdm/cdm_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

using CdmStorageHostOpenError = CdmStorageHost::CdmStorageHostOpenError;

CdmStorageDatabase::CdmStorageDatabase(const base::FilePath& path)
    : path_(path),
      // Use a smaller cache, since access will be fairly infrequent and random.
      // Given the expected record sizes (~100s of bytes) and key sizes (<100
      // bytes) and that we'll typically only be pulling one file at a time
      // (playback), specify a large page size to allow inner nodes can pack
      // many keys, to keep the index B-tree flat.
      db_(sql::DatabaseOptions{.exclusive_locking = true,
                               .page_size = 32768,
                               .cache_size = 8}) {}

CdmStorageHostOpenError CdmStorageDatabase::EnsureOpen(
    const blink::StorageKey& storage_key,
    const media::CdmType& cdm_type,
    const std::string& file_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTIMPLEMENTED();
  return CdmStorageHostOpenError::kOk;
}

absl::optional<std::vector<uint8_t>> CdmStorageDatabase::ReadFile(
    const blink::StorageKey& storage_key,
    const media::CdmType& cdm_type,
    const std::string& file_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTIMPLEMENTED();
  return absl::nullopt;
}

bool CdmStorageDatabase::WriteFile(const blink::StorageKey& storage_key,
                                   const media::CdmType& cdm_type,
                                   const std::string& file_name,
                                   const std::vector<uint8_t>& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTIMPLEMENTED();
  return true;
}

bool CdmStorageDatabase::DeleteFile(const blink::StorageKey& storage_key,
                                    const media::CdmType& cdm_type,
                                    const std::string& file_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTIMPLEMENTED();
  return true;
}

bool CdmStorageDatabase::DeleteDataForStorageKey(
    const blink::StorageKey& storage_key,
    const media::CdmType& cdm_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTIMPLEMENTED();
  return true;
}

bool CdmStorageDatabase::ClearDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTIMPLEMENTED();
  return true;
}

CdmStorageHostOpenError CdmStorageDatabase::OpenDatabase(bool is_retry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTIMPLEMENTED();
  return CdmStorageHostOpenError::kOk;
}

void CdmStorageDatabase::OnDatabaseError(int error, sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTIMPLEMENTED();
}
}  // namespace content
