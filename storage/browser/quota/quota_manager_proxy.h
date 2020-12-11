// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_PROXY_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_PROXY_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "storage/browser/quota/quota_callbacks.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_database.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_override_handle.h"
#include "storage/browser/quota/quota_task.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}

namespace storage {

// The proxy may be called and finally released on any thread.
class COMPONENT_EXPORT(STORAGE_BROWSER) QuotaManagerProxy
    : public base::RefCountedThreadSafe<QuotaManagerProxy> {
 public:
  using UsageAndQuotaCallback = QuotaManager::UsageAndQuotaCallback;

  virtual void RegisterClient(
      scoped_refptr<QuotaClient> client,
      QuotaClientType client_type,
      const std::vector<blink::mojom::StorageType>& storage_types);
  virtual void NotifyStorageAccessed(const url::Origin& origin,
                                     blink::mojom::StorageType type);
  virtual void NotifyStorageModified(QuotaClientType client_id,
                                     const url::Origin& origin,
                                     blink::mojom::StorageType type,
                                     int64_t delta);
  virtual void NotifyOriginInUse(const url::Origin& origin);
  virtual void NotifyOriginNoLongerInUse(const url::Origin& origin);
  virtual void NotifyWriteFailed(const url::Origin& origin);

  virtual void SetUsageCacheEnabled(QuotaClientType client_id,
                                    const url::Origin& origin,
                                    blink::mojom::StorageType type,
                                    bool enabled);
  virtual void GetUsageAndQuota(base::SequencedTaskRunner* original_task_runner,
                                const url::Origin& origin,
                                blink::mojom::StorageType type,
                                UsageAndQuotaCallback callback);

  // DevTools Quota Override methods:
  std::unique_ptr<QuotaOverrideHandle> GetQuotaOverrideHandle();
  // Called by QuotaOverrideHandle upon construction to asynchronously
  // fetch an id.
  void GetOverrideHandleId(base::OnceCallback<void(int)>);
  void OverrideQuotaForOrigin(int handle_id,
                              url::Origin origin,
                              base::Optional<int64_t> quota_size,
                              base::OnceClosure callback);
  void WithdrawOverridesForHandle(int handle_id);

  // This method may only be called on the IO thread.
  // It may return nullptr if the manager has already been deleted.
  QuotaManager* quota_manager() const;

 protected:
  friend class QuotaManager;
  friend class base::RefCountedThreadSafe<QuotaManagerProxy>;

  QuotaManagerProxy(QuotaManager* manager,
                    scoped_refptr<base::SingleThreadTaskRunner> io_thread);
  virtual ~QuotaManagerProxy();

 private:
  QuotaManager* manager_;  // only accessed on the io thread
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_;

  DISALLOW_COPY_AND_ASSIGN(QuotaManagerProxy);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_PROXY_H_
