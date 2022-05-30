// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BUCKETS_BUCKET_HOST_H_
#define CONTENT_BROWSER_BUCKETS_BUCKET_HOST_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/buckets/bucket_manager_host.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace storage {
class QuotaManagerProxy;
}  // namespace storage

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
             const storage::BucketInfo& bucket_info);
  ~BucketHost() override;

  BucketHost(const BucketHost&) = delete;
  BucketHost& operator=(const BucketHost&) = delete;

  // This callback returns a permission state for a given type. Effectively, it
  // wraps calls into `PermissionController`, storing relevant parameters. There
  // should be a `PermissionDecisionCallback` for each bound mojo remote, hence
  // `permission_decider_map_`.
  using PermissionDecisionCallback =
      base::RepeatingCallback<blink::mojom::PermissionStatus(
          blink::PermissionType)>;

  // Create mojo data pipe and return remote to pass to the renderer
  // for the StorageBucket object.
  mojo::PendingRemote<blink::mojom::BucketHost> CreateStorageBucketBinding(
      const PermissionDecisionCallback& permission_decision);

  // blink::mojom::BucketHost
  void Persist(PersistCallback callback) override;
  void Persisted(PersistedCallback callback) override;
  void Estimate(EstimateCallback callback) override;
  void Durability(DurabilityCallback callback) override;
  void SetExpires(base::Time expires, SetExpiresCallback callback) override;
  void Expires(ExpiresCallback callback) override;

 private:
  void OnReceiverDisconnected();

  storage::QuotaManagerProxy* GetQuotaManagerProxy();

  void DidUpdateBucket(base::OnceCallback<void(bool)> callback,
                       storage::QuotaErrorOr<storage::BucketInfo> bucket_info);

  void DidGetUsageAndQuota(EstimateCallback callback,
                           blink::mojom::QuotaStatusCode code,
                           int64_t usage,
                           int64_t quota);

  SEQUENCE_CHECKER(sequence_checker_);

  // Raw pointer use is safe here because BucketManagerHost owns this
  // BucketHost.
  raw_ptr<BucketManagerHost> bucket_manager_host_;

  // Holds the latest snapshot from the database.
  storage::BucketInfo bucket_info_;

  mojo::ReceiverSet<blink::mojom::BucketHost> receivers_;

  // A mapping from an integer which identifies the mojo receiver (found in
  // `receivers_`) to a callback which decides whether that receiver has
  // permission to use certain web features (namely, DURABLE_STORAGE).
  std::map<mojo::ReceiverId, PermissionDecisionCallback>
      permission_decider_map_;

  base::WeakPtrFactory<BucketHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_BUCKETS_BUCKET_HOST_H_
