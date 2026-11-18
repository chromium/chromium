// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Checks ThreadState::IsInGcFinalization() on a real Oilpan heap, as seen by
// the dangling pointer detector: a dead garbage collected object frees a
// native (PartitionAlloc-backed) allocation, and another dead garbage
// collected object releases its `raw_ptr` to it.
//
// The tests are observe-only: they intercept the PartitionAlloc dangling
// pointer callbacks and never crash on a dangling release. The native
// allocations come from a dedicated BRP-enabled partition, so the process-wide
// allocator needn't be configured with BackupRefPtr.

#include <array>
#include <cstdint>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "gin/v8_initializer.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/dangling_raw_ptr_checks.h"
#include "partition_alloc/partition_alloc.h"
#include "partition_alloc/partition_root.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/cppgc/heap-state.h"
#include "v8/include/cppgc/testing.h"
#include "v8/include/v8-initialization.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-snapshot.h"

#if PA_BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)

namespace blink {
namespace {

// The native, BRP-protected pointee ("B").
struct NativeB {
  int payload = 42;
};

// A partition with BackupRefPtr enabled, independent from the process-wide
// allocator configuration.
partition_alloc::PartitionRoot& BrpRoot() {
  static base::NoDestructor<partition_alloc::PartitionAllocator> allocator([] {
    partition_alloc::PartitionAllocGlobalInit(
        [](size_t) { CHECK(false) << "Out of memory"; });
    partition_alloc::PartitionOptions opts;
    opts.backup_ref_ptr = partition_alloc::PartitionOptions::kEnabled;
    return opts;
  }());
  return *allocator->root();
}

NativeB* AllocateNativeB() {
  return new (BrpRoot().Alloc(sizeof(NativeB), "NativeB")) NativeB();
}

void FreeNativeB(NativeB* b) {
  b->~NativeB();
  BrpRoot().Free(b);
}

// Intercepts the PartitionAlloc dangling pointer callbacks for the lifetime of
// the object, and records whether each free and release happened during GC
// finalization.
class DanglingEventRecorder final {
 public:
  struct Event {
    uintptr_t id = 0;
    bool in_gc_finalization = false;
  };

  DanglingEventRecorder() {
    CHECK(!g_instance);
    g_instance = this;
    old_detected_fn_ = partition_alloc::GetDanglingRawPtrDetectedFn();
    old_released_fn_ = partition_alloc::GetDanglingRawPtrReleasedFn();
    partition_alloc::SetDanglingRawPtrDetectedFn(&OnDetected);
    partition_alloc::SetDanglingRawPtrReleasedFn(&OnReleased);
  }
  ~DanglingEventRecorder() {
    partition_alloc::SetDanglingRawPtrDetectedFn(old_detected_fn_);
    partition_alloc::SetDanglingRawPtrReleasedFn(old_released_fn_);
    g_instance = nullptr;
  }

  size_t free_count() const { return free_count_; }
  const Event& free(size_t i) const { return frees_[i]; }
  const Vector<Event>& releases() const { return releases_; }

 private:
  static void OnDetected(uintptr_t id) {
    // Called from inside the allocator: must not allocate.
    DanglingEventRecorder& self = *g_instance;
    CHECK_LT(self.free_count_, self.frees_.size());
    self.frees_[self.free_count_++] = {id, ThreadState::IsInGcFinalization()};
  }

  static void OnReleased(uintptr_t id) {
    g_instance->releases_.push_back(
        Event{id, ThreadState::IsInGcFinalization()});
  }

  static DanglingEventRecorder* g_instance;

  std::array<Event, 128> frees_;
  size_t free_count_ = 0;
  Vector<Event> releases_;
  partition_alloc::DanglingRawPtrDetectedFn* old_detected_fn_ = nullptr;
  partition_alloc::DanglingRawPtrReleasedFn* old_released_fn_ = nullptr;
};

DanglingEventRecorder* DanglingEventRecorder::g_instance = nullptr;

enum class FreePhase { kPrefinalizer, kDestructor };

// "A": a garbage collected object owning a native `NativeB`.
class Owner final : public GarbageCollected<Owner> {
  USING_PRE_FINALIZER(Owner, PreFinalize);

