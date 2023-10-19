// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_MOCK_QUOTA_CLIENT_H_
#define STORAGE_BROWSER_TEST_MOCK_QUOTA_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace storage {

class QuotaManagerProxy;

// Default StorageKey data that the QuotaDatabase does not know about yet,
// and is to be fetched during QuotaDatabase bootstrapping via
// QuotaClient::GetStorageKeysForType.
struct UnmigratedStorageKeyData {
  const char* origin;
  blink::mojom::StorageType type;
  int64_t usage;
};

// Mock QuotaClient implementation for testing.
class MockQuotaClient : public mojom::QuotaClient {
 public:
  MockQuotaClient(scoped_refptr<QuotaManagerProxy> quota_manager_proxy,
                  QuotaClientType client_type,
                  base::span<const UnmigratedStorageKeyData> unmigrated_data =
                      base::span<const UnmigratedStorageKeyData>());

  MockQuotaClient(const MockQuotaClient&) = delete;
  MockQuotaClient& operator=(const MockQuotaClient&) = delete;

  ~MockQuotaClient() override;

  //  Adds bucket data the client has usage for.
  void AddBucketsData(const std::map<BucketLocator, int64_t>& mock_data);

  void ModifyBucketAndNotify(const BucketLocator& bucket, int64_t delta);

  void AddBucketToErrorSet(const BucketLocator& bucket);

  base::Time IncrementMockTime();

  size_t get_bucket_usage_call_count() const {
    return get_bucket_usage_call_count_;
  }

  // QuotaClient.
  void GetBucketUsage(const BucketLocator& bucket,
                      GetBucketUsageCallback callback) override;
  void GetStorageKeysForType(blink::mojom::StorageType type,
                             GetStorageKeysForTypeCallback callback) override;
  void DeleteBucketData(const BucketLocator& bucket,
                        DeleteBucketDataCallback callback) override;
  void PerformStorageCleanup(blink::mojom::StorageType type,
                             PerformStorageCleanupCallback callback) override;

 private:
  void RunGetBucketUsage(const BucketLocator& bucket,
                         GetBucketUsageCallback callback);
  void RunGetStorageKeysForType(blink::mojom::StorageType type,
                                GetStorageKeysForTypeCallback callback);
  void RunDeleteBucketData(const BucketLocator& bucket,
                           DeleteBucketDataCallback callback);

  const scoped_refptr<QuotaManagerProxy> quota_manager_proxy_;
  const QuotaClientType client_type_;

  std::map<BucketLocator, int64_t, CompareBucketLocators> bucket_data_;
  std::map<std::pair<blink::StorageKey, blink::mojom::StorageType>, int64_t>
      unmigrated_storage_key_data_;
  std::set<BucketLocator> error_buckets_;

  int mock_time_counter_ = 0;

  size_t get_bucket_usage_call_count_ = 0u;

  base::WeakPtrFactory<MockQuotaClient> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_MOCK_QUOTA_CLIENT_H_
