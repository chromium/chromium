// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BUCKETS_BUCKET_CONTEXT_H_
#define CONTENT_BROWSER_BUCKETS_BUCKET_CONTEXT_H_

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/buckets/bucket_manager_host.mojom-forward.h"
#include "url/origin.h"

namespace content {

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

  // Posts task on IO thread and calls BindBucketManagerHostOnIOThread to create
  // BucketManagerHost and bind blink::mojom::BucketManagerHost receiver.
  void BindBucketManagerHost(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver);

 private:
  friend class base::RefCountedDeleteOnSequence<BucketContext>;
  friend class base::DeleteHelper<BucketContext>;

  ~BucketContext();

  // Must be called on the IO thread. This will create a BucketManagerHost
  // and bind the blink::mojom::BucketManagerHost receiver.
  void BindBucketManagerHostOnIOThread(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver);

  mojo::ReceiverSet<blink::mojom::BucketManagerHost,
                    std::unique_ptr<BucketManagerHost>>
      receivers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BUCKETS_BUCKET_CONTEXT_H_
