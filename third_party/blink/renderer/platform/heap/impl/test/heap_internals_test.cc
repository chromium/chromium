// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"
#include "third_party/blink/renderer/platform/heap/heap_test_objects.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

namespace {
class HeapInternalsTest : public TestSupportingGC {};
}  // namespace

namespace {

class IntWrapper : public GarbageCollected<IntWrapper> {
 public:
  virtual ~IntWrapper() {
    destructor_calls_.fetch_add(1, std::memory_order_relaxed);
  }

  static std::atomic_int destructor_calls_;
  void Trace(Visitor* visitor) const {}

  int Value() const { return x_; }

  bool operator==(const IntWrapper& other) const {
    return other.Value() == Value();
  }

  unsigned GetHash() { return IntHash<int>::GetHash(x_); }

  explicit IntWrapper(int x) : x_(x) {}

  IntWrapper() = delete;

 private:
  int x_;
};
std::atomic_int IntWrapper::destructor_calls_{0};

struct SameSizeAsPersistent {
  void* pointers[2];
#if DCHECK_IS_ON()
  void* pointers_dcheck_[2];
#endif
#if BUILDFLAG(RAW_HEAP_SNAPSHOTS)
  PersistentLocation location;
#endif  // BUILDFLAG(RAW_HEAP_SNAPSHOTS)
};
ASSERT_SIZE(Persistent<IntWrapper>, SameSizeAsPersistent);

}  // namespace

class TestGCScope {
  STACK_ALLOCATED();

 public:
  explicit TestGCScope(BlinkGC::StackState state) {
    DCHECK(ThreadState::Current()->CheckThread());
    ThreadState::Current()->Heap().stats_collector()->NotifyMarkingStarted(
        BlinkGC::CollectionType::kMajor, BlinkGC::GCReason::kForcedGCForTesting,
        true /* is_forced_gc */);
    ThreadState::Current()->AtomicPauseMarkPrologue(
        BlinkGC::CollectionType::kMajor, state, BlinkGC::kAtomicMarking,
        BlinkGC::GCReason::kForcedGCForTesting);
  }
  ~TestGCScope() {
    ThreadState::Current()->AtomicPauseMarkEpilogue(BlinkGC::kAtomicMarking);
    ThreadState::Current()->AtomicPauseSweepAndCompact(
        BlinkGC::CollectionType::kMajor, BlinkGC::kAtomicMarking,
        BlinkGC::kEagerSweeping);
    ThreadState::Current()->AtomicPauseEpilogue();
    ThreadState::Current()->CompleteSweep();
  }
};

namespace {
class SimpleObject : public GarbageCollected<SimpleObject> {
 public:
  SimpleObject() = default;
  virtual void Trace(Visitor* visitor) const {}
  char GetPayload(int i) { return payload[i]; }
  // This virtual method is unused but it is here to make sure
  // that this object has a vtable. This object is used
  // as the super class for objects that also have garbage
  // collected mixins and having a virtual here makes sure
  // that adjustment is needed both for marking and for isAlive
  // checks.
  virtual void VirtualMethod() {}

 protected:
  char payload[64];
};

class LargeHeapObject final : public GarbageCollected<LargeHeapObject> {
 public:
  LargeHeapObject() { int_wrapper_ = MakeGarbageCollected<IntWrapper>(23); }
  ~LargeHeapObject() { destructor_calls_++; }

  char Get(size_t i) { return data_[i]; }
  void Set(size_t i, char c) { data_[i] = c; }
  size_t length() { return kLength; }
  void Trace(Visitor* visitor) const { visitor->Trace(int_wrapper_); }
  static int destructor_calls_;

 private:
  static const size_t kLength = 1024 * 1024;
  Member<IntWrapper> int_wrapper_;
  char data_[kLength];
};
int LargeHeapObject::destructor_calls_ = 0;

void ExpectObjectMarkedAndUnmark(MarkingWorklist* worklist, void* expected) {
  MarkingItem item;
  CHECK(worklist->Pop(0, &item));
  CHECK_EQ(expected, item.base_object_payload);
  HeapObjectHeader* header =
      HeapObjectHeader::FromPayload(item.base_object_payload);
  CHECK(header->IsMarked());
  header->Unmark();
  CHECK(worklist->IsGlobalEmpty());
}
}  // namespace

TEST_F(HeapInternalsTest, CheckAndMarkPointer) {
  // This test ensures that conservative marking primitives can use any address
  // contained within an object to mark the corresponding object.

  ThreadHeap& heap = ThreadState::Current()->Heap();
  ClearOutOldGarbage();

  Vector<Address> object_addresses;
  Vector<Address> end_addresses;
  for (int i = 0; i < 10; i++) {
    auto* object = MakeGarbageCollected<SimpleObject>();
    Address object_address = reinterpret_cast<Address>(object);
    object_addresses.push_back(object_address);
    end_addresses.push_back(object_address + sizeof(SimpleObject) - 1);
  }
  Address large_object_address =
      reinterpret_cast<Address>(MakeGarbageCollected<LargeHeapObject>());
  Address large_object_end_address =
      large_object_address + sizeof(LargeHeapObject) - 1;

  {
    TestGCScope scope(BlinkGC::kHeapPointersOnStack);
    MarkingVisitor visitor(ThreadState::Current(),
                           MarkingVisitor::kGlobalMarking);
    // Record marking speed as counter generation requires valid marking timings
    // for heaps >1MB.
    ThreadHeapStatsCollector::Scope stats_scope(
        heap.stats_collector(),
        ThreadHeapStatsCollector::kAtomicPauseMarkTransitiveClosure);

    // Conservative marker should find the interesting objects by using anything
    // between object start and end.
    MarkingWorklist* worklist = heap.GetMarkingWorklist();
    CHECK(worklist->IsGlobalEmpty());
    for (wtf_size_t i = 0; i < object_addresses.size(); i++) {
      heap.CheckAndMarkPointer(&visitor, object_addresses[i]);
      ExpectObjectMarkedAndUnmark(worklist, object_addresses[i]);
      heap.CheckAndMarkPointer(&visitor, end_addresses[i]);
      ExpectObjectMarkedAndUnmark(worklist, object_addresses[i]);
    }
    heap.CheckAndMarkPointer(&visitor, large_object_address);
    ExpectObjectMarkedAndUnmark(worklist, large_object_address);
    heap.CheckAndMarkPointer(&visitor, large_object_end_address);
    ExpectObjectMarkedAndUnmark(worklist, large_object_address);
  }

  // This forces a GC without stack scanning which results in the objects
  // being collected. This will also rebuild the above mentioned freelists,
  // however we don't rely on that below since we don't have any allocations.
  ClearOutOldGarbage();

  {
    TestGCScope scope(BlinkGC::kHeapPointersOnStack);
    MarkingVisitor visitor(ThreadState::Current(),
                           MarkingVisitor::kGlobalMarking);
    // Record marking speed as counter generation requires valid marking timings
    // for heaps >1MB.
    ThreadHeapStatsCollector::Scope stats_scope(
        heap.stats_collector(),
        ThreadHeapStatsCollector::kAtomicPauseMarkTransitiveClosure);

    // After collecting all interesting objects the conservative marker should
    // not find them anymore.
    MarkingWorklist* worklist = heap.GetMarkingWorklist();
    CHECK(worklist->IsGlobalEmpty());
    for (wtf_size_t i = 0; i < object_addresses.size(); i++) {
      heap.CheckAndMarkPointer(&visitor, object_addresses[i]);
      CHECK(worklist->IsGlobalEmpty());
      heap.CheckAndMarkPointer(&visitor, end_addresses[i]);
      CHECK(worklist->IsGlobalEmpty());
    }
    heap.CheckAndMarkPointer(&visitor, large_object_address);
    CHECK(worklist->IsGlobalEmpty());
    heap.CheckAndMarkPointer(&visitor, large_object_end_address);
    CHECK(worklist->IsGlobalEmpty());
  }
}

TEST_F(HeapInternalsTest, LazySweepingLargeObjectPages) {
  // Disable concurrent sweeping to check lazy sweeping on allocation.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      blink::features::kBlinkHeapConcurrentSweeping);

  ClearOutOldGarbage();

  // Create free lists that can be reused for IntWrappers created in
  // MakeGarbageCollected<LargeHeapObject>().
  Persistent<IntWrapper> p1 = MakeGarbageCollected<IntWrapper>(1);
  for (int i = 0; i < 100; i++) {
    MakeGarbageCollected<IntWrapper>(i);
  }
  Persistent<IntWrapper> p2 = MakeGarbageCollected<IntWrapper>(2);
  PreciselyCollectGarbage();
  PreciselyCollectGarbage();

  LargeHeapObject::destructor_calls_ = 0;
  EXPECT_EQ(0, LargeHeapObject::destructor_calls_);
  for (int i = 0; i < 10; i++)
    MakeGarbageCollected<LargeHeapObject>();
  ThreadState::Current()->CollectGarbageForTesting(
      BlinkGC::CollectionType::kMajor, BlinkGC::kNoHeapPointersOnStack,
      BlinkGC::kAtomicMarking, BlinkGC::kConcurrentAndLazySweeping,
      BlinkGC::GCReason::kForcedGCForTesting);
  EXPECT_EQ(0, LargeHeapObject::destructor_calls_);
  for (int i = 0; i < 10; i++) {
    MakeGarbageCollected<LargeHeapObject>();
    EXPECT_EQ(i + 1, LargeHeapObject::destructor_calls_);
  }
  MakeGarbageCollected<LargeHeapObject>();
  MakeGarbageCollected<LargeHeapObject>();
  EXPECT_EQ(10, LargeHeapObject::destructor_calls_);
  ThreadState::Current()->CollectGarbageForTesting(
      BlinkGC::CollectionType::kMajor, BlinkGC::kNoHeapPointersOnStack,
      BlinkGC::kAtomicMarking, BlinkGC::kConcurrentAndLazySweeping,
      BlinkGC::GCReason::kForcedGCForTesting);
  EXPECT_EQ(10, LargeHeapObject::destructor_calls_);
  PreciselyCollectGarbage();
  EXPECT_EQ(22, LargeHeapObject::destructor_calls_);
}

namespace {
class Mixin : public GarbageCollectedMixin {
 public:
  void Trace(Visitor* visitor) const override {}

  virtual char GetPayload(int i) { return padding_[i]; }

