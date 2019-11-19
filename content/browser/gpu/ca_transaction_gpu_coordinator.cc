// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/ca_transaction_gpu_coordinator.h"

#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/task/post_task.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"
#include "ui/accelerated_widget_mac/ca_transaction_observer.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"

namespace content {

// static
scoped_refptr<CATransactionGPUCoordinator> CATransactionGPUCoordinator::Create(
    GpuProcessHost* host) {
  scoped_refptr<CATransactionGPUCoordinator> result(
      new CATransactionGPUCoordinator(host));
  // Avoid modifying result's refcount in the constructor by performing this
  // PostTask afterward.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ui::WindowResizeHelperMac::Get()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CATransactionGPUCoordinator::AddPostCommitObserverOnUIThread,
          result));
  return result;
}

CATransactionGPUCoordinator::CATransactionGPUCoordinator(GpuProcessHost* host)
    : host_(host) {}

CATransactionGPUCoordinator::~CATransactionGPUCoordinator() {
  DCHECK(!host_);
  DCHECK(!registered_as_observer_);
}

void CATransactionGPUCoordinator::HostWillBeDestroyed() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ui::WindowResizeHelperMac::Get()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CATransactionGPUCoordinator::RemovePostCommitObserverOnUIThread,
          this));
  host_ = nullptr;
}

void CATransactionGPUCoordinator::AddPostCommitObserverOnUIThread() {
  DCHECK(!registered_as_observer_);
  ui::CATransactionCoordinator::Get().AddPostCommitObserver(this);
  registered_as_observer_ = true;
}

void CATransactionGPUCoordinator::RemovePostCommitObserverOnUIThread() {
  DCHECK(registered_as_observer_);
  ui::CATransactionCoordinator::Get().RemovePostCommitObserver(this);
  registered_as_observer_ = false;
}

void CATransactionGPUCoordinator::OnActivateForTransaction() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&CATransactionGPUCoordinator::OnActivateForTransactionOnIO,
                     this));
}

void CATransactionGPUCoordinator::OnEnterPostCommit() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If HostWillBeDestroyed() is called during a commit, pending_commit_count_
  // may be left non-zero. That's fine as long as this instance is destroyed
  // (and removed from the list of post-commit observers) soon after.
  pending_commit_count_++;

  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&CATransactionGPUCoordinator::OnEnterPostCommitOnIO,
                     this));
}

bool CATransactionGPUCoordinator::ShouldWaitInPostCommit() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return pending_commit_count_ > 0;
}

void CATransactionGPUCoordinator::OnActivateForTransactionOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (host_)
    host_->gpu_service()->BeginCATransaction();
}

void CATransactionGPUCoordinator::OnEnterPostCommitOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (host_)
    host_->gpu_service()->CommitCATransaction(base::BindOnce(
        &CATransactionGPUCoordinator::OnCommitCompletedOnIO, this));
}

void CATransactionGPUCoordinator::OnCommitCompletedOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ui::WindowResizeHelperMac::Get()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CATransactionGPUCoordinator::OnCommitCompletedOnUI,
                     this));
}

void CATransactionGPUCoordinator::OnCommitCompletedOnUI() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  pending_commit_count_--;
}

}  // namespace content
