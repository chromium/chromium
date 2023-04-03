// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/quota_manager_proxy_sync.h"

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

QuotaManagerProxySync::QuotaManagerProxySync(QuotaManagerProxy* proxy)
    : proxy_(proxy) {
  DCHECK(proxy);
}

QuotaManagerProxySync::~QuotaManagerProxySync() = default;

QuotaErrorOr<BucketInfo> QuotaManagerProxySync::GetBucket(
    const blink::StorageKey& storage_key,
    const std::string& bucket_name,
    blink::mojom::StorageType storage_type) {
  QuotaErrorOr<BucketInfo> result;
  base::RunLoop run_loop;
  proxy_->GetBucketByNameUnsafe(
      storage_key, bucket_name, storage_type,
      base::SingleThreadTaskRunner::GetCurrentDefault().get(),
      base::BindLambdaForTesting([&](QuotaErrorOr<BucketInfo> bucket_info) {
        result = std::move(bucket_info);
        run_loop.Quit();
      }));
  run_loop.Run();
  return result;
}

}  // namespace storage