 public:
  explicit Owner(FreePhase free_phase)
      : b_(AllocateNativeB()), free_phase_(free_phase) {}
  ~Owner() {
    if (free_phase_ == FreePhase::kDestructor) {
      FreeNativeB(b_.ExtractAsDangling());
    }
  }

  void PreFinalize() {
    if (free_phase_ == FreePhase::kPrefinalizer) {
      FreeNativeB(b_.ExtractAsDangling());
    }
  }

  NativeB* b() const { return b_; }

  void Trace(Visitor*) const {}

 private:
  // Owning. `raw_ptr` + `ExtractAsDangling()` rather than `std::unique_ptr`
  // to keep the custom partition's deleter out of the way.
  raw_ptr<NativeB> b_;
  const FreePhase free_phase_;
};

enum class ReleasePhase {
  kPrefinalizer,
  // The implicit `~raw_ptr`.
  kDestructor,
};

// "C": a garbage collected object borrowing "A"'s `NativeB` through a
// BRP-protected `raw_ptr`, and (optionally) keeping "A" alive.
class Holder final : public GarbageCollected<Holder> {
  USING_PRE_FINALIZER(Holder, PreFinalize);

 public:
  Holder(Owner* owner, NativeB* b, ReleasePhase release_phase)
      : owner_(owner), borrowed_b_(b), release_phase_(release_phase) {}
  ~Holder() = default;

  void PreFinalize() {
    if (release_phase_ == ReleasePhase::kPrefinalizer) {
      borrowed_b_ = nullptr;
    }
  }

  // Mutator-side clearing.
  void ClearBorrowedB() { borrowed_b_ = nullptr; }
  void SetBorrowedB(NativeB* b) { borrowed_b_ = b; }

  void Trace(Visitor* visitor) const { visitor->Trace(owner_); }

