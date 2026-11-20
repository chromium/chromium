// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ipcz_driver/mach_voucher_release_pool_mac.h"

#include <mach/mach.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/apple/scoped_mach_port.h"
#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/mock_log.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo::core::ipcz_driver {
namespace {

using base::test::TaskEnvironment;
using ::testing::_;

mach_port_urefs_t GetPortRefCount(mach_port_t port) {
  mach_port_urefs_t refs = 0;
  // CreateUniqueVoucher uses a send right to emulate a voucher, so count send
  // right references.
  const kern_return_t kr =
      mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, &refs);
  if (kr == KERN_INVALID_NAME) {
    // Port was deallocated when the refcount dropped to 0.
    return 0;
  }
  EXPECT_EQ(kr, KERN_SUCCESS);
  return refs;
}

// Test the release pool with and without a ThreadPool.
class MachVoucherReleasePoolTest : public ::testing::TestWithParam<bool> {
 protected:
  MachVoucherReleasePoolTest()
      : task_env_(TaskEnvironment::TimeSource::MOCK_TIME,
                  TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
                  has_thread_pool_
                      ? TaskEnvironment::ThreadingMode::MULTIPLE_THREADS
                      : TaskEnvironment::ThreadingMode::MAIN_THREAD_ONLY) {}

  bool has_thread_pool() const { return has_thread_pool_; }

  TaskEnvironment& task_env() { return task_env_; }

  // Drains pending tasks on the main thread, without executing any tasks on the
  // ThreadPool (due to `ThreadPoolExecutionMode::QUEUED`).
  void FlushMainThread() {
    base::RunLoop run_loop;
    task_env().GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                   run_loop.QuitClosure());
    run_loop.Run();
  }

  // Drains pending tasks on both the main thread and ThreadPool.
  void FlushAllTasks() { task_env().FastForwardBy(base::TimeDelta()); }

  base::apple::ScopedMachSendRight CreateUniqueVoucher() {
    // Retain a local send/receive port pair. A voucher is just a mach_port_t so
    // use the send port to emulate a voucher.
    base::apple::ScopedMachReceiveRight receive;
    base::apple::ScopedMachSendRight send;
    EXPECT_TRUE(base::apple::CreateMachPort(&receive, &send));
    owned_receive_rights_.push_back(std::move(receive));
    return send;
  }

 private:
  const bool has_thread_pool_ = GetParam();
  TaskEnvironment task_env_;
  std::vector<base::apple::ScopedMachReceiveRight> owned_receive_rights_;
};

INSTANTIATE_TEST_SUITE_P(WithThreadPool,
                         MachVoucherReleasePoolTest,
                         ::testing::Bool());

TEST_P(MachVoucherReleasePoolTest, DestroyEmptyPool) {
  {
    MachVoucherReleasePool pool(task_env().GetMainThreadTaskRunner());
  }
  // Pool should NOT have posted a task to clear vouchers.
  EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(), 0u);
}

