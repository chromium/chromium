// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BUCKETS_BUCKET_MANAGER_H_
#define CONTENT_BROWSER_BUCKETS_BUCKET_MANAGER_H_

#include <map>
#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/pass_key.h"
#include "content/browser/buckets/bucket_manager_host.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/mojom/buckets/bucket_manager_host.mojom-forward.h"
#include "url/origin.h"

namespace content {

class BucketContext;
class StoragePartitionImpl;

// One instance of BucketManager exists per StoragePartition, and is created and
// owned by the `StoragePartitionImpl`. This class creates and destroys
// BucketManagerHost instances per origin as a centeralized host for an origin's
// I/O operations so all frames & workers can be notified when a bucket is
// deleted, and have them mark their Bucket instance as closed.
class CONTENT_EXPORT BucketManager {
 public:
  explicit BucketManager(StoragePartitionImpl* storage_partition);
  ~BucketManager();

  BucketManager(const BucketManager&) = delete;
  BucketManager& operator=(const BucketManager&) = delete;

  void BindReceiver(
      base::WeakPtr<BucketContext> context,
      mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver,
      mojo::ReportBadMessageCallback bad_message_callback);

  // This method may only be called on the BucketManagerHost sequence.
  void OnHostReceiverDisconnect(BucketManagerHost* host,
                                base::PassKey<BucketManagerHost>);

  StoragePartitionImpl* storage_partition() { return storage_partition_; }

 private:
  friend class BucketManagerHostTest;
  FRIEND_TEST_ALL_PREFIXES(BucketManagerHostTest, OpenBucketValidateName);
  FRIEND_TEST_ALL_PREFIXES(BucketManagerHostTest, PermissionCheck);

  SEQUENCE_CHECKER(sequence_checker_);

  void DoBindReceiver(
      const blink::StorageKey& storage_key,
      const BucketContext& bucket_context,
      mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver,
      mojo::ReportBadMessageCallback bad_message_callback);

  // Owns all instances of BucketManagerHost associated with a StoragePartition.
  std::map<blink::StorageKey, std::unique_ptr<BucketManagerHost>> hosts_
      GUARDED_BY_CONTEXT(sequence_checker_);

  raw_ptr<StoragePartitionImpl> storage_partition_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BUCKETS_BUCKET_MANAGER_H_
