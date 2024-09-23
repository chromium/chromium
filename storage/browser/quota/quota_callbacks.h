// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_CALLBACKS_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_CALLBACKS_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-forward.h"

namespace blink {
class StorageKey;
}

namespace storage {

struct UsageInfo;
using UsageInfoEntries = std::vector<UsageInfo>;

// Common callback types that are used throughout in the quota module.
using UsageCallback =
    base::OnceCallback<void(int64_t usage, int64_t unlimited_usage)>;
using QuotaCallback =
    base::OnceCallback<void(blink::mojom::QuotaStatusCode status,
                            int64_t quota)>;
using UsageWithBreakdownCallback =
    base::OnceCallback<void(int64_t usage,
                            blink::mojom::UsageBreakdownPtr usage_breakdown)>;
using StatusCallback = base::OnceCallback<void(blink::mojom::QuotaStatusCode)>;
using GetBucketsCallback =
    base::OnceCallback<void(const std::set<BucketLocator>& buckets)>;
using GetStorageKeysCallback =
    base::OnceCallback<void(const std::set<blink::StorageKey>& storage_keys)>;
using GetUsageInfoCallback = base::OnceCallback<void(UsageInfoEntries)>;
using GetBucketCallback =
    base::OnceCallback<void(const std::optional<BucketLocator>& bucket_info)>;

// Simple template wrapper for a callback queue.
template <typename CallbackType, typename... Args>
class CallbackQueue {
 public:
  // Returns true if the given |callback| is the first one added to the queue.
  bool Add(CallbackType callback) {
    callbacks_.push_back(std::move(callback));
    return (callbacks_.size() == 1);
  }

  bool HasCallbacks() const { return !callbacks_.empty(); }

  // Runs the callbacks added to the queue and clears the queue.
  void Run(Args... args) {
    std::vector<CallbackType> callbacks;
    callbacks.swap(callbacks_);
    for (auto& callback : callbacks)
      std::move(callback).Run(args...);
  }

  void Swap(CallbackQueue<CallbackType, Args...>* other) {
    callbacks_.swap(other->callbacks_);
  }

  size_t size() const { return callbacks_.size(); }

 private:
  std::vector<CallbackType> callbacks_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_CALLBACKS_H_