TEST_P(MachVoucherReleasePoolTest, IgnoresInvalidVouchers) {
  // If `MACH_PORT_NULL` or `MACH_PORT_DEAD` were ever passed to
  // `ReleaseVouchers()`, `mach_port_mod_refs` would fail and log an ERROR.
  // (`ScopedMachSendRight`'s `mach_port_deallocate` succeeds on
  // `MACH_PORT_DEAD` in XNU without logging.)
  base::test::MockLog mock_log;
  EXPECT_CALL(mock_log, Log(_, _, _, _, _)).Times(::testing::AnyNumber());
  EXPECT_CALL(mock_log, Log(::logging::LOGGING_ERROR, _, _, _,
                            ::testing::HasSubstr("mach_port_mod_refs")))
      .Times(0);
  mock_log.StartCapturingLogs();

  MachVoucherReleasePool pool(task_env().GetMainThreadTaskRunner());
  auto callback = pool.GetAcceptVoucherCallback();

  // Direct calls on the IO sequence.
  pool.AcceptVoucher(base::apple::ScopedMachSendRight());
  pool.AcceptVoucher(base::apple::ScopedMachSendRight(MACH_PORT_DEAD));
  EXPECT_EQ(pool.voucher_count_for_testing(), 0u);
  EXPECT_EQ(pool.unique_voucher_count_for_testing(), 0u);
  EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(), 0u);

  // Same-sequence callback invocations.
  callback.Run(base::apple::ScopedMachSendRight());
  callback.Run(base::apple::ScopedMachSendRight(MACH_PORT_DEAD));
  EXPECT_EQ(pool.voucher_count_for_testing(), 0u);
  EXPECT_EQ(pool.unique_voucher_count_for_testing(), 0u);
  EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(), 0u);

  // Cross-sequence callback invocations post `AcceptVoucher` to the main
  // thread, which must still ignore invalid ports without queueing `EmptyPool`.
  base::Thread other_thread("other");
  ASSERT_TRUE(other_thread.Start());

  auto quit_closure = task_env().QuitClosure();
  other_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
        callback.Run(base::apple::ScopedMachSendRight());
        callback.Run(base::apple::ScopedMachSendRight(MACH_PORT_DEAD));
        task_env().GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       std::move(quit_closure));
      }));
  task_env().RunUntilQuit();
  other_thread.Stop();

  EXPECT_EQ(pool.voucher_count_for_testing(), 0u);
  EXPECT_EQ(pool.unique_voucher_count_for_testing(), 0u);
  EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(), 0u);

  // Mix invalid ports with a valid voucher so `EmptyPool` and
  // `ReleaseVouchers` actually run, verifying `mach_port_mod_refs` only runs on
  // the valid voucher.
  base::apple::ScopedMachSendRight observer = CreateUniqueVoucher();
  ASSERT_TRUE(observer.is_valid());
  base::apple::ScopedMachSendRight valid_voucher =
      base::apple::RetainMachSendRight(observer.get());
  ASSERT_TRUE(valid_voucher.is_valid());
  EXPECT_EQ(GetPortRefCount(observer.get()), 2u);

  pool.AcceptVoucher(base::apple::ScopedMachSendRight());
  pool.AcceptVoucher(std::move(valid_voucher));
  pool.AcceptVoucher(base::apple::ScopedMachSendRight(MACH_PORT_DEAD));
  EXPECT_EQ(pool.voucher_count_for_testing(), 1u);
  EXPECT_EQ(pool.unique_voucher_count_for_testing(), 1u);
  EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(), 1u);

  FlushAllTasks();
  EXPECT_EQ(GetPortRefCount(observer.get()), 1u);
  EXPECT_EQ(pool.voucher_count_for_testing(), 0u);
  EXPECT_EQ(pool.unique_voucher_count_for_testing(), 0u);
}

TEST_P(MachVoucherReleasePoolTest, DefersReleaseAfterQueuedTasks) {
  MachVoucherReleasePool pool(task_env().GetMainThreadTaskRunner());

  // The test will observe these ports' reference counts to track vouchers
  // cloned from them.
  base::apple::ScopedMachSendRight observer_a = CreateUniqueVoucher();
  base::apple::ScopedMachSendRight observer_b = CreateUniqueVoucher();
  ASSERT_TRUE(observer_a.is_valid());
  ASSERT_TRUE(observer_b.is_valid());
  ASSERT_NE(observer_a.get(), observer_b.get());

  base::apple::ScopedMachSendRight voucher_a =
      base::apple::RetainMachSendRight(observer_a.get());
  ASSERT_TRUE(voucher_a.is_valid());
  EXPECT_EQ(GetPortRefCount(observer_a.get()), 2u);

  base::apple::ScopedMachSendRight voucher_b =
      base::apple::RetainMachSendRight(observer_b.get());
  ASSERT_TRUE(voucher_b.is_valid());
  EXPECT_EQ(GetPortRefCount(observer_b.get()), 2u);

  EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(), 0u);

  // Queue four tasks together. `AcceptVoucher` should post `EmptyPool` to run
  // after all of them.
  auto barrier_closure = base::BarrierClosure(4, task_env().QuitClosure());

  task_env().GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&MachVoucherReleasePool::AcceptVoucher,
                                base::Unretained(&pool), std::move(voucher_a))
                     .Then(barrier_closure));
  task_env().GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
                   EXPECT_EQ(GetPortRefCount(observer_a.get()), 2u);
                   EXPECT_EQ(pool.voucher_count_for_testing(), 1u);
                 }).Then(barrier_closure));
  task_env().GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&MachVoucherReleasePool::AcceptVoucher,
                                base::Unretained(&pool), std::move(voucher_b))
                     .Then(barrier_closure));
  task_env().GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
                   EXPECT_EQ(GetPortRefCount(observer_a.get()), 2u);
                   EXPECT_EQ(GetPortRefCount(observer_b.get()), 2u);
                   EXPECT_EQ(pool.voucher_count_for_testing(), 2u);
                 }).Then(barrier_closure));

  task_env().RunUntilQuit();

  // A single `EmptyPool` is still pending. Adding `voucher_b` did not schedule
  // another one.
  EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(), 1u);
  EXPECT_EQ(GetPortRefCount(observer_a.get()), 2u);
  EXPECT_EQ(GetPortRefCount(observer_b.get()), 2u);
  EXPECT_EQ(pool.voucher_count_for_testing(), 2u);

  // `EmptyPool` runs on the main thread and offloads the kernel deallocation to
  // the ThreadPool, leaving refs intact while `ThreadPool` is paused. (If
  // there's no ThreadPool, it instead posts another task to the main thread.)
  FlushMainThread();
  EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(),
            has_thread_pool() ? 0u : 1u);
  EXPECT_EQ(GetPortRefCount(observer_a.get()), 2u);
  EXPECT_EQ(GetPortRefCount(observer_b.get()), 2u);
  EXPECT_EQ(pool.voucher_count_for_testing(), 0u);

  FlushAllTasks();
  EXPECT_EQ(GetPortRefCount(observer_a.get()), 1u);
  EXPECT_EQ(GetPortRefCount(observer_b.get()), 1u);
}

