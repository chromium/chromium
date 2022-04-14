// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BUCKETS_BUCKET_HOST_H_
#define CONTENT_BROWSER_BUCKETS_BUCKET_HOST_H_

#include "base/memory/raw_ptr.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/buckets/bucket_manager_host.mojom.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace content {

class BucketManagerHost;

// Implements a Storage Bucket object in the browser process.
//
// `BucketManagerHost` owns all `BucketHost` instances for an origin.
// A new instance is created for every request to open or create a
// StorageBucket. Instances are destroyed when all corresponding mojo
// connections are closed or when the owning `BucketManager` is destroyed.
class BucketHost : public blink::mojom::BucketHost {
 public:
  BucketHost(BucketManagerHost* bucket_manager_host,
             const storage::BucketInfo& bucket_info,
             blink::mojom::BucketPoliciesPtr policies);
  ~BucketHost() override;

  BucketHost(const BucketHost&) = delete;
  BucketHost& operator=(const BucketHost&) = delete;

  // Create mojo data pipe and return remote to pass to the renderer
  // for the StorageBucket object.
  mojo::PendingRemote<blink::mojom::BucketHost> CreateStorageBucketBinding();

  // blink::mojom::BucketHost
  void Persist(PersistCallback callback) override;
  void Persisted(PersistedCallback callback) override;
  void Estimate(EstimateCallback callback) override;
  void Durability(DurabilityCallback callback) override;
  void SetExpires(base::Time expires, SetExpiresCallback callback) override;
  void Expires(ExpiresCallback callback) override;

 private:
  void OnReceiverDisconnected();

  // Raw pointer use is safe here because BucketManagerHost owns this
  // BucketHost.
  raw_ptr<BucketManagerHost> bucket_manager_host_;

  const storage::BucketInfo bucket_info_;

  // TODO(ayui): The authoritative source of bucket policies should be the
  //             buckets database.
  blink::mojom::BucketPoliciesPtr policies_;

  mojo::ReceiverSet<blink::mojom::BucketHost> receivers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BUCKETS_BUCKET_HOST_H_
