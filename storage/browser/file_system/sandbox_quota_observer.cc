// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/sandbox_quota_observer.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "storage/browser/file_system/file_system_usage_cache.h"
#include "storage/browser/file_system/sandbox_file_system_backend_delegate.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/common/file_system/file_system_util.h"

namespace storage {

SandboxQuotaObserver::SandboxQuotaObserver(
    storage::QuotaManagerProxy* quota_manager_proxy,
    base::SequencedTaskRunner* update_notify_runner,
    ObfuscatedFileUtil* sandbox_file_util,
    FileSystemUsageCache* file_system_usage_cache)
    : quota_manager_proxy_(quota_manager_proxy),
      update_notify_runner_(update_notify_runner),
      sandbox_file_util_(sandbox_file_util),
      file_system_usage_cache_(file_system_usage_cache) {}

SandboxQuotaObserver::~SandboxQuotaObserver() = default;

void SandboxQuotaObserver::OnStartUpdate(const FileSystemURL& url) {
  DCHECK(update_notify_runner_->RunsTasksInCurrentSequence());
  base::FilePath usage_file_path = GetUsageCachePath(url);
  if (usage_file_path.empty())
    return;
  file_system_usage_cache_->IncrementDirty(usage_file_path);
}

void SandboxQuotaObserver::OnUpdate(const FileSystemURL& url, int64_t delta) {
  DCHECK(update_notify_runner_->RunsTasksInCurrentSequence());

  if (quota_manager_proxy_.get()) {
    quota_manager_proxy_->NotifyStorageModified(
        storage::QuotaClient::kFileSystem, url.origin(),
        FileSystemTypeToQuotaStorageType(url.type()), delta);
  }

  base::FilePath usage_file_path = GetUsageCachePath(url);
  if (usage_file_path.empty())
    return;

  pending_update_notification_[usage_file_path] += delta;
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

  base::FilePath usage_file_path = GetUsageCachePath(url);
  if (usage_file_path.empty())
    return;

  auto found = pending_update_notification_.find(usage_file_path);
  if (found != pending_update_notification_.end()) {
    UpdateUsageCacheFile(found->first, found->second);
    pending_update_notification_.erase(found);
  }

  file_system_usage_cache_->DecrementDirty(usage_file_path);
}

void SandboxQuotaObserver::OnAccess(const FileSystemURL& url) {
  if (quota_manager_proxy_.get()) {
    quota_manager_proxy_->NotifyStorageAccessed(
        storage::QuotaClient::kFileSystem, url.origin(),
        FileSystemTypeToQuotaStorageType(url.type()));
  }
}

void SandboxQuotaObserver::SetUsageCacheEnabled(const GURL& origin,
                                                FileSystemType type,
                                                bool enabled) {
  if (quota_manager_proxy_.get()) {
    quota_manager_proxy_->SetUsageCacheEnabled(
        storage::QuotaClient::kFileSystem, url::Origin::Create(origin),
        FileSystemTypeToQuotaStorageType(type), enabled);
  }
}

base::FilePath SandboxQuotaObserver::GetUsageCachePath(
    const FileSystemURL& url) {
  DCHECK(sandbox_file_util_);
  base::File::Error error = base::File::FILE_OK;
  base::FilePath path =
      SandboxFileSystemBackendDelegate::GetUsageCachePathForOriginAndType(
          sandbox_file_util_, url.origin().GetURL(), url.type(), &error);
  if (error != base::File::FILE_OK) {
    LOG(WARNING) << "Could not get usage cache path for: " << url.DebugString();
    return base::FilePath();
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
