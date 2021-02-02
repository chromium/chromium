// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BUCKETS_BUCKET_MANAGER_HOST_H_
#define CONTENT_BROWSER_BUCKETS_BUCKET_MANAGER_HOST_H_

#include "content/browser/buckets/bucket_host.h"
#include "third_party/blink/public/mojom/buckets/bucket_manager_host.mojom.h"
#include "url/origin.h"

namespace content {

// Implements the Storage Buckets API for a single origin.
//
// BucketContext owns all BucketManagerHost instances associated with
// a StorageParititon. A new instance is created for every incoming
// connection from a frame or worker. Instances are destroyed when
// their corresponding mojo connection is closed, or when BucketContext
// is destroyed.
//
// TODO(ayui): In the future the BucketManagerHost will be expected to be unique
// per origin, so all of an origin's frame and workers would be connected to
// the same Host. We will need a singleton BucketManagerHost so an origin's
// I/O operations (bucket creation / open / querying) are serialized, and so
// that we can notify all frames & workers when a bucket is deleted, and have
// them mark their Bucket instance as closed.
class BucketManagerHost : public blink::mojom::BucketManagerHost {
 public:
  explicit BucketManagerHost(const url::Origin& origin);
  ~BucketManagerHost() override;

  BucketManagerHost(const BucketManagerHost&) = delete;
  BucketManagerHost& operator=(const BucketManagerHost&) = delete;

  // blink::mojom::BucketsManagerHost:
  void OpenBucket(const std::string& name,
                  blink::mojom::BucketPoliciesPtr policy,
                  OpenBucketCallback callback) override;
  void Keys(KeysCallback callback) override;
  void DeleteBucket(const std::string& name,
                    DeleteBucketCallback callback) override;

  void RemoveBucketHost(const std::string& name);

 private:
  // The origin of the frame or worker connected to this BucketManagerHost.
  const url::Origin origin_;

  // TODO(ayui): Temporary map of buckets. This will eventually be stored in its
  // own database and connect to IndexDB, Cache, Blob Storage, etc.
  std::map<std::string, std::unique_ptr<BucketHost>> bucket_map_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BUCKETS_BUCKET_MANAGER_HOST_H_