 protected:
  int padding_[8];
};

class UseMixin : public SimpleObject, public Mixin {
 public:
  UseMixin() {
    // Verify that WTF::IsGarbageCollectedType<> works as expected for mixins.
    static_assert(WTF::IsGarbageCollectedType<UseMixin>::value,
                  "IsGarbageCollectedType<> sanity check failed for GC mixin.");
    trace_count_ = 0;
  }

  static int trace_count_;
  void Trace(Visitor* visitor) const override {
    SimpleObject::Trace(visitor);
    Mixin::Trace(visitor);
    ++trace_count_;
  }
};
int UseMixin::trace_count_ = 0;
}  // namespace

TEST_F(HeapInternalsTest, NeedsAdjustPointer) {
  // class Mixin : public GarbageCollectedMixin {};
  static_assert(NeedsAdjustPointer<Mixin>::value,
                "A Mixin pointer needs adjustment");
  static_assert(NeedsAdjustPointer<const Mixin>::value,
                "A const Mixin pointer needs adjustment");

  // class SimpleObject : public GarbageCollected<SimpleObject> {};
  static_assert(!NeedsAdjustPointer<SimpleObject>::value,
                "A SimpleObject pointer does not need adjustment");
  static_assert(!NeedsAdjustPointer<const SimpleObject>::value,
                "A const SimpleObject pointer does not need adjustment");

  // class UseMixin : public SimpleObject, public Mixin {};
  static_assert(!NeedsAdjustPointer<UseMixin>::value,
                "A UseMixin pointer does not need adjustment");
  static_assert(!NeedsAdjustPointer<const UseMixin>::value,
                "A const UseMixin pointer does not need adjustment");
}

namespace {
class DeepEagerly final : public GarbageCollected<DeepEagerly> {
 public:
  explicit DeepEagerly(DeepEagerly* next) : next_(next) {}

  void Trace(Visitor* visitor) const {
    int calls = ++s_trace_calls_;
    if (s_trace_lazy_ <= 2)
      visitor->Trace(next_);
    if (s_trace_calls_ == calls)
      s_trace_lazy_++;
  }

  Member<DeepEagerly> next_;

  static int s_trace_calls_;
  static int s_trace_lazy_;
};
int DeepEagerly::s_trace_calls_ = 0;
int DeepEagerly::s_trace_lazy_ = 0;
}  // namespace

TEST_F(HeapInternalsTest, TraceDeepEagerly) {
// The allocation & GC overhead is considerable for this test,
// straining debug builds and lower-end targets too much to be
// worth running.
#if !DCHECK_IS_ON() && !defined(OS_ANDROID)
  DeepEagerly* obj = nullptr;
  for (int i = 0; i < 10000000; i++)
    obj = MakeGarbageCollected<DeepEagerly>(obj);

  Persistent<DeepEagerly> persistent(obj);
  PreciselyCollectGarbage();

  // Verify that the DeepEagerly chain isn't completely unravelled
  // by performing eager trace() calls, but the explicit mark
  // stack is switched once some nesting limit is exceeded.
  EXPECT_GT(DeepEagerly::s_trace_lazy_, 2);
#endif
}

#if defined(ADDRESS_SANITIZER)
TEST_F(HeapInternalsTest, SuccessfulUnsanitizedAccessToObjectHeader) {
  auto* ptr = MakeGarbageCollected<IntWrapper>(1);
  HeapObjectHeader* header = HeapObjectHeader::FromPayload(ptr);
  auto* low = reinterpret_cast<uint16_t*>(header);
  volatile uint16_t half = internal::AsUnsanitizedAtomic(low)->load();
  internal::AsUnsanitizedAtomic(low)->store(half);
}

TEST(HeapInternalsDeathTest, DieOnPoisonedObjectHeaderAccess) {
  auto* ptr = MakeGarbageCollected<IntWrapper>(1);
  HeapObjectHeader* header = HeapObjectHeader::FromPayload(ptr);
  auto* low = reinterpret_cast<uint16_t*>(header);
  auto access = [low] {
    volatile uint16_t half = WTF::AsAtomicPtr(low)->load();
    WTF::AsAtomicPtr(low)->store(half);
  };
  EXPECT_DEATH(access(), "");
}
#endif  // ADDRESS_SANITIZER

namespace {
class LargeMixin : public GarbageCollected<LargeMixin>, public Mixin {
 protected:
  char data[65536];
};
}  // namespace

TEST(HeapInternalsDeathTest, LargeGarbageCollectedMixin) {
  EXPECT_DEATH(MakeGarbageCollected<LargeMixin>(AdditionalBytes(1)), "");
}

namespace {
class PreFinalizerAllocationForbidden
    : public GarbageCollected<PreFinalizerAllocationForbidden> {
  USING_PRE_FINALIZER(PreFinalizerAllocationForbidden, Dispose);

 public:
  void Dispose() {
    EXPECT_FALSE(ThreadState::Current()->IsAllocationAllowed());
#if DCHECK_IS_ON()
    EXPECT_DEATH(MakeGarbageCollected<IntWrapper>(1), "");
#endif  // DCHECK_IS_ON()
  }

  void Trace(Visitor* visitor) const {}
};
}  // namespace

TEST(HeapInternalsDeathTest, PreFinalizerAllocationForbidden) {
  MakeGarbageCollected<PreFinalizerAllocationForbidden>();
  TestSupportingGC::PreciselyCollectGarbage();
}

namespace {
class Bar : public GarbageCollected<Bar> {
 public:
  Bar() : magic_(kMagic) { live_++; }

  virtual ~Bar() {
    EXPECT_TRUE(magic_ == kMagic);
    magic_ = 0;
    live_--;
  }
  bool HasBeenFinalized() const { return !magic_; }

  virtual void Trace(Visitor* visitor) const {}
  static unsigned live_;

 protected:
  static const int kMagic = 1337;
  int magic_;
};

unsigned Bar::live_ = 0;

class Bars final : public Bar {
 public:
  Bars() {
    for (auto& bar : bars_) {
      bar = MakeGarbageCollected<Bar>();
      width_++;
    }
  }

  void Trace(Visitor* visitor) const override {
    Bar::Trace(visitor);
    for (unsigned i = 0; i < width_; i++)
      visitor->Trace(bars_[i]);
  }

  unsigned GetWidth() const { return width_; }

  static const unsigned kWidth = 7500;

 private:
  unsigned width_ = 0;
  Member<Bar> bars_[kWidth];
};
}  // namespace

TEST_F(HeapInternalsTest, WideTest) {
  ClearOutOldGarbage();
  Bar::live_ = 0;
  {
    auto* bars = MakeGarbageCollected<Bars>();
    unsigned width = Bars::kWidth;
    EXPECT_EQ(width + 1, Bar::live_);
    ConservativelyCollectGarbage();
    EXPECT_EQ(width + 1, Bar::live_);
    // Use bars here to make sure that it will be on the stack
    // for the conservative stack scan to find.
    EXPECT_EQ(width, bars->GetWidth());
  }
  EXPECT_EQ(Bars::kWidth + 1, Bar::live_);
  PreciselyCollectGarbage();
  EXPECT_EQ(0u, Bar::live_);
}

namespace {
// The accounting for memory includes the memory used by rounding up object
// sizes. This is done in a different way on 32 bit and 64 bit, so we have to
// have some slack in the tests.
template <typename T>
void CheckWithSlack(T expected, T actual, int slack) {
  EXPECT_LE(expected, actual);
  EXPECT_GE((intptr_t)expected + slack, (intptr_t)actual);
}
}  // namespace

TEST_F(HeapInternalsTest, LargeHeapObjects) {
  ThreadHeap& heap = ThreadState::Current()->Heap();
  ClearOutOldGarbage();
  size_t initial_object_payload_size = heap.ObjectPayloadSizeForTesting();
  size_t initial_allocated_space =
      heap.stats_collector()->allocated_space_bytes();
  IntWrapper::destructor_calls_ = 0;
  LargeHeapObject::destructor_calls_ = 0;
  {
    int slack =
        8;  // LargeHeapObject points to an IntWrapper that is also allocated.
    Persistent<LargeHeapObject> object =
        MakeGarbageCollected<LargeHeapObject>();
#if DCHECK_IS_ON()
    DCHECK(ThreadState::Current()->Heap().FindPageFromAddress(object));
    DCHECK(ThreadState::Current()->Heap().FindPageFromAddress(
        reinterpret_cast<char*>(object.Get()) + sizeof(LargeHeapObject) - 1));
#endif
    ClearOutOldGarbage();
    size_t after_allocation = heap.stats_collector()->allocated_space_bytes();
    {
      object->Set(0, 'a');
      EXPECT_EQ('a', object->Get(0));
      object->Set(object->length() - 1, 'b');
      EXPECT_EQ('b', object->Get(object->length() - 1));
      size_t expected_large_heap_object_payload_size =
          ThreadHeap::AllocationSizeFromSize(sizeof(LargeHeapObject)) -
          sizeof(HeapObjectHeader);
      size_t expected_object_payload_size =
          expected_large_heap_object_payload_size + sizeof(IntWrapper);
      size_t actual_object_payload_size =
          heap.ObjectPayloadSizeForTesting() - initial_object_payload_size;
      CheckWithSlack(expected_object_payload_size, actual_object_payload_size,
                     slack);
      // There is probably space for the IntWrapper in a heap page without
      // allocating extra pages. However, the IntWrapper allocation might cause
      // the addition of a heap page.
      size_t large_object_allocation_size =
          sizeof(LargeObjectPage) + expected_large_heap_object_payload_size;
      size_t allocated_space_lower_bound =
          initial_allocated_space + large_object_allocation_size;
      size_t allocated_space_upper_bound =
          allocated_space_lower_bound + slack + kBlinkPageSize;
      EXPECT_LE(allocated_space_lower_bound, after_allocation);
      EXPECT_LE(after_allocation, allocated_space_upper_bound);
      EXPECT_EQ(0, IntWrapper::destructor_calls_);
      EXPECT_EQ(0, LargeHeapObject::destructor_calls_);
      for (int i = 0; i < 10; i++)
        object = MakeGarbageCollected<LargeHeapObject>();
    }
    ClearOutOldGarbage();
    EXPECT_EQ(after_allocation,
              heap.stats_collector()->allocated_space_bytes());
    EXPECT_EQ(10, IntWrapper::destructor_calls_);
    EXPECT_EQ(10, LargeHeapObject::destructor_calls_);
  }
  ClearOutOldGarbage();
  EXPECT_TRUE(initial_object_payload_size ==
              heap.ObjectPayloadSizeForTesting());
  EXPECT_EQ(initial_allocated_space,
            heap.stats_collector()->allocated_space_bytes());
  EXPECT_EQ(11, IntWrapper::destructor_calls_);
  EXPECT_EQ(11, LargeHeapObject::destructor_calls_);
  PreciselyCollectGarbage();
}

