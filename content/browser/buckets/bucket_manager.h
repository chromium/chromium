// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BUCKETS_BUCKET_MANAGER_H_
#define CONTENT_BROWSER_BUCKETS_BUCKET_MANAGER_H_

#include <map>
#include <memory>

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/pass_key.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/mojom/buckets/bucket_manager_host.mojom-forward.h"
#include "url/origin.h"

namespace content {

class BucketManagerHost;

// One instance of BucketManager exists per StoragePartition, and is created and
// owned by the BucketContext. This class creates and destroys BucketManagerHost
// instances per origin as a centeralized host for an origin's I/O operations
// so all frames & workers can be notified when a bucket is deleted, and have
// them mark their Bucket instance as closed.
class CONTENT_EXPORT BucketManager {
 public:
  explicit BucketManager(
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy);
  ~BucketManager();

  BucketManager(const BucketManager&) = delete;
  BucketManager& operator=(const BucketManager&) = delete;

  // Binds `receiver` to the BucketManagerHost for `origin`.
  void BindReceiver(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver,
      mojo::ReportBadMessageCallback bad_message_callback);

  // This method may only be called on the BucketManagerHost sequence.
  void OnHostReceiverDisconnect(BucketManagerHost* host,
                                base::PassKey<BucketManagerHost>);

  const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy() const {
    return quota_manager_proxy_;
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Owns all instances of BucketManagerHost associated with a StoragePartition.
  std::map<url::Origin, std::unique_ptr<BucketManagerHost>> hosts_
      GUARDED_BY_CONTEXT(sequence_checker_);

  const scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BUCKETS_BUCKET_MANAGER_H_
