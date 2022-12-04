// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/quota/quota_backend_impl.h"

#include <stdint.h>

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/files/file_error_or.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "storage/browser/file_system/file_system_usage_cache.h"
#include "storage/browser/file_system/file_system_util.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace storage {

QuotaBackendImpl::QuotaBackendImpl(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    ObfuscatedFileUtil* obfuscated_file_util,
    FileSystemUsageCache* file_system_usage_cache,
    scoped_refptr<QuotaManagerProxy> quota_manager_proxy)
    : file_task_runner_(std::move(file_task_runner)),
      obfuscated_file_util_(obfuscated_file_util),
      file_system_usage_cache_(file_system_usage_cache),
      quota_manager_proxy_(std::move(quota_manager_proxy)) {}

QuotaBackendImpl::~QuotaBackendImpl() = default;

void QuotaBackendImpl::ReserveQuota(const url::Origin& origin,
                                    FileSystemType type,
                                    int64_t delta,
                                    ReserveQuotaCallback callback) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!origin.opaque());
  if (!delta) {
    std::move(callback).Run(base::File::FILE_OK, 0);
    return;
  }
  DCHECK(quota_manager_proxy_.get());
  quota_manager_proxy_->GetUsageAndQuota(
      blink::StorageKey(origin), FileSystemTypeToQuotaStorageType(type),
      file_task_runner_,
      base::BindOnce(&QuotaBackendImpl::DidGetUsageAndQuotaForReserveQuota,
                     weak_ptr_factory_.GetWeakPtr(),
                     QuotaReservationInfo(origin, type, delta),
                     std::move(callback)));
}

void QuotaBackendImpl::ReleaseReservedQuota(const url::Origin& origin,
                                            FileSystemType type,
                                            int64_t size) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!origin.opaque());
  DCHECK_LE(0, size);
  if (!size)
    return;
  ReserveQuotaInternal(QuotaReservationInfo(origin, type, -size));
}

void QuotaBackendImpl::CommitQuotaUsage(const url::Origin& origin,
                                        FileSystemType type,
                                        int64_t delta) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!origin.opaque());
  if (!delta)
    return;
  ReserveQuotaInternal(QuotaReservationInfo(origin, type, delta));
  base::FileErrorOr<base::FilePath> path = GetUsageCachePath(origin, type);
  if (!path.has_value())
    return;
  bool result =
      file_system_usage_cache_->AtomicUpdateUsageByDelta(path.value(), delta);
  DCHECK(result);
}

void QuotaBackendImpl::IncrementDirtyCount(const url::Origin& origin,
                                           FileSystemType type) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!origin.opaque());
  base::FileErrorOr<base::FilePath> path = GetUsageCachePath(origin, type);
  if (!path.has_value())
    return;
  DCHECK(file_system_usage_cache_);
  file_system_usage_cache_->IncrementDirty(path.value());
}

void QuotaBackendImpl::DecrementDirtyCount(const url::Origin& origin,
                                           FileSystemType type) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!origin.opaque());
  base::FileErrorOr<base::FilePath> path = GetUsageCachePath(origin, type);
  if (!path.has_value())
    return;
  DCHECK(file_system_usage_cache_);
  file_system_usage_cache_->DecrementDirty(path.value());
}

void QuotaBackendImpl::DidGetUsageAndQuotaForReserveQuota(
    const QuotaReservationInfo& info,
    ReserveQuotaCallback callback,
    blink::mojom::QuotaStatusCode status,
    int64_t usage,
    int64_t quota) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!info.origin.opaque());
  DCHECK_LE(0, usage);
  DCHECK_LE(0, quota);
  if (status != blink::mojom::QuotaStatusCode::kOk) {
    std::move(callback).Run(base::File::FILE_ERROR_FAILED, 0);
    return;
  }

  QuotaReservationInfo normalized_info = info;
  if (info.delta > 0) {
    int64_t new_usage = base::saturated_cast<int64_t>(
        usage + static_cast<uint64_t>(info.delta));
    if (quota < new_usage)
      new_usage = quota;
    normalized_info.delta =
        std::max(static_cast<int64_t>(0), new_usage - usage);
  }

  ReserveQuotaInternal(normalized_info);
  if (std::move(callback).Run(base::File::FILE_OK, normalized_info.delta))
    return;
  // The requester could not accept the reserved quota. Revert it.
  ReserveQuotaInternal(QuotaReservationInfo(
      normalized_info.origin, normalized_info.type, -normalized_info.delta));
}

void QuotaBackendImpl::ReserveQuotaInternal(const QuotaReservationInfo& info) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!info.origin.opaque());
  DCHECK(quota_manager_proxy_.get());
  auto bucket = BucketLocator::ForDefaultBucket(blink::StorageKey(info.origin));
  bucket.type = FileSystemTypeToQuotaStorageType(info.type);
  quota_manager_proxy_->NotifyBucketModified(
      QuotaClientType::kFileSystem, bucket, info.delta, base::Time::Now(),
      base::SequencedTaskRunner::GetCurrentDefault(), base::DoNothing());
}

base::FileErrorOr<base::FilePath> QuotaBackendImpl::GetUsageCachePath(
    const url::Origin& origin,
    FileSystemType type) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!origin.opaque());
  return SandboxFileSystemBackendDelegate::
      GetUsageCachePathForStorageKeyAndType(obfuscated_file_util_,
                                            blink::StorageKey(origin), type);
}

QuotaBackendImpl::QuotaReservationInfo::QuotaReservationInfo(
    const url::Origin& origin,
    FileSystemType type,
    int64_t delta)
    : origin(origin), type(type), delta(delta) {}

QuotaBackendImpl::QuotaReservationInfo::~QuotaReservationInfo() = default;

}  // namespace storage
