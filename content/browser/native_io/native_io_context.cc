// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_io/native_io_context.h"

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "content/browser/native_io/native_io_manager.h"
#include "content/public/browser/browser_thread.h"

namespace content {

NativeIOContext::NativeIOContext()
    : base::RefCountedDeleteOnSequence<NativeIOContext>(
          GetIOThreadTaskRunner({})) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

NativeIOContext::~NativeIOContext() {
  // The destructor must be called on the IO thread, because it
  // runs `native_io_manager_`s destructor, and the latter is only accessed on
  // the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void NativeIOContext::Initialize(
    const base::FilePath& profile_root,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  initialize_called_ = true;
#endif  // DCHECK_IS_ON()
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&NativeIOContext::InitializeOnIOThread, this,
                                profile_root, std::move(special_storage_policy),
                                std::move(quota_manager_proxy)));
}

void NativeIOContext::BindReceiver(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::NativeIOHost> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialize_called_) << __func__ << " called before Initialize()";
#endif  // DCHECK_IS_ON()
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&NativeIOContext::BindReceiverOnIOThread,
                                scoped_refptr<NativeIOContext>(this), origin,
                                std::move(receiver)));
}

void NativeIOContext::InitializeOnIOThread(
    const base::FilePath& profile_root,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!native_io_manager_) << __func__ << " called more than once";

  native_io_manager_ = std::make_unique<NativeIOManager>(
      profile_root, std::move(special_storage_policy),
      std::move(quota_manager_proxy));
}

void NativeIOContext::BindReceiverOnIOThread(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::NativeIOHost> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  native_io_manager_->BindReceiver(origin, std::move(receiver));
}

}  // namespace content
