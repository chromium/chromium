// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

class HeapThreadTest : public TestSupportingGC {};
class HeapThreadDeathTest : public TestSupportingGC {};

namespace heap_thread_test {

static Mutex& ActiveThreadMutex() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, active_thread_mutex, ());
  return active_thread_mutex;
}

static ThreadCondition& ActiveThreadCondition() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadCondition, active_thread_condition,
                                  (ActiveThreadMutex()));
  return active_thread_condition;
}

enum ActiveThreadState {
  kNoThreadActive,
  kMainThreadActive,
  kWorkerThreadActive,
};

static ActiveThreadState& ActiveThread() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ActiveThreadState, active_thread,
                                  (kNoThreadActive));
  return active_thread;
}

static void WakeMainThread() {
  ActiveThread() = kMainThreadActive;
  ActiveThreadCondition().Signal();
}

static void WakeWorkerThread() {
  ActiveThread() = kWorkerThreadActive;
  ActiveThreadCondition().Signal();
}

static void ParkMainThread() {
  while (ActiveThread() != kMainThreadActive) {
    ActiveThreadCondition().Wait();
  }
}

static void ParkWorkerThread() {
  while (ActiveThread() != kWorkerThreadActive) {
    ActiveThreadCondition().Wait();
  }
}

class Object : public GarbageCollected<Object> {
 public:
  Object() {}
  void Trace(blink::Visitor* visitor) {}
};

class AlternatingThreadTester {
  STACK_ALLOCATED();

 public:
  void Test() {
    MutexLocker locker(ActiveThreadMutex());
    ActiveThread() = kMainThreadActive;

    std::unique_ptr<Thread> worker_thread = Platform::Current()->CreateThread(
        ThreadCreationParams(ThreadType::kTestThread)
            .SetThreadNameForTest("Test Worker Thread"));
    PostCrossThreadTask(
        *worker_thread->GetTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(&AlternatingThreadTester::StartWorkerThread,
                            CrossThreadUnretained(this)));

    MainThreadMain();
  }

  void SwitchToWorkerThread() {
    WakeWorkerThread();
    ParkMainThread();
  }

  void SwitchToMainThread() {
    WakeMainThread();
    ParkWorkerThread();
  }

 protected:
  // Override with code you want to execute on the main thread.
  virtual void MainThreadMain() = 0;
  // Override with code you want to execute on the worker thread. At the end,
  // the ThreadState is detached and we switch back to the main thread
  // automatically.
  virtual void WorkerThreadMain() = 0;

 private:
  void StartWorkerThread() {
    ThreadState::AttachCurrentThread();

    MutexLocker locker(ActiveThreadMutex());

    WorkerThreadMain();

    ThreadState::DetachCurrentThread();
    WakeMainThread();
  }
};

class MemberSameThreadCheckTester : public AlternatingThreadTester {
 private:
  void MainThreadMain() override { SwitchToWorkerThread(); }

  void WorkerThreadMain() override {
    // Setting an object created on the worker thread to a Member allocated on
    // the main thread is not allowed.
    object_ = MakeGarbageCollected<Object>();
  }

  Member<Object> object_;
};

#if DCHECK_IS_ON()
// TODO(keishi) This test is flaky on mac-rel bot.
// crbug.com/709069
#if !defined(OS_MACOSX)
TEST_F(HeapThreadDeathTest, MemberSameThreadCheck) {
  EXPECT_DEATH(MemberSameThreadCheckTester().Test(), "");
}
#endif
#endif

class PersistentSameThreadCheckTester : public AlternatingThreadTester {
 private:
  void MainThreadMain() override { SwitchToWorkerThread(); }

  void WorkerThreadMain() override {
    // Setting an object created on the worker thread to a Persistent allocated
    // on the main thread is not allowed.
    object_ = MakeGarbageCollected<Object>();
  }

  Persistent<Object> object_;
};

