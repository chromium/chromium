// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BUCKETS_BUCKET_MANAGER_HOST_H_
#define CONTENT_BROWSER_BUCKETS_BUCKET_MANAGER_HOST_H_

#include <map>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "content/browser/buckets/bucket_context.h"
#include "content/browser/buckets/bucket_host.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/buckets/bucket_manager_host.mojom.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"

namespace storage {
struct BucketInfo;
class QuotaManagerProxy;
}  // namespace storage

namespace content {

class BucketManager;
class StoragePartitionImpl;

// Implements the Storage Buckets API for a single StorageKey.
//
// BucketManager owns all BucketManagerHost instances associated with
// a StorageParititon. A new instance is created for every `StorageKey`.
// Instances are destroyed when all their corresponding mojo connection are
// closed, or when BucketManager is destroyed.
class BucketManagerHost : public blink::mojom::BucketManagerHost {
 public:
  explicit BucketManagerHost(BucketManager* manager,
                             const blink::StorageKey& storage_key);
  ~BucketManagerHost() override;

  BucketManagerHost(const BucketManagerHost&) = delete;
  BucketManagerHost& operator=(const BucketManagerHost&) = delete;

  // Binds |receiver| to the BucketManagerHost. The |receiver| must belong to
  // the frame or worker from this host's `StorageKey`. `context` is used to
  // determine permissions for the receiver.
  void BindReceiver(
      mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver,
      base::WeakPtr<BucketContext> context);

  // The `StorageKey` served by this host.
  const blink::StorageKey& storage_key() const { return storage_key_; }

  // Returns true if there are no receivers connected to this host.
  //
  // The BucketManager that owns this host is expected to destroy the host when
  // it is not serving any receivers.
  bool has_connected_receivers() const { return !receivers_.empty(); }

  // blink::mojom::BucketsManagerHost:
  void OpenBucket(const std::string& name,
                  blink::mojom::BucketPoliciesPtr policy,
                  OpenBucketCallback callback) override;
  // Gets the bucket with the given name. Doesn't create the bucket if it
  // doesn't exist.
  void GetBucketForDevtools(
      const std::string& name,
      mojo::PendingReceiver<blink::mojom::BucketHost> receiver) override;
  void Keys(KeysCallback callback) override;
  void DeleteBucket(const std::string& name,
                    DeleteBucketCallback callback) override;

  void RemoveBucketHost(storage::BucketId id);

  StoragePartitionImpl* GetStoragePartition();
  storage::QuotaManagerProxy* GetQuotaManagerProxy();

 private:
  // Called when a receiver in the receiver set is disconnected.
  void OnReceiverDisconnect();

  void DidGetBucket(base::WeakPtr<BucketContext> bucket_context,
                    OpenBucketCallback callback,
                    storage::QuotaErrorOr<storage::BucketInfo> result);

  void DidGetBuckets(
      KeysCallback callback,
      storage::QuotaErrorOr<std::set<storage::BucketInfo>> result);

  void DidDeleteBucket(const std::string& bucket_name,
                       DeleteBucketCallback callback,
                       blink::mojom::QuotaStatusCode status);

  void DidGetBucketForDevtools(
      base::WeakPtr<BucketContext> bucket_context,
      mojo::PendingReceiver<blink::mojom::BucketHost> receiver,
      storage::QuotaErrorOr<storage::BucketInfo> result);

  SEQUENCE_CHECKER(sequence_checker_);

  // Raw pointer is safe because BucketManager owns this BucketManagerHost, and
  // is therefore guaranteed to outlive it.
  const raw_ptr<BucketManager> manager_;

  // The `StorageKey` of the frame or worker connected to this
  // BucketManagerHost.
  const blink::StorageKey storage_key_;

  // Map of currently open/used buckets. The lifetime matches that of the remote
  // which means they can outlive the bucket's data.
  std::map<storage::BucketId, std::unique_ptr<BucketHost>> bucket_map_;

  // Add receivers for frames & workers for `storage_key_` associated with
  // the StoragePartition that owns `manager_`.
  mojo::ReceiverSet<blink::mojom::BucketManagerHost,
                    base::WeakPtr<BucketContext>>
      receivers_;

  base::WeakPtrFactory<BucketManagerHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_BUCKETS_BUCKET_MANAGER_HOST_H_
