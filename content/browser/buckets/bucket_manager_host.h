// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BUCKETS_BUCKET_MANAGER_HOST_H_
#define CONTENT_BROWSER_BUCKETS_BUCKET_MANAGER_HOST_H_

#include "base/sequence_checker.h"
#include "content/browser/buckets/bucket_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/buckets/bucket_manager_host.mojom.h"
#include "url/origin.h"

namespace content {

class BucketManager;

// Implements the Storage Buckets API for a single origin.
//
// BucketManager owns all BucketManagerHost instances associated with
// a StorageParititon. A new instance is created for every origin.
// Instances are destroyed when all their corresponding mojo connection are
// closed, or when BucketManager is destroyed.
class BucketManagerHost : public blink::mojom::BucketManagerHost {
 public:
  explicit BucketManagerHost(BucketManager* manager, url::Origin origin);
  ~BucketManagerHost() override;

  BucketManagerHost(const BucketManagerHost&) = delete;
  BucketManagerHost& operator=(const BucketManagerHost&) = delete;

  // Binds |receiver| to the BucketManagerHost. The |receiver| must belong to
  // the frame or worker from this host's origin.
  void BindReceiver(
      mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver);

  // The origin served by this host.
  const url::Origin& origin() const { return origin_; }

  // Returns true if there are no receivers connected to this host.
  //
  // The BucketManager that owns this host is expected to destroy the host when
  // it is not serving any receivers.
  bool has_connected_receivers() const { return !receivers_.empty(); }

  // blink::mojom::BucketsManagerHost:
  void OpenBucket(const std::string& name,
                  blink::mojom::BucketPoliciesPtr policy,
                  OpenBucketCallback callback) override;
  void Keys(KeysCallback callback) override;
  void DeleteBucket(const std::string& name,
                    DeleteBucketCallback callback) override;

  void RemoveBucketHost(const std::string& name);

 private:
  // Called when a receiver in the receiver set is disconnected.
  void OnReceiverDisconnect();

  SEQUENCE_CHECKER(sequence_checker_);

  // Raw pointer is safe because BucketManager owns this BucketManagerHost, and
  // is therefore guaranteed to outlive it.
  BucketManager* const manager_;

  // The origin of the frame or worker connected to this BucketManagerHost.
  const url::Origin origin_;

  // TODO(ayui): Temporary map of buckets. This will eventually be stored in its
  // own database and connect to IndexDB, Cache, Blob Storage, etc.
  std::map<std::string, std::unique_ptr<BucketHost>> bucket_map_;

  // Add receivers for frames & workers for `origin_` associated with
  // the StoragePartition that owns `manager_`.
  mojo::ReceiverSet<blink::mojom::BucketManagerHost> receivers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BUCKETS_BUCKET_MANAGER_HOST_H_
