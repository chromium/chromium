// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_CA_TRANSACTION_GPU_COORDINATOR_H_
#define CONTENT_BROWSER_GPU_CA_TRANSACTION_GPU_COORDINATOR_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/accelerated_widget_mac/ca_transaction_observer.h"

namespace content {

class GpuProcessHost;

// Synchronizes CATransaction commits between the browser and GPU processes.
class CATransactionGPUCoordinator
    : public ui::CATransactionCoordinator::PostCommitObserver {
 public:
  static scoped_refptr<CATransactionGPUCoordinator> Create(
      GpuProcessHost* host);
  void HostWillBeDestroyed();

 private:
  friend class base::RefCountedThreadSafe<CATransactionGPUCoordinator>;
  CATransactionGPUCoordinator(GpuProcessHost* host);
  ~CATransactionGPUCoordinator() override;

  // ui::CATransactionObserver implementation
  void OnActivateForTransaction() override;
  void OnEnterPostCommit() override;
  bool ShouldWaitInPostCommit() override;

  void AddPostCommitObserverOnUIThread();
  void RemovePostCommitObserverOnUIThread();

  void OnActivateForTransactionOnProcessThread();
  void OnEnterPostCommitOnProcessThread();
  void OnCommitCompletedOnProcessThread();
  void OnCommitCompletedOnUI();

  // The GpuProcessHost to use to initiate GPU-side CATransactions. This is only
  // to be accessed on the IO thread.
  GpuProcessHost* host_ = nullptr;

  // The number CATransactions that have not yet completed. This is only to be
  // accessed on the UI thread.
  int pending_commit_count_ = 0;

  // Egregious state tracking to debug https://crbug.com/871430
  bool registered_as_observer_ = false;

  DISALLOW_COPY_AND_ASSIGN(CATransactionGPUCoordinator);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_CA_TRANSACTION_GPU_COORDINATOR_H_
