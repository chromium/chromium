// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

namespace WTF {

template <>
struct CrossThreadCopier<
    base::internal::UnretainedWrapper<void,
                                      base::unretained_traits::MayNotDangle>>
    : public CrossThreadCopierPassThrough<base::internal::UnretainedWrapper<
          void,
          base::unretained_traits::MayNotDangle>> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace blink {
namespace {

class CrossThreadHandleTest : public TestSupportingGC {};

class PingPongBase;
class GCed final : public GarbageCollected<GCed> {
 public:
  void Trace(Visitor*) const {}

  void SetReceivedPong(scoped_refptr<PingPongBase>);
};

TEST_F(CrossThreadHandleTest, GetOnCreationThread) {
  auto* gced = MakeGarbageCollected<GCed>();
  auto handle = MakeCrossThreadHandle(gced);
  PreciselyCollectGarbage();
  EXPECT_EQ(
      gced,
      MakeUnwrappingCrossThreadHandle(std::move(handle)).GetOnCreationThread());
}

TEST_F(CrossThreadHandleTest, UnwrapperGetOnCreationThread) {
  auto* gced = MakeGarbageCollected<GCed>();
  auto handle = MakeCrossThreadHandle(gced);
  PreciselyCollectGarbage();
  auto unwrapping_handle = MakeUnwrappingCrossThreadHandle(std::move(handle));
  PreciselyCollectGarbage();
  EXPECT_EQ(gced, unwrapping_handle.GetOnCreationThread());
}

class PingPongBase : public WTF::ThreadSafeRefCounted<PingPongBase> {
 public:
  PingPongBase(scoped_refptr<base::SingleThreadTaskRunner> main_runner,
               scoped_refptr<base::SequencedTaskRunner> thread_runner)
      : main_runner_(std::move(main_runner)),
        thread_runner_(std::move(thread_runner)),
        needle_(MakeGarbageCollected<GCed>()) {}

  bool ReceivedPong() const { return received_pong_; }

  void SetReceivedPong() { received_pong_ = true; }

 protected:
  scoped_refptr<base::SingleThreadTaskRunner> main_runner_;
  scoped_refptr<base::SequencedTaskRunner> thread_runner_;
  WeakPersistent<GCed> needle_;
  bool received_pong_ = false;
};

void GCed::SetReceivedPong(scoped_refptr<PingPongBase> ping_pong) {
  ping_pong->SetReceivedPong();
}

class PassThroughPingPong final : public PingPongBase {
 public:
  PassThroughPingPong(scoped_refptr<base::SingleThreadTaskRunner> main_runner,
                      scoped_refptr<base::SequencedTaskRunner> thread_runner)
      : PingPongBase(std::move(main_runner), std::move(thread_runner)) {}

  void Ping() {
    PostCrossThreadTask(
        *thread_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(&PassThroughPingPong::PingOnOtherThread,
                                 scoped_refptr(this),
                                 MakeCrossThreadHandle(needle_.Get())));
    TestSupportingGC::PreciselyCollectGarbage();
  }

 private:
  static void PingOnOtherThread(scoped_refptr<PassThroughPingPong> ping_pong,
                                CrossThreadHandle<GCed> handle) {
    auto main_runner = ping_pong->main_runner_;
    PostCrossThreadTask(
        *main_runner, FROM_HERE,
        WTF::CrossThreadBindOnce(&PassThroughPingPong::PongOnMainThread,
                                 std::move(ping_pong), std::move(handle)));
  }

  static void PongOnMainThread(scoped_refptr<PassThroughPingPong> ping_pong,
                               CrossThreadHandle<GCed> handle) {
    TestSupportingGC::PreciselyCollectGarbage();
    EXPECT_EQ(ping_pong->needle_.Get(),
              MakeUnwrappingCrossThreadHandle(std::move(handle))
                  .GetOnCreationThread());
    ping_pong->SetReceivedPong();
  }
};

TEST_F(CrossThreadHandleTest, PassThroughPingPong) {
  auto thread_runner = base::ThreadPool::CreateSequencedTaskRunner({});
  auto main_runner = task_environment_.GetMainThreadTaskRunner();
  auto ping_pong =
      base::MakeRefCounted<PassThroughPingPong>(main_runner, thread_runner);
  ping_pong->Ping();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(ping_pong->ReceivedPong());
}

class UnwrappingPingPong final : public PingPongBase {
 public:
  UnwrappingPingPong(scoped_refptr<base::SingleThreadTaskRunner> main_runner,
                     scoped_refptr<base::SequencedTaskRunner> thread_runner)
      : PingPongBase(std::move(main_runner), std::move(thread_runner)) {}

  void Ping() {
    PostCrossThreadTask(
        *thread_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(&UnwrappingPingPong::PingOnOtherThread,
                                 scoped_refptr(this),
                                 MakeCrossThreadHandle(needle_.Get())));
    TestSupportingGC::PreciselyCollectGarbage();
  }

 private:
  static void PingOnOtherThread(scoped_refptr<UnwrappingPingPong> ping_pong,
                                CrossThreadHandle<GCed> handle) {
    auto main_runner = ping_pong->main_runner_;
    PostCrossThreadTask(
        *main_runner, FROM_HERE,
        WTF::CrossThreadBindOnce(
            &UnwrappingPingPong::PongOnMainThread, std::move(ping_pong),
            MakeUnwrappingCrossThreadHandle(std::move(handle))));
  }

  static void PongOnMainThread(scoped_refptr<UnwrappingPingPong> ping_pong,
                               GCed* gced) {
    // Unwrapping keeps the handle in scope during the call, so even a GC
    // without stack cannot reclaim the object here.
    TestSupportingGC::PreciselyCollectGarbage();
    EXPECT_EQ(ping_pong->needle_.Get(), gced);
    ping_pong->SetReceivedPong();
  }
};

TEST_F(CrossThreadHandleTest, UnwrappingPingPong) {
  auto thread_runner = base::ThreadPool::CreateSequencedTaskRunner({});
  auto main_runner = task_environment_.GetMainThreadTaskRunner();
  auto ping_pong =
      base::MakeRefCounted<UnwrappingPingPong>(main_runner, thread_runner);
  ping_pong->Ping();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(ping_pong->ReceivedPong());
}

class BindToMethodPingPong final : public PingPongBase {
 public:
  BindToMethodPingPong(scoped_refptr<base::SingleThreadTaskRunner> main_runner,
                       scoped_refptr<base::SequencedTaskRunner> thread_runner)
      : PingPongBase(std::move(main_runner), std::move(thread_runner)) {}

  void Ping() {
    PostCrossThreadTask(
        *thread_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(&BindToMethodPingPong::PingOnOtherThread,
                                 scoped_refptr(this),
                                 MakeCrossThreadHandle(needle_.Get())));
    TestSupportingGC::PreciselyCollectGarbage();
    ASSERT_TRUE(needle_);
  }

 private:
  static void PingOnOtherThread(scoped_refptr<BindToMethodPingPong> ping_pong,
                                CrossThreadHandle<GCed> handle) {
    auto main_runner = ping_pong->main_runner_;
    PostCrossThreadTask(*main_runner, FROM_HERE,
                        WTF::CrossThreadBindOnce(
                            &GCed::SetReceivedPong,
                            MakeUnwrappingCrossThreadHandle(std::move(handle)),
                            std::move(ping_pong)));
  }
};

TEST_F(CrossThreadHandleTest, BindToMethodPingPong) {
  auto thread_runner = base::ThreadPool::CreateSequencedTaskRunner({});
  auto main_runner = task_environment_.GetMainThreadTaskRunner();
  auto ping_pong =
      base::MakeRefCounted<BindToMethodPingPong>(main_runner, thread_runner);
  ping_pong->Ping();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(ping_pong->ReceivedPong());
}

class BindToMethodDiscardingPingPong final : public PingPongBase {
 public:
  BindToMethodDiscardingPingPong(
      scoped_refptr<base::SingleThreadTaskRunner> main_runner,
      scoped_refptr<base::SequencedTaskRunner> thread_runner)
      : PingPongBase(std::move(main_runner), std::move(thread_runner)) {}

  void Ping() {
    PostCrossThreadTask(
        *thread_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(
            &BindToMethodDiscardingPingPong::PingOnOtherThread,
            scoped_refptr(this), MakeCrossThreadWeakHandle(needle_.Get())));
    TestSupportingGC::PreciselyCollectGarbage();
    ASSERT_FALSE(needle_);
  }

 private:
  static void PingOnOtherThread(
      scoped_refptr<BindToMethodDiscardingPingPong> ping_pong,
      CrossThreadWeakHandle<GCed> handle) {
    auto main_runner = ping_pong->main_runner_;
    PostCrossThreadTask(
        *main_runner, FROM_HERE,
        WTF::CrossThreadBindOnce(
            &GCed::SetReceivedPong,
            MakeUnwrappingCrossThreadWeakHandle(std::move(handle)),
            std::move(ping_pong)));
  }
};

TEST_F(CrossThreadHandleTest, BindToMethodDiscardingPingPong) {
  auto thread_runner = base::ThreadPool::CreateSequencedTaskRunner({});
  auto main_runner = task_environment_.GetMainThreadTaskRunner();
  auto ping_pong = base::MakeRefCounted<BindToMethodDiscardingPingPong>(
      main_runner, thread_runner);
  ping_pong->Ping();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(ping_pong->ReceivedPong());
}

}  // namespace
}  // namespace blink
