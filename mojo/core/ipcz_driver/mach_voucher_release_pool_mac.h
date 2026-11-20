// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_IPCZ_DRIVER_MACH_VOUCHER_RELEASE_POOL_MAC_H_
#define MOJO_CORE_IPCZ_DRIVER_MACH_VOUCHER_RELEASE_POOL_MAC_H_

#include <mach/mach.h>

#include "base/apple/scoped_mach_port.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/core/system_impl_export.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace mojo::core::ipcz_driver {

// Mach vouchers can be expensive to release. The release pool gathers vouchers
// from closed Envelopes and releases them all in a batch to prevent that cost
// being paid on every message.
//
// The pool is owned by a ChannelMac and lives on its IO sequence, which is
// where incoming vouchers are received. Envelopes may be transferred to another
// sequence so they must pass the voucher back to the IO sequence to add them to
// the pool, by invoking the callback returned from GetAcceptVoucherCallback.
//
// After the pool receives a voucher, it posts a task back to the IO sequence
// to release all its vouchers. This gives the IO sequence a chance to process
// more incoming messages before releasing vouchers.
//
// The benefit is that the OS will reuse the same mach_port_t for vouchers
// carrying the same values (eg. in messages from the same process/thread) as
// long as at least one reference exists. Keeping vouchers alive through a
// series of messages can stop the system from sending different mach_port_t's
// for the same voucher values, each with refcount 1. That in turn reduces
// contention because dropping the refcount from 1 to 0 takes extra bookkeeping
// (requiring extra locks) to check whether this is the last voucher keeping the
// importance boosted.
class MOJO_SYSTEM_IMPL_EXPORT MachVoucherReleasePool {
 public:
  // Mach port refcounts are held in a 16-bit field. Don't put vouchers with a
  // high known refcount in the pool, to prevent overflows. (This is well below
  // the maximum because the port will have references we don't know about.)
  static constexpr mach_port_delta_t kMaxRefCount = 4096;

  explicit MachVoucherReleasePool(
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);
  ~MachVoucherReleasePool();

  MachVoucherReleasePool(const MachVoucherReleasePool&) = delete;
  MachVoucherReleasePool& operator=(const MachVoucherReleasePool&) = delete;

  // Adds a voucher to the pool. It will be released next time the pool is
  // drained. Must be called on the IO sequence.
  void AcceptVoucher(base::apple::ScopedMachSendRight voucher);

  // Returns a callback that calls AcceptVoucher. Must be called on the IO
  // sequence, but the returned callback can be invoked on any sequence.
  base::RepeatingCallback<void(base::apple::ScopedMachSendRight)>
  GetAcceptVoucherCallback();

  // Returns the current number of vouchers in the pool.
  size_t voucher_count_for_testing() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return voucher_count_;
  }

  // Returns the current number of vouchers with unique values in the pool.
  size_t unique_voucher_count_for_testing() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return vouchers_.size();
  }

 private:
  // Drains the pool, releasing all vouchers.
  void EmptyPool() VALID_CONTEXT_REQUIRED(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  // The IO sequence that owns the release pool.
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  // A worker that releases the vouchers.
  scoped_refptr<base::SequencedTaskRunner> release_task_runner_;

  // Voucher ports mapped to the count of that voucher the pool has seen.
  absl::flat_hash_map<mach_port_t, mach_port_delta_t> vouchers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Total number of vouchers before coalescing, for traces.
  size_t voucher_count_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  base::WeakPtrFactory<MachVoucherReleasePool> weak_factory_{this};
};

}  // namespace mojo::core::ipcz_driver

#endif  // MOJO_CORE_IPCZ_DRIVER_MACH_VOUCHER_RELEASE_POOL_MAC_H_
