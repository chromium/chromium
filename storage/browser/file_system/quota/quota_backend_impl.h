// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_QUOTA_QUOTA_BACKEND_IMPL_H_
#define STORAGE_BROWSER_FILE_SYSTEM_QUOTA_QUOTA_BACKEND_IMPL_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/file_system/quota/quota_reservation_manager.h"
#include "storage/browser/file_system/sandbox_file_system_backend_delegate.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

namespace base {
class SequencedTaskRunner;
}

namespace storage {

class FileSystemUsageCache;
class ObfuscatedFileUtil;
class QuotaManagerProxy;

// An instance of this class is owned by QuotaReservationManager.
class COMPONENT_EXPORT(STORAGE_BROWSER) QuotaBackendImpl
    : public QuotaReservationManager::QuotaBackend {
 public:
  using ReserveQuotaCallback = QuotaReservationManager::ReserveQuotaCallback;

  QuotaBackendImpl(base::SequencedTaskRunner* file_task_runner,
                   ObfuscatedFileUtil* obfuscated_file_util,
                   FileSystemUsageCache* file_system_usage_cache,
                   QuotaManagerProxy* quota_manager_proxy);
  ~QuotaBackendImpl() override;

  // QuotaReservationManager::QuotaBackend overrides.
  void ReserveQuota(const url::Origin& origin,
                    FileSystemType type,
                    int64_t delta,
                    ReserveQuotaCallback callback) override;
  void ReleaseReservedQuota(const url::Origin& origin,
                            FileSystemType type,
                            int64_t size) override;
  void CommitQuotaUsage(const url::Origin& origin,
                        FileSystemType type,
                        int64_t delta) override;
  void IncrementDirtyCount(const url::Origin& origin,
                           FileSystemType type) override;
  void DecrementDirtyCount(const url::Origin& origin,
                           FileSystemType type) override;

 private:
  friend class QuotaBackendImplTest;

  struct QuotaReservationInfo {
    QuotaReservationInfo(const url::Origin& origin,
                         FileSystemType type,
                         int64_t delta);
    ~QuotaReservationInfo();

    url::Origin origin;
    FileSystemType type;
    int64_t delta;
  };

  void DidGetUsageAndQuotaForReserveQuota(const QuotaReservationInfo& info,
                                          ReserveQuotaCallback callback,
                                          blink::mojom::QuotaStatusCode status,
                                          int64_t usage,
                                          int64_t quota);

  void ReserveQuotaInternal(const QuotaReservationInfo& info);
  base::File::Error GetUsageCachePath(const url::Origin& origin,
                                      FileSystemType type,
                                      base::FilePath* usage_file_path);

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // Owned by SandboxFileSystemBackendDelegate.
  ObfuscatedFileUtil* obfuscated_file_util_;
  FileSystemUsageCache* file_system_usage_cache_;

  scoped_refptr<QuotaManagerProxy> quota_manager_proxy_;

  base::WeakPtrFactory<QuotaBackendImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(QuotaBackendImpl);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_QUOTA_QUOTA_BACKEND_IMPL_H_