TEST_P(MachVoucherReleasePoolTest, DestructorDrainsNonEmptyPool) {
  base::apple::ScopedMachSendRight observer = CreateUniqueVoucher();
  ASSERT_TRUE(observer.is_valid());

  base::apple::ScopedMachSendRight voucher =
      base::apple::RetainMachSendRight(observer.get());
  ASSERT_TRUE(voucher.is_valid());
  EXPECT_EQ(GetPortRefCount(observer.get()), 2u);

  {
    MachVoucherReleasePool pool(task_env().GetMainThreadTaskRunner());
    pool.AcceptVoucher(std::move(voucher));

    EXPECT_EQ(pool.voucher_count_for_testing(), 1u);
    EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(), 1u);
  }

  // The outstanding vouchers should be destroyed (on the ThreadPool if there is
  // one) even though the queued `EmptyPool` task will do nothing when it runs
  // because the release pool is gone. If there's no thread pool there will be
  // another cleanup task queued after `EmptyPool`.
  EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(),
            has_thread_pool() ? 1u : 2u);
  FlushAllTasks();
  EXPECT_EQ(GetPortRefCount(observer.get()), 1u);
}

TEST_P(MachVoucherReleasePoolTest, CoalescesDuplicates) {
  MachVoucherReleasePool pool(task_env().GetMainThreadTaskRunner());

  // The test will observe these ports' reference counts to track vouchers
  // cloned from them.
  base::apple::ScopedMachSendRight observer_a = CreateUniqueVoucher();
  base::apple::ScopedMachSendRight observer_b = CreateUniqueVoucher();
  ASSERT_TRUE(observer_a.is_valid());
  ASSERT_TRUE(observer_b.is_valid());
  ASSERT_NE(observer_a.get(), observer_b.get());

  EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(), 0u);

  // Batch 1: interleave 3 refs to `a` and 2 refs to `b`.
  for (mach_port_t port : {observer_a.get(), observer_b.get(), observer_a.get(),
                           observer_a.get(), observer_b.get()}) {
    base::apple::ScopedMachSendRight ref =
        base::apple::RetainMachSendRight(port);
    ASSERT_TRUE(ref.is_valid());
    pool.AcceptVoucher(std::move(ref));
  }

  EXPECT_EQ(GetPortRefCount(observer_a.get()), 4u);
  EXPECT_EQ(GetPortRefCount(observer_b.get()), 3u);
  EXPECT_EQ(pool.voucher_count_for_testing(), 5u);
  EXPECT_EQ(pool.unique_voucher_count_for_testing(), 2u);

  // A single `EmptyPool` should be scheduled for all these vouchers.
  EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(), 1u);

  // When the pool is emptied, each ref passed to `AcceptVoucher` should be
  // dropped, leaving exactly 1 ref remaining for each port.
  FlushAllTasks();
  EXPECT_EQ(GetPortRefCount(observer_a.get()), 1u);
  EXPECT_EQ(GetPortRefCount(observer_b.get()), 1u);
  EXPECT_EQ(pool.voucher_count_for_testing(), 0u);
  EXPECT_EQ(pool.unique_voucher_count_for_testing(), 0u);
}

