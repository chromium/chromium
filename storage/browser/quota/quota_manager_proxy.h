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
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/thread_annotations.h"
#include "base/types/pass_key.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "storage/browser/quota/quota_callbacks.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_override_handle.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace base {

class SequencedTaskRunner;

}  // namespace base

namespace url {

class Origin;

}  // namespace url

namespace storage {

// Thread-safe proxy for QuotaManager.
//
// Most methods can be called from any thread. The few exceptions are marked
// accordingly in the associated comments.
class COMPONENT_EXPORT(STORAGE_BROWSER) QuotaManagerProxy
    : public base::RefCountedThreadSafe<QuotaManagerProxy> {
 public:
  using UsageAndQuotaCallback = QuotaManager::UsageAndQuotaCallback;

  // The caller is responsible for calling InvalidateQuotaManager() before
  // `quota_manager` is destroyed. `quota_manager_task_runner` must be
  // associated with the sequence that `quota_manager` may be used on.
  //
  // See the comment on `quota_manager_` for an explanation why `quota_manager`
  // isn't a base::WeakPtr<QuotaManager>.
  QuotaManagerProxy(
      QuotaManager* quota_manager,
      scoped_refptr<base::SequencedTaskRunner> quota_manager_task_runner);

  QuotaManagerProxy(const QuotaManagerProxy&) = delete;
  QuotaManagerProxy& operator=(const QuotaManagerProxy&) = delete;

  // TODO(crbug.com/1163009): Remove this method after all QuotaClients have
  //                          been mojofied.
  virtual void RegisterLegacyClient(
      scoped_refptr<QuotaClient> client,
      QuotaClientType client_type,
      const std::vector<blink::mojom::StorageType>& storage_types);

  virtual void RegisterClient(
      mojo::PendingRemote<mojom::QuotaClient> client,
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

  virtual void IsStorageUnlimited(
      const url::Origin& origin,
      blink::mojom::StorageType type,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(bool)> callback);

  // DevTools Quota Override methods:
  std::unique_ptr<QuotaOverrideHandle> GetQuotaOverrideHandle();
  // Called by QuotaOverrideHandle upon construction to asynchronously
  // fetch an id.
  void GetOverrideHandleId(
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(int)> callback);
  void OverrideQuotaForOrigin(
      int handle_id,
      url::Origin origin,
      base::Optional<int64_t> quota_size,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceClosure callback);
  void WithdrawOverridesForHandle(int handle_id);

  // This method may only be called on the QuotaManager sequence.
  // It may return nullptr if the manager has already been deleted.
  QuotaManager* quota_manager() const;

  // Called right before the QuotaManager is destroyed.
  // This method may only be called on the QuotaManager sequence.
  void InvalidateQuotaManager(base::PassKey<QuotaManager>);

 protected:
  friend class base::RefCountedThreadSafe<QuotaManagerProxy>;
  friend class base::DeleteHelper<QuotaManagerProxy>;

  virtual ~QuotaManagerProxy();

 private:
  // Bound to QuotaManager's sequence.
  //
  // At the time this code is written, there is no way to explicitly bind a
  // SequenceChecker to a sequence. The implementation ensures that the
  // SequenceChecker binds to the right sequence by DCHECKing that
  // `quota_manager_task_runner_` runs on the current sequence before calling
  // DCHECK_CALLED_ON_VALID_SEQUENCE(). New methods added to QuotaManagerProxy
  // must follow the same pattern.
  SEQUENCE_CHECKER(quota_manager_sequence_checker_);

  // Conceptually, this member is a base::WeakPtr<QuotaManager>. Like a WeakPtr,
  // it becomes null after the target QuotaManager is destroyed.
  //
  // base::WeakPtr cannot be used here for two reasons:
  // 1) QuotaManagerProxy can be deleted on any thread, whereas WeakPtrs must be
  //    invalidated on the same sequence where they are accessed. In this case,
  //    the WeakPtr would need to be accessed on the QuotaManager sequence, and
  //    then invalidated wherever the destructor happens to run.
  // 2) QuotaManagerProxy instances must be created during the QuotaManager
  //    constructor, before the QuotaManager's WeakPtrFactory is constructed.
  //    This is because the easiest way to ensure that QuotaManager exposes its
  //    QuotaManagerProxy in a thread-safe manner is to have the QuotaManager's
  //    QuotaManagerProxy reference be const.
  QuotaManager* quota_manager_
      GUARDED_BY_CONTEXT(quota_manager_sequence_checker_);

  // TaskRunner that accesses QuotaManager's sequence.
  //
  // This member is not GUARDED_BY_CONTEXT() and may be accessed from any
  // thread. This is safe because the scoped_refptr is immutable (always points
  // to the same object), and the object it points to is thread-safe.
  const scoped_refptr<base::SequencedTaskRunner> quota_manager_task_runner_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_PROXY_H_
