// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_MOCK_QUOTA_MANAGER_PROXY_H_
#define STORAGE_BROWSER_TEST_MOCK_QUOTA_MANAGER_PROXY_H_

#include <stdint.h>

#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
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

  void RegisterClient(
      mojo::PendingRemote<mojom::QuotaClient> client,
      QuotaClientType client_type,
      const std::vector<blink::mojom::StorageType>& storage_types) override;

  void UpdateOrCreateBucket(
      const BucketInitParams& bucket_params,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) override;

  void GetBucket(const blink::StorageKey& storage_key,
                 const std::string& bucket_name,
                 blink::mojom::StorageType type,
                 scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
                 base::OnceCallback<void(QuotaErrorOr<BucketInfo>)>) override;

  void GetBucketById(
      const BucketId& bucket_id,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) override;

  // We don't mock them.
  void SetUsageCacheEnabled(storage::QuotaClientType client_id,
                            const blink::StorageKey& storage_key,
                            blink::mojom::StorageType type,
                            bool enabled) override {}
  void GetUsageAndQuota(
      const blink::StorageKey& storage_key,
      blink::mojom::StorageType type,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      UsageAndQuotaCallback callback) override;

  // Validates the `client_id` and updates the internal access count
  // which can be accessed via notify_storage_accessed_count().
  // The also records the `storage_key` and `type` in last_notified_storage_key_
  // and last_notified_type_.
  void NotifyStorageAccessed(const blink::StorageKey& storage_key,
                             blink::mojom::StorageType type,
                             base::Time access_time) override;

  // Records the `storage_key`, `type` and `delta` as
  // last_notified_storage_key_, last_notified_type_ and last_notified_delta_
  // respectively. If non-null MockQuotaManager is given to the constructor this
  // also updates the manager's internal usage information.
  void NotifyStorageModified(
      storage::QuotaClientType client_id,
      const blink::StorageKey& storage_key,
      blink::mojom::StorageType type,
      int64_t delta,
      base::Time modification_time,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceClosure callback) override;

  void CreateBucketForTesting(
      const blink::StorageKey& storage_key,
      const std::string& bucket_name,
      blink::mojom::StorageType storage_type,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) override;

  int notify_storage_accessed_count() const { return storage_accessed_count_; }
  int notify_storage_modified_count() const { return storage_modified_count_; }
  blink::StorageKey last_notified_storage_key() const {
    return last_notified_storage_key_;
  }
  blink::mojom::StorageType last_notified_type() const {
    return last_notified_type_;
  }
  int64_t last_notified_delta() const { return last_notified_delta_; }

 protected:
  ~MockQuotaManagerProxy() override;

 private:
  const raw_ptr<MockQuotaManager> mock_quota_manager_;

  int storage_accessed_count_;
  int storage_modified_count_;
  blink::StorageKey last_notified_storage_key_;
  blink::mojom::StorageType last_notified_type_;
  int64_t last_notified_delta_;

  mojo::Remote<mojom::QuotaClient> registered_client_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_MOCK_QUOTA_MANAGER_PROXY_H_