#if DCHECK_IS_ON()
// TODO(keishi) This test is flaky on mac-rel bot.
// crbug.com/709069
#if !defined(OS_MACOSX)
TEST_F(HeapThreadDeathTest, PersistentSameThreadCheck) {
  EXPECT_DEATH(PersistentSameThreadCheckTester().Test(), "");
}
#endif
#endif

class MarkingSameThreadCheckTester : public AlternatingThreadTester {
 private:
  class MainThreadObject final : public GarbageCollected<MainThreadObject> {
   public:
    void Trace(blink::Visitor* visitor) { visitor->Trace(set_); }
    void AddToSet(Object* object) { set_.insert(42, object); }

   private:
    HeapHashMap<int, Member<Object>> set_;
  };

  void MainThreadMain() override {
    main_thread_object_ = MakeGarbageCollected<MainThreadObject>();

    SwitchToWorkerThread();

    // This will try to mark MainThreadObject when it tries to mark Object
    // it should crash.
    TestSupportingGC::PreciselyCollectGarbage();
  }

  void WorkerThreadMain() override {
    // Adding a reference to an object created on the worker thread to a
    // HeapHashMap created on the main thread is not allowed.
    main_thread_object_->AddToSet(MakeGarbageCollected<Object>());
  }

  CrossThreadPersistent<MainThreadObject> main_thread_object_;
};

#if DCHECK_IS_ON()
// TODO(keishi) This test is flaky on mac-rel bot. https://crbug.com/709069, and
// times out on other bots. https://crbug.com/993148.
TEST_F(HeapThreadDeathTest, DISABLED_MarkingSameThreadCheck) {
  // This will crash during marking, at the DCHECK in Visitor::markHeader() or
  // earlier.
  EXPECT_DEATH(MarkingSameThreadCheckTester().Test(), "");
}
#endif

class DestructorLockingObject
    : public GarbageCollected<DestructorLockingObject> {
 public:
  DestructorLockingObject() = default;
  virtual ~DestructorLockingObject() { ++destructor_calls_; }

  static int destructor_calls_;
  void Trace(blink::Visitor* visitor) {}
};

int DestructorLockingObject::destructor_calls_ = 0;

class CrossThreadWeakPersistentTester : public AlternatingThreadTester {
 private:
  void MainThreadMain() override {
    // Create an object in the worker thread, have a CrossThreadWeakPersistent
    // pointing to it on the main thread, run a GC in the worker thread, and see
    // if the CrossThreadWeakPersistent is cleared.

    DestructorLockingObject::destructor_calls_ = 0;

    // Step 1: Initiate a worker thread, and wait for |Object| to get allocated
    // on the worker thread.
    SwitchToWorkerThread();

    // Step 3: Set up a CrossThreadWeakPersistent.
    ASSERT_TRUE(object_);
    EXPECT_EQ(0, DestructorLockingObject::destructor_calls_);

    // Pretend we have no pointers on stack during the step 4.
    SwitchToWorkerThread();

    // Step 5: Make sure the weak persistent is cleared.
    EXPECT_FALSE(object_.Get());
    EXPECT_EQ(1, DestructorLockingObject::destructor_calls_);

    SwitchToWorkerThread();
  }

  void WorkerThreadMain() override {
    // Step 2: Create an object and store the pointer.
    object_ = MakeGarbageCollected<DestructorLockingObject>();
    SwitchToMainThread();

    // Step 4: Run a GC.
    ThreadState::Current()->CollectGarbage(
        BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
        BlinkGC::kEagerSweeping, BlinkGC::GCReason::kForcedGCForTesting);
    SwitchToMainThread();
  }

  CrossThreadWeakPersistent<DestructorLockingObject> object_;
};

TEST_F(HeapThreadTest, CrossThreadWeakPersistent) {
  CrossThreadWeakPersistentTester().Test();
}

}  // namespace heap_thread_test
}  // namespace blink
