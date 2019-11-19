// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cookie_store/cookie_store_context.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"

namespace content {

CookieStoreContext::CookieStoreContext()
    : base::RefCountedDeleteOnSequence<CookieStoreContext>(
          base::CreateSingleThreadTaskRunner(
              {ServiceWorkerContext::GetCoreThreadId()})) {}

CookieStoreContext::~CookieStoreContext() {
  // The destructor must be called on the service worker core thread, because it
  // runs cookie_store_manager_'s destructor, and the latter is only accessed on
  // the core thread.
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
}

void CookieStoreContext::Initialize(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    base::OnceCallback<void(bool)> success_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(!initialize_called_) << __func__ << " called twice";
  initialize_called_ = true;
#endif  // DCHECK_IS_ON()

  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(
          &CookieStoreContext::InitializeOnCoreThread, this,
          std::move(service_worker_context),
          base::BindOnce(
              [](scoped_refptr<base::SequencedTaskRunner> task_runner,
                 base::OnceCallback<void(bool)> callback, bool result) {
                task_runner->PostTask(
                    FROM_HERE, base::BindOnce(std::move(callback), result));
              },
              base::SequencedTaskRunnerHandle::Get(),
              std::move(success_callback))));
}

void CookieStoreContext::ListenToCookieChanges(
    ::network::mojom::NetworkContext* network_context,
    base::OnceCallback<void(bool)> success_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialize_called_) << __func__ << " called before Initialize()";
#endif  // DCHECK_IS_ON()

  mojo::PendingRemote<::network::mojom::CookieManager> cookie_manager_remote;
  network_context->GetCookieManager(
      cookie_manager_remote.InitWithNewPipeAndPassReceiver());

  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(
          &CookieStoreContext::ListenToCookieChangesOnCoreThread, this,
          std::move(cookie_manager_remote),
          base::BindOnce(
              [](scoped_refptr<base::SequencedTaskRunner> task_runner,
                 base::OnceCallback<void(bool)> callback, bool result) {
                task_runner->PostTask(
                    FROM_HERE, base::BindOnce(std::move(callback), result));
              },
              base::SequencedTaskRunnerHandle::Get(),
              std::move(success_callback))));
}

// static
void CookieStoreContext::CreateServiceForFrame(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::CookieStore> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(render_frame_host);
  RenderProcessHost* render_process_host = render_frame_host->GetProcess();
  DCHECK(render_process_host);

  StoragePartitionImpl* storage_partition = static_cast<StoragePartitionImpl*>(
      render_process_host->GetStoragePartition());
  storage_partition->GetCookieStoreContext()->CreateServiceForTesting(
      render_frame_host->GetLastCommittedOrigin(), std::move(receiver));
}

// static
void CookieStoreContext::CreateServiceForWorker(
    const ServiceWorkerVersionInfo& info,
    mojo::PendingReceiver<blink::mojom::CookieStore> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(info.process_id);
  if (render_process_host == nullptr)
    return;

  StoragePartitionImpl* storage_partition = static_cast<StoragePartitionImpl*>(
      render_process_host->GetStoragePartition());
  storage_partition->GetCookieStoreContext()->CreateServiceForTesting(
      info.script_origin, std::move(receiver));
}

void CookieStoreContext::CreateServiceForTesting(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::CookieStore> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialize_called_) << __func__ << " called before Initialize()";
#endif  // DCHECK_IS_ON()
  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&CookieStoreContext::CreateServiceOnCoreThread, this,
                     origin, std::move(receiver)));
}

void CookieStoreContext::InitializeOnCoreThread(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    base::OnceCallback<void(bool)> success_callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(!cookie_store_manager_) << __func__ << " called more than once";

  cookie_store_manager_ =
      std::make_unique<CookieStoreManager>(std::move(service_worker_context));
  cookie_store_manager_->LoadAllSubscriptions(std::move(success_callback));
}

void CookieStoreContext::ListenToCookieChangesOnCoreThread(
    mojo::PendingRemote<::network::mojom::CookieManager> cookie_manager_remote,
    base::OnceCallback<void(bool)> success_callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(cookie_store_manager_);

  cookie_store_manager_->ListenToCookieChanges(std::move(cookie_manager_remote),
                                               std::move(success_callback));
}

void CookieStoreContext::CreateServiceOnCoreThread(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::CookieStore> receiver) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(cookie_store_manager_);

  cookie_store_manager_->CreateService(std::move(receiver), origin);
}

}  // namespace content
