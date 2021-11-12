// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_MOCK_QUOTA_CLIENT_H_
#define STORAGE_BROWSER_TEST_MOCK_QUOTA_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace storage {

class QuotaManagerProxy;

struct MockStorageKeyData {
  const char* origin;
  blink::mojom::StorageType type;
  int64_t usage;
};

// Mock QuotaClient implementation for testing.
class MockQuotaClient : public mojom::QuotaClient {
 public:
  MockQuotaClient(scoped_refptr<QuotaManagerProxy> quota_manager_proxy,
                  base::span<const MockStorageKeyData> mock_data,
                  QuotaClientType client_type);

  MockQuotaClient(const MockQuotaClient&) = delete;
  MockQuotaClient& operator=(const MockQuotaClient&) = delete;

  ~MockQuotaClient() override;

  // To add or modify mock data in this client.
  void AddStorageKeyAndNotify(const blink::StorageKey& storage_key,
                              blink::mojom::StorageType type,
                              int64_t size);
  void ModifyStorageKeyAndNotify(const blink::StorageKey& storage_key,
                                 blink::mojom::StorageType type,
                                 int64_t delta);
  void TouchAllStorageKeysAndNotify();

  void AddStorageKeyToErrorSet(const blink::StorageKey& storage_key,
                               blink::mojom::StorageType type);

  base::Time IncrementMockTime();

  // QuotaClient.
  void GetStorageKeyUsage(const blink::StorageKey& storage_key,
                          blink::mojom::StorageType type,
                          GetStorageKeyUsageCallback callback) override;
  void GetStorageKeysForType(blink::mojom::StorageType type,
                             GetStorageKeysForTypeCallback callback) override;
  void GetStorageKeysForHost(blink::mojom::StorageType type,
                             const std::string& host,
                             GetStorageKeysForHostCallback callback) override;
  void DeleteStorageKeyData(const blink::StorageKey& storage_key,
                            blink::mojom::StorageType type,
                            DeleteStorageKeyDataCallback callback) override;
  void PerformStorageCleanup(blink::mojom::StorageType type,
                             PerformStorageCleanupCallback callback) override;

 private:
  void RunGetStorageKeyUsage(const blink::StorageKey& storage_key,
                             blink::mojom::StorageType type,
                             GetStorageKeyUsageCallback callback);
  void RunGetStorageKeysForType(blink::mojom::StorageType type,
                                GetStorageKeysForTypeCallback callback);
  void RunGetStorageKeysForHost(blink::mojom::StorageType type,
                                const std::string& host,
                                GetStorageKeysForTypeCallback callback);
  void RunDeleteStorageKeyData(const blink::StorageKey& storage_key,
                               blink::mojom::StorageType type,
                               DeleteStorageKeyDataCallback callback);

  const scoped_refptr<QuotaManagerProxy> quota_manager_proxy_;
  const QuotaClientType client_type_;

  std::map<std::pair<blink::StorageKey, blink::mojom::StorageType>, int64_t>
      storage_key_data_;
  std::set<std::pair<blink::StorageKey, blink::mojom::StorageType>>
      error_storage_keys_;

  int mock_time_counter_ = 0;

  base::WeakPtrFactory<MockQuotaClient> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_MOCK_QUOTA_CLIENT_H_
