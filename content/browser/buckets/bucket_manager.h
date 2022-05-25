// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BUCKETS_BUCKET_MANAGER_H_
#define CONTENT_BROWSER_BUCKETS_BUCKET_MANAGER_H_

#include <map>
#include <memory>

#include "base/gtest_prod_util.h"
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

// One instance of BucketManager exists per StoragePartition, and is created and
// owned by the `RenderProcessHostImpl`. This class creates and destroys
// BucketManagerHost instances per origin as a centeralized host for an origin's
// I/O operations so all frames & workers can be notified when a bucket is
// deleted, and have them mark their Bucket instance as closed.
class CONTENT_EXPORT BucketManager {
 public:
  explicit BucketManager(
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy);
  ~BucketManager();

  BucketManager(const BucketManager&) = delete;
  BucketManager& operator=(const BucketManager&) = delete;

  // Binds `receiver` to the BucketManagerHost for the last committed origin in
  // the RenderFrameHost referenced by  `render_frame_host_id`.
  void BindReceiverForRenderFrame(
      const content::GlobalRenderFrameHostId& render_frame_host_id,
      mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver,
      mojo::ReportBadMessageCallback bad_message_callback);

  // Binds `receiver` to the BucketManagerHost for `origin`. `render_process_id`
  // represents the service worker that is connecting to the bucket service.
  void BindReceiverForWorker(
      int render_process_id,
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
  friend class BucketManagerHostTest;
  FRIEND_TEST_ALL_PREFIXES(BucketManagerHostTest, OpenBucketValidateName);
  FRIEND_TEST_ALL_PREFIXES(BucketManagerHostTest, PermissionCheck);

  SEQUENCE_CHECKER(sequence_checker_);

  void DoBindReceiver(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver,
      const BucketHost::PermissionDecisionCallback& permission_decision,
      mojo::ReportBadMessageCallback bad_message_callback);

  // Owns all instances of BucketManagerHost associated with a StoragePartition.
  std::map<url::Origin, std::unique_ptr<BucketManagerHost>> hosts_
      GUARDED_BY_CONTEXT(sequence_checker_);

  const scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BUCKETS_BUCKET_MANAGER_H_
