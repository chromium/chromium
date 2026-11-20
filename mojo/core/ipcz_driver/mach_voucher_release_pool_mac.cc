// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ipcz_driver/mach_voucher_release_pool_mac.h"

#include <mach/mach.h>

#include <utility>

#include "base/apple/mach_logging.h"
#include "base/apple/scoped_mach_port.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace mojo::core::ipcz_driver {

namespace {

using ChromeTrackEvent = perfetto::protos::pbzero::ChromeTrackEvent;

// Writes the total number of vouchers and number of unique vouchers into `ctx`.
void TraceVoucherCounts(
    const perfetto::EventContext& ctx,
    const absl::flat_hash_map<mach_port_t, mach_port_delta_t>& vouchers,
    size_t voucher_count) {
  auto* mac_voucher_release_pool =
      ctx.event<ChromeTrackEvent>()->set_mac_voucher_release_pool();
  mac_voucher_release_pool->set_voucher_count(voucher_count);
  mac_voucher_release_pool->set_unique_voucher_count(vouchers.size());
}

void AcceptVoucherOnIOSequence(
    base::WeakPtr<MachVoucherReleasePool> pool,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    base::apple::ScopedMachSendRight voucher) {
  // Normally this idiom makes it hard to reason about ordering. But here it's
  // the correct semantics: AcceptVoucher expects to run from a task on the IO
  // sequence, and may post an EmptyPool task to run after all the tasks that
  // are queued after it. If already on the IO thread, call AcceptVoucher
  // directly so the EmptyPool task runs at the end of the current queue.
  // Otherwise create a new IO thread task and post it.
  if (io_task_runner->RunsTasksInCurrentSequence()) {
    if (pool) {
      pool->AcceptVoucher(std::move(voucher));
    }
  } else {
    io_task_runner->PostTask(
        FROM_HERE, base::BindOnce(&MachVoucherReleasePool::AcceptVoucher,
                                  std::move(pool), std::move(voucher)));
  }
}

void ReleaseVoucherRefs(mach_port_t voucher, mach_port_delta_t ref_count) {
  // Drop all references we know about in a single kernel call.
  TRACE_EVENT(
      "ipc", "Release QoS Voucher", [ref_count](perfetto::EventContext ctx) {
        ctx.event<ChromeTrackEvent>()->set_mac_voucher_release()->set_ref_count(
            ref_count);
      });
  const kern_return_t kr = mach_port_mod_refs(mach_task_self(), voucher,
                                              MACH_PORT_RIGHT_SEND, -ref_count);
  MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr)
      << "MachVoucherReleasePool mach_port_mod_refs";
}

void ReleaseVouchers(
    absl::flat_hash_map<mach_port_t, mach_port_delta_t> vouchers,
    size_t voucher_count) {
  TRACE_EVENT("ipc", "MachVoucherReleasePool ReleaseVouchers",
              [&](perfetto::EventContext ctx) {
                TraceVoucherCounts(ctx, vouchers, voucher_count);
              });
  for (auto [voucher, count] : vouchers) {
    ReleaseVoucherRefs(voucher, count);
  }
}

}  // namespace

MachVoucherReleasePool::MachVoucherReleasePool(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : io_task_runner_(std::move(io_task_runner)) {
  CHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (base::ThreadPoolInstance::Get()) {
    // Delete the vouchers on a worker thread, so that they don't contend for
    // global OS locks on the IO thread. This must be done at foreground
    // priority to avoid priority inversions in the OS locks. It uses
    // USER_VISIBLE instead of BEST_EFFORT because if the task is starved the
    // vouchers might keep the priority boosted for an excessively long time.
    release_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::TaskPriority::USER_VISIBLE,
         base::ThreadPolicy::MUST_USE_FOREGROUND,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  } else {
    // In apps that don't use a ThreadPool, fall back to deleting on this
    // sequence. This will add an extra PostTask, but these will be simple
    // helper or test apps that don't warrant the complexity of avoiding it.
    release_task_runner_ = io_task_runner_;
  }
}

MachVoucherReleasePool::~MachVoucherReleasePool() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT("ipc", "MachVoucherReleasePool EmptyPoolInDestructor");
  EmptyPool();
}

void MachVoucherReleasePool::AcceptVoucher(
    base::apple::ScopedMachSendRight voucher) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_BEGIN("ipc", "MachVoucherReleasePool::AcceptVoucher");
  absl::Cleanup end_trace_event = [this] {
    // Add trace arguments *after* the method updates the counts.
    TRACE_EVENT_END("ipc", [this](perfetto::EventContext ctx) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      TraceVoucherCounts(ctx, vouchers_, voucher_count_);
    });
  };

  // TODO(crbug.com/520055607): Make this a CHECK when kSkipInvalidMachVouchers
  // is globally enabled. (Don't forget to update the function header comment.)
  if (!MACH_PORT_VALID(voucher.get())) {
    return;
  }

  const bool pool_was_empty = vouchers_.empty();

  // The same mach_port_t can be received in multiple messages. In fact this is
  // common when the messages are sent from the same process and thread. Store
  // one copy of the port along with the refcount we're aware of, so it can be
  // decremented in a single kernel call.
  auto& ref_count = vouchers_[voucher.get()];
  if (ref_count == kMaxRefCount) {
    // Drop refs that have been building up to avoid overflow. This is
    // guaranteed not to take the real refcount to 0, which could trigger heavy
    // work, because `voucher` contains another ref.
    ReleaseVoucherRefs(voucher.get(), kMaxRefCount);
    ref_count = 0;
    voucher_count_ -= kMaxRefCount;
  }
  // Transfer ownership of `voucher` to the pool.
  ref_count += 1;
  voucher_count_ += 1;
  std::ignore = voucher.release();

  // Drain the pool after all tasks queued behind this one on the IO thread run.
  if (pool_was_empty) {
    // This was the first voucher added, so there won't be an EmptyPool task
    // queued yet. EmptyPool() will clear `vouchers_` so the next voucher added
    // will post another task.
    io_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(&MachVoucherReleasePool::EmptyPool,
                                             weak_factory_.GetWeakPtr()));
  }
}

base::RepeatingCallback<void(base::apple::ScopedMachSendRight)>
MachVoucherReleasePool::GetAcceptVoucherCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::BindRepeating(&AcceptVoucherOnIOSequence,
                             weak_factory_.GetWeakPtr(), io_task_runner_);
}

void MachVoucherReleasePool::EmptyPool() {
  if (vouchers_.empty()) {
    return;
  }

  TRACE_EVENT("ipc", "MachVoucherReleasePool::EmptyPool");
  release_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ReleaseVouchers, std::exchange(vouchers_, {}),
                                std::exchange(voucher_count_, 0)));
}

}  // namespace mojo::core::ipcz_driver
