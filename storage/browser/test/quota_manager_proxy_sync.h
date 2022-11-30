// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_QUOTA_MANAGER_PROXY_SYNC_H_
#define STORAGE_BROWSER_TEST_QUOTA_MANAGER_PROXY_SYNC_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace storage {

class QuotaManagerProxy;
struct BucketInfo;

// Helper class for QuotaManagerProxy that returns results synchronously.
class QuotaManagerProxySync {
 public:
  // `proxy` must outlive the newly created instance.
  explicit QuotaManagerProxySync(QuotaManagerProxy* proxy);
  ~QuotaManagerProxySync();

  QuotaManagerProxySync(const QuotaManagerProxySync&) = delete;
  QuotaManagerProxySync& operator=(const QuotaManagerProxySync&) = delete;

  QuotaErrorOr<BucketInfo> GetBucket(const blink::StorageKey& storage_key,
                                     const std::string& bucket_name,
                                     blink::mojom::StorageType storage_type);

 private:
  const raw_ptr<QuotaManagerProxy> proxy_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_QUOTA_MANAGER_PROXY_SYNC_H_
