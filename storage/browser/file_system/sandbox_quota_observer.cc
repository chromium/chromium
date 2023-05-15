// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/sandbox_quota_observer.h"

#include <stdint.h>

#include "base/files/file_error_or.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "storage/browser/file_system/file_system_usage_cache.h"
#include "storage/browser/file_system/file_system_util.h"
#include "storage/browser/file_system/sandbox_file_system_backend_delegate.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

SandboxQuotaObserver::SandboxQuotaObserver(
    scoped_refptr<QuotaManagerProxy> quota_manager_proxy,
    scoped_refptr<base::SequencedTaskRunner> update_notify_runner,
    ObfuscatedFileUtil* sandbox_file_util,
    FileSystemUsageCache* file_system_usage_cache)
    : quota_manager_proxy_(std::move(quota_manager_proxy)),
      update_notify_runner_(std::move(update_notify_runner)),
      sandbox_file_util_(sandbox_file_util),
      file_system_usage_cache_(file_system_usage_cache) {}

SandboxQuotaObserver::~SandboxQuotaObserver() = default;

void SandboxQuotaObserver::OnStartUpdate(const FileSystemURL& url) {
  DCHECK(update_notify_runner_->RunsTasksInCurrentSequence());
  base::FileErrorOr<base::FilePath> usage_file_path = GetUsageCachePath(url);
  if (!usage_file_path.has_value() || usage_file_path->empty())
    return;
  file_system_usage_cache_->IncrementDirty(usage_file_path.value());
}

void SandboxQuotaObserver::OnUpdate(const FileSystemURL& url, int64_t delta) {
  DCHECK(update_notify_runner_->RunsTasksInCurrentSequence());

  if (quota_manager_proxy_.get()) {
    quota_manager_proxy_->NotifyBucketModified(
        QuotaClientType::kFileSystem, url.GetBucket(), delta, base::Time::Now(),
        base::SequencedTaskRunner::GetCurrentDefault(), base::DoNothing());
  }

  base::FileErrorOr<base::FilePath> usage_file_path = GetUsageCachePath(url);
  if (!usage_file_path.has_value() || usage_file_path->empty())
    return;

  pending_update_notification_[usage_file_path.value()] += delta;
  if (!delayed_cache_update_helper_.IsRunning()) {
    delayed_cache_update_helper_.Start(
        FROM_HERE,
        base::TimeDelta(),  // No delay.
        base::BindOnce(&SandboxQuotaObserver::ApplyPendingUsageUpdate,
                       base::Unretained(this)));
  }
}

void SandboxQuotaObserver::OnEndUpdate(const FileSystemURL& url) {
  DCHECK(update_notify_runner_->RunsTasksInCurrentSequence());

  base::FileErrorOr<base::FilePath> usage_file_path = GetUsageCachePath(url);
  if (!usage_file_path.has_value() || usage_file_path->empty())
    return;

  auto found = pending_update_notification_.find(usage_file_path.value());
  if (found != pending_update_notification_.end()) {
    UpdateUsageCacheFile(found->first, found->second);
    pending_update_notification_.erase(found);
  }

  file_system_usage_cache_->DecrementDirty(usage_file_path.value());
}

void SandboxQuotaObserver::OnAccess(const FileSystemURL& url) {
  if (quota_manager_proxy_.get()) {
    quota_manager_proxy_->NotifyBucketAccessed(url.GetBucket(),
                                               base::Time::Now());
  }
}

void SandboxQuotaObserver::SetUsageCacheEnabled(const url::Origin& origin,
                                                FileSystemType type,
                                                bool enabled) {
  if (quota_manager_proxy_.get()) {
    quota_manager_proxy_->SetUsageCacheEnabled(
        QuotaClientType::kFileSystem,
        blink::StorageKey::CreateFirstParty(origin),
        FileSystemTypeToQuotaStorageType(type), enabled);
  }
}

base::FileErrorOr<base::FilePath> SandboxQuotaObserver::GetUsageCachePath(
    const FileSystemURL& url) {
  DCHECK(sandbox_file_util_);
  base::FileErrorOr<base::FilePath> path;
  if (url.bucket().has_value()) {
    path = SandboxFileSystemBackendDelegate::GetUsageCachePathForBucketAndType(
        sandbox_file_util_, url.bucket().value(), url.type());
  } else {
    path =
        SandboxFileSystemBackendDelegate::GetUsageCachePathForStorageKeyAndType(
            sandbox_file_util_, url.storage_key(), url.type());
  }
  if (!path.has_value()) {
    LOG(WARNING) << "Could not get usage cache path for: " << url.DebugString();
  }
  return path;
}

void SandboxQuotaObserver::ApplyPendingUsageUpdate() {
  delayed_cache_update_helper_.Stop();
  for (const auto& path_delta_pair : pending_update_notification_)
    UpdateUsageCacheFile(path_delta_pair.first, path_delta_pair.second);
  pending_update_notification_.clear();
}

void SandboxQuotaObserver::UpdateUsageCacheFile(
    const base::FilePath& usage_file_path,
    int64_t delta) {
  DCHECK(!usage_file_path.empty());
  if (!usage_file_path.empty() && delta != 0)
    file_system_usage_cache_->AtomicUpdateUsageByDelta(usage_file_path, delta);
}

}  // namespace storage