namespace {
class MixinA : public GarbageCollectedMixin {
 public:
  MixinA() : obj_(MakeGarbageCollected<IntWrapper>(100)) {}
  void Trace(Visitor* visitor) const override {
    trace_count_++;
    visitor->Trace(obj_);
  }

  static int trace_count_;

  Member<IntWrapper> obj_;
};

int MixinA::trace_count_ = 0;

class MixinB : public GarbageCollectedMixin {
 public:
  MixinB() : obj_(MakeGarbageCollected<IntWrapper>(101)) {}
  void Trace(Visitor* visitor) const override { visitor->Trace(obj_); }
  Member<IntWrapper> obj_;
};

class MultipleMixins : public GarbageCollected<MultipleMixins>,
                       public MixinA,
                       public MixinB {
 public:
  MultipleMixins() : obj_(MakeGarbageCollected<IntWrapper>(102)) {}
  void Trace(Visitor* visitor) const override {
    visitor->Trace(obj_);
    MixinA::Trace(visitor);
    MixinB::Trace(visitor);
  }
  Member<IntWrapper> obj_;
};

static const bool kIsMixinTrue = IsGarbageCollectedMixin<MultipleMixins>::value;
static const bool kIsMixinFalse = IsGarbageCollectedMixin<IntWrapper>::value;
}  // namespace

TEST_F(HeapInternalsTest, MultipleMixins) {
  EXPECT_TRUE(kIsMixinTrue);
  EXPECT_FALSE(kIsMixinFalse);

  ClearOutOldGarbage();
  IntWrapper::destructor_calls_ = 0;
  MultipleMixins* obj = MakeGarbageCollected<MultipleMixins>();
  {
    Persistent<MixinA> a = obj;
    PreciselyCollectGarbage();
    EXPECT_EQ(0, IntWrapper::destructor_calls_);
  }
  {
    Persistent<MixinB> b = obj;
    PreciselyCollectGarbage();
    EXPECT_EQ(0, IntWrapper::destructor_calls_);
  }
  PreciselyCollectGarbage();
  EXPECT_EQ(3, IntWrapper::destructor_calls_);
}

namespace {
class DerivedMultipleMixins : public MultipleMixins {
 public:
  DerivedMultipleMixins() : obj_(MakeGarbageCollected<IntWrapper>(103)) {}

  void Trace(Visitor* visitor) const override {
    trace_called_++;
    visitor->Trace(obj_);
    MultipleMixins::Trace(visitor);
  }

  static int trace_called_;

 private:
  Member<IntWrapper> obj_;
};
int DerivedMultipleMixins::trace_called_ = 0;
}  // namespace

TEST_F(HeapInternalsTest, DerivedMultipleMixins) {
  ClearOutOldGarbage();
  IntWrapper::destructor_calls_ = 0;
  DerivedMultipleMixins::trace_called_ = 0;

  DerivedMultipleMixins* obj = MakeGarbageCollected<DerivedMultipleMixins>();
  {
    Persistent<MixinA> a = obj;
    PreciselyCollectGarbage();
    EXPECT_EQ(0, IntWrapper::destructor_calls_);
    EXPECT_LT(0, DerivedMultipleMixins::trace_called_);
  }
  {
    Persistent<MixinB> b = obj;
    PreciselyCollectGarbage();
    EXPECT_EQ(0, IntWrapper::destructor_calls_);
    EXPECT_LT(0, DerivedMultipleMixins::trace_called_);
  }
  PreciselyCollectGarbage();
  EXPECT_EQ(4, IntWrapper::destructor_calls_);
}

namespace {
static bool AllocateAndReturnBool() {
  TestSupportingGC::ConservativelyCollectGarbage();
  return true;
}

class AllocInSuperConstructorArgumentSuper
    : public GarbageCollected<AllocInSuperConstructorArgumentSuper> {
 public:
  explicit AllocInSuperConstructorArgumentSuper(bool value) : value_(value) {}
  virtual ~AllocInSuperConstructorArgumentSuper() = default;
  virtual void Trace(Visitor* visitor) const {}
  bool Value() { return value_; }

 private:
  bool value_;
};

class AllocInSuperConstructorArgument
    : public AllocInSuperConstructorArgumentSuper {
 public:
  AllocInSuperConstructorArgument()
      : AllocInSuperConstructorArgumentSuper(AllocateAndReturnBool()) {}
};
}  // namespace

// Regression test for crbug.com/404511. Tests conservative marking of
// an object with an uninitialized vtable.
TEST_F(HeapInternalsTest, AllocationInSuperConstructorArgument) {
  AllocInSuperConstructorArgument* object =
      MakeGarbageCollected<AllocInSuperConstructorArgument>();
  EXPECT_TRUE(object);
  ThreadState::Current()->CollectAllGarbageForTesting();
}

namespace {
class TestMixinAllocationA : public GarbageCollected<TestMixinAllocationA>,
                             public GarbageCollectedMixin {
 public:
  TestMixinAllocationA() = default;

  void Trace(Visitor* visitor) const override {}
};

class TestMixinAllocationB : public TestMixinAllocationA {
 public:
  TestMixinAllocationB()
      // Construct object during a mixin construction.
      : a_(MakeGarbageCollected<TestMixinAllocationA>()) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(a_);
    TestMixinAllocationA::Trace(visitor);
  }

 private:
  Member<TestMixinAllocationA> a_;
};

class TestMixinAllocationC final : public TestMixinAllocationB {
 public:
  TestMixinAllocationC() { DCHECK(!ThreadState::Current()->IsGCForbidden()); }

  void Trace(Visitor* visitor) const override {
    TestMixinAllocationB::Trace(visitor);
  }
};
}  // namespace

TEST_F(HeapInternalsTest, NestedMixinConstruction) {
  TestMixinAllocationC* object = MakeGarbageCollected<TestMixinAllocationC>();
  EXPECT_TRUE(object);
}

TEST_F(HeapInternalsTest, IsHeapObjectAliveForConstPointer) {
  // See http://crbug.com/661363.
  auto* object = MakeGarbageCollected<SimpleObject>();
  HeapObjectHeader* header = HeapObjectHeader::FromPayload(object);
  LivenessBroker broker = internal::LivenessBrokerFactory::Create();
  EXPECT_TRUE(header->TryMark());
  EXPECT_TRUE(broker.IsHeapObjectAlive(object));
  const SimpleObject* const_object = const_cast<const SimpleObject*>(object);
  EXPECT_TRUE(broker.IsHeapObjectAlive(const_object));
}

namespace {
// This test class served a more important role while Blink
// was transitioned over to using Oilpan. That required classes
// that were hybrid, both ref-counted and on the Oilpan heap
// (the RefCountedGarbageCollected<> class providing just that.)
//
// There's no current need for having a ref-counted veneer on
// top of a GCed class, but we preserve it here to exercise the
// implementation technique that it used -- keeping an internal
// "keep alive" persistent reference that is set & cleared across
// ref-counting operations.
//
class RefCountedAndGarbageCollected final
    : public GarbageCollected<RefCountedAndGarbageCollected> {
 public:
  RefCountedAndGarbageCollected() : keep_alive_(PERSISTENT_FROM_HERE) {}
  ~RefCountedAndGarbageCollected() { ++destructor_calls_; }

  void AddRef() {
    if (UNLIKELY(!ref_count_)) {
#if DCHECK_IS_ON()
      DCHECK(ThreadState::Current()->Heap().FindPageFromAddress(
          reinterpret_cast<Address>(this)));
#endif
      keep_alive_ = this;
    }
    ++ref_count_;
  }

  void Release() {
    DCHECK_GT(ref_count_, 0);
    if (!--ref_count_)
      keep_alive_.Clear();
  }

  void Trace(Visitor* visitor) const {}

  static int destructor_calls_;

 private:
  int ref_count_ = 0;
  SelfKeepAlive<RefCountedAndGarbageCollected> keep_alive_;
};
int RefCountedAndGarbageCollected::destructor_calls_ = 0;

class SuperClass;

class PointsBack final : public GarbageCollected<PointsBack> {
 public:
  PointsBack() : back_pointer_(nullptr) { ++alive_count_; }
  ~PointsBack() { --alive_count_; }

  void SetBackPointer(SuperClass* back_pointer) {
    back_pointer_ = back_pointer;
  }

  SuperClass* BackPointer() const { return back_pointer_; }

  void Trace(Visitor* visitor) const { visitor->Trace(back_pointer_); }

  static int alive_count_;

 private:
  WeakMember<SuperClass> back_pointer_;
};
int PointsBack::alive_count_ = 0;

class SuperClass : public GarbageCollected<SuperClass> {
 public:
  explicit SuperClass(PointsBack* points_back) : points_back_(points_back) {
    points_back_->SetBackPointer(this);
    ++alive_count_;
  }
  virtual ~SuperClass() { --alive_count_; }

  void DoStuff(SuperClass* target,
               PointsBack* points_back,
               int super_class_count) {
    TestSupportingGC::ConservativelyCollectGarbage();
    EXPECT_EQ(points_back, target->GetPointsBack());
    EXPECT_EQ(super_class_count, SuperClass::alive_count_);
  }

  virtual void Trace(Visitor* visitor) const { visitor->Trace(points_back_); }

  PointsBack* GetPointsBack() const { return points_back_.Get(); }

  static int alive_count_;

 private:
  Member<PointsBack> points_back_;
};

int SuperClass::alive_count_ = 0;
class SubData final : public GarbageCollected<SubData> {
 public:
  SubData() { ++alive_count_; }
  ~SubData() { --alive_count_; }

  void Trace(Visitor* visitor) const {}

  static int alive_count_;
};
int SubData::alive_count_ = 0;

class SubClass : public SuperClass {
 public:
  explicit SubClass(PointsBack* points_back)
      : SuperClass(points_back), data_(MakeGarbageCollected<SubData>()) {
    ++alive_count_;
  }
  ~SubClass() override { --alive_count_; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(data_);
    SuperClass::Trace(visitor);
  }

  static int alive_count_;

 private:
  Member<SubData> data_;
};
int SubClass::alive_count_ = 0;
}  // namespace

