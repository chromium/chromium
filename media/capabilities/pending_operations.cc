// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capabilities/pending_operations.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"

namespace media {
namespace {
// Timeout threshold for DB operations. See OnOperationTimeout().
// NOTE: Used by UmaHistogramOpTime. Change the name if you change the time.
static constexpr base::TimeDelta kPendingOpTimeout = base::Seconds(30);
}  // namespace

PendingOperations::PendingOperation::PendingOperation(
    const std::string& uma_prefix,
    std::string uma_str,
    std::unique_ptr<base::CancelableOnceClosure> timeout_closure)
    : uma_prefix_(uma_prefix),
      uma_str_(uma_str),
      timeout_closure_(std::move(timeout_closure)),
      start_ticks_(base::TimeTicks::Now()) {
  DVLOG(3) << __func__ << " Started " << uma_str_;
}

PendingOperations::PendingOperation::~PendingOperation() {
  // Destroying a pending operation that hasn't timed out yet implies the
  // operation has completed.
  if (timeout_closure_ && !timeout_closure_->IsCancelled()) {
    base::TimeDelta op_duration = base::TimeTicks::Now() - start_ticks_;
    UmaHistogramOpTime(uma_str_, op_duration);
    DVLOG(3) << __func__ << " Completed " << uma_str_ << " ("
             << op_duration.InMilliseconds() << ")";

    // Ensure the timeout doesn't fire. Destruction should cancel the callback
    // implicitly, but that's not a documented contract, so just taking the safe
    // route.
    timeout_closure_->Cancel();
  }
}

void PendingOperations::PendingOperation::UmaHistogramOpTime(
    const std::string& op_name,
    base::TimeDelta duration) {
  base::UmaHistogramCustomMicrosecondsTimes(*uma_prefix_ + op_name, duration,
                                            base::Milliseconds(1),
                                            kPendingOpTimeout, 50);
}

void PendingOperations::PendingOperation::OnTimeout() {
  UmaHistogramOpTime(uma_str_, kPendingOpTimeout);
  LOG(WARNING) << " Timeout performing " << uma_str_
               << " operation on WebrtcVideoStatsDB";

  // Cancel the closure to ensure we don't double report the task as completed
  // in ~PendingOperation().
  timeout_closure_->Cancel();
}

PendingOperations::PendingOperations(std::string uma_prefix)
    : uma_prefix_(uma_prefix) {}

PendingOperations::~PendingOperations() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

PendingOperations::Id PendingOperations::Start(std::string uma_str) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Id op_id = next_op_id_++;

  auto timeout_closure = std::make_unique<base::CancelableOnceClosure>(
      base::BindOnce(&PendingOperations::OnTimeout,
                     weak_ptr_factory_.GetWeakPtr(), op_id));

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, timeout_closure->callback(), kPendingOpTimeout);

  pending_ops_.emplace(
      op_id, std::make_unique<PendingOperation>(uma_prefix_, uma_str,
                                                std::move(timeout_closure)));

  return op_id;
}

void PendingOperations::Complete(Id op_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Destructing the PendingOperation will trigger UMA for completion timing.
  int count = pending_ops_.erase(op_id);

  // No big deal, but very unusual. Timeout is very generous, so tasks that
  // timeout are generally assumed to be permanently hung.
  if (!count)
    DVLOG(2) << __func__ << " DB operation completed after timeout.";
}

void PendingOperations::OnTimeout(Id op_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = pending_ops_.find(op_id);
  DCHECK(it != pending_ops_.end());

  it->second->OnTimeout();
  pending_ops_.erase(it);
}

}  // namespace media
