// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/file_system_quota_client.h"

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_usage_cache.h"
#include "storage/browser/file_system/sandbox_file_system_backend.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/gurl.h"

using blink::mojom::StorageType;

namespace storage {

namespace {

void GetOriginsForTypeOnFileTaskRunner(FileSystemContext* context,
                                       StorageType storage_type,
                                       std::set<url::Origin>* origins_ptr) {
  FileSystemType type = QuotaStorageTypeToFileSystemType(storage_type);
  DCHECK(type != kFileSystemTypeUnknown);

  FileSystemQuotaUtil* quota_util = context->GetQuotaUtil(type);
  if (!quota_util)
    return;
  std::set<GURL> origins;
  quota_util->GetOriginsForTypeOnFileTaskRunner(type, &origins);
  for (auto origin : origins)
    origins_ptr->insert(url::Origin::Create(origin));
}

void GetOriginsForHostOnFileTaskRunner(FileSystemContext* context,
                                       StorageType storage_type,
                                       const std::string& host,
                                       std::set<url::Origin>* origins_ptr) {
  FileSystemType type = QuotaStorageTypeToFileSystemType(storage_type);
  DCHECK(type != kFileSystemTypeUnknown);

  FileSystemQuotaUtil* quota_util = context->GetQuotaUtil(type);
  if (!quota_util)
    return;
  std::set<GURL> origins;
  quota_util->GetOriginsForHostOnFileTaskRunner(type, host, &origins);
  for (auto origin : origins)
    origins_ptr->insert(url::Origin::Create(origin));
}

void DidGetFileSystemQuotaClientOrigins(
    storage::QuotaClient::GetOriginsCallback callback,
    std::set<url::Origin>* origins_ptr) {
  std::move(callback).Run(*origins_ptr);
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
          context, context->quota_manager_proxy(), origin.GetURL(), type);
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
    : file_system_context_(file_system_context) {}

FileSystemQuotaClient::~FileSystemQuotaClient() = default;

storage::QuotaClient::ID FileSystemQuotaClient::id() const {
  return storage::QuotaClient::kFileSystem;
}

void FileSystemQuotaClient::OnQuotaManagerDestroyed() {
  delete this;
}

void FileSystemQuotaClient::GetOriginUsage(const url::Origin& origin,
                                           StorageType storage_type,
                                           GetUsageCallback callback) {
  DCHECK(!callback.is_null());

  FileSystemType type = QuotaStorageTypeToFileSystemType(storage_type);
  DCHECK(type != kFileSystemTypeUnknown);

  FileSystemQuotaUtil* quota_util = file_system_context_->GetQuotaUtil(type);
  if (!quota_util) {
    std::move(callback).Run(0);
    return;
  }

  base::PostTaskAndReplyWithResult(
      file_task_runner(), FROM_HERE,
      // It is safe to pass Unretained(quota_util) since context owns it.
      base::BindOnce(&FileSystemQuotaUtil::GetOriginUsageOnFileTaskRunner,
                     base::Unretained(quota_util),
                     base::RetainedRef(file_system_context_), origin.GetURL(),
                     type),
      std::move(callback));
}

void FileSystemQuotaClient::GetOriginsForType(StorageType storage_type,
                                              GetOriginsCallback callback) {
  DCHECK(!callback.is_null());

  std::set<url::Origin>* origins_ptr = new std::set<url::Origin>();
  file_task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&GetOriginsForTypeOnFileTaskRunner,
                     base::RetainedRef(file_system_context_), storage_type,
                     base::Unretained(origins_ptr)),
      base::BindOnce(&DidGetFileSystemQuotaClientOrigins, std::move(callback),
                     base::Owned(origins_ptr)));
}

void FileSystemQuotaClient::GetOriginsForHost(StorageType storage_type,
                                              const std::string& host,
                                              GetOriginsCallback callback) {
  DCHECK(!callback.is_null());

  std::set<url::Origin>* origins_ptr = new std::set<url::Origin>();
  file_task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&GetOriginsForHostOnFileTaskRunner,
                     base::RetainedRef(file_system_context_), storage_type,
                     host, base::Unretained(origins_ptr)),
      base::BindOnce(&DidGetFileSystemQuotaClientOrigins, std::move(callback),
                     base::Owned(origins_ptr)));
}

void FileSystemQuotaClient::DeleteOriginData(const url::Origin& origin,
                                             StorageType type,
                                             DeletionCallback callback) {
  FileSystemType fs_type = QuotaStorageTypeToFileSystemType(type);
  DCHECK(fs_type != kFileSystemTypeUnknown);

  base::PostTaskAndReplyWithResult(
      file_task_runner(), FROM_HERE,
      base::BindOnce(&DeleteOriginOnFileTaskRunner,
                     base::RetainedRef(file_system_context_), origin, fs_type),
      std::move(callback));
}

void FileSystemQuotaClient::PerformStorageCleanup(StorageType type,
                                                  base::OnceClosure callback) {
  FileSystemType fs_type = QuotaStorageTypeToFileSystemType(type);
  DCHECK(fs_type != kFileSystemTypeUnknown);
  file_task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&PerformStorageCleanupOnFileTaskRunner,
                     base::RetainedRef(file_system_context_), fs_type),
      std::move(callback));
}

bool FileSystemQuotaClient::DoesSupport(StorageType storage_type) const {
  FileSystemType type = QuotaStorageTypeToFileSystemType(storage_type);
  DCHECK(type != kFileSystemTypeUnknown);
  return file_system_context_->IsSandboxFileSystem(type);
}

base::SequencedTaskRunner* FileSystemQuotaClient::file_task_runner() const {
  return file_system_context_->default_file_task_runner();
}

}  // namespace storage
