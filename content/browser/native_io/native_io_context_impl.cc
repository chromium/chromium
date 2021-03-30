// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_io/native_io_context_impl.h"

#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/native_io/native_io_manager.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_MAC)
#include "base/mac/mac_util.h"
#endif  // defined(OS_MAC)

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
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::NativeIOHost> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialize_called_) << __func__ << " called before Initialize()";
#endif  // DCHECK_IS_ON()
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeIOContextImpl::BindReceiverOnIOThread,
                     scoped_refptr<NativeIOContextImpl>(this), origin,
                     std::move(receiver), mojo::GetBadMessageCallback()));
}

void NativeIOContextImpl::DeleteOriginData(
    const url::Origin& origin,
    storage::mojom::QuotaClient::DeleteOriginDataCallback success_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialize_called_) << __func__ << " called before Initialize()";
#endif  // DCHECK_IS_ON()
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &NativeIOContextImpl::DeleteOriginDataOnIOThread,
          scoped_refptr<NativeIOContextImpl>(this), std::move(origin),
          base::BindOnce(
              [](scoped_refptr<base::SequencedTaskRunner> task_runner,
                 storage::mojom::QuotaClient::DeleteOriginDataCallback callback,
                 blink::mojom::QuotaStatusCode result) {
                task_runner->PostTask(
                    FROM_HERE, base::BindOnce(std::move(callback), result));
              },
              base::SequencedTaskRunnerHandle::Get(),
              std::move(success_callback))));
}

void NativeIOContextImpl::GetOriginUsageMap(
    base::OnceCallback<void(const std::map<url::Origin, int64_t>)>
        success_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialize_called_) << __func__ << " called before Initialize()";
#endif  // DCHECK_IS_ON()
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &NativeIOContextImpl::GetOriginUsageMapOnIOThread,
          scoped_refptr<NativeIOContextImpl>(this),
          base::BindOnce(
              [](scoped_refptr<base::SequencedTaskRunner> task_runner,
                 base::OnceCallback<void(const std::map<url::Origin, int64_t>)>
                     callback,
                 std::map<url::Origin, int64_t> result) {
                task_runner->PostTask(
                    FROM_HERE, base::BindOnce(std::move(callback), result));
              },
              base::SequencedTaskRunnerHandle::Get(),
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
#if defined(OS_MAC)
      !base::mac::IsAtLeastOS10_15(),
#endif  // defined(OS_MAC)
      std::move(special_storage_policy), std::move(quota_manager_proxy));
}

void NativeIOContextImpl::BindReceiverOnIOThread(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::NativeIOHost> receiver,
    mojo::ReportBadMessageCallback bad_message_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  native_io_manager_->BindReceiver(origin, std::move(receiver),
                                   std::move(bad_message_callback));
}

void NativeIOContextImpl::DeleteOriginDataOnIOThread(
    const url::Origin& origin,
    storage::mojom::QuotaClient::DeleteOriginDataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  native_io_manager_->DeleteOriginData(origin, std::move(callback));
}

void NativeIOContextImpl::GetOriginUsageMapOnIOThread(
    base::OnceCallback<void(const std::map<url::Origin, int64_t>)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::map<url::Origin, int64_t> result = std::map<url::Origin, int64_t>();

  native_io_manager_->GetOriginUsageMap(std::move(callback));
}

}  // namespace content
