// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/file_system_quota_client.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/files/file.h"
#include "base/location.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

namespace storage {

namespace {

std::vector<url::Origin> GetOriginsForTypeOnFileTaskRunner(
    FileSystemContext* context,
    blink::mojom::StorageType storage_type) {
  FileSystemType type =
      FileSystemQuotaClient::QuotaStorageTypeToFileSystemType(storage_type);
  DCHECK(type != kFileSystemTypeUnknown);

  FileSystemQuotaUtil* quota_util = context->GetQuotaUtil(type);
  if (!quota_util)
    return {};
  return quota_util->GetOriginsForTypeOnFileTaskRunner(type);
}

std::vector<url::Origin> GetOriginsForHostOnFileTaskRunner(
    FileSystemContext* context,
    blink::mojom::StorageType storage_type,
    const std::string& host) {
  FileSystemType type =
      FileSystemQuotaClient::QuotaStorageTypeToFileSystemType(storage_type);
  DCHECK(type != kFileSystemTypeUnknown);

  FileSystemQuotaUtil* quota_util = context->GetQuotaUtil(type);
  if (!quota_util)
    return {};
  return quota_util->GetOriginsForHostOnFileTaskRunner(type, host);
}

blink::mojom::QuotaStatusCode DeleteOriginOnFileTaskRunner(
    FileSystemContext* context,
    const url::Origin& origin,
    FileSystemType type) {
  FileSystemBackend* provider = context->GetFileSystemBackend(type);
  if (!provider || !provider->GetQuotaUtil())
    return blink::mojom::QuotaStatusCode::kErrorNotSupported;
  base::File::Error result =
      provider->GetQuotaUtil()->DeleteOriginDataOnFileTaskRunner(
          context, context->quota_manager_proxy(), origin, type);
  if (result == base::File::FILE_OK)
    return blink::mojom::QuotaStatusCode::kOk;
  return blink::mojom::QuotaStatusCode::kErrorInvalidModification;
}

void PerformStorageCleanupOnFileTaskRunner(FileSystemContext* context,
                                           FileSystemType type) {
  FileSystemBackend* provider = context->GetFileSystemBackend(type);
  if (!provider || !provider->GetQuotaUtil())
    return;
  provider->GetQuotaUtil()->PerformStorageCleanupOnFileTaskRunner(
      context, context->quota_manager_proxy(), type);
}

}  // namespace

FileSystemQuotaClient::FileSystemQuotaClient(
    FileSystemContext* file_system_context)
    : file_system_context_(file_system_context) {
  DCHECK(file_system_context_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

FileSystemQuotaClient::~FileSystemQuotaClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FileSystemQuotaClient::GetOriginUsage(
    const url::Origin& origin,
    blink::mojom::StorageType storage_type,
    GetOriginUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());

  FileSystemType type =
      FileSystemQuotaClient::QuotaStorageTypeToFileSystemType(storage_type);
  DCHECK(type != kFileSystemTypeUnknown);

  FileSystemQuotaUtil* quota_util = file_system_context_->GetQuotaUtil(type);
  if (!quota_util) {
    std::move(callback).Run(0);
    return;
  }

  file_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      // It is safe to pass Unretained(quota_util) since context owns it.
      base::BindOnce(&FileSystemQuotaUtil::GetOriginUsageOnFileTaskRunner,
                     base::Unretained(quota_util),
                     base::RetainedRef(file_system_context_), origin, type),
      std::move(callback));
}

void FileSystemQuotaClient::GetOriginsForType(
    blink::mojom::StorageType storage_type,
    GetOriginsForTypeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());

  file_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetOriginsForTypeOnFileTaskRunner,
                     base::RetainedRef(file_system_context_), storage_type),
      std::move(callback));
}

void FileSystemQuotaClient::GetOriginsForHost(
    blink::mojom::StorageType storage_type,
    const std::string& host,
    GetOriginsForHostCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());

  file_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetOriginsForHostOnFileTaskRunner,
                     base::RetainedRef(file_system_context_), storage_type,
                     host),
      std::move(callback));
}

void FileSystemQuotaClient::DeleteOriginData(
    const url::Origin& origin,
    blink::mojom::StorageType type,
    DeleteOriginDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());

  FileSystemType fs_type = QuotaStorageTypeToFileSystemType(type);
  DCHECK(fs_type != kFileSystemTypeUnknown);

  file_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DeleteOriginOnFileTaskRunner,
                     base::RetainedRef(file_system_context_), origin, fs_type),
      std::move(callback));
}

void FileSystemQuotaClient::PerformStorageCleanup(
    blink::mojom::StorageType type,
    PerformStorageCleanupCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());

  FileSystemType fs_type = QuotaStorageTypeToFileSystemType(type);
  DCHECK(fs_type != kFileSystemTypeUnknown);
  file_task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&PerformStorageCleanupOnFileTaskRunner,
                     base::RetainedRef(file_system_context_), fs_type),
      std::move(callback));
}

// static
FileSystemType FileSystemQuotaClient::QuotaStorageTypeToFileSystemType(
    blink::mojom::StorageType storage_type) {
  switch (storage_type) {
    case blink::mojom::StorageType::kTemporary:
      return kFileSystemTypeTemporary;
    case blink::mojom::StorageType::kPersistent:
      return kFileSystemTypePersistent;
    case blink::mojom::StorageType::kSyncable:
      return kFileSystemTypeSyncable;
    case blink::mojom::StorageType::kQuotaNotManaged:
    case blink::mojom::StorageType::kUnknown:
      return kFileSystemTypeUnknown;
  }
  return kFileSystemTypeUnknown;
}

base::SequencedTaskRunner* FileSystemQuotaClient::file_task_runner() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return file_system_context_->default_file_task_runner();
}

}  // namespace storage