TEST_F(HeapInternalsTest, Transition) {
  {
    RefCountedAndGarbageCollected::destructor_calls_ = 0;
    Persistent<RefCountedAndGarbageCollected> ref_counted =
        MakeGarbageCollected<RefCountedAndGarbageCollected>();
    PreciselyCollectGarbage();
    EXPECT_EQ(0, RefCountedAndGarbageCollected::destructor_calls_);
  }
  PreciselyCollectGarbage();
  EXPECT_EQ(1, RefCountedAndGarbageCollected::destructor_calls_);
  RefCountedAndGarbageCollected::destructor_calls_ = 0;

  Persistent<PointsBack> points_back1 = MakeGarbageCollected<PointsBack>();
  Persistent<PointsBack> points_back2 = MakeGarbageCollected<PointsBack>();
  Persistent<SuperClass> super_class =
      MakeGarbageCollected<SuperClass>(points_back1);
  Persistent<SubClass> sub_class = MakeGarbageCollected<SubClass>(points_back2);
  EXPECT_EQ(2, PointsBack::alive_count_);
  EXPECT_EQ(2, SuperClass::alive_count_);
  EXPECT_EQ(1, SubClass::alive_count_);
  EXPECT_EQ(1, SubData::alive_count_);

  PreciselyCollectGarbage();
  EXPECT_EQ(0, RefCountedAndGarbageCollected::destructor_calls_);
  EXPECT_EQ(2, PointsBack::alive_count_);
  EXPECT_EQ(2, SuperClass::alive_count_);
  EXPECT_EQ(1, SubClass::alive_count_);
  EXPECT_EQ(1, SubData::alive_count_);

  super_class->DoStuff(super_class.Release(), points_back1.Get(), 2);
  PreciselyCollectGarbage();
  EXPECT_EQ(2, PointsBack::alive_count_);
  EXPECT_EQ(1, SuperClass::alive_count_);
  EXPECT_EQ(1, SubClass::alive_count_);
  EXPECT_EQ(1, SubData::alive_count_);
  EXPECT_EQ(nullptr, points_back1->BackPointer());

  points_back1.Release();
  PreciselyCollectGarbage();
  EXPECT_EQ(1, PointsBack::alive_count_);
  EXPECT_EQ(1, SuperClass::alive_count_);
  EXPECT_EQ(1, SubClass::alive_count_);
  EXPECT_EQ(1, SubData::alive_count_);

  sub_class->DoStuff(sub_class.Release(), points_back2.Get(), 1);
  PreciselyCollectGarbage();
  EXPECT_EQ(1, PointsBack::alive_count_);
  EXPECT_EQ(0, SuperClass::alive_count_);
  EXPECT_EQ(0, SubClass::alive_count_);
  EXPECT_EQ(0, SubData::alive_count_);
  EXPECT_EQ(nullptr, points_back2->BackPointer());

  points_back2.Release();
  PreciselyCollectGarbage();
  EXPECT_EQ(0, PointsBack::alive_count_);
  EXPECT_EQ(0, SuperClass::alive_count_);
  EXPECT_EQ(0, SubClass::alive_count_);
  EXPECT_EQ(0, SubData::alive_count_);

  EXPECT_TRUE(super_class == sub_class);
}

namespace {
class DynamicallySizedObject : public GarbageCollected<DynamicallySizedObject> {
 public:
  static DynamicallySizedObject* Create(size_t size) {
    return MakeGarbageCollected<DynamicallySizedObject>(AdditionalBytes(
        base::checked_cast<wtf_size_t>(size - sizeof(DynamicallySizedObject))));
  }

  uint8_t Get(int i) { return *(reinterpret_cast<uint8_t*>(this) + i); }

  void Trace(Visitor* visitor) const {}
};
}  // namespace

TEST_F(HeapInternalsTest, BasicFunctionality) {
  ThreadHeap& heap = ThreadState::Current()->Heap();
  ClearOutOldGarbage();
  size_t initial_object_payload_size = heap.ObjectPayloadSizeForTesting();
  {
    wtf_size_t slack = 0;

    // When the test starts there may already have been leaked some memory
    // on the heap, so we establish a base line.
    size_t base_level = initial_object_payload_size;
    bool test_pages_allocated = !base_level;
    if (test_pages_allocated)
      EXPECT_EQ(0ul, heap.stats_collector()->allocated_space_bytes());

    // This allocates objects on the general heap which should add a page of
    // memory.
    DynamicallySizedObject* alloc32 = DynamicallySizedObject::Create(32);
    slack += 4;
    memset(alloc32, 40, 32);
    DynamicallySizedObject* alloc64 = DynamicallySizedObject::Create(64);
    slack += 4;
    memset(alloc64, 27, 64);

    size_t total = 96;

    CheckWithSlack(base_level + total, heap.ObjectPayloadSizeForTesting(),
                   slack);
    if (test_pages_allocated) {
      EXPECT_EQ(kBlinkPageSize * 2,
                heap.stats_collector()->allocated_space_bytes());
    }

    EXPECT_EQ(alloc32->Get(0), 40);
    EXPECT_EQ(alloc32->Get(31), 40);
    EXPECT_EQ(alloc64->Get(0), 27);
    EXPECT_EQ(alloc64->Get(63), 27);

    ConservativelyCollectGarbage();

    EXPECT_EQ(alloc32->Get(0), 40);
    EXPECT_EQ(alloc32->Get(31), 40);
    EXPECT_EQ(alloc64->Get(0), 27);
    EXPECT_EQ(alloc64->Get(63), 27);
  }

  ClearOutOldGarbage();
  size_t total = 0;
  wtf_size_t slack = 0;
  size_t base_level = heap.ObjectPayloadSizeForTesting();
  bool test_pages_allocated = !base_level;
  if (test_pages_allocated)
    EXPECT_EQ(0ul, heap.stats_collector()->allocated_space_bytes());

  size_t big = 1008;
  Persistent<DynamicallySizedObject> big_area =
      DynamicallySizedObject::Create(big);
  total += big;
  slack += 4;

  size_t persistent_count = 0;
  const size_t kNumPersistents = 100000;
  Persistent<DynamicallySizedObject>* persistents[kNumPersistents];

  for (int i = 0; i < 1000; i++) {
    size_t size = 128 + i * 8;
    total += size;
    persistents[persistent_count++] = new Persistent<DynamicallySizedObject>(
        DynamicallySizedObject::Create(size));
    slack += 4;
    // The allocations in the loop may trigger GC with lazy sweeping.
    if (ThreadState::Current()->IsSweepingInProgress())
      ThreadState::Current()->CompleteSweep();
    CheckWithSlack(base_level + total, heap.ObjectPayloadSizeForTesting(),
                   slack);
    if (test_pages_allocated) {
      EXPECT_EQ(0ul, heap.stats_collector()->allocated_space_bytes() &
                         (kBlinkPageSize - 1));
    }
  }

  {
    DynamicallySizedObject* alloc32b(DynamicallySizedObject::Create(32));
    slack += 4;
    memset(alloc32b, 40, 32);
    DynamicallySizedObject* alloc64b(DynamicallySizedObject::Create(64));
    slack += 4;
    memset(alloc64b, 27, 64);
    EXPECT_TRUE(alloc32b != alloc64b);

    total += 96;
    CheckWithSlack(base_level + total, heap.ObjectPayloadSizeForTesting(),
                   slack);
    if (test_pages_allocated) {
      EXPECT_EQ(0ul, heap.stats_collector()->allocated_space_bytes() &
                         (kBlinkPageSize - 1));
    }
  }

  ClearOutOldGarbage();
  total -= 96;
  slack -= 8;
  if (test_pages_allocated) {
    EXPECT_EQ(0ul, heap.stats_collector()->allocated_space_bytes() &
                       (kBlinkPageSize - 1));
  }

  // Clear the persistent, so that the big area will be garbage collected.
  big_area.Release();
  ClearOutOldGarbage();

  total -= big;
  slack -= 4;
  CheckWithSlack(base_level + total, heap.ObjectPayloadSizeForTesting(), slack);
  if (test_pages_allocated) {
    EXPECT_EQ(0ul, heap.stats_collector()->allocated_space_bytes() &
                       (kBlinkPageSize - 1));
  }

  CheckWithSlack(base_level + total, heap.ObjectPayloadSizeForTesting(), slack);
  if (test_pages_allocated) {
    EXPECT_EQ(0ul, heap.stats_collector()->allocated_space_bytes() &
                       (kBlinkPageSize - 1));
  }

  for (size_t i = 0; i < persistent_count; i++) {
    delete persistents[i];
    persistents[i] = nullptr;
  }
}

namespace {
class HeapAllocatedArray : public GarbageCollected<HeapAllocatedArray> {
 public:
  HeapAllocatedArray() {
    for (int i = 0; i < kArraySize; ++i) {
      array_[i] = i % 128;
    }
  }

  int8_t at(size_t i) { return array_[i]; }
  void Trace(Visitor* visitor) const {}

 private:
  static const int kArraySize = 1000;
  int8_t array_[kArraySize];
};
}  // namespace

TEST_F(HeapInternalsTest, SimpleAllocation) {
  ThreadHeap& heap = ThreadState::Current()->Heap();
  ClearOutOldGarbage();
  EXPECT_EQ(0ul, heap.ObjectPayloadSizeForTesting());

  // Allocate an object in the heap.
  HeapAllocatedArray* array = MakeGarbageCollected<HeapAllocatedArray>();
  EXPECT_TRUE(heap.ObjectPayloadSizeForTesting() >= sizeof(HeapAllocatedArray));

  // Sanity check of the contents in the heap.
  EXPECT_EQ(0, array->at(0));
  EXPECT_EQ(42, array->at(42));
  EXPECT_EQ(0, array->at(128));
  EXPECT_EQ(999 % 128, array->at(999));
}

namespace {
class TraceCounter final : public GarbageCollected<TraceCounter> {
 public:
  void Trace(Visitor* visitor) const { trace_count_++; }
  int TraceCount() const { return trace_count_; }

 private:
  mutable int trace_count_ = 0;
};

class ClassWithMember : public GarbageCollected<ClassWithMember> {
 public:
  ClassWithMember() : trace_counter_(MakeGarbageCollected<TraceCounter>()) {}

  void Trace(Visitor* visitor) const { visitor->Trace(trace_counter_); }
  int TraceCount() const { return trace_counter_->TraceCount(); }

 private:
  Member<TraceCounter> trace_counter_;
};
}  // namespace

