// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/buckets/bucket_context.h"

#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/buckets/bucket_manager.h"
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

void BucketContext::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(!initialize_called_) << __func__ << " called twice";
  initialize_called_ = true;
#endif  // DCHECK_IS_ON()
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&BucketContext::InitializeOnIOThread, this));
}

void BucketContext::BindBucketManagerHost(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialize_called_) << __func__ << " called before Initialize()";
#endif  // DCHECK_IS_ON()

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&BucketContext::BindBucketManagerHostOnIOThread,
                     scoped_refptr<BucketContext>(this), origin,
                     std::move(receiver), mojo::GetBadMessageCallback()));
}

void BucketContext::InitializeOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!bucket_manager_) << __func__ << "  called more than once";

  bucket_manager_ = std::make_unique<BucketManager>();
}

void BucketContext::BindBucketManagerHostOnIOThread(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver,
    mojo::ReportBadMessageCallback bad_message_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  bucket_manager_->BindReceiver(origin, std::move(receiver),
                                std::move(bad_message_callback));
}

}  // namespace content
