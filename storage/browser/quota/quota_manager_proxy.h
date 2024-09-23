// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_PROXY_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_PROXY_H_

#include <stdint.h>

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "components/services/storage/public/cpp/buckets/bucket_init_params.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "components/services/storage/public/mojom/quota_client.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "storage/browser/quota/quota_callbacks.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_impl.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace storage {

struct BucketLocator;
class QuotaOverrideHandle;

// Thread-safe proxy for QuotaManagerImpl.
//
// Most methods can be called from any thread. The few exceptions are marked
// accordingly in the associated comments.
class COMPONENT_EXPORT(STORAGE_BROWSER) QuotaManagerProxy
    : public base::RefCountedThreadSafe<QuotaManagerProxy> {
 public:
  using UsageAndQuotaCallback = QuotaManagerImpl::UsageAndQuotaCallback;
  using UsageAndQuotaWithBreakdownCallback =
      QuotaManagerImpl::UsageAndQuotaWithBreakdownCallback;

  // The caller is responsible for calling InvalidateQuotaManagerImpl() before
  // `quota_manager_impl` is destroyed. `quota_manager_impl_task_runner` must be
  // associated with the sequence that `quota_manager_impl` may be used on.
  //
  // See the comment on `quota_manager_impl_` for an explanation why
  // `quota_manager_impl` isn't a base::WeakPtr<QuotaManagerImpl>.
  QuotaManagerProxy(
      QuotaManagerImpl* quota_manager_impl,
      scoped_refptr<base::SequencedTaskRunner> quota_manager_impl_task_runner,
      const base::FilePath& profile_path);

  QuotaManagerProxy(const QuotaManagerProxy&) = delete;
  QuotaManagerProxy& operator=(const QuotaManagerProxy&) = delete;

  virtual void RegisterClient(
      mojo::PendingRemote<mojom::QuotaClient> client,
      QuotaClientType client_type,
      const base::flat_set<blink::mojom::StorageType>& storage_types);

  virtual void BindInternalsHandler(
      mojo::PendingReceiver<mojom::QuotaInternalsHandler> receiver);

  // Constructs path where `bucket` data is persisted to disk for partitioned
  // storage.
  base::FilePath GetBucketPath(const BucketLocator& bucket);

  // Constructs path where `bucket` and `client_type` data is persisted to disk
  // for partitioned storage. NOTE: this will happily construct a path even for
  // incognito profiles. It is up to the caller to handle incognito cases
  // appropriately, i.e. not saving anything to disk at that path.
  base::FilePath GetClientBucketPath(const BucketLocator& bucket,
                                     QuotaClientType client_type);

  // Gets the bucket with the name, storage key, quota and expiration
  // specified in `bucket_params` for StorageType kTemporary and returns a
  // BucketInfo with all fields filled in. This will also update the bucket's
  // policies to match `bucket_params` for those parameters which may be
  // updated, but only if it's a user created bucket (not the default bucket).
  // If one doesn't exist, it creates a new bucket with the specified policies.
  // Returns a QuotaError if the operation has failed.
  virtual void UpdateOrCreateBucket(
      const BucketInitParams& bucket_params,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback);

  // This function calls the asynchronous GetOrCreateBucket function but blocks
  // until completion. Be strongly advised NOT to use this method, see
  // crbug.com/1444138
  virtual QuotaErrorOr<BucketInfo> GetOrCreateBucketSync(
      const BucketInitParams& params);

  // Same as GetOrCreateBucket but takes in StorageType. This should only be
  // used by FileSystem, and is expected to be removed when
  // StorageType::kSyncable and StorageType::kPersistent are deprecated.
  // (crbug.com/1233525, crbug.com/1286964).
  virtual void GetOrCreateBucketDeprecated(
      const BucketInitParams& params,
      blink::mojom::StorageType storage_type,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback);

  // Creates a bucket for `origin` with `bucket_name` and returns the
  // BucketInfo to the callback. Returns a QuotaError to the callback
  // on operation failure.
  //
  // TODO(crbug.com/40181609): Remove `storage_type` when the only supported
  // StorageType is kTemporary.
  virtual void CreateBucketForTesting(
      const blink::StorageKey& storage_key,
      const std::string& bucket_name,
      blink::mojom::StorageType storage_type,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback);

  // Retrieves the BucketInfo of the bucket with `bucket_id` and returns it to
  // the callback. Will return a QuotaError if a bucket does not exist or on
  // operation failure.
  virtual void GetBucketById(
      const BucketId& bucket_id,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback);

  // Retrieves the BucketInfo of the bucket with `bucket_name` for
  // `storage_key` and returns it to the callback. Will return a QuotaError if a
  // bucket does not exist or on operation failure.
  // This SHOULD NOT be used once you have the ID for a bucket. Prefer
  // GetBucketById.
  virtual void GetBucketByNameUnsafe(
      const blink::StorageKey& storage_key,
      const std::string& bucket_name,
      blink::mojom::StorageType type,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback);

  // Retrieves all buckets for `storage_key` and `type` that are in the buckets
  // table. If `delete_expired` is true, expired buckets will be filtered out of
  // the reply and also deleted from disk.
  virtual void GetBucketsForStorageKey(
      const blink::StorageKey& storage_key,
      blink::mojom::StorageType type,
      bool delete_expired,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(QuotaErrorOr<std::set<BucketInfo>>)> callback);

  // Deletes bucket with `bucket_name` for `storage_key` for
  // StorageType::kTemporary for all registered QuotaClients if a bucket exists.
  // Will return QuotaStatusCode to the callback. Called by Storage Buckets API
  // for deleting buckets via StorageBucketManager.
  virtual void DeleteBucket(
      const blink::StorageKey& storage_key,
      const std::string& bucket_name,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(blink::mojom::QuotaStatusCode)> callback);

  // Updates the expiration for a bucket.
  virtual void UpdateBucketExpiration(
      BucketId bucket,
      const base::Time& expiration,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback);

  // Updates the persistence for a bucket.
  virtual void UpdateBucketPersistence(
      BucketId bucket,
      bool persistent,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback);

  // Notifies the quota manager that a bucket has been accessed to maintain LRU
  // ordering.
  virtual void NotifyBucketAccessed(const BucketLocator& bucket,
                                    base::Time access_time);

  // Notifies the quota manager that a bucket has been modified for the given
  // client.  A `callback` may be optionally provided to be invoked on the
  // given task runner when the quota system's state in memory has been
  // updated.  If a `callback` is provided then `callback_task_runner` must
  // also be provided.  If the quota manager runs on `callback_task_runner`,
  // then the `callback` may be invoked synchronously.
  virtual void NotifyBucketModified(
      QuotaClientType client_id,
      const BucketLocator& bucket,
      std::optional<int64_t> delta,
      base::Time modification_time,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceClosure callback);

  virtual void OnClientWriteFailed(const blink::StorageKey& storage_key);

  virtual void SetUsageCacheEnabled(QuotaClientType client_id,
                                    const blink::StorageKey& storage_key,
                                    blink::mojom::StorageType type,
                                    bool enabled);
  virtual void GetUsageAndQuota(
      const blink::StorageKey& storage_key,
      blink::mojom::StorageType type,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      UsageAndQuotaCallback callback);

  void GetBucketUsageAndQuota(
      BucketId bucket,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      UsageAndQuotaCallback callback);

  // To be called by a client when bytes are about to be written to get the
  // amount of space left in the given bucket. Checks against quota remaining in
  // the bucket and across the whole StorageKey. The returned value is the
  // amount of space remaining in bytes.
  void GetBucketSpaceRemaining(
      const BucketLocator& bucket,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(QuotaErrorOr<int64_t>)> callback);

  virtual void IsStorageUnlimited(
      const blink::StorageKey& storage_key,
      blink::mojom::StorageType type,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(bool)> callback);

  void GetStorageKeyUsageWithBreakdown(
      const blink::StorageKey& storage_key,
      blink::mojom::StorageType type,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      UsageWithBreakdownCallback callback);

  // DevTools Quota Override methods:
  std::unique_ptr<QuotaOverrideHandle> GetQuotaOverrideHandle();
  // Called by QuotaOverrideHandle upon construction to asynchronously
  // fetch an id.
  void GetOverrideHandleId(
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(int)> callback);
  void OverrideQuotaForStorageKey(
      int handle_id,
      const blink::StorageKey& storage_key,
      std::optional<int64_t> quota_size,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceClosure callback);
  void WithdrawOverridesForHandle(int handle_id);

  // Called right before the QuotaManagerImpl is destroyed.
  // This method may only be called on the QuotaManagerImpl sequence.
  void InvalidateQuotaManagerImpl(base::PassKey<QuotaManagerImpl>);

  // Adds an observer which is notified of changes to the QuotaManager.
  void AddObserver(
      mojo::PendingRemote<storage::mojom::QuotaManagerObserver> observer);

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
  raw_ptr<QuotaManagerImpl, AcrossTasksDanglingUntriaged> quota_manager_impl_
      GUARDED_BY_CONTEXT(quota_manager_impl_sequence_checker_);

  // TaskRunner that accesses QuotaManagerImpl's sequence.
  //
  // This member is not GUARDED_BY_CONTEXT() and may be accessed from any
  // thread. This is safe because the scoped_refptr is immutable (always points
  // to the same object), and the object it points to is thread-safe.
  const scoped_refptr<base::SequencedTaskRunner>
      quota_manager_impl_task_runner_;

  const base::FilePath profile_path_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_PROXY_H_