TEST_P(MachVoucherReleasePoolTest, ReleasesFinalSendRights) {
  MachVoucherReleasePool pool(task_env().GetMainThreadTaskRunner());

  // No observer to retain a ref in this test.
  base::apple::ScopedMachSendRight voucher = CreateUniqueVoucher();
  ASSERT_TRUE(voucher.is_valid());
  const mach_port_t raw_port = voucher.get();

  base::apple::ScopedMachSendRight ref2 =
      base::apple::RetainMachSendRight(raw_port);
  base::apple::ScopedMachSendRight ref3 =
      base::apple::RetainMachSendRight(raw_port);
  ASSERT_TRUE(ref2.is_valid());
  ASSERT_TRUE(ref3.is_valid());
  EXPECT_EQ(GetPortRefCount(raw_port), 3u);

  pool.AcceptVoucher(std::move(voucher));
  pool.AcceptVoucher(std::move(ref2));
  pool.AcceptVoucher(std::move(ref3));
  EXPECT_EQ(GetPortRefCount(raw_port), 3u);

  FlushAllTasks();
  EXPECT_EQ(GetPortRefCount(raw_port), 0u);
}

TEST_P(MachVoucherReleasePoolTest, GetAcceptVoucherCallback) {
  MachVoucherReleasePool pool(task_env().GetMainThreadTaskRunner());

  // The test will observe these ports' reference counts to track vouchers
  // cloned from them.
  base::apple::ScopedMachSendRight observer_a = CreateUniqueVoucher();
  base::apple::ScopedMachSendRight observer_b = CreateUniqueVoucher();
  base::apple::ScopedMachSendRight observer_c = CreateUniqueVoucher();
  ASSERT_TRUE(observer_a.is_valid());
  ASSERT_TRUE(observer_b.is_valid());
  ASSERT_TRUE(observer_c.is_valid());

  base::apple::ScopedMachSendRight voucher_a =
      base::apple::RetainMachSendRight(observer_a.get());
  ASSERT_TRUE(voucher_a.is_valid());
  EXPECT_EQ(GetPortRefCount(observer_a.get()), 2u);

  base::apple::ScopedMachSendRight voucher_b =
      base::apple::RetainMachSendRight(observer_b.get());
  ASSERT_TRUE(voucher_b.is_valid());
  EXPECT_EQ(GetPortRefCount(observer_b.get()), 2u);

  base::apple::ScopedMachSendRight voucher_c =
      base::apple::RetainMachSendRight(observer_c.get());
  ASSERT_TRUE(voucher_c.is_valid());
  EXPECT_EQ(GetPortRefCount(observer_c.get()), 2u);

  auto barrier_closure = base::BarrierClosure(3, task_env().QuitClosure());
  auto accept_voucher_callback =
      pool.GetAcceptVoucherCallback().Then(barrier_closure);

  // Start on some other thread.
  base::Thread other_thread("other");
  ASSERT_TRUE(other_thread.Start());

  other_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
        // Call `accept_voucher_callback` on this thread. It should implicitly
        // post back to the main thread (validated by the pool's
        // SEQUENCE_CHECKER).
        accept_voucher_callback.Run(std::move(voucher_a));

        // Post `accept_voucher_callback` back to the main thread. When it runs,
        // it should call AcceptVoucher inline. (If not, the voucher would be
        // accepted AFTER the `EmptyPool` call.)
        task_env().GetMainThreadTaskRunner()->PostTask(
            FROM_HERE, base::BindLambdaForTesting([&] {
              // `AcceptVoucher` should already have run for `voucher_a`, which
              // schedules `EmptyPool`.
              EXPECT_EQ(pool.voucher_count_for_testing(), 1u);
              accept_voucher_callback.Run(std::move(voucher_b));
              EXPECT_EQ(pool.voucher_count_for_testing(), 2u);

              // Post another copy of `accept_voucher_callback`. This will be
              // queued after `EmptyPool`.
              task_env().GetMainThreadTaskRunner()->PostTask(
                  FROM_HERE, base::BindOnce(accept_voucher_callback,
                                            std::move(voucher_c)));
            }));
      }));

  task_env().RunUntilQuit();

  // The order of events above was (on the main thread unless otherwise noted):
  //
  // - Add `voucher_a` to pool (queues `EmptyPool`).
  // - Add `voucher_b` to pool.
  // - EmptyPool removes `voucher_a` and `voucher_b`, binds them to the kernel
  //   deallocation task. (Posts that task to the ThreadPool, if there is one,
  //   or back to the main thread.)
  // - Add `voucher_c` to pool (queues another `EmptyPool`).
  //
  // So the pool should now only contain 1 voucher, with another `EmptyPool`
  // queued on the main thread. If there's a ThreadPool, the kernel deallocation
  // task may or may not have already run. Otherwise it's also queued on the
  // main thread.
  EXPECT_EQ(pool.voucher_count_for_testing(), 1u);
  EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(),
            has_thread_pool() ? 1u : 2u);

  // Run the pending `EmptyPool` and kernel deallocation tasks.
  FlushAllTasks();
  EXPECT_EQ(pool.voucher_count_for_testing(), 0u);
  EXPECT_EQ(GetPortRefCount(observer_a.get()), 1u);
  EXPECT_EQ(GetPortRefCount(observer_b.get()), 1u);
  EXPECT_EQ(GetPortRefCount(observer_c.get()), 1u);
}