TEST_F(HeapInternalsTest, SimplePersistent) {
  Persistent<TraceCounter> trace_counter = MakeGarbageCollected<TraceCounter>();
  EXPECT_EQ(0, trace_counter->TraceCount());
  PreciselyCollectGarbage();
  int saved_trace_count = trace_counter->TraceCount();
  EXPECT_LT(0, saved_trace_count);

  Persistent<ClassWithMember> class_with_member =
      MakeGarbageCollected<ClassWithMember>();
  EXPECT_EQ(0, class_with_member->TraceCount());
  PreciselyCollectGarbage();
  EXPECT_LT(0, class_with_member->TraceCount());
  EXPECT_LT(saved_trace_count, trace_counter->TraceCount());
}

namespace {
class SimpleFinalizedObject final
    : public GarbageCollected<SimpleFinalizedObject> {
 public:
  SimpleFinalizedObject() = default;
  ~SimpleFinalizedObject() { ++destructor_calls_; }

  static int destructor_calls_;

  void Trace(Visitor* visitor) const {}
};
int SimpleFinalizedObject::destructor_calls_ = 0;
}  // namespace

TEST_F(HeapInternalsTest, SimpleFinalization) {
  ClearOutOldGarbage();
  {
    SimpleFinalizedObject::destructor_calls_ = 0;
    Persistent<SimpleFinalizedObject> finalized =
        MakeGarbageCollected<SimpleFinalizedObject>();
    EXPECT_EQ(0, SimpleFinalizedObject::destructor_calls_);
    PreciselyCollectGarbage();
    EXPECT_EQ(0, SimpleFinalizedObject::destructor_calls_);
  }

  PreciselyCollectGarbage();
  EXPECT_EQ(1, SimpleFinalizedObject::destructor_calls_);
}

#if DCHECK_IS_ON() || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER)
TEST_F(HeapInternalsTest, FreelistReuse) {
  ClearOutOldGarbage();

  for (int i = 0; i < 100; i++)
    MakeGarbageCollected<IntWrapper>(i);
  IntWrapper* p1 = MakeGarbageCollected<IntWrapper>(100);
  PreciselyCollectGarbage();
  // In non-production builds, we delay reusing freed memory for at least
  // one GC cycle.
  for (int i = 0; i < 100; i++) {
    IntWrapper* p2 = MakeGarbageCollected<IntWrapper>(i);
    EXPECT_NE(p1, p2);
  }

  PreciselyCollectGarbage();
  PreciselyCollectGarbage();
  // Now the freed memory in the first GC should be reused.
  bool reused_memory_found = false;
  for (int i = 0; i < 10000; i++) {
    IntWrapper* p2 = MakeGarbageCollected<IntWrapper>(i);
    if (p1 == p2) {
      reused_memory_found = true;
      break;
    }
  }
  EXPECT_TRUE(reused_memory_found);
}
#endif

TEST_F(HeapInternalsTest, LazySweepingPages) {
  ClearOutOldGarbage();

  SimpleFinalizedObject::destructor_calls_ = 0;
  EXPECT_EQ(0, SimpleFinalizedObject::destructor_calls_);
  for (int i = 0; i < 1000; i++)
    MakeGarbageCollected<SimpleFinalizedObject>();
  ThreadState::Current()->CollectGarbageForTesting(
      BlinkGC::CollectionType::kMajor, BlinkGC::kNoHeapPointersOnStack,
      BlinkGC::kAtomicMarking, BlinkGC::kConcurrentAndLazySweeping,
      BlinkGC::GCReason::kForcedGCForTesting);
  EXPECT_EQ(0, SimpleFinalizedObject::destructor_calls_);
  for (int i = 0; i < 10000; i++)
    MakeGarbageCollected<SimpleFinalizedObject>();
  EXPECT_EQ(1000, SimpleFinalizedObject::destructor_calls_);
  PreciselyCollectGarbage();
  EXPECT_EQ(11000, SimpleFinalizedObject::destructor_calls_);
}

namespace {
class HeapTestSuperClass : public GarbageCollected<HeapTestSuperClass> {
 public:
  HeapTestSuperClass() = default;
  virtual ~HeapTestSuperClass() { ++destructor_calls_; }

  static int destructor_calls_;
  void Trace(Visitor* visitor) const {}
};
int HeapTestSuperClass::destructor_calls_ = 0;

class HeapTestOtherSuperClass {
 public:
  int payload;
};

class HeapTestSubClass : public HeapTestSuperClass,
                         public HeapTestOtherSuperClass {
  static constexpr size_t kClassMagic = 0xABCDDBCA;

 public:
  HeapTestSubClass() : magic_(kClassMagic) {}
  ~HeapTestSubClass() override {
    EXPECT_EQ(kClassMagic, magic_);
    ++destructor_calls_;
  }

  static int destructor_calls_;

 private:
  const size_t magic_;
};
int HeapTestSubClass::destructor_calls_ = 0;
constexpr size_t HeapTestSubClass::kClassMagic;
}  // namespace

TEST_F(HeapInternalsTest, Finalization) {
  {
    HeapTestSubClass::destructor_calls_ = 0;
    HeapTestSuperClass::destructor_calls_ = 0;
    auto* t1 = MakeGarbageCollected<HeapTestSubClass>();
    auto* t2 = MakeGarbageCollected<HeapTestSubClass>();
    auto* t3 = MakeGarbageCollected<HeapTestSuperClass>();
    // FIXME(oilpan): Ignore unused variables.
    (void)t1;
    (void)t2;
    (void)t3;
  }
  // Nothing is marked so the GC should free everything and call
  // the finalizer on all three objects.
  PreciselyCollectGarbage();
  EXPECT_EQ(2, HeapTestSubClass::destructor_calls_);
  EXPECT_EQ(3, HeapTestSuperClass::destructor_calls_);
  // Destructors not called again when GCing again.
  PreciselyCollectGarbage();
  EXPECT_EQ(2, HeapTestSubClass::destructor_calls_);
  EXPECT_EQ(3, HeapTestSuperClass::destructor_calls_);
}

namespace {
class IntNode : public GarbageCollected<IntNode> {
 public:
  template <typename T>
  static void* AllocateObject(size_t size) {
    ThreadState* state = ThreadState::Current();
    const char* type_name = WTF_HEAP_PROFILER_TYPE_NAME(IntNode);
    return state->Heap().AllocateOnArenaIndex(
        state, size, BlinkGC::kNodeArenaIndex,
        GCInfoTrait<GCInfoFoldedType<IntNode>>::Index(), type_name);
  }

  explicit IntNode(int i) : value_(i) {}

  static IntNode* Create(int i) { return MakeGarbageCollected<IntNode>(i); }

  void Trace(Visitor* visitor) const {}

  int Value() { return value_; }

 private:
  int value_;
};
}  // namespace

TEST_F(HeapInternalsTest, TypedArenaSanity) {
  // We use TraceCounter for allocating an object on the general heap.
  Persistent<TraceCounter> general_heap_object =
      MakeGarbageCollected<TraceCounter>();
  Persistent<IntNode> typed_heap_object = IntNode::Create(0);
  EXPECT_NE(PageFromObject(general_heap_object.Get()),
            PageFromObject(typed_heap_object.Get()));
}

TEST_F(HeapInternalsTest, NoAllocation) {
  ThreadState* state = ThreadState::Current();
  EXPECT_TRUE(state->IsAllocationAllowed());
  {
    // Disallow allocation
    ThreadState::NoAllocationScope no_allocation_scope(state);
    EXPECT_FALSE(state->IsAllocationAllowed());
  }
  EXPECT_TRUE(state->IsAllocationAllowed());
}

namespace {
class Baz : public GarbageCollected<Baz> {
 public:
  explicit Baz(Bar* bar) : bar_(bar) {}

  void Trace(Visitor* visitor) const { visitor->Trace(bar_); }

  void Clear() { bar_.Release(); }

  // willFinalize is called by FinalizationObserver.
  void WillFinalize() { EXPECT_TRUE(!bar_->HasBeenFinalized()); }

 private:
  Member<Bar> bar_;
};
}  // namespace

TEST_F(HeapInternalsTest, Members) {
  ClearOutOldGarbage();
  Bar::live_ = 0;
  {
    Persistent<Baz> h1;
    Persistent<Baz> h2;
    {
      h1 = MakeGarbageCollected<Baz>(MakeGarbageCollected<Bar>());
      PreciselyCollectGarbage();
      EXPECT_EQ(1u, Bar::live_);
      h2 = MakeGarbageCollected<Baz>(MakeGarbageCollected<Bar>());
      PreciselyCollectGarbage();
      EXPECT_EQ(2u, Bar::live_);
    }
    PreciselyCollectGarbage();
    EXPECT_EQ(2u, Bar::live_);
    h1->Clear();
    PreciselyCollectGarbage();
    EXPECT_EQ(1u, Bar::live_);
  }
  PreciselyCollectGarbage();
  EXPECT_EQ(0u, Bar::live_);
}

namespace {
class Foo final : public Bar {
 public:
  explicit Foo(Bar* bar) : bar_(bar), points_to_foo_(false) {}

  explicit Foo(Foo* foo) : bar_(foo), points_to_foo_(true) {}

  void Trace(Visitor* visitor) const override {
    Bar::Trace(visitor);
    if (points_to_foo_)
      visitor->Trace(static_cast<const Foo*>(bar_.Get()));
    else
      visitor->Trace(bar_);
  }

 private:
  const Member<Bar> bar_;
  const bool points_to_foo_;
};
}  // namespace

TEST_F(HeapInternalsTest, MarkTest) {
  ClearOutOldGarbage();
  {
    Bar::live_ = 0;
    Persistent<Bar> bar = MakeGarbageCollected<Bar>();
#if DCHECK_IS_ON()
    DCHECK(ThreadState::Current()->Heap().FindPageFromAddress(bar));
#endif
    EXPECT_EQ(1u, Bar::live_);
    {
      auto* foo = MakeGarbageCollected<Foo>(bar);
#if DCHECK_IS_ON()
      DCHECK(ThreadState::Current()->Heap().FindPageFromAddress(foo));
#endif
      EXPECT_EQ(2u, Bar::live_);
      EXPECT_TRUE(reinterpret_cast<Address>(foo) !=
                  reinterpret_cast<Address>(bar.Get()));
      ConservativelyCollectGarbage();
      EXPECT_TRUE(foo != bar);  // To make sure foo is kept alive.
      EXPECT_EQ(2u, Bar::live_);
    }
    PreciselyCollectGarbage();
    EXPECT_EQ(1u, Bar::live_);
  }
  PreciselyCollectGarbage();
  EXPECT_EQ(0u, Bar::live_);
}

