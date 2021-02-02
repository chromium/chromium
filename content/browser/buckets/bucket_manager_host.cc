// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/buckets/bucket_manager_host.h"

namespace content {

BucketManagerHost::BucketManagerHost(const url::Origin& origin)
    : origin_(origin) {}
BucketManagerHost::~BucketManagerHost() = default;

void BucketManagerHost::OpenBucket(const std::string& name,
                                   blink::mojom::BucketPoliciesPtr policy,
                                   OpenBucketCallback callback) {
  auto it = bucket_map_.find(name);
  if (it != bucket_map_.end()) {
    std::move(callback).Run(it->second->CreateStorageBucketBinding());
    return;
  }

  auto bucket = std::make_unique<BucketHost>(this, name, std::move(policy));
  auto pending_remote = bucket->CreateStorageBucketBinding();
  bucket_map_.emplace(name, std::move(bucket));
  std::move(callback).Run(std::move(pending_remote));
}

void BucketManagerHost::Keys(KeysCallback callback) {
  std::vector<std::string> keys;
  for (auto& bucket : bucket_map_)
    keys.push_back(bucket.first);
  std::move(callback).Run(keys, true);
}

void BucketManagerHost::DeleteBucket(const std::string& name,
                                     DeleteBucketCallback callback) {
  bucket_map_.erase(name);
  std::move(callback).Run(true);
}

void BucketManagerHost::RemoveBucketHost(const std::string& bucket_name) {
  DCHECK(base::Contains(bucket_map_, bucket_name));
  bucket_map_.erase(bucket_name);
}

}  // namespace content
