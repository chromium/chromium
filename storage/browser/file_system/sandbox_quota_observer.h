// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_QUOTA_OBSERVER_H_
#define STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_QUOTA_OBSERVER_H_

#include <stdint.h>

#include <map>

#include "base/compiler_specific.h"
#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "storage/browser/file_system/file_observers.h"
#include "storage/browser/file_system/file_system_url.h"

namespace base {
class SequencedTaskRunner;
}

namespace url {
class Origin;
}

namespace storage {

class FileSystemUsageCache;
class FileSystemURL;
class ObfuscatedFileUtil;
class QuotaManagerProxy;

class SandboxQuotaObserver : public FileUpdateObserver,
                             public FileAccessObserver {
 public:
  SandboxQuotaObserver(
      scoped_refptr<QuotaManagerProxy> quota_manager_proxy,
      scoped_refptr<base::SequencedTaskRunner> update_notify_runner,
      ObfuscatedFileUtil* sandbox_file_util,
      FileSystemUsageCache* file_system_usage_cache_);

  SandboxQuotaObserver(const SandboxQuotaObserver&) = delete;
  SandboxQuotaObserver& operator=(const SandboxQuotaObserver&) = delete;

  ~SandboxQuotaObserver() override;

  // FileUpdateObserver overrides.
  void OnStartUpdate(const FileSystemURL& url) override;
  void OnUpdate(const FileSystemURL& url, int64_t delta) override;
  void OnEndUpdate(const FileSystemURL& url) override;

  // FileAccessObserver overrides.
  void OnAccess(const FileSystemURL& url) override;

  void SetUsageCacheEnabled(const url::Origin& origin,
                            FileSystemType type,
                            bool enabled);

 private:
  void ApplyPendingUsageUpdate();
  void UpdateUsageCacheFile(const base::FilePath& usage_file_path,
                            int64_t delta);

  base::FileErrorOr<base::FilePath> GetUsageCachePath(const FileSystemURL& url);

  const scoped_refptr<QuotaManagerProxy> quota_manager_proxy_;
  const scoped_refptr<base::SequencedTaskRunner> update_notify_runner_;

  // Not owned; sandbox_file_util_ should have identical lifetime with this.
  const raw_ptr<ObfuscatedFileUtil> sandbox_file_util_;

  // Not owned; file_system_usage_cache_ should have longer lifetime than this.
  const raw_ptr<FileSystemUsageCache> file_system_usage_cache_;

  std::map<base::FilePath, int64_t> pending_update_notification_;
  base::OneShotTimer delayed_cache_update_helper_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_QUOTA_OBSERVER_H_
