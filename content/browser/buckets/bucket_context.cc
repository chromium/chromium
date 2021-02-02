// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/buckets/bucket_context.h"

#include "content/browser/buckets/bucket_manager_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {

BucketContext::BucketContext()
    : base::RefCountedDeleteOnSequence<BucketContext>(
          GetIOThreadTaskRunner({})) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

BucketContext::~BucketContext() {
  // The destructor must be called on IO thread, because it runs
  // BucketManagerHost's destructor which can only be access by the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void BucketContext::BindBucketManagerHost(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&BucketContext::BindBucketManagerHostOnIOThread,
                                scoped_refptr<BucketContext>(this), origin,
                                std::move(receiver)));
}

void BucketContext::BindBucketManagerHostOnIOThread(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto bucket_manager_host = std::make_unique<BucketManagerHost>(origin);
  auto* bucket_manager_host_ptr = bucket_manager_host.get();
  receivers_.Add(bucket_manager_host_ptr, std::move(receiver),
                 std::move(bucket_manager_host));
}

}  // namespace content