TEST_P(MachVoucherReleasePoolTest,
       CallbackAfterPoolDestroyedDoesNotLeakVouchers) {
  base::apple::ScopedMachSendRight observer = CreateUniqueVoucher();
  ASSERT_TRUE(observer.is_valid());

  base::Thread other_thread("other");
  ASSERT_TRUE(other_thread.Start());

  // Same-sequence invocation after pool destruction drops `voucher` inline.
  {
    auto pool = std::make_unique<MachVoucherReleasePool>(
        task_env().GetMainThreadTaskRunner());
    auto callback = pool->GetAcceptVoucherCallback();
    pool.reset();

    base::apple::ScopedMachSendRight voucher =
        base::apple::RetainMachSendRight(observer.get());
    ASSERT_TRUE(voucher.is_valid());
    EXPECT_EQ(GetPortRefCount(observer.get()), 2u);

    callback.Run(std::move(voucher));

    // No `EmptyPool` queued since the pool doesn't exist.
    EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(), 0u);

    // But the voucher is gone.
    EXPECT_EQ(GetPortRefCount(observer.get()), 1u);
  }

  // Cross-sequence invocation after pool destruction drops `voucher` when
  // the callback is discarded.
  {
    auto pool = std::make_unique<MachVoucherReleasePool>(
        task_env().GetMainThreadTaskRunner());
    auto callback = pool->GetAcceptVoucherCallback();
    pool.reset();

    base::apple::ScopedMachSendRight voucher =
        base::apple::RetainMachSendRight(observer.get());
    ASSERT_TRUE(voucher.is_valid());
    EXPECT_EQ(GetPortRefCount(observer.get()), 2u);

    other_thread.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(callback, std::move(voucher))
                       .Then(task_env().QuitClosure()));
    task_env().RunUntilQuit();

    // No `EmptyPool` queued, voucher is gone.
    EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(), 0u);
    EXPECT_EQ(GetPortRefCount(observer.get()), 1u);
  }

  // Cross-sequence invocation while pool is alive, followed by pool destruction
  // before the posted task runs.
  {
    auto pool = std::make_unique<MachVoucherReleasePool>(
        task_env().GetMainThreadTaskRunner());
    auto callback = pool->GetAcceptVoucherCallback();

    base::apple::ScopedMachSendRight voucher =
        base::apple::RetainMachSendRight(observer.get());
    ASSERT_TRUE(voucher.is_valid());
    EXPECT_EQ(GetPortRefCount(observer.get()), 2u);

    auto quit_closure = task_env().QuitClosure();
    other_thread.task_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&] {
          // Running `callback` posts a task. Destroy the pool before it runs,
          // and quit the loop after it.
          task_env().GetMainThreadTaskRunner()->PostTask(
              FROM_HERE, base::BindLambdaForTesting([&] { pool.reset(); }));
          callback.Run(std::move(voucher));
          task_env().GetMainThreadTaskRunner()->PostTask(
              FROM_HERE, std::move(quit_closure));
        }));
    task_env().RunUntilQuit();

    // No `EmptyPool` queued, voucher is gone.
    EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(), 0u);
    EXPECT_EQ(GetPortRefCount(observer.get()), 1u);
  }
}

