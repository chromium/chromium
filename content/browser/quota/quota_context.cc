// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/quota/quota_context.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/quota/quota_change_dispatcher.h"
#include "content/browser/quota/quota_manager_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_settings.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

QuotaContext::QuotaContext(
    bool is_incognito,
    const base::FilePath& profile_path,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    storage::GetQuotaSettingsFunc get_settings_function)
    : base::RefCountedDeleteOnSequence<QuotaContext>(GetIOThreadTaskRunner({})),
      io_thread_(GetIOThreadTaskRunner({})),
      quota_change_dispatcher_(
          base::MakeRefCounted<QuotaChangeDispatcher>(io_thread_)),
      quota_manager_(base::MakeRefCounted<storage::QuotaManager>(
          is_incognito,
          profile_path,
          io_thread_,
          base::BindRepeating(&QuotaChangeDispatcher::MaybeDispatchEvents,
                              quota_change_dispatcher_),
          std::move(special_storage_policy),
          std::move(get_settings_function))) {}

void QuotaContext::BindQuotaManagerHost(
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<blink::mojom::QuotaManagerHost> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  io_thread_->PostTask(
      FROM_HERE, base::BindOnce(&QuotaContext::BindQuotaManagerHostOnIOThread,
                                this, storage_key, std::move(receiver)));
}

QuotaContext::~QuotaContext() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void QuotaContext::BindQuotaManagerHostOnIOThread(
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<blink::mojom::QuotaManagerHost> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The quota manager currently runs on the I/O thread.
  auto host = std::make_unique<QuotaManagerHost>(
      storage_key, quota_manager_.get(), quota_change_dispatcher_);
  auto* host_ptr = host.get();
  receivers_.Add(host_ptr, std::move(receiver), std::move(host));
}

void QuotaContext::OverrideQuotaManagerForTesting(
    scoped_refptr<storage::QuotaManager> new_manager) {
  quota_manager_ = std::move(new_manager);
}

}  // namespace content