 private:
  Member<Owner> owner_;
  raw_ptr<NativeB> borrowed_b_;
  const ReleasePhase release_phase_;
};

// A native object holding a raw_ptr to B, owned by a garbage collected object
// through std::unique_ptr: the raw_ptr lives outside the Oilpan heap.
struct NativeHolderOfB {
  explicit NativeHolderOfB(NativeB* b) : borrowed_b(b) {}
  raw_ptr<NativeB> borrowed_b;
};

class NativeContainerHolder final
    : public GarbageCollected<NativeContainerHolder> {
 public:
  NativeContainerHolder(Owner* owner, NativeB* b)
      : owner_(owner), native_(std::make_unique<NativeHolderOfB>(b)) {}

  void Trace(Visitor* visitor) const { visitor->Trace(owner_); }

 private:
  Member<Owner> owner_;
  std::unique_ptr<NativeHolderOfB> native_;
};

class GcFinalizationTest : public TestSupportingGC {};

TEST_F(GcFinalizationTest, NotInFinalizationOutsideGc) {
  EXPECT_FALSE(ThreadState::IsInGcFinalization());
}

// A frees B from its pre-finalizer (atomic pause), C releases from its
// destructor (sweeping on the owning thread). Pre-finalizers always run before
// destructors, so B is dangling when C releases it.
TEST_F(GcFinalizationTest, OwnerPrefinalizerFreesHolderDestructorReleases) {
  DanglingEventRecorder recorder;
  Owner* owner = MakeGarbageCollected<Owner>(FreePhase::kPrefinalizer);
  MakeGarbageCollected<Holder>(owner, owner->b(), ReleasePhase::kDestructor);
  owner = nullptr;

  PreciselyCollectGarbage();

  ASSERT_EQ(recorder.free_count(), 1u);
  EXPECT_TRUE(recorder.free(0).in_gc_finalization);
  ASSERT_EQ(recorder.releases().size(), 1u);
  EXPECT_TRUE(recorder.releases()[0].in_gc_finalization);
}

// Both A and C pre-finalize. Either order is fine; a dangling release, if any,
// happens in the atomic pause.
TEST_F(GcFinalizationTest, BothPrefinalize) {
  DanglingEventRecorder recorder;
  Holder* holder = MakeGarbageCollected<Holder>(nullptr, nullptr,
                                                ReleasePhase::kPrefinalizer);
  Owner* owner = MakeGarbageCollected<Owner>(FreePhase::kPrefinalizer);
  holder->SetBorrowedB(owner->b());
  owner = nullptr;
  holder = nullptr;

  PreciselyCollectGarbage();

  if (recorder.free_count() == 0) {
    EXPECT_TRUE(recorder.releases().empty());
    return;
  }
  ASSERT_EQ(recorder.releases().size(), 1u);
  EXPECT_TRUE(recorder.releases()[0].in_gc_finalization);
}

// Both A and C are torn down from their destructors. The sweeper doesn't
// guarantee an order: either there is no dangling interval, or the release
// happens during sweeping.
TEST_F(GcFinalizationTest, BothDestructorsAnyOrder) {
  DanglingEventRecorder recorder;
  Owner* owner = MakeGarbageCollected<Owner>(FreePhase::kDestructor);
  MakeGarbageCollected<Holder>(owner, owner->b(), ReleasePhase::kDestructor);
  owner = nullptr;

  PreciselyCollectGarbage();

  if (recorder.free_count() == 0) {
    EXPECT_TRUE(recorder.releases().empty());
    return;
  }
  ASSERT_EQ(recorder.releases().size(), 1u);
  EXPECT_TRUE(recorder.releases()[0].in_gc_finalization);
}

// C releases first (pre-finalizer), then A frees B (destructor): no dangling
// interval, hence no callbacks at all.
TEST_F(GcFinalizationTest, HolderReleasesBeforeOwnerFrees) {
  DanglingEventRecorder recorder;
  Owner* owner = MakeGarbageCollected<Owner>(FreePhase::kDestructor);
  MakeGarbageCollected<Holder>(owner, owner->b(), ReleasePhase::kPrefinalizer);
  owner = nullptr;

  PreciselyCollectGarbage();

  EXPECT_EQ(recorder.free_count(), 0u);
  EXPECT_TRUE(recorder.releases().empty());
}

// A dies and frees B; live C releases B later from mutator code: not exempt.
TEST_F(GcFinalizationTest, LiveHolderClearingInMutatorIsNotExempt) {
  DanglingEventRecorder recorder;
  Owner* owner = MakeGarbageCollected<Owner>(FreePhase::kDestructor);
  // No `Member<Owner>`, so that the owner can die while the holder lives.
  Persistent<Holder> holder = MakeGarbageCollected<Holder>(
      nullptr, owner->b(), ReleasePhase::kDestructor);
  owner = nullptr;

  PreciselyCollectGarbage();
  ASSERT_EQ(recorder.free_count(), 1u);
  EXPECT_TRUE(recorder.free(0).in_gc_finalization);
  EXPECT_TRUE(recorder.releases().empty());

  holder->ClearBorrowedB();
  ASSERT_EQ(recorder.releases().size(), 1u);
  EXPECT_FALSE(recorder.releases()[0].in_gc_finalization);
  holder.Clear();
  PreciselyCollectGarbage();
}

// The exemption only looks at the release: B freed by mutator code and
// released by a dying C is exempt too.
TEST_F(GcFinalizationTest, FreeFromMutatorHolderDestructorReleases) {
  DanglingEventRecorder recorder;
  NativeB* b = AllocateNativeB();
  MakeGarbageCollected<Holder>(nullptr, b, ReleasePhase::kDestructor);

  FreeNativeB(b);
  ASSERT_EQ(recorder.free_count(), 1u);
  EXPECT_FALSE(recorder.free(0).in_gc_finalization);

  PreciselyCollectGarbage();

  ASSERT_EQ(recorder.releases().size(), 1u);
  EXPECT_TRUE(recorder.releases()[0].in_gc_finalization);
}

// The raw_ptr lives in a native object owned by the dead object, outside the
// Oilpan heap. Its release still happens during sweeping.
TEST_F(GcFinalizationTest, NativeContainerReleasesDuringSweeping) {
  DanglingEventRecorder recorder;
  Owner* owner = MakeGarbageCollected<Owner>(FreePhase::kPrefinalizer);
  MakeGarbageCollected<NativeContainerHolder>(owner, owner->b());
  owner = nullptr;

  PreciselyCollectGarbage();

  ASSERT_EQ(recorder.free_count(), 1u);
  ASSERT_EQ(recorder.releases().size(), 1u);
  EXPECT_TRUE(recorder.releases()[0].in_gc_finalization);
}

// Many owner/holder pairs dying in one GC.
TEST_F(GcFinalizationTest, ManyConcurrentOwnersAndHolders) {
  DanglingEventRecorder recorder;
  constexpr size_t kPairCount = 40;
  for (size_t i = 0; i < kPairCount; ++i) {
    Owner* owner = MakeGarbageCollected<Owner>(FreePhase::kPrefinalizer);
    MakeGarbageCollected<Holder>(owner, owner->b(), ReleasePhase::kDestructor);
  }

  PreciselyCollectGarbage();

  ASSERT_EQ(recorder.free_count(), kPairCount);
  ASSERT_EQ(recorder.releases().size(), kPairCount);
  for (const DanglingEventRecorder::Event& release : recorder.releases()) {
    EXPECT_TRUE(release.in_gc_finalization);
  }
}

struct OwnedNativeB {
  explicit OwnedNativeB(NativeB* b) : b(b) {}
  ~OwnedNativeB() {
    if (b) {
      FreeNativeB(b.ExtractAsDangling());
    }
  }
  raw_ptr<NativeB> b;
};

// Weak-container cleanup runs in the atomic pause after marking and before
// pre-finalizers and sweeping. Removing a dead key's entry frees B; the dead
// key's destructor releases it during sweeping.
TEST_F(GcFinalizationTest, WeakContainerCleanupFreesHolderDestructorReleases) {
  DanglingEventRecorder recorder;
  using WeakMap =
      GCedHeapHashMap<WeakMember<Holder>, std::unique_ptr<OwnedNativeB>>;
  Persistent<WeakMap> map = MakeGarbageCollected<WeakMap>();

  NativeB* b = AllocateNativeB();
  Holder* holder =
      MakeGarbageCollected<Holder>(nullptr, b, ReleasePhase::kDestructor);
  map->insert(holder, std::make_unique<OwnedNativeB>(b));
  holder = nullptr;

  PreciselyCollectGarbage();

  ASSERT_EQ(recorder.free_count(), 1u);
  EXPECT_TRUE(recorder.free(0).in_gc_finalization);
  ASSERT_EQ(recorder.releases().size(), 1u);
  EXPECT_TRUE(recorder.releases()[0].in_gc_finalization);

  map.Clear();
  PreciselyCollectGarbage();
}

class CustomWeakOwner final : public GarbageCollected<CustomWeakOwner> {
 public:
  explicit CustomWeakOwner(Holder* holder, NativeB* b)
      : holder_(holder), b_(b) {}
  ~CustomWeakOwner() {
    if (b_) {
      FreeNativeB(b_.ExtractAsDangling());
    }
  }

