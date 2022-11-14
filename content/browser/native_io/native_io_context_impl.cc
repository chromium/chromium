// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_io/native_io_context_impl.h"

#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "content/browser/native_io/native_io_manager.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif  // BUILDFLAG(IS_MAC)

namespace content {

NativeIOContextImpl::NativeIOContextImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

NativeIOContextImpl::~NativeIOContextImpl() {
  // The destructor must be called on the IO thread, because it
  // runs `native_io_manager_`s destructor, and the latter is only accessed on
  // the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void NativeIOContextImpl::Initialize(
    const base::FilePath& profile_root,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  initialize_called_ = true;
#endif  // DCHECK_IS_ON()
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeIOContextImpl::InitializeOnIOThread, this,
                     profile_root, std::move(special_storage_policy),
                     std::move(quota_manager_proxy)));
}

void NativeIOContextImpl::BindReceiver(
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<blink::mojom::NativeIOHost> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialize_called_) << __func__ << " called before Initialize()";
#endif  // DCHECK_IS_ON()
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeIOContextImpl::BindReceiverOnIOThread,
                     scoped_refptr<NativeIOContextImpl>(this), storage_key,
                     std::move(receiver), mojo::GetBadMessageCallback()));
}

void NativeIOContextImpl::DeleteStorageKeyData(
    const blink::StorageKey& storage_key,
    storage::mojom::QuotaClient::DeleteBucketDataCallback success_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialize_called_) << __func__ << " called before Initialize()";
#endif  // DCHECK_IS_ON()
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &NativeIOContextImpl::DeleteStorageKeyDataOnIOThread,
          scoped_refptr<NativeIOContextImpl>(this), std::move(storage_key),
          base::BindOnce(
              [](scoped_refptr<base::SequencedTaskRunner> task_runner,
                 storage::mojom::QuotaClient::DeleteBucketDataCallback callback,
                 blink::mojom::QuotaStatusCode result) {
                task_runner->PostTask(
                    FROM_HERE, base::BindOnce(std::move(callback), result));
              },
              base::SequencedTaskRunner::GetCurrentDefault(),
              std::move(success_callback))));
}

void NativeIOContextImpl::GetStorageKeyUsageMap(
    base::OnceCallback<void(const std::map<blink::StorageKey, int64_t>&)>
        success_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialize_called_) << __func__ << " called before Initialize()";
#endif  // DCHECK_IS_ON()
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &NativeIOContextImpl::GetStorageKeyUsageMapOnIOThread,
          scoped_refptr<NativeIOContextImpl>(this),
          base::BindOnce(
              [](scoped_refptr<base::SequencedTaskRunner> task_runner,
                 base::OnceCallback<void(
                     const std::map<blink::StorageKey, int64_t>&)> callback,
                 const std::map<blink::StorageKey, int64_t>& result) {
                task_runner->PostTask(
                    FROM_HERE, base::BindOnce(std::move(callback), result));
              },
              base::SequencedTaskRunner::GetCurrentDefault(),
              std::move(success_callback))));
}

void NativeIOContextImpl::InitializeOnIOThread(
    const base::FilePath& profile_root,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!native_io_manager_) << __func__ << " called more than once";

  native_io_manager_ = std::make_unique<NativeIOManager>(
      profile_root,
#if BUILDFLAG(IS_MAC)
      !base::mac::IsAtLeastOS10_15(),
#endif  // BUILDFLAG(IS_MAC)
      std::move(special_storage_policy), std::move(quota_manager_proxy));
}

void NativeIOContextImpl::BindReceiverOnIOThread(
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<blink::mojom::NativeIOHost> receiver,
    mojo::ReportBadMessageCallback bad_message_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  native_io_manager_->BindReceiver(storage_key, std::move(receiver),
                                   std::move(bad_message_callback));
}

void NativeIOContextImpl::DeleteStorageKeyDataOnIOThread(
    const blink::StorageKey& storage_key,
    storage::mojom::QuotaClient::DeleteBucketDataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  native_io_manager_->DeleteStorageKeyData(storage_key, std::move(callback));
}

void NativeIOContextImpl::GetStorageKeyUsageMapOnIOThread(
    base::OnceCallback<void(const std::map<blink::StorageKey, int64_t>&)>
        callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  native_io_manager_->GetStorageKeyUsageMap(std::move(callback));
}

}  // namespace content
