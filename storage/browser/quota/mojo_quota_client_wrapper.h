// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_MOJO_QUOTA_CLIENT_WRAPPER_H_
#define STORAGE_BROWSER_QUOTA_MOJO_QUOTA_CLIENT_WRAPPER_H_

#include <string>

#include "base/component_export.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "storage/browser/quota/quota_client.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace url {
class Origin;
}  // namespace url

namespace storage {

// TODO(crbug.com/1163009): Remove this class after all QuotaClients have been
//                          mojofied.
class COMPONENT_EXPORT(STORAGE_BROWSER) MojoQuotaClientWrapper
    : public storage::QuotaClient {
 public:
  // `wrapped_client` must outlive this instance.
  explicit MojoQuotaClientWrapper(storage::mojom::QuotaClient* wrapped_client);

  MojoQuotaClientWrapper(const MojoQuotaClientWrapper&) = delete;
  MojoQuotaClientWrapper& operator=(const MojoQuotaClientWrapper&) = delete;

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
  void OnQuotaManagerDestroyed() override;

 private:
  ~MojoQuotaClientWrapper() override;

  SEQUENCE_CHECKER(sequence_checker_);

  storage::mojom::QuotaClient* const wrapped_client_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_MOJO_QUOTA_CLIENT_WRAPPER_H_
