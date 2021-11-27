// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_QUOTA_CLIENT_H_
#define CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_QUOTA_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/services/storage/public/cpp/storage_key_quota_client.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {

class NativeIOManager;

// Integrates NativeIO with the quota system.
//
// Each NativeIOManager owns exactly one NativeIOQuotaClient.
class NativeIOQuotaClient : public storage::StorageKeyQuotaClient {
 public:
  explicit NativeIOQuotaClient(NativeIOManager* manager);
  ~NativeIOQuotaClient() override;

  NativeIOQuotaClient(const NativeIOQuotaClient&) = delete;
  NativeIOQuotaClient& operator=(const NativeIOQuotaClient&) = delete;

  // storage::StorageKeyQuotaClient method overrides.
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
  SEQUENCE_CHECKER(sequence_checker_);

  const raw_ptr<NativeIOManager> manager_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_QUOTA_CLIENT_H_