TEST_F(HeapInternalsTest, DeepTest) {
  ClearOutOldGarbage();
  const unsigned kDepth = 100000;
  Bar::live_ = 0;
  {
    auto* bar = MakeGarbageCollected<Bar>();
#if DCHECK_IS_ON()
    DCHECK(ThreadState::Current()->Heap().FindPageFromAddress(bar));
#endif
    auto* foo = MakeGarbageCollected<Foo>(bar);
#if DCHECK_IS_ON()
    DCHECK(ThreadState::Current()->Heap().FindPageFromAddress(foo));
#endif
    EXPECT_EQ(2u, Bar::live_);
    for (unsigned i = 0; i < kDepth; i++) {
      auto* foo2 = MakeGarbageCollected<Foo>(foo);
      foo = foo2;
#if DCHECK_IS_ON()
      DCHECK(ThreadState::Current()->Heap().FindPageFromAddress(foo));
#endif
    }
    EXPECT_EQ(kDepth + 2, Bar::live_);
    ConservativelyCollectGarbage();
    EXPECT_TRUE(foo != bar);  // To make sure foo and bar are kept alive.
    EXPECT_EQ(kDepth + 2, Bar::live_);
  }
  PreciselyCollectGarbage();
  EXPECT_EQ(0u, Bar::live_);
}

namespace {
class ConstructorAllocation : public GarbageCollected<ConstructorAllocation> {
 public:
  ConstructorAllocation() {
    int_wrapper_ = MakeGarbageCollected<IntWrapper>(42);
  }

  void Trace(Visitor* visitor) const { visitor->Trace(int_wrapper_); }

 private:
  Member<IntWrapper> int_wrapper_;
};
}  // namespace

TEST_F(HeapInternalsTest, NestedAllocation) {
  ThreadHeap& heap = ThreadState::Current()->Heap();
  ClearOutOldGarbage();
  size_t initial_object_payload_size = heap.ObjectPayloadSizeForTesting();
  {
    Persistent<ConstructorAllocation> constructor_allocation =
        MakeGarbageCollected<ConstructorAllocation>();
  }
  ClearOutOldGarbage();
  size_t after_free = heap.ObjectPayloadSizeForTesting();
  EXPECT_TRUE(initial_object_payload_size == after_free);
}

namespace {
class Weak final : public Bar {
 public:
  Weak(Bar* strong_bar, Bar* weak_bar)
      : strong_bar_(strong_bar), weak_bar_(weak_bar) {}

  void Trace(Visitor* visitor) const override {
    Bar::Trace(visitor);
    visitor->Trace(strong_bar_);
    visitor->template RegisterWeakCallbackMethod<Weak, &Weak::ZapWeakMembers>(
        this);
  }

  void ZapWeakMembers(const LivenessBroker& info) {
    if (!info.IsHeapObjectAlive(weak_bar_))
      weak_bar_ = nullptr;
  }

  bool StrongIsThere() { return !!strong_bar_; }
  bool WeakIsThere() { return !!weak_bar_; }

 private:
  Member<Bar> strong_bar_;
  UntracedMember<Bar> weak_bar_;
};

class WithWeakMember final : public Bar {
 public:
  WithWeakMember(Bar* strong_bar, Bar* weak_bar)
      : strong_bar_(strong_bar), weak_bar_(weak_bar) {}

  void Trace(Visitor* visitor) const override {
    Bar::Trace(visitor);
    visitor->Trace(strong_bar_);
    visitor->Trace(weak_bar_);
  }

  bool StrongIsThere() { return !!strong_bar_; }
  bool WeakIsThere() { return !!weak_bar_; }

 private:
  Member<Bar> strong_bar_;
  WeakMember<Bar> weak_bar_;
};
}  // namespace

TEST_F(HeapInternalsTest, WeakMembers) {
  ClearOutOldGarbage();
  Bar::live_ = 0;
  {
    Persistent<Bar> h1 = MakeGarbageCollected<Bar>();
    Persistent<Weak> h4;
    Persistent<WithWeakMember> h5;
    PreciselyCollectGarbage();
    ASSERT_EQ(1u, Bar::live_);  // h1 is live.
    {
      auto* h2 = MakeGarbageCollected<Bar>();
      auto* h3 = MakeGarbageCollected<Bar>();
      h4 = MakeGarbageCollected<Weak>(h2, h3);
      h5 = MakeGarbageCollected<WithWeakMember>(h2, h3);
      ConservativelyCollectGarbage();
      EXPECT_EQ(5u, Bar::live_);  // The on-stack pointer keeps h3 alive.
      EXPECT_FALSE(h3->HasBeenFinalized());
      EXPECT_TRUE(h4->StrongIsThere());
      EXPECT_TRUE(h4->WeakIsThere());
      EXPECT_TRUE(h5->StrongIsThere());
      EXPECT_TRUE(h5->WeakIsThere());
    }
    // h3 is collected, weak pointers from h4 and h5 don't keep it alive.
    PreciselyCollectGarbage();
    EXPECT_EQ(4u, Bar::live_);
    EXPECT_TRUE(h4->StrongIsThere());
    EXPECT_FALSE(h4->WeakIsThere());  // h3 is gone from weak pointer.
    EXPECT_TRUE(h5->StrongIsThere());
    EXPECT_FALSE(h5->WeakIsThere());  // h3 is gone from weak pointer.
    h1.Release();                     // Zero out h1.
    PreciselyCollectGarbage();
    EXPECT_EQ(3u, Bar::live_);         // Only h4, h5 and h2 are left.
    EXPECT_TRUE(h4->StrongIsThere());  // h2 is still pointed to from h4.
    EXPECT_TRUE(h5->StrongIsThere());  // h2 is still pointed to from h5.
  }
  // h4 and h5 have gone out of scope now and they were keeping h2 alive.
  PreciselyCollectGarbage();
  EXPECT_EQ(0u, Bar::live_);  // All gone.
}

namespace {
class Observable final : public GarbageCollected<Observable> {
  USING_PRE_FINALIZER(Observable, WillFinalize);

 public:
  explicit Observable(Bar* bar) : bar_(bar), was_destructed_(false) {}
  ~Observable() { was_destructed_ = true; }
  void Trace(Visitor* visitor) const { visitor->Trace(bar_); }

  // willFinalize is called by FinalizationObserver. willFinalize can touch
  // other on-heap objects.
  void WillFinalize() {
    EXPECT_FALSE(was_destructed_);
    EXPECT_FALSE(bar_->HasBeenFinalized());
    will_finalize_was_called_ = true;
  }
  static bool will_finalize_was_called_;

 private:
  Member<Bar> bar_;
  bool was_destructed_;
};
bool Observable::will_finalize_was_called_ = false;

template <typename T>
class FinalizationObserver : public GarbageCollected<FinalizationObserver<T>> {
 public:
  explicit FinalizationObserver(T* data)
      : data_(data), did_call_will_finalize_(false) {}

  bool DidCallWillFinalize() const { return did_call_will_finalize_; }

  void Trace(Visitor* visitor) const {
    visitor->template RegisterWeakCallbackMethod<
        FinalizationObserver<T>, &FinalizationObserver<T>::ZapWeakMembers>(
        this);
  }

  void ZapWeakMembers(const LivenessBroker& info) {
    if (data_ && !info.IsHeapObjectAlive(data_)) {
      data_->WillFinalize();
      data_ = nullptr;
      did_call_will_finalize_ = true;
    }
  }

 private:
  UntracedMember<T> data_;
  bool did_call_will_finalize_;
};

class FinalizationObserverWithHashMap {
 public:
  typedef HeapHashMap<WeakMember<Observable>,
                      std::unique_ptr<FinalizationObserverWithHashMap>>
      ObserverMap;

  explicit FinalizationObserverWithHashMap(Observable* target)
      : target_(target) {}
  ~FinalizationObserverWithHashMap() {
    target_->WillFinalize();
    did_call_will_finalize_ = true;
  }

  static ObserverMap& Observe(Observable* target) {
    ObserverMap& map = Observers();
    ObserverMap::AddResult result = map.insert(target, nullptr);
    if (result.is_new_entry) {
      result.stored_value->value =
          std::make_unique<FinalizationObserverWithHashMap>(target);
    } else {
      DCHECK(result.stored_value->value);
    }
    return map;
  }

  static void ClearObservers() {
    delete observer_map_;
    observer_map_ = nullptr;
  }

  static bool did_call_will_finalize_;

 private:
  static ObserverMap& Observers() {
    if (!observer_map_) {
      observer_map_ =
          new Persistent<ObserverMap>(MakeGarbageCollected<ObserverMap>());
    }
    return **observer_map_;
  }

  UntracedMember<Observable> target_;
  static Persistent<ObserverMap>* observer_map_;
};
bool FinalizationObserverWithHashMap::did_call_will_finalize_ = false;
Persistent<FinalizationObserverWithHashMap::ObserverMap>*
    FinalizationObserverWithHashMap::observer_map_;
}  // namespace

TEST_F(HeapInternalsTest, FinalizationObserver) {
  Persistent<FinalizationObserver<Observable>> o;
  {
    auto* foo = MakeGarbageCollected<Observable>(MakeGarbageCollected<Bar>());
    // |o| observes |foo|.
    o = MakeGarbageCollected<FinalizationObserver<Observable>>(foo);
  }
  // FinalizationObserver doesn't have a strong reference to |foo|. So |foo|
  // and its member will be collected.
  PreciselyCollectGarbage();
  EXPECT_EQ(0u, Bar::live_);
  EXPECT_TRUE(o->DidCallWillFinalize());

  FinalizationObserverWithHashMap::did_call_will_finalize_ = false;
  auto* foo = MakeGarbageCollected<Observable>(MakeGarbageCollected<Bar>());
  FinalizationObserverWithHashMap::ObserverMap& map =
      FinalizationObserverWithHashMap::Observe(foo);
  EXPECT_EQ(1u, map.size());
  foo = nullptr;
  // FinalizationObserverWithHashMap doesn't have a strong reference to
  // |foo|. So |foo| and its member will be collected.
  PreciselyCollectGarbage();
  EXPECT_EQ(0u, Bar::live_);
  EXPECT_EQ(0u, map.size());
  EXPECT_TRUE(FinalizationObserverWithHashMap::did_call_will_finalize_);

  FinalizationObserverWithHashMap::ClearObservers();
}

