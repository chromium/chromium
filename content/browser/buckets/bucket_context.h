// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BUCKETS_BUCKET_CONTEXT_H_
#define CONTENT_BROWSER_BUCKETS_BUCKET_CONTEXT_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/thread_annotations.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/mojom/buckets/bucket_manager_host.mojom-forward.h"
#include "url/origin.h"

namespace content {

class BucketManager;
class BucketManagerHost;

// One instance of this exists per StoragePartition, and services multiple
// child processes/origins. An instance must only be used on the sequence
// it was created on.
//
// The reference counting is a consequence of the need to interact with the
// BucketManager on the I/O thread, and will probably disappear when
// BucketManager moves to the Storage Service.
class BucketContext : public base::RefCountedDeleteOnSequence<BucketContext> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  BucketContext();

  BucketContext(const BucketContext&) = delete;
  BucketContext& operator=(const BucketContext&) = delete;

  void Initialize(
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy);

  // Posts task on IO thread and calls BindBucketManagerHostOnIOThread to create
  // BucketManagerHost and bind blink::mojom::BucketManagerHost receiver.
  void BindBucketManagerHost(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver);

 private:
  friend class base::RefCountedDeleteOnSequence<BucketContext>;
  friend class base::DeleteHelper<BucketContext>;

  ~BucketContext();

  void InitializeOnIOThread(
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy);

  // Must be called on the IO thread. This will create a BucketManagerHost
  // and bind the blink::mojom::BucketManagerHost receiver.
  void BindBucketManagerHostOnIOThread(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver,
      mojo::ReportBadMessageCallback bad_message_callback);

  SEQUENCE_CHECKER(sequence_checker_);

#if DCHECK_IS_ON()
  // Only accessed on the UI thread.
  bool initialize_called_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
#endif  // DCHECK_IS_ON()

  // Must be accessed on the IO thread.
  std::unique_ptr<BucketManager> bucket_manager_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BUCKETS_BUCKET_CONTEXT_H_
