// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/ca_transaction_gpu_coordinator.h"

#include "base/cancelable_callback.h"
#include "base/functional/bind.h"
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
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
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
  CHECK(!host_, base::NotFatalUntil::M159);
  CHECK(!registered_as_observer_, base::NotFatalUntil::M159);
}

void CATransactionGPUCoordinator::HostWillBeDestroyed() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  ui::WindowResizeHelperMac::Get()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CATransactionGPUCoordinator::RemovePostCommitObserverOnUIThread,
          this));
  host_ = nullptr;
}

void CATransactionGPUCoordinator::AddPostCommitObserverOnUIThread() {
  CHECK(!registered_as_observer_, base::NotFatalUntil::M159);
  ui::CATransactionCoordinator::Get().AddPostCommitObserver(this);
  registered_as_observer_ = true;
}

void CATransactionGPUCoordinator::RemovePostCommitObserverOnUIThread() {
  CHECK(registered_as_observer_, base::NotFatalUntil::M159);
  ui::CATransactionCoordinator::Get().RemovePostCommitObserver(this);
  registered_as_observer_ = false;
}

void CATransactionGPUCoordinator::OnActivateForTransaction() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  if (host_)
    host_->gpu_service()->BeginCATransaction();
}

void CATransactionGPUCoordinator::OnEnterPostCommit() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);

  // If HostWillBeDestroyed() is called during a commit, pending_commit_count_
  // may be left non-zero. That's fine as long as this instance is destroyed
  // (and removed from the list of post-commit observers) soon after.
  pending_commit_count_++;

  if (host_)
    host_->gpu_service()->CommitCATransaction(base::BindOnce(
        &CATransactionGPUCoordinator::OnCommitCompletedOnProcessThread, this));
}

bool CATransactionGPUCoordinator::ShouldWaitInPostCommit() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  return pending_commit_count_ > 0;
}

void CATransactionGPUCoordinator::OnCommitCompletedOnProcessThread() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  ui::WindowResizeHelperMac::Get()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CATransactionGPUCoordinator::OnCommitCompletedOnUI,
                     this));
}

void CATransactionGPUCoordinator::OnCommitCompletedOnUI() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  pending_commit_count_--;
}

}  // namespace content
