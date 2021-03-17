// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_QUOTA_CLIENT_H_
#define CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_QUOTA_CLIENT_H_

#include "base/sequence_checker.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "content/common/content_export.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

namespace content {

class NativeIOManager;

// Integrates NativeIO with the quota system.
//
// Each NativeIOManager owns exactly one NativeIOQuotaClient.
class CONTENT_EXPORT NativeIOQuotaClient : public storage::mojom::QuotaClient {
 public:
  explicit NativeIOQuotaClient(NativeIOManager* manager);
  ~NativeIOQuotaClient() override;

  NativeIOQuotaClient(const NativeIOQuotaClient&) = delete;
  NativeIOQuotaClient& operator=(const NativeIOQuotaClient&) = delete;

  // QuotaClient.
  void GetOriginUsage(const url::Origin& origin,
                      blink::mojom::StorageType type,
                      GetOriginUsageCallback callback) override;
  void GetOriginsForType(blink::mojom::StorageType type,
                         GetOriginsForTypeCallback callback) override;
  void GetOriginsForHost(blink::mojom::StorageType type,
                         const std::string& host,
                         GetOriginsForHostCallback callback) override;
  void DeleteOriginData(const url::Origin& origin,
                        blink::mojom::StorageType type,
                        DeleteOriginDataCallback callback) override;
  void PerformStorageCleanup(blink::mojom::StorageType type,
                             PerformStorageCleanupCallback callback) override;

 private:
  NativeIOManager* manager_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_QUOTA_CLIENT_H_
