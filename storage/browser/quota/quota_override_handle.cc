// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_override_handle.h"

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

QuotaOverrideHandle::QuotaOverrideHandle(
    scoped_refptr<QuotaManagerProxy> quota_manager_proxy)
    : quota_manager_proxy_(std::move(quota_manager_proxy)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  quota_manager_proxy_->GetOverrideHandleId(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&QuotaOverrideHandle::DidGetOverrideHandleId,
                     weak_ptr_factory_.GetWeakPtr()));
}

QuotaOverrideHandle::~QuotaOverrideHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (id_.has_value()) {
    quota_manager_proxy_->WithdrawOverridesForHandle(id_.value());
  }
}

void QuotaOverrideHandle::OverrideQuotaForStorageKey(
    const blink::StorageKey& storage_key,
    std::optional<int64_t> quota_size,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!id_.has_value()) {
    // base::Unretained is safe here because this class owns the callback queue
    // and the callbacks within, so it's guaranteed to be alive when the
    // callback is dispatched.
    override_callback_queue_.push_back(base::BindOnce(
        &QuotaOverrideHandle::OverrideQuotaForStorageKey,
        base::Unretained(this), storage_key, quota_size, std::move(callback)));
    return;
  }
  quota_manager_proxy_->OverrideQuotaForStorageKey(
      id_.value(), storage_key, quota_size,
      base::SequencedTaskRunner::GetCurrentDefault(), std::move(callback));
}

void QuotaOverrideHandle::DidGetOverrideHandleId(int id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!id_.has_value());
  id_ = std::make_optional(id);

  for (auto& callback : override_callback_queue_) {
    std::move(callback).Run();
  }
  override_callback_queue_.clear();
}

}  // namespace storage
