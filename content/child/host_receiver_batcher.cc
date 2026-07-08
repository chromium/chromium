// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/host_receiver_batcher.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"

namespace content {

HostReceiverBatcher::HostReceiverBatcher(
    SendCallback send_callback,
    scoped_refptr<base::SequencedTaskRunner> flush_runner)
    : send_callback_(std::move(send_callback)),
      flush_runner_(std::move(flush_runner)) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

HostReceiverBatcher::~HostReceiverBatcher() = default;

void HostReceiverBatcher::AddReceiver(mojo::GenericPendingReceiver receiver) {
  bool needs_flush = false;
  {
    base::AutoLock lock(lock_);
    pending_.push_back(std::move(receiver));
    if (!flush_scheduled_) {
      flush_scheduled_ = true;
      needs_flush = true;
    }
  }
  // Post with the lock released so the call out to the scheduler can't re-enter
  // under the lock. The copied `weak_this_` makes the cross-thread post safe.
  if (needs_flush) {
    flush_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HostReceiverBatcher::Flush, weak_this_));
  }
}

void HostReceiverBatcher::Clear() {
  base::AutoLock lock(lock_);
  pending_.clear();
}

void HostReceiverBatcher::Flush() {
  std::vector<mojo::GenericPendingReceiver> to_send;
  {
    base::AutoLock lock(lock_);
    flush_scheduled_ = false;
    to_send.swap(pending_);
  }
  if (to_send.empty()) {
    return;
  }
  base::UmaHistogramCounts100("ChildProcess.BindHostReceiver.BatchSize",
                              to_send.size());
  // Run the send with the lock released so it never blocks AddReceiver().
  send_callback_.Run(std::move(to_send));
}

}  // namespace content
