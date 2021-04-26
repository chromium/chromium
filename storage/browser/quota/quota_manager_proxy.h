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
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "components/services/storage/public/mojom/quota_client.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "storage/browser/quota/quota_callbacks.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_impl.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace base {

class SequencedTaskRunner;

}  // namespace base

namespace url {

class Origin;

}  // namespace url

namespace storage {

class QuotaClient;
class QuotaOverrideHandle;

// Thread-safe proxy for QuotaManagerImpl.
//
// Most methods can be called from any thread. The few exceptions are marked
// accordingly in the associated comments.
class COMPONENT_EXPORT(STORAGE_BROWSER) QuotaManagerProxy
    : public base::RefCountedThreadSafe<QuotaManagerProxy> {
 public:
  using UsageAndQuotaCallback = QuotaManagerImpl::UsageAndQuotaCallback;

  // The caller is responsible for calling InvalidateQuotaManagerImpl() before
  // `quota_manager_impl` is destroyed. `quota_manager_impl_task_runner` must be
  // associated with the sequence that `quota_manager_impl` may be used on.
  //
  // See the comment on `quota_manager_impl_` for an explanation why
  // `quota_manager_impl` isn't a base::WeakPtr<QuotaManagerImpl>.
  QuotaManagerProxy(
      QuotaManagerImpl* quota_manager_impl,
      scoped_refptr<base::SequencedTaskRunner> quota_manager_impl_task_runner);

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
                                     blink::mojom::StorageType type,
                                     base::Time access_time);

  // Notify the quota manager that storage has been modified for the given
  // client.  A |callback| may be optionally provided to be invoked on the
  // given task runner when the quota system's state in memory has been
  // updated.  If a |callback| is provided then |callback_task_runner| must
  // also be provided.  If the quota manager runs on |callback_task_runner|,
  // then the |callback| may be invoked synchronously.
  virtual void NotifyStorageModified(
      QuotaClientType client_id,
      const url::Origin& origin,
      blink::mojom::StorageType type,
      int64_t delta,
      base::Time modification_time,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner = nullptr,
      base::OnceClosure callback = base::OnceClosure());

  virtual void NotifyOriginInUse(const url::Origin& origin);
  virtual void NotifyOriginNoLongerInUse(const url::Origin& origin);
  virtual void NotifyWriteFailed(const url::Origin& origin);

  virtual void SetUsageCacheEnabled(QuotaClientType client_id,
                                    const url::Origin& origin,
                                    blink::mojom::StorageType type,
                                    bool enabled);
  virtual void GetUsageAndQuota(
      const url::Origin& origin,
      blink::mojom::StorageType type,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
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

  // Called right before the QuotaManagerImpl is destroyed.
  // This method may only be called on the QuotaManagerImpl sequence.
  void InvalidateQuotaManagerImpl(base::PassKey<QuotaManagerImpl>);

 protected:
  friend class base::RefCountedThreadSafe<QuotaManagerProxy>;
  friend class base::DeleteHelper<QuotaManagerProxy>;

  virtual ~QuotaManagerProxy();

 private:
  // Bound to QuotaManagerImpl's sequence.
  //
  // At the time this code is written, there is no way to explicitly bind a
  // SequenceChecker to a sequence. The implementation ensures that the
  // SequenceChecker binds to the right sequence by DCHECKing that
  // `quota_manager_impl_task_runner_` runs on the current sequence before
  // calling DCHECK_CALLED_ON_VALID_SEQUENCE(). New methods added to
  // QuotaManagerProxy must follow the same pattern.
  SEQUENCE_CHECKER(quota_manager_impl_sequence_checker_);

  // Conceptually, this member is a base::WeakPtr<QuotaManagerImpl>. Like a
  // WeakPtr, it becomes null after the target QuotaManagerImpl is destroyed.
  //
  // base::WeakPtr cannot be used here for two reasons:
  // 1) QuotaManagerProxy can be deleted on any thread, whereas WeakPtrs must be
  //    invalidated on the same sequence where they are accessed. In this case,
  //    the WeakPtr would need to be accessed on the QuotaManagerImpl sequence,
  //    and then invalidated wherever the destructor happens to run.
  // 2) QuotaManagerProxy instances must be created during the QuotaManagerImpl
  //    constructor, before the QuotaManagerImpl's WeakPtrFactory is
  //    constructed. This is because the easiest way to ensure that
  //    QuotaManagerImpl exposes its QuotaManagerProxy in a thread-safe manner
  //    is to have the QuotaManagerImpl's QuotaManagerProxy reference be const.
  QuotaManagerImpl* quota_manager_impl_
      GUARDED_BY_CONTEXT(quota_manager_impl_sequence_checker_);

  // TaskRunner that accesses QuotaManagerImpl's sequence.
  //
  // This member is not GUARDED_BY_CONTEXT() and may be accessed from any
  // thread. This is safe because the scoped_refptr is immutable (always points
  // to the same object), and the object it points to is thread-safe.
  const scoped_refptr<base::SequencedTaskRunner>
      quota_manager_impl_task_runner_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_PROXY_H_