TEST_F(HeapInternalsTest, PreFinalizer) {
  Observable::will_finalize_was_called_ = false;
  { MakeGarbageCollected<Observable>(MakeGarbageCollected<Bar>()); }
  PreciselyCollectGarbage();
  EXPECT_TRUE(Observable::will_finalize_was_called_);
}

namespace {
class ObservableWithPreFinalizer final
    : public GarbageCollected<ObservableWithPreFinalizer> {
  USING_PRE_FINALIZER(ObservableWithPreFinalizer, Dispose);

 public:
  ~ObservableWithPreFinalizer() { was_destructed_ = true; }
  void Trace(Visitor* visitor) const {}
  void Dispose() {
    EXPECT_FALSE(was_destructed_);
    dispose_was_called_ = true;
  }
  static bool dispose_was_called_;

 protected:
  bool was_destructed_ = false;
};
bool ObservableWithPreFinalizer::dispose_was_called_ = false;
}  // namespace

TEST_F(HeapInternalsTest, PreFinalizerUnregistersItself) {
  ObservableWithPreFinalizer::dispose_was_called_ = false;
  MakeGarbageCollected<ObservableWithPreFinalizer>();
  PreciselyCollectGarbage();
  EXPECT_TRUE(ObservableWithPreFinalizer::dispose_was_called_);
  // Don't crash, and assertions don't fail.
}

namespace {
bool g_dispose_was_called_for_pre_finalizer_base = false;
bool g_dispose_was_called_for_pre_finalizer_mixin = false;
bool g_dispose_was_called_for_pre_finalizer_sub_class = false;

class PreFinalizerBase : public GarbageCollected<PreFinalizerBase> {
  USING_PRE_FINALIZER(PreFinalizerBase, Dispose);

 public:
  virtual ~PreFinalizerBase() { was_destructed_ = true; }
  virtual void Trace(Visitor* visitor) const {}
  void Dispose() {
    EXPECT_FALSE(g_dispose_was_called_for_pre_finalizer_base);
    EXPECT_TRUE(g_dispose_was_called_for_pre_finalizer_sub_class);
    EXPECT_TRUE(g_dispose_was_called_for_pre_finalizer_mixin);
    EXPECT_FALSE(was_destructed_);
    g_dispose_was_called_for_pre_finalizer_base = true;
  }

 protected:
  bool was_destructed_ = false;
};

class PreFinalizerMixin : public GarbageCollectedMixin {
  USING_PRE_FINALIZER(PreFinalizerMixin, Dispose);

 public:
  ~PreFinalizerMixin() { was_destructed_ = true; }
  void Trace(Visitor* visitor) const override {}
  void Dispose() {
    EXPECT_FALSE(g_dispose_was_called_for_pre_finalizer_base);
    EXPECT_TRUE(g_dispose_was_called_for_pre_finalizer_sub_class);
    EXPECT_FALSE(g_dispose_was_called_for_pre_finalizer_mixin);
    EXPECT_FALSE(was_destructed_);
    g_dispose_was_called_for_pre_finalizer_mixin = true;
  }

 protected:
  bool was_destructed_ = false;
};

class PreFinalizerSubClass : public PreFinalizerBase, public PreFinalizerMixin {
  USING_PRE_FINALIZER(PreFinalizerSubClass, Dispose);

 public:
  ~PreFinalizerSubClass() override { was_destructed_ = true; }
  void Trace(Visitor* visitor) const override {
    PreFinalizerBase::Trace(visitor);
    PreFinalizerMixin::Trace(visitor);
  }
  void Dispose() {
    EXPECT_FALSE(g_dispose_was_called_for_pre_finalizer_base);
    EXPECT_FALSE(g_dispose_was_called_for_pre_finalizer_sub_class);
    EXPECT_FALSE(g_dispose_was_called_for_pre_finalizer_mixin);
    EXPECT_FALSE(was_destructed_);
    g_dispose_was_called_for_pre_finalizer_sub_class = true;
  }

 protected:
  bool was_destructed_ = false;
};
}  // namespace

TEST_F(HeapInternalsTest, NestedPreFinalizer) {
  g_dispose_was_called_for_pre_finalizer_base = false;
  g_dispose_was_called_for_pre_finalizer_sub_class = false;
  g_dispose_was_called_for_pre_finalizer_mixin = false;
  MakeGarbageCollected<PreFinalizerSubClass>();
  PreciselyCollectGarbage();
  EXPECT_TRUE(g_dispose_was_called_for_pre_finalizer_base);
  EXPECT_TRUE(g_dispose_was_called_for_pre_finalizer_sub_class);
  EXPECT_TRUE(g_dispose_was_called_for_pre_finalizer_mixin);
  // Don't crash, and assertions don't fail.
}

TEST_F(HeapInternalsTest, Comparisons) {
  Persistent<Bar> bar_persistent = MakeGarbageCollected<Bar>();
  Persistent<Foo> foo_persistent = MakeGarbageCollected<Foo>(bar_persistent);
  EXPECT_TRUE(bar_persistent != foo_persistent);
  bar_persistent = foo_persistent;
  EXPECT_TRUE(bar_persistent == foo_persistent);
}

namespace {
class DisableHeapVerificationScope {
 public:
  explicit DisableHeapVerificationScope(const char*) {
    ThreadState::Current()->EnterNoHeapVerificationScopeForTesting();
  }
  ~DisableHeapVerificationScope() {
    ThreadState::Current()->LeaveNoHeapVerificationScopeForTesting();
  }
};
}  // namespace

TEST_F(HeapInternalsTest, GarbageCollectedMixin) {
  ClearOutOldGarbage();

  Persistent<UseMixin> usemixin = MakeGarbageCollected<UseMixin>();
  EXPECT_EQ(0, UseMixin::trace_count_);
  {
    DisableHeapVerificationScope scope(
        "Avoid tracing UseMixin during verification");
    PreciselyCollectGarbage();
  }
  EXPECT_EQ(1, UseMixin::trace_count_);

  Persistent<Mixin> mixin = usemixin;
  usemixin = nullptr;
  {
    DisableHeapVerificationScope scope(
        "Avoid tracing UseMixin during verification");
    PreciselyCollectGarbage();
  }
  EXPECT_EQ(2, UseMixin::trace_count_);

  Persistent<HeapHashSet<WeakMember<Mixin>>> weak_map =
      MakeGarbageCollected<HeapHashSet<WeakMember<Mixin>>>();
  weak_map->insert(MakeGarbageCollected<UseMixin>());
  PreciselyCollectGarbage();
  EXPECT_EQ(0u, weak_map->size());
}

namespace {
class OneKiloByteObject final : public GarbageCollected<OneKiloByteObject> {
 public:
  ~OneKiloByteObject() { destructor_calls_++; }
  char* Data() { return data_; }
  void Trace(Visitor* visitor) const {}
  static int destructor_calls_;

 private:
  static const size_t kLength = 1024;
  char data_[kLength];
};
int OneKiloByteObject::destructor_calls_ = 0;

class FinalizationAllocator final
    : public GarbageCollected<FinalizationAllocator> {
 public:
  explicit FinalizationAllocator(Persistent<IntWrapper>* wrapper) {
    wrapper_ = wrapper;
  }

  ~FinalizationAllocator() {
    for (int i = 0; i < 10; ++i)
      *wrapper_ = MakeGarbageCollected<IntWrapper>(42);
    for (int i = 0; i < 512; ++i)
      MakeGarbageCollected<OneKiloByteObject>();
    for (int i = 0; i < 32; ++i)
      MakeGarbageCollected<LargeHeapObject>();
  }

  void Trace(Visitor* visitor) const {}

 private:
  static Persistent<IntWrapper>* wrapper_;
};

Persistent<IntWrapper>* FinalizationAllocator::wrapper_;
}  // namespace

TEST_F(HeapInternalsTest, AllocationDuringFinalization) {
  ClearOutOldGarbage();
  IntWrapper::destructor_calls_ = 0;
  OneKiloByteObject::destructor_calls_ = 0;
  LargeHeapObject::destructor_calls_ = 0;

  Persistent<IntWrapper> wrapper;
  MakeGarbageCollected<FinalizationAllocator>(&wrapper);

  PreciselyCollectGarbage();
  EXPECT_EQ(0, IntWrapper::destructor_calls_);
  EXPECT_EQ(0, OneKiloByteObject::destructor_calls_);
  EXPECT_EQ(0, LargeHeapObject::destructor_calls_);
  // Check that the wrapper allocated during finalization is not
  // swept away and zapped later in the same sweeping phase.
  EXPECT_EQ(42, wrapper->Value());

  wrapper.Clear();
  PreciselyCollectGarbage();
  // The 42 IntWrappers were the ones allocated in ~FinalizationAllocator
  // and the ones allocated in LargeHeapObject.
  EXPECT_EQ(42, IntWrapper::destructor_calls_);
  EXPECT_EQ(512, OneKiloByteObject::destructor_calls_);
  EXPECT_EQ(32, LargeHeapObject::destructor_calls_);
}

namespace {
class MixinInstanceWithoutTrace
    : public GarbageCollected<MixinInstanceWithoutTrace>,
      public MixinA {
 public:
  MixinInstanceWithoutTrace() = default;
};
}  // namespace

TEST_F(HeapInternalsTest, MixinInstanceWithoutTrace) {
  // Verify that a mixin instance without any traceable
  // references inherits the mixin's trace implementation.
  ClearOutOldGarbage();
  MixinA::trace_count_ = 0;
  MixinInstanceWithoutTrace* obj =
      MakeGarbageCollected<MixinInstanceWithoutTrace>();
  int saved_trace_count = 0;
  {
    Persistent<MixinA> a = obj;
    PreciselyCollectGarbage();
    saved_trace_count = MixinA::trace_count_;
    EXPECT_LT(0, saved_trace_count);
  }
  {
    Persistent<MixinInstanceWithoutTrace> b = obj;
    PreciselyCollectGarbage();
    EXPECT_LT(saved_trace_count, MixinA::trace_count_);
    saved_trace_count = MixinA::trace_count_;
  }
  PreciselyCollectGarbage();
  // Oilpan might still call trace on dead objects for various reasons which is
  // valid before sweeping started.
  EXPECT_LE(saved_trace_count, MixinA::trace_count_);
}