  void Trace(Visitor* visitor) const {
    visitor->RegisterWeakCallbackMethod<
        CustomWeakOwner, &CustomWeakOwner::ProcessCustomWeakness>(this);
  }

 private:
  void ProcessCustomWeakness(const LivenessBroker& info) {
    if (holder_ && !info.IsHeapObjectAlive(holder_)) {
      holder_ = nullptr;
      if (b_) {
        FreeNativeB(b_.ExtractAsDangling());
      }
    }
  }

  UntracedMember<Holder> holder_;
  raw_ptr<NativeB> b_;
};

// Custom weak callbacks (`RegisterWeakCallbackMethod`) also run in the atomic
// pause after marking and before pre-finalizers and sweeping.
TEST_F(GcFinalizationTest, CustomWeakCallbackFreesHolderDestructorReleases) {
  DanglingEventRecorder recorder;
  NativeB* b = AllocateNativeB();
  Holder* holder =
      MakeGarbageCollected<Holder>(nullptr, b, ReleasePhase::kDestructor);
  Persistent<CustomWeakOwner> weak_owner =
      MakeGarbageCollected<CustomWeakOwner>(holder, b);
  holder = nullptr;

  PreciselyCollectGarbage();

  ASSERT_EQ(recorder.free_count(), 1u);
  EXPECT_TRUE(recorder.free(0).in_gc_finalization);
  ASSERT_EQ(recorder.releases().size(), 1u);
  EXPECT_TRUE(recorder.releases()[0].in_gc_finalization);

  weak_owner.Clear();
  PreciselyCollectGarbage();
}

// Incremental marking steps run interleaved with mutator code and aren't
// finalization; the atomic pause and sweeping of the same GC are.
TEST_F(GcFinalizationTest, IncrementalMarking) {
  DanglingEventRecorder recorder;
  Owner* owner = MakeGarbageCollected<Owner>(FreePhase::kPrefinalizer);
  MakeGarbageCollected<Holder>(owner, owner->b(), ReleasePhase::kDestructor);
  owner = nullptr;

  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  EXPECT_FALSE(ThreadState::IsInGcFinalization());
  driver.TriggerMarkingSteps();
  EXPECT_FALSE(ThreadState::IsInGcFinalization());
  driver.FinishGC();

  ASSERT_EQ(recorder.free_count(), 1u);
  EXPECT_TRUE(recorder.free(0).in_gc_finalization);
  ASSERT_EQ(recorder.releases().size(), 1u);
  EXPECT_TRUE(recorder.releases()[0].in_gc_finalization);
}

void EnsureV8SnapshotLoadedForTest() {
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  gin::V8Initializer::LoadV8Snapshot();
  v8::StartupData snapshot;
  gin::V8Initializer::GetV8ExternalSnapshotData(&snapshot);
  if (snapshot.data) {
    v8::V8::SetSnapshotDataBlob(&snapshot);
  }
#endif
}

// Unified heap GCs defer sweeping; the next GC finishes it on the owning
// thread before its own marking and atomic pause. Isolate disposal terminates
// the CppHeap.
void RunUnifiedHeapTestOnWorker(
    scoped_refptr<base::SingleThreadTaskRunner> main_runner,
    base::RunLoop* run_loop) {
  EnsureV8SnapshotLoadedForTest();

  cppgc::StackStartMarker stack_start_marker;
  ThreadState* state = ThreadState::AttachCurrentThread(stack_start_marker);
  v8::Isolate* isolate = V8PerIsolateData::Initialize(
      base::SingleThreadTaskRunner::GetCurrentDefault(), nullptr, nullptr,
      V8PerIsolateData::V8ContextSnapshotMode::kDontUseSnapshot, nullptr,
      nullptr, state->ReleaseCppHeap());
  state->AttachToIsolate(isolate, nullptr);

  {
    DanglingEventRecorder recorder;
    Owner* owner = MakeGarbageCollected<Owner>(FreePhase::kPrefinalizer);
    MakeGarbageCollected<Holder>(owner, owner->b(), ReleasePhase::kDestructor);
    owner = nullptr;

    {
      cppgc::testing::OverrideEmbedderStackStateScope stack_scope(
          state->heap_handle(), cppgc::EmbedderStackState::kNoHeapPointers);
      isolate->MemoryPressureNotification(v8::MemoryPressureLevel::kCritical);
    }

    // The owner pre-finalized in the atomic pause; the holder's destructor
    // awaits deferred sweeping. Mutator code isn't finalization even while
    // `IsSweeping()`.
    EXPECT_EQ(recorder.free_count(), 1u);
    EXPECT_TRUE(recorder.free_count() == 1u &&
                recorder.free(0).in_gc_finalization);
    EXPECT_TRUE(cppgc::subtle::HeapState::IsSweeping(state->heap_handle()));
    EXPECT_FALSE(ThreadState::IsInGcFinalization());
    EXPECT_TRUE(recorder.releases().empty());

    {
      cppgc::testing::OverrideEmbedderStackStateScope stack_scope(
          state->heap_handle(), cppgc::EmbedderStackState::kNoHeapPointers);
      isolate->MemoryPressureNotification(v8::MemoryPressureLevel::kCritical);
    }
    state->cpp_heap().FinishSweepingForTesting();

    EXPECT_EQ(recorder.releases().size(), 1u);
    EXPECT_TRUE(recorder.releases().size() == 1u &&
                recorder.releases()[0].in_gc_finalization);
  }

  // Disposing the isolate terminates its CppHeap, finalizing every remaining
  // object after ThreadState detached from the isolate.
  {
    DanglingEventRecorder recorder;
    Owner* owner = MakeGarbageCollected<Owner>(FreePhase::kPrefinalizer);
    MakeGarbageCollected<Holder>(owner, owner->b(), ReleasePhase::kDestructor);
    owner = nullptr;

    V8PerIsolateData::WillBeDestroyed(isolate);
    EXPECT_FALSE(ThreadState::IsInGcFinalization());
    V8PerIsolateData::Destroy(isolate);

    EXPECT_EQ(recorder.free_count(), 1u);
    EXPECT_TRUE(recorder.free_count() == 1u &&
                recorder.free(0).in_gc_finalization);
    EXPECT_EQ(recorder.releases().size(), 1u);
    EXPECT_TRUE(recorder.releases().size() == 1u &&
                recorder.releases()[0].in_gc_finalization);
  }
  EXPECT_FALSE(ThreadState::IsInGcFinalization());
  ThreadState::DetachCurrentThread();

  PostCrossThreadTask(*main_runner, FROM_HERE,
                      CrossThreadBindOnce(&base::RunLoop::Quit,
                                          CrossThreadUnretained(run_loop)));
}

TEST_F(GcFinalizationTest, UnifiedHeapDeferredSweepingAndIsolateDisposal) {
  base::RunLoop run_loop;
  std::unique_ptr<NonMainThread> thread = NonMainThread::CreateThread(
      ThreadCreationParams(ThreadType::kTestThread)
          .SetThreadNameForTest("oilpan unified heap test"));
  PostCrossThreadTask(
      *thread->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&RunUnifiedHeapTestOnWorker,
                          base::SingleThreadTaskRunner::GetCurrentDefault(),
                          CrossThreadUnretained(&run_loop)));
  run_loop.Run();
  thread.reset();
}

// Heap termination finalizes every remaining object and is finalization too.
void RunHeapTerminationTestOnWorker(
    scoped_refptr<base::SingleThreadTaskRunner> main_runner,
    base::RunLoop* run_loop) {
  cppgc::StackStartMarker stack_start_marker;
  ThreadState::AttachCurrentThread(stack_start_marker);
  {
    DanglingEventRecorder recorder;
    Owner* owner = MakeGarbageCollected<Owner>(FreePhase::kPrefinalizer);
    MakeGarbageCollected<Holder>(owner, owner->b(), ReleasePhase::kDestructor);
    owner = nullptr;

    ThreadState::DetachCurrentThread();

    EXPECT_EQ(recorder.free_count(), 1u);
    EXPECT_TRUE(recorder.free_count() == 1u &&
                recorder.free(0).in_gc_finalization);
    EXPECT_EQ(recorder.releases().size(), 1u);
    EXPECT_TRUE(recorder.releases().size() == 1u &&
                recorder.releases()[0].in_gc_finalization);
  }
  PostCrossThreadTask(*main_runner, FROM_HERE,
                      CrossThreadBindOnce(&base::RunLoop::Quit,
                                          CrossThreadUnretained(run_loop)));
}

TEST_F(GcFinalizationTest, HeapTermination) {
  base::RunLoop run_loop;
  std::unique_ptr<NonMainThread> thread = NonMainThread::CreateThread(
      ThreadCreationParams(ThreadType::kTestThread)
          .SetThreadNameForTest("oilpan heap termination test"));
  PostCrossThreadTask(
      *thread->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&RunHeapTerminationTestOnWorker,
                          base::SingleThreadTaskRunner::GetCurrentDefault(),
                          CrossThreadUnretained(&run_loop)));
  run_loop.Run();
  thread.reset();
}

}  // namespace
}  // namespace blink

#endif  // PA_BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