TEST_P(MachVoucherReleasePoolTest, MaxRefCount) {
  const size_t kMaxRefCount = MachVoucherReleasePool::kMaxRefCount;

  MachVoucherReleasePool pool(task_env().GetMainThreadTaskRunner());

  // The test will observe these ports' reference counts to track vouchers
  // cloned from them.
  base::apple::ScopedMachSendRight observer_a = CreateUniqueVoucher();
  base::apple::ScopedMachSendRight observer_b = CreateUniqueVoucher();
  ASSERT_TRUE(observer_a.is_valid());
  ASSERT_TRUE(observer_b.is_valid());
  ASSERT_NE(observer_a.get(), observer_b.get());

  for (size_t i = 0; i < kMaxRefCount; ++i) {
    pool.AcceptVoucher(base::apple::RetainMachSendRight(observer_a.get()));
    pool.AcceptVoucher(base::apple::RetainMachSendRight(observer_b.get()));
  }
  EXPECT_EQ(GetPortRefCount(observer_a.get()), kMaxRefCount + 1);
  EXPECT_EQ(GetPortRefCount(observer_b.get()), kMaxRefCount + 1);
  EXPECT_EQ(pool.voucher_count_for_testing(), kMaxRefCount * 2);

  // Adding another voucher A should clear its previous refs from the pool
  // (leaving only 1), without affecting voucher B.
  pool.AcceptVoucher(base::apple::RetainMachSendRight(observer_a.get()));
  EXPECT_EQ(GetPortRefCount(observer_a.get()), 2u);
  EXPECT_EQ(GetPortRefCount(observer_b.get()), kMaxRefCount + 1);
  EXPECT_EQ(pool.voucher_count_for_testing(), kMaxRefCount + 1);

  pool.AcceptVoucher(base::apple::RetainMachSendRight(observer_b.get()));
  EXPECT_EQ(GetPortRefCount(observer_a.get()), 2u);
  EXPECT_EQ(GetPortRefCount(observer_b.get()), 2u);
  EXPECT_EQ(pool.voucher_count_for_testing(), 2u);

  // Reaching and overflowing kMaxRefCount again should do the same thing.
  // There's 1 ref for voucher A already in the pool, although GetPortRefCount
  // returns 2 because it also includes a ref for `observer_a`.
  for (size_t i = 1; i < kMaxRefCount; ++i) {
    pool.AcceptVoucher(base::apple::RetainMachSendRight(observer_a.get()));
  }
  EXPECT_EQ(GetPortRefCount(observer_a.get()), kMaxRefCount + 1);
  EXPECT_EQ(GetPortRefCount(observer_b.get()), 2u);
  EXPECT_EQ(pool.voucher_count_for_testing(), kMaxRefCount + 1);

  pool.AcceptVoucher(base::apple::RetainMachSendRight(observer_a.get()));
  EXPECT_EQ(GetPortRefCount(observer_a.get()), 2u);
  EXPECT_EQ(GetPortRefCount(observer_b.get()), 2u);
  EXPECT_EQ(pool.voucher_count_for_testing(), 2u);

  // A single `EmptyPool` should still be scheduled for all these vouchers.
  EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(), 1u);

  FlushAllTasks();
  EXPECT_EQ(GetPortRefCount(observer_a.get()), 1u);
  EXPECT_EQ(GetPortRefCount(observer_b.get()), 1u);
  EXPECT_EQ(pool.voucher_count_for_testing(), 0u);
}

}  // namespace
}  // namespace mojo::core::ipcz_driver
