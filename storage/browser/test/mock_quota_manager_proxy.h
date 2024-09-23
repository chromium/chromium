// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_MOCK_QUOTA_MANAGER_PROXY_H_
#define STORAGE_BROWSER_TEST_MOCK_QUOTA_MANAGER_PROXY_H_

#include <stdint.h>

#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

namespace storage {

class MockQuotaManager;
enum class QuotaClientType;

class MockQuotaManagerProxy : public QuotaManagerProxy {
 public:
  // It is ok to give nullptr to `quota_manager`.
  MockQuotaManagerProxy(
      MockQuotaManager* quota_manager,
      scoped_refptr<base::SequencedTaskRunner> quota_manager_task_runner);

  MockQuotaManagerProxy(const MockQuotaManagerProxy&) = delete;
  MockQuotaManagerProxy& operator=(const MockQuotaManagerProxy&) = delete;

  void UpdateOrCreateBucket(
      const BucketInitParams& bucket_params,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) override;

  QuotaErrorOr<BucketInfo> GetOrCreateBucketSync(
      const BucketInitParams& params) override;

  void GetBucketByNameUnsafe(
      const blink::StorageKey& storage_key,
      const std::string& bucket_name,
      blink::mojom::StorageType type,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)>) override;

  void GetBucketById(
      const BucketId& bucket_id,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) override;

  void GetBucketsForStorageKey(
      const blink::StorageKey& storage_key,
      blink::mojom::StorageType type,
      bool delete_expired,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(QuotaErrorOr<std::set<BucketInfo>>)> callback)
      override;

  // We don't mock them.
  void SetUsageCacheEnabled(QuotaClientType client_id,
                            const blink::StorageKey& storage_key,
                            blink::mojom::StorageType type,
                            bool enabled) override {}
  void GetUsageAndQuota(
      const blink::StorageKey& storage_key,
      blink::mojom::StorageType type,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      UsageAndQuotaCallback callback) override;

  void GetUsageAndQuota(
      const BucketLocator& bucket_locator,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      UsageAndQuotaCallback callback);

  // Updates the internal access count which can be accessed via
  // `notify_bucket_accessed_count()`. Also, records the `bucket_id` in
  // `last_notified_bucket_id_`.
  void NotifyBucketAccessed(const BucketLocator& bucket,
                            base::Time access_time) override;

  // Records the `bucket_id` and `delta` as `last_notified_bucket_id_` and
  // `last_notified_bucket_delta_` respectively. If a non-null
  // `MockQuotaManager` is given to the constructor, this also updates the
  // manager's internal usage information.
  void NotifyBucketModified(
      QuotaClientType client_id,
      const BucketLocator& bucket,
      std::optional<int64_t> delta,
      base::Time modification_time,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceClosure callback) override;

  void CreateBucketForTesting(
      const blink::StorageKey& storage_key,
      const std::string& bucket_name,
      blink::mojom::StorageType storage_type,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) override;

  blink::StorageKey last_notified_storage_key() const {
    return last_notified_storage_key_;
  }
  blink::mojom::StorageType last_notified_type() const {
    return last_notified_type_;
  }

  int notify_bucket_accessed_count() const { return bucket_accessed_count_; }
  int notify_bucket_modified_count() const { return bucket_modified_count_; }
  BucketId last_notified_bucket_id() const { return last_notified_bucket_id_; }
  std::optional<int64_t> last_notified_bucket_delta() const {
    return last_notified_bucket_delta_;
  }

 protected:
  ~MockQuotaManagerProxy() override;

 private:
  const raw_ptr<MockQuotaManager, AcrossTasksDanglingUntriaged>
      mock_quota_manager_;

  // The real QuotaManagerProxy is safe to call into from any thread, therefore
  // this mock quota manager must also be safe to call into from any thread.
  base::Lock lock_;

  blink::StorageKey last_notified_storage_key_;
  blink::mojom::StorageType last_notified_type_ =
      blink::mojom::StorageType::kUnknown;

  int bucket_accessed_count_ = 0;
  int bucket_modified_count_ = 0;
  BucketId last_notified_bucket_id_ = BucketId::FromUnsafeValue(-1);
  std::optional<int64_t> last_notified_bucket_delta_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_MOCK_QUOTA_MANAGER_PROXY_H_