namespace {
class PartObjectWithVirtualMethod {
 public:
  virtual void Trace(Visitor* visitor) const {}
};

class ObjectWithVirtualPartObject
    : public GarbageCollected<ObjectWithVirtualPartObject> {
 public:
  ObjectWithVirtualPartObject() : dummy_(AllocateAndReturnBool()) {}
  void Trace(Visitor* visitor) const { visitor->Trace(part_); }

 private:
  bool dummy_;
  PartObjectWithVirtualMethod part_;
};
}  // namespace

TEST_F(HeapInternalsTest, PartObjectWithVirtualMethod) {
  ObjectWithVirtualPartObject* object =
      MakeGarbageCollected<ObjectWithVirtualPartObject>();
  EXPECT_TRUE(object);
}

namespace {
class NonNodeAllocatingNodeInDestructor final
    : public GarbageCollected<NonNodeAllocatingNodeInDestructor> {
 public:
  ~NonNodeAllocatingNodeInDestructor() {
    node_ = new Persistent<IntNode>(IntNode::Create(10));
  }

  void Trace(Visitor* visitor) const {}

  static Persistent<IntNode>* node_;
};

Persistent<IntNode>* NonNodeAllocatingNodeInDestructor::node_ = nullptr;
}  // namespace

TEST_F(HeapInternalsTest, NonNodeAllocatingNodeInDestructor) {
  MakeGarbageCollected<NonNodeAllocatingNodeInDestructor>();
  PreciselyCollectGarbage();
  EXPECT_EQ(10, (*NonNodeAllocatingNodeInDestructor::node_)->Value());
  delete NonNodeAllocatingNodeInDestructor::node_;
  NonNodeAllocatingNodeInDestructor::node_ = nullptr;
}

namespace {
class WeakPersistentHolder final {
 public:
  explicit WeakPersistentHolder(IntWrapper* object) : object_(object) {}
  IntWrapper* Object() const { return object_; }

 private:
  WeakPersistent<IntWrapper> object_;
};
}  // namespace

TEST_F(HeapInternalsTest, WeakPersistent) {
  Persistent<IntWrapper> object = MakeGarbageCollected<IntWrapper>(20);
  std::unique_ptr<WeakPersistentHolder> holder =
      std::make_unique<WeakPersistentHolder>(object);
  PreciselyCollectGarbage();
  EXPECT_TRUE(holder->Object());
  object = nullptr;
  PreciselyCollectGarbage();
  EXPECT_FALSE(holder->Object());
}

namespace {
class WithWeakConstObject final : public GarbageCollected<WithWeakConstObject> {
 public:
  explicit WithWeakConstObject(const IntWrapper* int_wrapper)
      : wrapper_(int_wrapper) {}

  void Trace(Visitor* visitor) const { visitor->Trace(wrapper_); }

  const IntWrapper* Value() const { return wrapper_; }

 private:
  WeakMember<const IntWrapper> wrapper_;
};
}  // namespace

TEST_F(HeapInternalsTest, TestWeakConstObject) {
  Persistent<WithWeakConstObject> weak_wrapper;
  {
    const auto* wrapper = MakeGarbageCollected<IntWrapper>(42);
    weak_wrapper = MakeGarbageCollected<WithWeakConstObject>(wrapper);
    ConservativelyCollectGarbage();
    EXPECT_EQ(wrapper, weak_wrapper->Value());
    // Stub out any stack reference.
    wrapper = nullptr;
  }
  PreciselyCollectGarbage();
  EXPECT_EQ(nullptr, weak_wrapper->Value());
}

namespace {
class EmptyMixin : public GarbageCollectedMixin {};
class UseMixinFromLeftmostInherited : public UseMixin, public EmptyMixin {
 public:
  ~UseMixinFromLeftmostInherited() = default;
};
}  // namespace

TEST_F(HeapInternalsTest, IsGarbageCollected) {
  // Static sanity checks covering the correct operation of
  // IsGarbageCollectedType<>.

  static_assert(WTF::IsGarbageCollectedType<SimpleObject>::value,
                "GarbageCollected<>");
  static_assert(WTF::IsGarbageCollectedType<const SimpleObject>::value,
                "const GarbageCollected<>");
  static_assert(WTF::IsGarbageCollectedType<IntWrapper>::value,
                "GarbageCollected<>");
  static_assert(WTF::IsGarbageCollectedType<GarbageCollectedMixin>::value,
                "GarbageCollectedMixin");
  static_assert(WTF::IsGarbageCollectedType<const GarbageCollectedMixin>::value,
                "const GarbageCollectedMixin");
  static_assert(WTF::IsGarbageCollectedType<UseMixin>::value,
                "GarbageCollectedMixin instance");
  static_assert(WTF::IsGarbageCollectedType<const UseMixin>::value,
                "const GarbageCollectedMixin instance");
  static_assert(
      WTF::IsGarbageCollectedType<UseMixinFromLeftmostInherited>::value,
      "GarbageCollectedMixin derived instance");
  static_assert(WTF::IsGarbageCollectedType<MultipleMixins>::value,
                "GarbageCollectedMixin");

  static_assert(
      WTF::IsGarbageCollectedType<HeapHashSet<Member<IntWrapper>>>::value,
      "HeapHashSet");
  static_assert(
      WTF::IsGarbageCollectedType<HeapLinkedHashSet<Member<IntWrapper>>>::value,
      "HeapLinkedHashSet");
  static_assert(
      WTF::IsGarbageCollectedType<HeapListHashSet<Member<IntWrapper>>>::value,
      "HeapListHashSet");
  static_assert(WTF::IsGarbageCollectedType<
                    HeapHashCountedSet<Member<IntWrapper>>>::value,
                "HeapHashCountedSet");
  static_assert(
      WTF::IsGarbageCollectedType<HeapHashMap<int, Member<IntWrapper>>>::value,
      "HeapHashMap");
  static_assert(
      WTF::IsGarbageCollectedType<HeapVector<Member<IntWrapper>>>::value,
      "HeapVector");
  static_assert(
      WTF::IsGarbageCollectedType<HeapDeque<Member<IntWrapper>>>::value,
      "HeapDeque");
}

TEST_F(HeapInternalsTest, ShrinkVector) {
  // Regression test: https://crbug.com/823289

  HeapVector<Member<IntWrapper>> vector;
  vector.ReserveCapacity(32);
  for (int i = 0; i < 4; i++) {
    vector.push_back(MakeGarbageCollected<IntWrapper>(i));
  }

  ConservativelyCollectGarbage(BlinkGC::kConcurrentAndLazySweeping);

  // The following call tries to promptly free the left overs. In the buggy
  // scenario that would create a free HeapObjectHeader that is assumed to be
  // black which it is not.
  vector.ShrinkToFit();
}

TEST_F(HeapInternalsTest, GarbageCollectedInConstruction) {
  using O = ObjectWithCallbackBeforeInitializer<IntWrapper>;
  MakeGarbageCollected<O>(base::BindOnce([](O* thiz) {
    CHECK(HeapObjectHeader::FromPayload(thiz)->IsInConstruction());
  }));
}

TEST_F(HeapInternalsTest, GarbageCollectedMixinInConstruction) {
  using O = ObjectWithMixinWithCallbackBeforeInitializer<IntWrapper>;
  MakeGarbageCollected<O>(base::BindOnce([](O::Mixin* thiz) {
    const HeapObjectHeader* const header =
        HeapObjectHeader::FromInnerAddress(reinterpret_cast<Address>(thiz));
    CHECK(header->IsInConstruction());
  }));
}

TEST_F(HeapInternalsTest, PersistentAssignsDeletedValue) {
  // Regression test: https://crbug.com/982313

  Persistent<IntWrapper> deleted(WTF::kHashTableDeletedValue);
  Persistent<IntWrapper> pre_initialized(MakeGarbageCollected<IntWrapper>(1));
  pre_initialized = deleted;
  PreciselyCollectGarbage();
}

namespace {
struct HeapHashMapWrapper final : GarbageCollected<HeapHashMapWrapper> {
  HeapHashMapWrapper() {
    for (int i = 0; i < 100; ++i) {
      map_.insert(MakeGarbageCollected<IntWrapper>(i),
                  NonTriviallyDestructible());
    }
  }
  // This should call ~HeapHapMap() -> ~HashMap() -> ~HashTable().
  ~HeapHashMapWrapper() = default;

  void Trace(Visitor* visitor) const { visitor->Trace(map_); }

 private:
  struct NonTriviallyDestructible {
    ~NonTriviallyDestructible() {}
  };
  HeapHashMap<Member<IntWrapper>, NonTriviallyDestructible> map_;
};
}  // namespace

TEST_F(HeapInternalsTest, AccessDeletedBackingStore) {
  // Regression test: https://crbug.com/985443
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      blink::features::kBlinkHeapConcurrentSweeping);
  ClearOutOldGarbage();

  ThreadState* thread_state = ThreadState::Current();

  auto* map = MakeGarbageCollected<HeapHashMapWrapper>();
  // Run marking.
  PreciselyCollectGarbage(BlinkGC::kConcurrentAndLazySweeping);
  // Perform complete sweep on hash_arena.
  BaseArena* hash_arena =
      thread_state->Heap().Arena(BlinkGC::kHashTableArenaIndex);
  {
    ThreadState::AtomicPauseScope scope(thread_state);
    ScriptForbiddenScope script_forbidden_scope;
    ThreadState::SweepForbiddenScope sweep_forbidden(thread_state);
    hash_arena->CompleteSweep();
  }
  BaseArena* map_arena = PageFromObject(map)->Arena();
  // Sweep normal arena, but don't call finalizers.
  while (!map_arena->ConcurrentSweepOnePage()) {
  }
  // Now complete sweeping with PerformIdleLazySweep and call finalizers.
  while (thread_state->IsSweepingInProgress()) {
    thread_state->PerformIdleLazySweep(base::TimeTicks::Max());
  }
}

namespace {
class GCBase : public GarbageCollected<GCBase> {
 public:
  virtual void Trace(Visitor*) const {}
};

class GCDerived final : public GCBase {
 public:
  static int destructor_called;
  void Trace(Visitor* visitor) const override { GCBase::Trace(visitor); }
  ~GCDerived() { ++destructor_called; }
};
int GCDerived::destructor_called = 0;
}  // namespace

TEST_F(HeapInternalsTest, CallMostDerivedFinalizer) {
  MakeGarbageCollected<GCDerived>();
  PreciselyCollectGarbage();
  EXPECT_EQ(1, GCDerived::destructor_called);
}

}  // namespace blink
