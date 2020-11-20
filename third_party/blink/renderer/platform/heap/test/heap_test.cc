/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <algorithm>
#include <atomic>
#include <memory>
#include <utility>

#include "base/atomic_ref_count.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/bindings/buildflags.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_stack.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/impl/marking_visitor.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/heap/thread_state_scopes.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

namespace {

class HeapTest : public TestSupportingGC {};

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

  IntWrapper(int x) : x_(x) {}

 private:
  IntWrapper() = delete;
  int x_;
};

std::atomic_int IntWrapper::destructor_calls_{0};

struct IntWrapperHash {
  static unsigned GetHash(const IntWrapper& key) {
    return WTF::HashInt(static_cast<uint32_t>(key.Value()));
  }

  static bool Equal(const IntWrapper& a, const IntWrapper& b) { return a == b; }
};

static_assert(WTF::IsTraceable<IntWrapper>::value,
              "IsTraceable<> template failed to recognize trace method.");
static_assert(WTF::IsTraceable<HeapVector<IntWrapper>>::value,
              "HeapVector<IntWrapper> must be traceable.");
static_assert(WTF::IsTraceable<HeapDeque<IntWrapper>>::value,
              "HeapDeque<IntWrapper> must be traceable.");
static_assert(WTF::IsTraceable<HeapHashSet<IntWrapper, IntWrapperHash>>::value,
              "HeapHashSet<IntWrapper> must be traceable.");
static_assert(WTF::IsTraceable<HeapHashMap<int, IntWrapper>>::value,
              "HeapHashMap<int, IntWrapper> must be traceable.");

class KeyWithCopyingMoveConstructor final {
  DISALLOW_NEW();

 public:
  struct Hash final {
    STATIC_ONLY(Hash);

   public:
    static unsigned GetHash(const KeyWithCopyingMoveConstructor& key) {
      return key.hash_;
    }

    static bool Equal(const KeyWithCopyingMoveConstructor& x,
                      const KeyWithCopyingMoveConstructor& y) {
      return x.hash_ == y.hash_;
    }

    static constexpr bool safe_to_compare_to_empty_or_deleted = true;
  };

  KeyWithCopyingMoveConstructor() = default;
  KeyWithCopyingMoveConstructor(WTF::HashTableDeletedValueType) : hash_(-1) {}
  ~KeyWithCopyingMoveConstructor() = default;
  KeyWithCopyingMoveConstructor(unsigned hash, const String& string)
      : hash_(hash), string_(string) {
    DCHECK_NE(hash_, 0);
    DCHECK_NE(hash_, -1);
  }
  KeyWithCopyingMoveConstructor(const KeyWithCopyingMoveConstructor&) = default;
  // The move constructor delegates to the copy constructor intentionally.
  KeyWithCopyingMoveConstructor(KeyWithCopyingMoveConstructor&& x)
      : KeyWithCopyingMoveConstructor(x) {}
  KeyWithCopyingMoveConstructor& operator=(
      const KeyWithCopyingMoveConstructor&) = default;
  bool operator==(const KeyWithCopyingMoveConstructor& x) const {
    return hash_ == x.hash_;
  }

  bool IsHashTableDeletedValue() const { return hash_ == -1; }

 private:
  int hash_ = 0;
  String string_;
};

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

class ThreadMarker {
  DISALLOW_NEW();

 public:
  ThreadMarker()
      : creating_thread_(reinterpret_cast<ThreadState*>(0)), num_(0) {}
  ThreadMarker(unsigned i)
      : creating_thread_(ThreadState::Current()), num_(i) {}
  ThreadMarker(WTF::HashTableDeletedValueType deleted)
      : creating_thread_(reinterpret_cast<ThreadState*>(-1)), num_(0) {}
  ~ThreadMarker() {
    EXPECT_TRUE((creating_thread_ == ThreadState::Current()) ||
                (creating_thread_ == reinterpret_cast<ThreadState*>(0)) ||
                (creating_thread_ == reinterpret_cast<ThreadState*>(-1)));
  }
  bool IsHashTableDeletedValue() const {
    return creating_thread_ == reinterpret_cast<ThreadState*>(-1);
  }
  bool operator==(const ThreadMarker& other) const {
    return other.creating_thread_ == creating_thread_ && other.num_ == num_;
  }
  ThreadState* creating_thread_;
  unsigned num_;
};

struct ThreadMarkerHash {
  static unsigned GetHash(const ThreadMarker& key) {
    return static_cast<unsigned>(
        reinterpret_cast<uintptr_t>(key.creating_thread_) + key.num_);
  }

  static bool Equal(const ThreadMarker& a, const ThreadMarker& b) {
    return a == b;
  }

  static const bool safe_to_compare_to_empty_or_deleted = false;
};

}  // namespace

}  // namespace blink

namespace WTF {

template <typename T>
struct DefaultHash;
template <>
struct DefaultHash<blink::ThreadMarker> {
  typedef blink::ThreadMarkerHash Hash;
};

// ThreadMarkerHash is the default hash for ThreadMarker
template <>
struct HashTraits<blink::ThreadMarker>
    : GenericHashTraits<blink::ThreadMarker> {
  static const bool kEmptyValueIsZero = true;
  static void ConstructDeletedValue(blink::ThreadMarker& slot, bool) {
    new (NotNull, &slot) blink::ThreadMarker(kHashTableDeletedValue);
  }
  static bool IsDeletedValue(const blink::ThreadMarker& slot) {
    return slot.IsHashTableDeletedValue();
  }
};

template <>
struct DefaultHash<blink::KeyWithCopyingMoveConstructor> {
  using Hash = blink::KeyWithCopyingMoveConstructor::Hash;
};

template <>
struct HashTraits<blink::KeyWithCopyingMoveConstructor>
    : public SimpleClassHashTraits<blink::KeyWithCopyingMoveConstructor> {};

}  // namespace WTF

namespace blink {

class TestGCCollectGarbageScope {
  STACK_ALLOCATED();

 public:
  explicit TestGCCollectGarbageScope(BlinkGC::StackState state) {
    DCHECK(ThreadState::Current()->CheckThread());
  }

  ~TestGCCollectGarbageScope() { ThreadState::Current()->CompleteSweep(); }
};

class TestGCScope : public TestGCCollectGarbageScope {
 public:
  explicit TestGCScope(BlinkGC::StackState state)
      : TestGCCollectGarbageScope(state) {
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
  }
};

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

static const size_t kClassMagic = 0xABCDDBCA;

class HeapTestSubClass : public HeapTestSuperClass,
                         public HeapTestOtherSuperClass {
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

class OffHeapInt : public RefCounted<OffHeapInt> {
  USING_FAST_MALLOC(OffHeapInt);

 public:
  static scoped_refptr<OffHeapInt> Create(int x) {
    return base::AdoptRef(new OffHeapInt(x));
  }

  virtual ~OffHeapInt() { ++destructor_calls_; }

  static int destructor_calls_;

  int Value() const { return x_; }

  bool operator==(const OffHeapInt& other) const {
    return other.Value() == Value();
  }

  unsigned GetHash() { return IntHash<int>::GetHash(x_); }
  void VoidFunction() {}

 protected:
  OffHeapInt(int x) : x_(x) {}

 private:
  OffHeapInt() = delete;
  int x_;
};

int OffHeapInt::destructor_calls_ = 0;

class ThreadedTesterBase {
 protected:
  static void Test(ThreadedTesterBase* tester) {
    std::unique_ptr<Thread> threads[kNumberOfThreads];
    for (auto& thread : threads) {
      thread = Platform::Current()->CreateThread(
          ThreadCreationParams(ThreadType::kTestThread)
              .SetThreadNameForTest("blink gc testing thread"));
      PostCrossThreadTask(
          *thread->GetTaskRunner(), FROM_HERE,
          CrossThreadBindOnce(ThreadFunc, CrossThreadUnretained(tester)));
    }
    tester->done_.Wait();
    delete tester;
  }

  virtual void RunThread() = 0;

 protected:
  static const int kNumberOfThreads = 10;
  static const int kGcPerThread = 5;
  static const int kNumberOfAllocations = 50;

  virtual ~ThreadedTesterBase() = default;

  inline bool Done() const {
    return gc_count_.load(std::memory_order_acquire) >=
           kNumberOfThreads * kGcPerThread;
  }

  std::atomic_int gc_count_{0};

 private:
  static void ThreadFunc(ThreadedTesterBase* tester) {
    ThreadState::AttachCurrentThread();
    tester->RunThread();
    ThreadState::DetachCurrentThread();
    if (!tester->threads_to_finish_.Decrement())
      tester->done_.Signal();
  }

  base::AtomicRefCount threads_to_finish_{kNumberOfThreads};
  base::WaitableEvent done_;
};

// Needed to give this variable a definition (the initializer above is only a
// declaration), so that subclasses can use it.
const int ThreadedTesterBase::kNumberOfThreads;

class ThreadedHeapTester : public ThreadedTesterBase {
 public:
  static void Test() { ThreadedTesterBase::Test(new ThreadedHeapTester); }

  ~ThreadedHeapTester() override {
    // Verify that the threads cleared their CTPs when
    // terminating, preventing access to a finalized heap.
    for (auto& global_int_wrapper : cross_persistents_) {
      DCHECK(global_int_wrapper.get());
      EXPECT_FALSE(global_int_wrapper.get()->Get());
    }
  }

 protected:
  using GlobalIntWrapperPersistent = CrossThreadPersistent<IntWrapper>;

  Mutex mutex_;
  Vector<std::unique_ptr<GlobalIntWrapperPersistent>> cross_persistents_;

  std::unique_ptr<GlobalIntWrapperPersistent> CreateGlobalPersistent(
      int value) {
    return std::make_unique<GlobalIntWrapperPersistent>(
        MakeGarbageCollected<IntWrapper>(value));
  }

  void AddGlobalPersistent() {
    MutexLocker lock(mutex_);
    cross_persistents_.push_back(CreateGlobalPersistent(0x2a2a2a2a));
  }

  void RunThread() override {
    // Add a cross-thread persistent from this thread; the test object
    // verifies that it will have been cleared out after the threads
    // have all detached, running their termination GCs while doing so.
    AddGlobalPersistent();

    int gc_count = 0;
    while (!Done()) {
      {
        Persistent<IntWrapper> wrapper;

        std::unique_ptr<GlobalIntWrapperPersistent> global_persistent =
            CreateGlobalPersistent(0x0ed0cabb);

        for (int i = 0; i < kNumberOfAllocations; i++) {
          wrapper = MakeGarbageCollected<IntWrapper>(0x0bbac0de);
          if (!(i % 10)) {
            global_persistent = CreateGlobalPersistent(0x0ed0cabb);
          }
          test::YieldCurrentThread();
        }

        if (gc_count < kGcPerThread) {
          TestSupportingGC::PreciselyCollectGarbage();
          gc_count++;
          gc_count_.fetch_add(1, std::memory_order_release);
        }

        TestSupportingGC::PreciselyCollectGarbage();
        EXPECT_EQ(wrapper->Value(), 0x0bbac0de);
        EXPECT_EQ((*global_persistent)->Value(), 0x0ed0cabb);
      }
      test::YieldCurrentThread();
    }
  }
};

class ThreadedWeaknessTester : public ThreadedTesterBase {
 public:
  static void Test() { ThreadedTesterBase::Test(new ThreadedWeaknessTester); }

 private:
  void RunThread() override {
    int gc_count = 0;
    while (!Done()) {
      {
        Persistent<HeapHashMap<ThreadMarker, WeakMember<IntWrapper>>> weak_map =
            MakeGarbageCollected<
                HeapHashMap<ThreadMarker, WeakMember<IntWrapper>>>();

        for (int i = 0; i < kNumberOfAllocations; i++) {
          weak_map->insert(static_cast<unsigned>(i),
                           MakeGarbageCollected<IntWrapper>(0));
          test::YieldCurrentThread();
        }

        if (gc_count < kGcPerThread) {
          TestSupportingGC::PreciselyCollectGarbage();
          gc_count++;
          gc_count_.fetch_add(1, std::memory_order_release);
        }

        TestSupportingGC::PreciselyCollectGarbage();
        EXPECT_TRUE(weak_map->IsEmpty());
      }
      test::YieldCurrentThread();
    }
  }
};

class ThreadPersistentHeapTester : public ThreadedTesterBase {
 public:
  static void Test() {
    ThreadedTesterBase::Test(new ThreadPersistentHeapTester);
  }

 protected:
  class Local final : public GarbageCollected<Local> {
   public:
    Local() = default;

    void Trace(Visitor* visitor) const {}
  };

  class PersistentChain;

  class RefCountedChain : public RefCounted<RefCountedChain> {
   public:
    static RefCountedChain* Create(int count) {
      return new RefCountedChain(count);
    }

   private:
    explicit RefCountedChain(int count) {
      if (count > 0) {
        --count;
        persistent_chain_ = MakeGarbageCollected<PersistentChain>(count);
      }
    }

    Persistent<PersistentChain> persistent_chain_;
  };

  class PersistentChain final : public GarbageCollected<PersistentChain> {
   public:
    explicit PersistentChain(int count) {
      ref_counted_chain_ = base::AdoptRef(RefCountedChain::Create(count));
    }

    void Trace(Visitor* visitor) const {}

   private:
    scoped_refptr<RefCountedChain> ref_counted_chain_;
  };

  void RunThread() override {
    MakeGarbageCollected<PersistentChain>(100);

    // Upon thread detach, GCs will run until all persistents have been
    // released. We verify that the draining of persistents proceeds
    // as expected by dropping one Persistent<> per GC until there
    // are none left.
  }
};

// The accounting for memory includes the memory used by rounding up object
// sizes. This is done in a different way on 32 bit and 64 bit, so we have to
// have some slack in the tests.
template <typename T>
void CheckWithSlack(T expected, T actual, int slack) {
  EXPECT_LE(expected, actual);
  EXPECT_GE((intptr_t)expected + slack, (intptr_t)actual);
}

class TraceCounter final : public GarbageCollected<TraceCounter> {
 public:
  TraceCounter() : trace_count_(0) {}

  void Trace(Visitor* visitor) const { trace_count_++; }
  int TraceCount() const { return trace_count_; }

 private:
  mutable int trace_count_;
};

TEST_F(HeapTest, IsHeapObjectAliveForConstPointer) {
  // See http://crbug.com/661363.
  auto* object = MakeGarbageCollected<SimpleObject>();
  HeapObjectHeader* header = HeapObjectHeader::FromPayload(object);
  LivenessBroker broker = internal::LivenessBrokerFactory::Create();
  EXPECT_TRUE(header->TryMark());
  EXPECT_TRUE(broker.IsHeapObjectAlive(object));
  const SimpleObject* const_object = const_cast<const SimpleObject*>(object);
  EXPECT_TRUE(broker.IsHeapObjectAlive(const_object));
}

class ClassWithMember : public GarbageCollected<ClassWithMember> {
 public:
  ClassWithMember() : trace_counter_(MakeGarbageCollected<TraceCounter>()) {}

  void Trace(Visitor* visitor) const { visitor->Trace(trace_counter_); }
  int TraceCount() const { return trace_counter_->TraceCount(); }

 private:
  Member<TraceCounter> trace_counter_;
};

class SimpleFinalizedObject final
    : public GarbageCollected<SimpleFinalizedObject> {
 public:
  SimpleFinalizedObject() = default;
  ~SimpleFinalizedObject() { ++destructor_calls_; }

  static int destructor_calls_;

  void Trace(Visitor* visitor) const {}
};

int SimpleFinalizedObject::destructor_calls_ = 0;

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

class Foo final : public Bar {
 public:
  Foo(Bar* bar) : Bar(), bar_(bar), points_to_foo_(false) {}

  Foo(Foo* foo) : Bar(), bar_(foo), points_to_foo_(true) {}

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

class Bars final : public Bar {
 public:
  Bars() : width_(0) {
    for (unsigned i = 0; i < kWidth; i++) {
      bars_[i] = MakeGarbageCollected<Bar>();
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
  unsigned width_;
  Member<Bar> bars_[kWidth];
};

class ConstructorAllocation : public GarbageCollected<ConstructorAllocation> {
 public:
  ConstructorAllocation() {
    int_wrapper_ = MakeGarbageCollected<IntWrapper>(42);
  }

  void Trace(Visitor* visitor) const { visitor->Trace(int_wrapper_); }

 private:
  Member<IntWrapper> int_wrapper_;
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

class RefCountedAndGarbageCollected2 final
    : public GarbageCollected<RefCountedAndGarbageCollected2>,
      public HeapTestOtherSuperClass {
 public:
  RefCountedAndGarbageCollected2() : keep_alive_(PERSISTENT_FROM_HERE) {}
  ~RefCountedAndGarbageCollected2() { ++destructor_calls_; }

  void Ref() {
    if (UNLIKELY(!ref_count_)) {
#if DCHECK_IS_ON()
      DCHECK(ThreadState::Current()->Heap().FindPageFromAddress(
          reinterpret_cast<Address>(this)));
#endif
      keep_alive_ = this;
    }
    ++ref_count_;
  }

  void Deref() {
    DCHECK_GT(ref_count_, 0);
    if (!--ref_count_)
      keep_alive_.Clear();
  }

  void Trace(Visitor* visitor) const {}

  static int destructor_calls_;

 private:
  int ref_count_ = 0;
  SelfKeepAlive<RefCountedAndGarbageCollected2> keep_alive_;
};

int RefCountedAndGarbageCollected2::destructor_calls_ = 0;

class Weak final : public Bar {
 public:
  Weak(Bar* strong_bar, Bar* weak_bar)
      : Bar(), strong_bar_(strong_bar), weak_bar_(weak_bar) {}

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
      : Bar(), strong_bar_(strong_bar), weak_bar_(weak_bar) {}

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

class ObservableWithPreFinalizer final
    : public GarbageCollected<ObservableWithPreFinalizer> {
  USING_PRE_FINALIZER(ObservableWithPreFinalizer, Dispose);

 public:
  ObservableWithPreFinalizer() : was_destructed_(false) {}
  ~ObservableWithPreFinalizer() { was_destructed_ = true; }
  void Trace(Visitor* visitor) const {}
  void Dispose() {
    EXPECT_FALSE(was_destructed_);
    dispose_was_called_ = true;
  }
  static bool dispose_was_called_;

 protected:
  bool was_destructed_;
};

bool ObservableWithPreFinalizer::dispose_was_called_ = false;

bool g_dispose_was_called_for_pre_finalizer_base = false;
bool g_dispose_was_called_for_pre_finalizer_mixin = false;
bool g_dispose_was_called_for_pre_finalizer_sub_class = false;

class PreFinalizerBase : public GarbageCollected<PreFinalizerBase> {
  USING_PRE_FINALIZER(PreFinalizerBase, Dispose);

 public:
  PreFinalizerBase() : was_destructed_(false) {}
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
  bool was_destructed_;
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
  PreFinalizerMixin() : was_destructed_(false) {}
  bool was_destructed_;
};

class PreFinalizerSubClass : public PreFinalizerBase, public PreFinalizerMixin {
  USING_PRE_FINALIZER(PreFinalizerSubClass, Dispose);

 public:
  PreFinalizerSubClass() : was_destructed_(false) {}
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
  bool was_destructed_;
};

template <typename T>
class FinalizationObserver : public GarbageCollected<FinalizationObserver<T>> {
 public:
  FinalizationObserver(T* data) : data_(data), did_call_will_finalize_(false) {}

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

class VectorObject {
  DISALLOW_NEW();

 public:
  VectorObject() { value_ = MakeGarbageCollected<SimpleFinalizedObject>(); }

  void Trace(Visitor* visitor) const { visitor->Trace(value_); }

 private:
  Member<SimpleFinalizedObject> value_;
};

class VectorObjectInheritedTrace : public VectorObject {};

class TerminatedArrayItem {
  DISALLOW_NEW();

 public:
  TerminatedArrayItem(IntWrapper* payload)
      : payload_(payload), is_last_(false) {}

  void Trace(Visitor* visitor) const { visitor->Trace(payload_); }

  bool IsLastInArray() const { return is_last_; }
  void SetLastInArray(bool value) { is_last_ = value; }

  IntWrapper* Payload() const { return payload_; }

 private:
  Member<IntWrapper> payload_;
  bool is_last_;
};

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::TerminatedArrayItem)
WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::VectorObject)
WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(
    blink::VectorObjectInheritedTrace)

namespace blink {

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

class DynamicallySizedObject : public GarbageCollected<DynamicallySizedObject> {
 public:
  static DynamicallySizedObject* Create(size_t size) {
    return MakeGarbageCollected<DynamicallySizedObject>(
        AdditionalBytes(size - sizeof(DynamicallySizedObject)));
  }

  uint8_t Get(int i) { return *(reinterpret_cast<uint8_t*>(this) + i); }

  void Trace(Visitor* visitor) const {}
};

class FinalizationAllocator final
    : public GarbageCollected<FinalizationAllocator> {
 public:
  FinalizationAllocator(Persistent<IntWrapper>* wrapper) { wrapper_ = wrapper; }

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

class PreFinalizerBackingShrinkForbidden final
    : public GarbageCollected<PreFinalizerBackingShrinkForbidden> {
  USING_PRE_FINALIZER(PreFinalizerBackingShrinkForbidden, Dispose);

 public:
  PreFinalizerBackingShrinkForbidden() {
    for (int i = 0; i < 32; ++i) {
      vector_.push_back(MakeGarbageCollected<IntWrapper>(i));
    }
    EXPECT_LT(31ul, vector_.capacity());

    for (int i = 0; i < 32; ++i) {
      map_.insert(i + 1, MakeGarbageCollected<IntWrapper>(i + 1));
    }
    EXPECT_LT(31ul, map_.Capacity());
  }

  void Dispose() {
    // Remove all elemets except one so that vector_ will try to shrink.
    for (int i = 1; i < 32; ++i) {
      vector_.pop_back();
    }
    // Check that vector_ hasn't shrunk.
    EXPECT_LT(31ul, vector_.capacity());
    // Just releasing the backing is allowed.
    vector_.clear();
    EXPECT_EQ(0ul, vector_.capacity());

    // Remove elemets so that map_ will try to shrink.
    for (int i = 0; i < 32; ++i) {
      map_.erase(i + 1);
    }
    // Check that map_ hasn't shrunk.
    EXPECT_LT(31ul, map_.Capacity());
    // Just releasing the backing is allowed.
    map_.clear();
    EXPECT_EQ(0ul, map_.Capacity());
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(vector_);
    visitor->Trace(map_);
  }

 private:
  HeapVector<Member<IntWrapper>> vector_;
  HeapHashMap<int, Member<IntWrapper>> map_;
};

// Following 2 tests check for allocation failures. These failures happen
// only when DCHECK is on.
#if DCHECK_IS_ON()
TEST_F(HeapTest, PreFinalizerBackingShrinkForbidden) {
  MakeGarbageCollected<PreFinalizerBackingShrinkForbidden>();
  PreciselyCollectGarbage();
}

class PreFinalizerVectorBackingExpandForbidden final
    : public GarbageCollected<PreFinalizerVectorBackingExpandForbidden> {
  USING_PRE_FINALIZER(PreFinalizerVectorBackingExpandForbidden, Dispose);

 public:
  PreFinalizerVectorBackingExpandForbidden() {
    vector_.push_back(MakeGarbageCollected<IntWrapper>(1));
  }

  void Dispose() { EXPECT_DEATH(Test(), ""); }

  void Test() {
    // vector_'s backing will need to expand.
    for (int i = 0; i < 32; ++i) {
      vector_.push_back(nullptr);
    }
  }

  void Trace(Visitor* visitor) const { visitor->Trace(vector_); }

 private:
  HeapVector<Member<IntWrapper>> vector_;
};

TEST(HeapDeathTest, PreFinalizerVectorBackingExpandForbidden) {
  MakeGarbageCollected<PreFinalizerVectorBackingExpandForbidden>();
  TestSupportingGC::PreciselyCollectGarbage();
}

class PreFinalizerHashTableBackingExpandForbidden final
    : public GarbageCollected<PreFinalizerHashTableBackingExpandForbidden> {
  USING_PRE_FINALIZER(PreFinalizerHashTableBackingExpandForbidden, Dispose);

 public:
  PreFinalizerHashTableBackingExpandForbidden() {
    map_.insert(123, MakeGarbageCollected<IntWrapper>(123));
  }

  void Dispose() { EXPECT_DEATH(Test(), ""); }

  void Test() {
    // map_'s backing will need to expand.
    for (int i = 1; i < 32; ++i) {
      map_.insert(i, nullptr);
    }
  }

  void Trace(Visitor* visitor) const { visitor->Trace(map_); }

 private:
  HeapHashMap<int, Member<IntWrapper>> map_;
};

TEST(HeapDeathTest, PreFinalizerHashTableBackingExpandForbidden) {
  MakeGarbageCollected<PreFinalizerHashTableBackingExpandForbidden>();
  TestSupportingGC::PreciselyCollectGarbage();
}
#endif  // DCHECK_IS_ON()

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

TEST(HeapDeathTest, PreFinalizerAllocationForbidden) {
  MakeGarbageCollected<PreFinalizerAllocationForbidden>();
  TestSupportingGC::PreciselyCollectGarbage();
}

#if DCHECK_IS_ON()
namespace {

class HeapTestResurrectingPreFinalizer
    : public GarbageCollected<HeapTestResurrectingPreFinalizer> {
  USING_PRE_FINALIZER(HeapTestResurrectingPreFinalizer, Dispose);

 public:
  enum TestType {
    kHeapVectorMember,
    kHeapHashSetMember,
    kHeapHashSetWeakMember
  };

  class GlobalStorage : public GarbageCollected<GlobalStorage> {
   public:
    GlobalStorage() {
      // Reserve storage upfront to avoid allocations during pre-finalizer
      // insertion.
      vector_member.ReserveCapacity(32);
      hash_set_member.ReserveCapacityForSize(32);
      hash_set_weak_member.ReserveCapacityForSize(32);
    }

    void Trace(Visitor* visitor) const {
      visitor->Trace(vector_member);
      visitor->Trace(hash_set_member);
      visitor->Trace(hash_set_weak_member);
    }

    HeapVector<Member<LinkedObject>> vector_member;
    HeapHashSet<Member<LinkedObject>> hash_set_member;
    HeapHashSet<WeakMember<LinkedObject>> hash_set_weak_member;
  };

  HeapTestResurrectingPreFinalizer(TestType test_type,
                                   GlobalStorage* storage,
                                   LinkedObject* object_that_dies)
      : test_type_(test_type),
        storage_(storage),
        object_that_dies_(object_that_dies) {}

  void Trace(Visitor* visitor) const {
    visitor->Trace(storage_);
    visitor->Trace(object_that_dies_);
  }

 private:
  void Dispose() { EXPECT_DEATH(Test(), ""); }

  void Test() {
    switch (test_type_) {
      case TestType::kHeapVectorMember:
        storage_->vector_member.push_back(object_that_dies_);
        break;
      case TestType::kHeapHashSetMember:
        storage_->hash_set_member.insert(object_that_dies_);
        break;
      case TestType::kHeapHashSetWeakMember:
        storage_->hash_set_weak_member.insert(object_that_dies_);
        break;
    }
  }

  TestType test_type_;
  Member<GlobalStorage> storage_;
  Member<LinkedObject> object_that_dies_;
};

}  // namespace

TEST(HeapDeathTest, DiesOnResurrectedHeapVectorMember) {
  Persistent<HeapTestResurrectingPreFinalizer::GlobalStorage> storage(
      MakeGarbageCollected<HeapTestResurrectingPreFinalizer::GlobalStorage>());
  MakeGarbageCollected<HeapTestResurrectingPreFinalizer>(
      HeapTestResurrectingPreFinalizer::kHeapVectorMember, storage.Get(),
      MakeGarbageCollected<LinkedObject>());
  TestSupportingGC::PreciselyCollectGarbage();
}

TEST(HeapDeathTest, DiesOnResurrectedHeapHashSetMember) {
  Persistent<HeapTestResurrectingPreFinalizer::GlobalStorage> storage(
      MakeGarbageCollected<HeapTestResurrectingPreFinalizer::GlobalStorage>());
  MakeGarbageCollected<HeapTestResurrectingPreFinalizer>(
      HeapTestResurrectingPreFinalizer::kHeapHashSetMember, storage.Get(),
      MakeGarbageCollected<LinkedObject>());
  TestSupportingGC::PreciselyCollectGarbage();
}

TEST(HeapDeathTest, DiesOnResurrectedHeapHashSetWeakMember) {
  Persistent<HeapTestResurrectingPreFinalizer::GlobalStorage> storage(
      MakeGarbageCollected<HeapTestResurrectingPreFinalizer::GlobalStorage>());
  MakeGarbageCollected<HeapTestResurrectingPreFinalizer>(
      HeapTestResurrectingPreFinalizer::kHeapHashSetWeakMember, storage.Get(),
      MakeGarbageCollected<LinkedObject>());
  TestSupportingGC::PreciselyCollectGarbage();
}
#endif  // DCHECK_IS_ON()

class LargeMixin : public GarbageCollected<LargeMixin>, public Mixin {
 protected:
  char data[65536];
};

TEST(HeapDeathTest, LargeGarbageCollectedMixin) {
  EXPECT_DEATH(MakeGarbageCollected<LargeMixin>(AdditionalBytes(1)), "");
}

TEST_F(HeapTest, Transition) {
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

TEST_F(HeapTest, Threading) {
  ThreadedHeapTester::Test();
}

TEST_F(HeapTest, ThreadedWeakness) {
  ThreadedWeaknessTester::Test();
}

TEST_F(HeapTest, ThreadPersistent) {
  ThreadPersistentHeapTester::Test();
}

TEST_F(HeapTest, BasicFunctionality) {
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
    CompleteSweepingIfNeeded();
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

TEST_F(HeapTest, SimpleAllocation) {
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

TEST_F(HeapTest, SimplePersistent) {
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

TEST_F(HeapTest, SimpleFinalization) {
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
TEST_F(HeapTest, FreelistReuse) {
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

TEST_F(HeapTest, LazySweepingPages) {
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

TEST_F(HeapTest, LazySweepingLargeObjectPages) {
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

TEST_F(HeapTest, Finalization) {
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

TEST_F(HeapTest, TypedArenaSanity) {
  // We use TraceCounter for allocating an object on the general heap.
  Persistent<TraceCounter> general_heap_object =
      MakeGarbageCollected<TraceCounter>();
  Persistent<IntNode> typed_heap_object = IntNode::Create(0);
  EXPECT_NE(PageFromObject(general_heap_object.Get()),
            PageFromObject(typed_heap_object.Get()));
}

TEST_F(HeapTest, NoAllocation) {
  ThreadState* state = ThreadState::Current();
  EXPECT_TRUE(state->IsAllocationAllowed());
  {
    // Disallow allocation
    ThreadState::NoAllocationScope no_allocation_scope(state);
    EXPECT_FALSE(state->IsAllocationAllowed());
  }
  EXPECT_TRUE(state->IsAllocationAllowed());
}

TEST_F(HeapTest, Members) {
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

TEST_F(HeapTest, MarkTest) {
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

TEST_F(HeapTest, DeepTest) {
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

TEST_F(HeapTest, WideTest) {
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

TEST_F(HeapTest, HashMapOfMembers) {
  ClearOutOldGarbage();
  ThreadHeap& heap = ThreadState::Current()->Heap();
  IntWrapper::destructor_calls_ = 0;
  size_t initial_object_payload_size = heap.ObjectPayloadSizeForTesting();
  {
    typedef HeapHashMap<Member<IntWrapper>, Member<IntWrapper>,
                        DefaultHash<Member<IntWrapper>>::Hash,
                        HashTraits<Member<IntWrapper>>,
                        HashTraits<Member<IntWrapper>>>
        HeapObjectIdentityMap;

    Persistent<HeapObjectIdentityMap> map =
        MakeGarbageCollected<HeapObjectIdentityMap>();

    map->clear();
    size_t after_set_was_created = heap.ObjectPayloadSizeForTesting();
    EXPECT_TRUE(after_set_was_created > initial_object_payload_size);

    PreciselyCollectGarbage();
    size_t after_gc = heap.ObjectPayloadSizeForTesting();
    EXPECT_EQ(after_gc, after_set_was_created);

    // If the additions below cause garbage collections, these
    // pointers should be found by conservative stack scanning.
    auto* one(MakeGarbageCollected<IntWrapper>(1));
    auto* another_one(MakeGarbageCollected<IntWrapper>(1));

    map->insert(one, one);

    size_t after_one_add = heap.ObjectPayloadSizeForTesting();
    EXPECT_TRUE(after_one_add > after_gc);

    HeapObjectIdentityMap::iterator it(map->begin());
    HeapObjectIdentityMap::iterator it2(map->begin());
    ++it;
    ++it2;

    map->insert(another_one, one);

    // The addition above can cause an allocation of a new
    // backing store. We therefore garbage collect before
    // taking the heap stats in order to get rid of the old
    // backing store. We make sure to not use conservative
    // stack scanning as that could find a pointer to the
    // old backing.
    PreciselyCollectGarbage();
    size_t after_add_and_gc = heap.ObjectPayloadSizeForTesting();
    EXPECT_TRUE(after_add_and_gc >= after_one_add);

    EXPECT_EQ(map->size(), 2u);  // Two different wrappings of '1' are distinct.

    PreciselyCollectGarbage();
    EXPECT_TRUE(map->Contains(one));
    EXPECT_TRUE(map->Contains(another_one));

    IntWrapper* gotten(map->at(one));
    EXPECT_EQ(gotten->Value(), one->Value());
    EXPECT_EQ(gotten, one);

    size_t after_gc2 = heap.ObjectPayloadSizeForTesting();
    EXPECT_EQ(after_gc2, after_add_and_gc);

    IntWrapper* dozen = nullptr;

    for (int i = 1; i < 1000; i++) {  // 999 iterations.
      auto* i_wrapper(MakeGarbageCollected<IntWrapper>(i));
      auto* i_squared(MakeGarbageCollected<IntWrapper>(i * i));
      map->insert(i_wrapper, i_squared);
      if (i == 12)
        dozen = i_wrapper;
    }
    size_t after_adding1000 = heap.ObjectPayloadSizeForTesting();
    EXPECT_TRUE(after_adding1000 > after_gc2);

    IntWrapper* gross(map->at(dozen));
    EXPECT_EQ(gross->Value(), 144);

    // This should clear out any junk backings created by all the adds.
    PreciselyCollectGarbage();
    size_t after_gc3 = heap.ObjectPayloadSizeForTesting();
    EXPECT_TRUE(after_gc3 <= after_adding1000);
  }

  PreciselyCollectGarbage();
  // The objects 'one', anotherOne, and the 999 other pairs.
  EXPECT_EQ(IntWrapper::destructor_calls_, 2000);
  size_t after_gc4 = heap.ObjectPayloadSizeForTesting();
  EXPECT_EQ(after_gc4, initial_object_payload_size);
}

TEST_F(HeapTest, NestedAllocation) {
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

TEST_F(HeapTest, LargeHeapObjects) {
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

// This test often fails on Android (https://crbug.com/843032).
// We run out of memory on Android devices because ReserveCapacityForSize
// actually allocates a much larger backing than specified (in this case 400MB).
#if defined(OS_ANDROID)
#define MAYBE_LargeHashMap DISABLED_LargeHashMap
#else
#define MAYBE_LargeHashMap LargeHashMap
#endif
TEST_F(HeapTest, MAYBE_LargeHashMap) {
  ClearOutOldGarbage();

  // Try to allocate a HashTable larger than kMaxHeapObjectSize
  // (crbug.com/597953).
  wtf_size_t size = HeapAllocator::kMaxHeapObjectSize /
                    sizeof(HeapHashMap<int, Member<IntWrapper>>::ValueType);
  Persistent<HeapHashMap<int, Member<IntWrapper>>> map =
      MakeGarbageCollected<HeapHashMap<int, Member<IntWrapper>>>();
  map->ReserveCapacityForSize(size);
  EXPECT_LE(size, map->Capacity());
}

TEST_F(HeapTest, LargeVector) {
  ClearOutOldGarbage();

  // Try to allocate a HeapVectors larger than kMaxHeapObjectSize
  // (crbug.com/597953).
  const wtf_size_t size =
      HeapAllocator::kMaxHeapObjectSize / sizeof(Member<IntWrapper>);
  Persistent<HeapVector<Member<IntWrapper>>> vector =
      MakeGarbageCollected<HeapVector<Member<IntWrapper>>>(size);
  EXPECT_LE(size, vector->capacity());
}

typedef std::pair<Member<IntWrapper>, int> PairWrappedUnwrapped;
typedef std::pair<int, Member<IntWrapper>> PairUnwrappedWrapped;
typedef std::pair<WeakMember<IntWrapper>, Member<IntWrapper>> PairWeakStrong;
typedef std::pair<Member<IntWrapper>, WeakMember<IntWrapper>> PairStrongWeak;
typedef std::pair<WeakMember<IntWrapper>, int> PairWeakUnwrapped;
typedef std::pair<int, WeakMember<IntWrapper>> PairUnwrappedWeak;

class Container final : public GarbageCollected<Container> {
 public:
  HeapHashMap<Member<IntWrapper>, Member<IntWrapper>> map;
  HeapHashSet<Member<IntWrapper>> set;
  HeapHashSet<Member<IntWrapper>> set2;
  HeapHashCountedSet<Member<IntWrapper>> set3;
  HeapVector<Member<IntWrapper>, 2> vector;
  HeapVector<PairWrappedUnwrapped, 2> vector_wu;
  HeapVector<PairUnwrappedWrapped, 2> vector_uw;
  HeapDeque<Member<IntWrapper>> deque;
  void Trace(Visitor* visitor) const {
    visitor->Trace(map);
    visitor->Trace(set);
    visitor->Trace(set2);
    visitor->Trace(set3);
    visitor->Trace(vector);
    visitor->Trace(vector_wu);
    visitor->Trace(vector_uw);
    visitor->Trace(deque);
  }
};

TEST_F(HeapTest, HeapVectorFilledWithValue) {
  auto* val = MakeGarbageCollected<IntWrapper>(1);
  HeapVector<Member<IntWrapper>> vector(10, val);
  EXPECT_EQ(10u, vector.size());
  for (wtf_size_t i = 0; i < vector.size(); i++)
    EXPECT_EQ(val, vector[i]);
}

TEST_F(HeapTest, HeapVectorWithInlineCapacity) {
  auto* one = MakeGarbageCollected<IntWrapper>(1);
  auto* two = MakeGarbageCollected<IntWrapper>(2);
  auto* three = MakeGarbageCollected<IntWrapper>(3);
  auto* four = MakeGarbageCollected<IntWrapper>(4);
  auto* five = MakeGarbageCollected<IntWrapper>(5);
  auto* six = MakeGarbageCollected<IntWrapper>(6);
  {
    HeapVector<Member<IntWrapper>, 2> vector;
    vector.push_back(one);
    vector.push_back(two);
    ConservativelyCollectGarbage();
    EXPECT_TRUE(vector.Contains(one));
    EXPECT_TRUE(vector.Contains(two));

    vector.push_back(three);
    vector.push_back(four);
    ConservativelyCollectGarbage();
    EXPECT_TRUE(vector.Contains(one));
    EXPECT_TRUE(vector.Contains(two));
    EXPECT_TRUE(vector.Contains(three));
    EXPECT_TRUE(vector.Contains(four));

    vector.Shrink(1);
    ConservativelyCollectGarbage();
    EXPECT_TRUE(vector.Contains(one));
    EXPECT_FALSE(vector.Contains(two));
    EXPECT_FALSE(vector.Contains(three));
    EXPECT_FALSE(vector.Contains(four));
  }
  {
    HeapVector<Member<IntWrapper>, 2> vector1;
    HeapVector<Member<IntWrapper>, 2> vector2;

    vector1.push_back(one);
    vector2.push_back(two);
    vector1.swap(vector2);
    ConservativelyCollectGarbage();
    EXPECT_TRUE(vector1.Contains(two));
    EXPECT_TRUE(vector2.Contains(one));
  }
  {
    HeapVector<Member<IntWrapper>, 2> vector1;
    HeapVector<Member<IntWrapper>, 2> vector2;

    vector1.push_back(one);
    vector1.push_back(two);
    vector2.push_back(three);
    vector2.push_back(four);
    vector2.push_back(five);
    vector2.push_back(six);
    vector1.swap(vector2);
    ConservativelyCollectGarbage();
    EXPECT_TRUE(vector1.Contains(three));
    EXPECT_TRUE(vector1.Contains(four));
    EXPECT_TRUE(vector1.Contains(five));
    EXPECT_TRUE(vector1.Contains(six));
    EXPECT_TRUE(vector2.Contains(one));
    EXPECT_TRUE(vector2.Contains(two));
  }
}

TEST_F(HeapTest, HeapVectorShrinkCapacity) {
  ClearOutOldGarbage();
  HeapVector<Member<IntWrapper>> vector1;
  HeapVector<Member<IntWrapper>> vector2;
  vector1.ReserveCapacity(96);
  EXPECT_LE(96u, vector1.capacity());
  vector1.Grow(vector1.capacity());

  // Assumes none was allocated just after a vector backing of vector1.
  vector1.Shrink(56);
  vector1.ShrinkToFit();
  EXPECT_GT(96u, vector1.capacity());

  vector2.ReserveCapacity(20);
  // Assumes another vector backing was allocated just after the vector
  // backing of vector1.
  vector1.Shrink(10);
  vector1.ShrinkToFit();
  EXPECT_GT(56u, vector1.capacity());

  vector1.Grow(192);
  EXPECT_LE(192u, vector1.capacity());
}

TEST_F(HeapTest, HeapVectorShrinkInlineCapacity) {
  ClearOutOldGarbage();
  const size_t kInlineCapacity = 64;
  HeapVector<Member<IntWrapper>, kInlineCapacity> vector1;
  vector1.ReserveCapacity(128);
  EXPECT_LE(128u, vector1.capacity());
  vector1.Grow(vector1.capacity());

  // Shrink the external buffer.
  vector1.Shrink(90);
  vector1.ShrinkToFit();
  EXPECT_GT(128u, vector1.capacity());

// TODO(sof): if the ASan support for 'contiguous containers' is enabled,
// Vector inline buffers are disabled; that constraint should be attempted
// removed, but until that time, disable testing handling of capacities
// of inline buffers.
#if !defined(ANNOTATE_CONTIGUOUS_CONTAINER)
  // Shrinking switches the buffer from the external one to the inline one.
  vector1.Shrink(kInlineCapacity - 1);
  vector1.ShrinkToFit();
  EXPECT_EQ(kInlineCapacity, vector1.capacity());

  // Try to shrink the inline buffer.
  vector1.Shrink(1);
  vector1.ShrinkToFit();
  EXPECT_EQ(kInlineCapacity, vector1.capacity());
#endif
}

TEST_F(HeapTest, HeapVectorOnStackLargeObjectPageSized) {
  ClearOutOldGarbage();
  // Try to allocate a vector of a size that will end exactly where the
  // LargeObjectPage ends.
  using Container = HeapVector<Member<IntWrapper>>;
  Container vector;
  wtf_size_t size =
      (kLargeObjectSizeThreshold + BlinkGuardPageSize() -
       static_cast<wtf_size_t>(LargeObjectPage::PageHeaderSize()) -
       sizeof(HeapObjectHeader)) /
      sizeof(Container::ValueType);
  vector.ReserveCapacity(size);
  for (unsigned i = 0; i < size; ++i)
    vector.push_back(MakeGarbageCollected<IntWrapper>(i));
  ConservativelyCollectGarbage();
}

template <typename T, typename U>
bool DequeContains(HeapDeque<T>& deque, U u) {
  typedef typename HeapDeque<T>::iterator iterator;
  for (iterator it = deque.begin(); it != deque.end(); ++it) {
    if (*it == u)
      return true;
  }
  return false;
}

TEST_F(HeapTest, HeapCollectionTypes) {
  IntWrapper::destructor_calls_ = 0;

  typedef HeapHashMap<Member<IntWrapper>, Member<IntWrapper>> MemberMember;
  typedef HeapHashMap<Member<IntWrapper>, int> MemberPrimitive;
  typedef HeapHashMap<int, Member<IntWrapper>> PrimitiveMember;

  typedef HeapHashSet<Member<IntWrapper>> MemberSet;
  typedef HeapHashCountedSet<Member<IntWrapper>> MemberCountedSet;

  typedef HeapVector<Member<IntWrapper>, 2> MemberVector;
  typedef HeapDeque<Member<IntWrapper>> MemberDeque;

  typedef HeapVector<PairWrappedUnwrapped, 2> VectorWU;
  typedef HeapVector<PairUnwrappedWrapped, 2> VectorUW;

  Persistent<MemberMember> member_member = MakeGarbageCollected<MemberMember>();
  Persistent<MemberMember> member_member2 =
      MakeGarbageCollected<MemberMember>();
  Persistent<MemberMember> member_member3 =
      MakeGarbageCollected<MemberMember>();
  Persistent<MemberPrimitive> member_primitive =
      MakeGarbageCollected<MemberPrimitive>();
  Persistent<PrimitiveMember> primitive_member =
      MakeGarbageCollected<PrimitiveMember>();
  Persistent<MemberSet> set = MakeGarbageCollected<MemberSet>();
  Persistent<MemberSet> set2 = MakeGarbageCollected<MemberSet>();
  Persistent<MemberCountedSet> set3 = MakeGarbageCollected<MemberCountedSet>();
  Persistent<MemberVector> vector = MakeGarbageCollected<MemberVector>();
  Persistent<MemberVector> vector2 = MakeGarbageCollected<MemberVector>();
  Persistent<VectorWU> vector_wu = MakeGarbageCollected<VectorWU>();
  Persistent<VectorWU> vector_wu2 = MakeGarbageCollected<VectorWU>();
  Persistent<VectorUW> vector_uw = MakeGarbageCollected<VectorUW>();
  Persistent<VectorUW> vector_uw2 = MakeGarbageCollected<VectorUW>();
  Persistent<MemberDeque> deque = MakeGarbageCollected<MemberDeque>();
  Persistent<MemberDeque> deque2 = MakeGarbageCollected<MemberDeque>();
  Persistent<Container> container = MakeGarbageCollected<Container>();

  ClearOutOldGarbage();
  {
    Persistent<IntWrapper> one(MakeGarbageCollected<IntWrapper>(1));
    Persistent<IntWrapper> two(MakeGarbageCollected<IntWrapper>(2));
    Persistent<IntWrapper> one_b(MakeGarbageCollected<IntWrapper>(1));
    Persistent<IntWrapper> two_b(MakeGarbageCollected<IntWrapper>(2));
    Persistent<IntWrapper> one_c(MakeGarbageCollected<IntWrapper>(1));
    Persistent<IntWrapper> one_d(MakeGarbageCollected<IntWrapper>(1));
    Persistent<IntWrapper> one_e(MakeGarbageCollected<IntWrapper>(1));
    Persistent<IntWrapper> one_f(MakeGarbageCollected<IntWrapper>(1));
    {
      auto* three_b(MakeGarbageCollected<IntWrapper>(3));
      auto* three_c(MakeGarbageCollected<IntWrapper>(3));
      auto* three_d(MakeGarbageCollected<IntWrapper>(3));
      auto* three_e(MakeGarbageCollected<IntWrapper>(3));
      auto* three(MakeGarbageCollected<IntWrapper>(3));
      auto* four_b(MakeGarbageCollected<IntWrapper>(4));
      auto* four_c(MakeGarbageCollected<IntWrapper>(4));
      auto* four_d(MakeGarbageCollected<IntWrapper>(4));
      auto* four_e(MakeGarbageCollected<IntWrapper>(4));
      auto* four(MakeGarbageCollected<IntWrapper>(4));
      auto* five_c(MakeGarbageCollected<IntWrapper>(5));
      auto* five_d(MakeGarbageCollected<IntWrapper>(5));

      // Member Collections.
      member_member2->insert(one, two);
      member_member2->insert(two, three);
      member_member2->insert(three, four);
      member_member2->insert(four, one);
      primitive_member->insert(1, two);
      primitive_member->insert(2, three);
      primitive_member->insert(3, four);
      primitive_member->insert(4, one);
      member_primitive->insert(one, 2);
      member_primitive->insert(two, 3);
      member_primitive->insert(three, 4);
      member_primitive->insert(four, 1);
      set2->insert(one);
      set2->insert(two);
      set2->insert(three);
      set2->insert(four);
      set->insert(one_b);
      set3->insert(one_b);
      set3->insert(one_b);
      vector->push_back(one_b);
      deque->push_back(one_b);
      vector2->push_back(three_b);
      vector2->push_back(four_b);
      deque2->push_back(three_e);
      deque2->push_back(four_e);
      vector_wu->push_back(PairWrappedUnwrapped(&*one_c, 42));
      vector_wu2->push_back(PairWrappedUnwrapped(&*three_c, 43));
      vector_wu2->push_back(PairWrappedUnwrapped(&*four_c, 44));
      vector_wu2->push_back(PairWrappedUnwrapped(&*five_c, 45));
      vector_uw->push_back(PairUnwrappedWrapped(1, &*one_d));
      vector_uw2->push_back(PairUnwrappedWrapped(103, &*three_d));
      vector_uw2->push_back(PairUnwrappedWrapped(104, &*four_d));
      vector_uw2->push_back(PairUnwrappedWrapped(105, &*five_d));

      EXPECT_TRUE(DequeContains(*deque, one_b));

      // Collect garbage. This should change nothing since we are keeping
      // alive the IntWrapper objects with on-stack pointers.
      ConservativelyCollectGarbage();

      EXPECT_TRUE(DequeContains(*deque, one_b));

      EXPECT_EQ(0u, member_member->size());
      EXPECT_EQ(4u, member_member2->size());
      EXPECT_EQ(4u, primitive_member->size());
      EXPECT_EQ(4u, member_primitive->size());
      EXPECT_EQ(1u, set->size());
      EXPECT_EQ(4u, set2->size());
      EXPECT_EQ(1u, set3->size());
      EXPECT_EQ(1u, vector->size());
      EXPECT_EQ(2u, vector2->size());
      EXPECT_EQ(1u, vector_wu->size());
      EXPECT_EQ(3u, vector_wu2->size());
      EXPECT_EQ(1u, vector_uw->size());
      EXPECT_EQ(3u, vector_uw2->size());
      EXPECT_EQ(1u, deque->size());
      EXPECT_EQ(2u, deque2->size());

      MemberVector& cvec = container->vector;
      cvec.swap(*vector.Get());
      vector2->swap(cvec);
      vector->swap(cvec);

      VectorWU& cvec_wu = container->vector_wu;
      cvec_wu.swap(*vector_wu.Get());
      vector_wu2->swap(cvec_wu);
      vector_wu->swap(cvec_wu);

      VectorUW& cvec_uw = container->vector_uw;
      cvec_uw.swap(*vector_uw.Get());
      vector_uw2->swap(cvec_uw);
      vector_uw->swap(cvec_uw);

      MemberDeque& c_deque = container->deque;
      c_deque.Swap(*deque.Get());
      deque2->Swap(c_deque);
      deque->Swap(c_deque);

      // Swap set and set2 in a roundabout way.
      MemberSet& cset1 = container->set;
      MemberSet& cset2 = container->set2;
      set->swap(cset1);
      set2->swap(cset2);
      set->swap(cset2);
      cset1.swap(cset2);
      cset2.swap(*set2);

      MemberCountedSet& c_counted_set = container->set3;
      set3->swap(c_counted_set);
      EXPECT_EQ(0u, set3->size());
      set3->swap(c_counted_set);

      // Triple swap.
      container->map.swap(*member_member2);
      MemberMember& contained_map = container->map;
      member_member3->swap(contained_map);
      member_member3->swap(*member_member);

      EXPECT_TRUE(member_member->at(one) == two);
      EXPECT_TRUE(member_member->at(two) == three);
      EXPECT_TRUE(member_member->at(three) == four);
      EXPECT_TRUE(member_member->at(four) == one);
      EXPECT_TRUE(primitive_member->at(1) == two);
      EXPECT_TRUE(primitive_member->at(2) == three);
      EXPECT_TRUE(primitive_member->at(3) == four);
      EXPECT_TRUE(primitive_member->at(4) == one);
      EXPECT_EQ(1, member_primitive->at(four));
      EXPECT_EQ(2, member_primitive->at(one));
      EXPECT_EQ(3, member_primitive->at(two));
      EXPECT_EQ(4, member_primitive->at(three));
      EXPECT_TRUE(set->Contains(one));
      EXPECT_TRUE(set->Contains(two));
      EXPECT_TRUE(set->Contains(three));
      EXPECT_TRUE(set->Contains(four));
      EXPECT_TRUE(set2->Contains(one_b));
      EXPECT_TRUE(set3->Contains(one_b));
      EXPECT_TRUE(vector->Contains(three_b));
      EXPECT_TRUE(vector->Contains(four_b));
      EXPECT_TRUE(DequeContains(*deque, three_e));
      EXPECT_TRUE(DequeContains(*deque, four_e));
      EXPECT_TRUE(vector2->Contains(one_b));
      EXPECT_FALSE(vector2->Contains(three_b));
      EXPECT_TRUE(DequeContains(*deque2, one_b));
      EXPECT_FALSE(DequeContains(*deque2, three_e));
      EXPECT_TRUE(vector_wu->Contains(PairWrappedUnwrapped(&*three_c, 43)));
      EXPECT_TRUE(vector_wu->Contains(PairWrappedUnwrapped(&*four_c, 44)));
      EXPECT_TRUE(vector_wu->Contains(PairWrappedUnwrapped(&*five_c, 45)));
      EXPECT_TRUE(vector_wu2->Contains(PairWrappedUnwrapped(&*one_c, 42)));
      EXPECT_FALSE(vector_wu2->Contains(PairWrappedUnwrapped(&*three_c, 43)));
      EXPECT_TRUE(vector_uw->Contains(PairUnwrappedWrapped(103, &*three_d)));
      EXPECT_TRUE(vector_uw->Contains(PairUnwrappedWrapped(104, &*four_d)));
      EXPECT_TRUE(vector_uw->Contains(PairUnwrappedWrapped(105, &*five_d)));
      EXPECT_TRUE(vector_uw2->Contains(PairUnwrappedWrapped(1, &*one_d)));
      EXPECT_FALSE(vector_uw2->Contains(PairUnwrappedWrapped(103, &*three_d)));
    }

    PreciselyCollectGarbage();

    EXPECT_EQ(4u, member_member->size());
    EXPECT_EQ(0u, member_member2->size());
    EXPECT_EQ(4u, primitive_member->size());
    EXPECT_EQ(4u, member_primitive->size());
    EXPECT_EQ(4u, set->size());
    EXPECT_EQ(1u, set2->size());
    EXPECT_EQ(1u, set3->size());
    EXPECT_EQ(2u, vector->size());
    EXPECT_EQ(1u, vector2->size());
    EXPECT_EQ(3u, vector_uw->size());
    EXPECT_EQ(1u, vector2->size());
    EXPECT_EQ(2u, deque->size());
    EXPECT_EQ(1u, deque2->size());
    EXPECT_EQ(1u, deque2->size());

    EXPECT_TRUE(member_member->at(one) == two);
    EXPECT_TRUE(primitive_member->at(1) == two);
    EXPECT_TRUE(primitive_member->at(4) == one);
    EXPECT_EQ(2, member_primitive->at(one));
    EXPECT_EQ(3, member_primitive->at(two));
    EXPECT_TRUE(set->Contains(one));
    EXPECT_TRUE(set->Contains(two));
    EXPECT_FALSE(set->Contains(one_b));
    EXPECT_TRUE(set2->Contains(one_b));
    EXPECT_TRUE(set3->Contains(one_b));
    EXPECT_EQ(2u, set3->find(one_b)->value);
    EXPECT_EQ(3, vector->at(0)->Value());
    EXPECT_EQ(4, vector->at(1)->Value());
    EXPECT_EQ(3, deque->begin()->Get()->Value());
  }

  PreciselyCollectGarbage();
  PreciselyCollectGarbage();

  EXPECT_EQ(4u, member_member->size());
  EXPECT_EQ(4u, primitive_member->size());
  EXPECT_EQ(4u, member_primitive->size());
  EXPECT_EQ(4u, set->size());
  EXPECT_EQ(1u, set2->size());
  EXPECT_EQ(2u, vector->size());
  EXPECT_EQ(1u, vector2->size());
  EXPECT_EQ(3u, vector_wu->size());
  EXPECT_EQ(1u, vector_wu2->size());
  EXPECT_EQ(3u, vector_uw->size());
  EXPECT_EQ(1u, vector_uw2->size());
  EXPECT_EQ(2u, deque->size());
  EXPECT_EQ(1u, deque2->size());
}

TEST_F(HeapTest, PersistentVector) {
  IntWrapper::destructor_calls_ = 0;

  typedef Vector<Persistent<IntWrapper>> PersistentVector;

  Persistent<IntWrapper> one(MakeGarbageCollected<IntWrapper>(1));
  Persistent<IntWrapper> two(MakeGarbageCollected<IntWrapper>(2));
  Persistent<IntWrapper> three(MakeGarbageCollected<IntWrapper>(3));
  Persistent<IntWrapper> four(MakeGarbageCollected<IntWrapper>(4));
  Persistent<IntWrapper> five(MakeGarbageCollected<IntWrapper>(5));
  Persistent<IntWrapper> six(MakeGarbageCollected<IntWrapper>(6));
  {
    PersistentVector vector;
    vector.push_back(one);
    vector.push_back(two);
    ConservativelyCollectGarbage();
    EXPECT_TRUE(vector.Contains(one));
    EXPECT_TRUE(vector.Contains(two));

    vector.push_back(three);
    vector.push_back(four);
    ConservativelyCollectGarbage();
    EXPECT_TRUE(vector.Contains(one));
    EXPECT_TRUE(vector.Contains(two));
    EXPECT_TRUE(vector.Contains(three));
    EXPECT_TRUE(vector.Contains(four));

    vector.Shrink(1);
    ConservativelyCollectGarbage();
    EXPECT_TRUE(vector.Contains(one));
    EXPECT_FALSE(vector.Contains(two));
    EXPECT_FALSE(vector.Contains(three));
    EXPECT_FALSE(vector.Contains(four));
  }
  {
    PersistentVector vector1;
    PersistentVector vector2;

    vector1.push_back(one);
    vector2.push_back(two);
    vector1.swap(vector2);
    ConservativelyCollectGarbage();
    EXPECT_TRUE(vector1.Contains(two));
    EXPECT_TRUE(vector2.Contains(one));
  }
  {
    PersistentVector vector1;
    PersistentVector vector2;

    vector1.push_back(one);
    vector1.push_back(two);
    vector2.push_back(three);
    vector2.push_back(four);
    vector2.push_back(five);
    vector2.push_back(six);
    vector1.swap(vector2);
    ConservativelyCollectGarbage();
    EXPECT_TRUE(vector1.Contains(three));
    EXPECT_TRUE(vector1.Contains(four));
    EXPECT_TRUE(vector1.Contains(five));
    EXPECT_TRUE(vector1.Contains(six));
    EXPECT_TRUE(vector2.Contains(one));
    EXPECT_TRUE(vector2.Contains(two));
  }
}

TEST_F(HeapTest, CrossThreadPersistentVector) {
  IntWrapper::destructor_calls_ = 0;

  typedef Vector<CrossThreadPersistent<IntWrapper>> CrossThreadPersistentVector;

  CrossThreadPersistent<IntWrapper> one(MakeGarbageCollected<IntWrapper>(1));
  CrossThreadPersistent<IntWrapper> two(MakeGarbageCollected<IntWrapper>(2));
  CrossThreadPersistent<IntWrapper> three(MakeGarbageCollected<IntWrapper>(3));
  CrossThreadPersistent<IntWrapper> four(MakeGarbageCollected<IntWrapper>(4));
  CrossThreadPersistent<IntWrapper> five(MakeGarbageCollected<IntWrapper>(5));
  CrossThreadPersistent<IntWrapper> six(MakeGarbageCollected<IntWrapper>(6));
  {
    CrossThreadPersistentVector vector;
    vector.push_back(one);
    vector.push_back(two);
    ConservativelyCollectGarbage();
    EXPECT_TRUE(vector.Contains(one));
    EXPECT_TRUE(vector.Contains(two));

    vector.push_back(three);
    vector.push_back(four);
    ConservativelyCollectGarbage();
    EXPECT_TRUE(vector.Contains(one));
    EXPECT_TRUE(vector.Contains(two));
    EXPECT_TRUE(vector.Contains(three));
    EXPECT_TRUE(vector.Contains(four));

    vector.Shrink(1);
    ConservativelyCollectGarbage();
    EXPECT_TRUE(vector.Contains(one));
    EXPECT_FALSE(vector.Contains(two));
    EXPECT_FALSE(vector.Contains(three));
    EXPECT_FALSE(vector.Contains(four));
  }
  {
    CrossThreadPersistentVector vector1;
    CrossThreadPersistentVector vector2;

    vector1.push_back(one);
    vector2.push_back(two);
    vector1.swap(vector2);
    ConservativelyCollectGarbage();
    EXPECT_TRUE(vector1.Contains(two));
    EXPECT_TRUE(vector2.Contains(one));
  }
  {
    CrossThreadPersistentVector vector1;
    CrossThreadPersistentVector vector2;

    vector1.push_back(one);
    vector1.push_back(two);
    vector2.push_back(three);
    vector2.push_back(four);
    vector2.push_back(five);
    vector2.push_back(six);
    vector1.swap(vector2);
    ConservativelyCollectGarbage();
    EXPECT_TRUE(vector1.Contains(three));
    EXPECT_TRUE(vector1.Contains(four));
    EXPECT_TRUE(vector1.Contains(five));
    EXPECT_TRUE(vector1.Contains(six));
    EXPECT_TRUE(vector2.Contains(one));
    EXPECT_TRUE(vector2.Contains(two));
  }
}

TEST_F(HeapTest, PersistentSet) {
  IntWrapper::destructor_calls_ = 0;

  typedef HashSet<Persistent<IntWrapper>> PersistentSet;

  auto* one_raw = MakeGarbageCollected<IntWrapper>(1);
  Persistent<IntWrapper> one(one_raw);
  Persistent<IntWrapper> one2(one_raw);
  Persistent<IntWrapper> two(MakeGarbageCollected<IntWrapper>(2));
  Persistent<IntWrapper> three(MakeGarbageCollected<IntWrapper>(3));
  Persistent<IntWrapper> four(MakeGarbageCollected<IntWrapper>(4));
  Persistent<IntWrapper> five(MakeGarbageCollected<IntWrapper>(5));
  Persistent<IntWrapper> six(MakeGarbageCollected<IntWrapper>(6));
  {
    PersistentSet set;
    set.insert(one);
    set.insert(two);
    ConservativelyCollectGarbage();
    EXPECT_TRUE(set.Contains(one));
    EXPECT_TRUE(set.Contains(one2));
    EXPECT_TRUE(set.Contains(two));

    set.insert(three);
    set.insert(four);
    ConservativelyCollectGarbage();
    EXPECT_TRUE(set.Contains(one));
    EXPECT_TRUE(set.Contains(two));
    EXPECT_TRUE(set.Contains(three));
    EXPECT_TRUE(set.Contains(four));

    set.clear();
    ConservativelyCollectGarbage();
    EXPECT_FALSE(set.Contains(one));
    EXPECT_FALSE(set.Contains(two));
    EXPECT_FALSE(set.Contains(three));
    EXPECT_FALSE(set.Contains(four));
  }
  {
    PersistentSet set1;
    PersistentSet set2;

    set1.insert(one);
    set2.insert(two);
    set1.swap(set2);
    ConservativelyCollectGarbage();
    EXPECT_TRUE(set1.Contains(two));
    EXPECT_TRUE(set2.Contains(one));
    EXPECT_TRUE(set2.Contains(one2));
  }
}

TEST_F(HeapTest, CrossThreadPersistentSet) {
  IntWrapper::destructor_calls_ = 0;

  typedef HashSet<CrossThreadPersistent<IntWrapper>> CrossThreadPersistentSet;

  auto* one_raw = MakeGarbageCollected<IntWrapper>(1);
  CrossThreadPersistent<IntWrapper> one(one_raw);
  CrossThreadPersistent<IntWrapper> one2(one_raw);
  CrossThreadPersistent<IntWrapper> two(MakeGarbageCollected<IntWrapper>(2));
  CrossThreadPersistent<IntWrapper> three(MakeGarbageCollected<IntWrapper>(3));
  CrossThreadPersistent<IntWrapper> four(MakeGarbageCollected<IntWrapper>(4));
  CrossThreadPersistent<IntWrapper> five(MakeGarbageCollected<IntWrapper>(5));
  CrossThreadPersistent<IntWrapper> six(MakeGarbageCollected<IntWrapper>(6));
  {
    CrossThreadPersistentSet set;
    set.insert(one);
    set.insert(two);
    ConservativelyCollectGarbage();
    EXPECT_TRUE(set.Contains(one));
    EXPECT_TRUE(set.Contains(one2));
    EXPECT_TRUE(set.Contains(two));

    set.insert(three);
    set.insert(four);
    ConservativelyCollectGarbage();
    EXPECT_TRUE(set.Contains(one));
    EXPECT_TRUE(set.Contains(two));
    EXPECT_TRUE(set.Contains(three));
    EXPECT_TRUE(set.Contains(four));

    set.clear();
    ConservativelyCollectGarbage();
    EXPECT_FALSE(set.Contains(one));
    EXPECT_FALSE(set.Contains(two));
    EXPECT_FALSE(set.Contains(three));
    EXPECT_FALSE(set.Contains(four));
  }
  {
    CrossThreadPersistentSet set1;
    CrossThreadPersistentSet set2;

    set1.insert(one);
    set2.insert(two);
    set1.swap(set2);
    ConservativelyCollectGarbage();
    EXPECT_TRUE(set1.Contains(two));
    EXPECT_TRUE(set2.Contains(one));
    EXPECT_TRUE(set2.Contains(one2));
  }
}

class NonTrivialObject final : public GarbageCollected<NonTrivialObject> {
 public:
  NonTrivialObject() = default;
  explicit NonTrivialObject(int num) {
    deque_.push_back(MakeGarbageCollected<IntWrapper>(num));
    vector_.push_back(MakeGarbageCollected<IntWrapper>(num));
  }
  void Trace(Visitor* visitor) const {
    visitor->Trace(deque_);
    visitor->Trace(vector_);
  }

 private:
  HeapDeque<Member<IntWrapper>> deque_;
  HeapVector<Member<IntWrapper>> vector_;
};

TEST_F(HeapTest, HeapHashMapWithInlinedObject) {
  HeapHashMap<int, Member<NonTrivialObject>> map;
  for (int num = 1; num < 1000; num++) {
    NonTrivialObject* object = MakeGarbageCollected<NonTrivialObject>(num);
    map.insert(num, object);
  }
}

template <typename T>
void MapIteratorCheck(T& it, const T& end, int expected) {
  int found = 0;
  while (it != end) {
    found++;
    int key = it->key->Value();
    int value = it->value->Value();
    EXPECT_TRUE(key >= 0 && key < 1100);
    EXPECT_TRUE(value >= 0 && value < 1100);
    ++it;
  }
  EXPECT_EQ(expected, found);
}

template <typename T>
void SetIteratorCheck(T& it, const T& end, int expected) {
  int found = 0;
  while (it != end) {
    found++;
    int value = (*it)->Value();
    EXPECT_TRUE(value >= 0 && value < 1100);
    ++it;
  }
  EXPECT_EQ(expected, found);
}

TEST_F(HeapTest, HeapWeakCollectionSimple) {
  ClearOutOldGarbage();
  IntWrapper::destructor_calls_ = 0;

  Persistent<HeapVector<Member<IntWrapper>>> keep_numbers_alive =
      MakeGarbageCollected<HeapVector<Member<IntWrapper>>>();

  typedef HeapHashMap<WeakMember<IntWrapper>, Member<IntWrapper>> WeakStrong;
  typedef HeapHashMap<Member<IntWrapper>, WeakMember<IntWrapper>> StrongWeak;
  typedef HeapHashMap<WeakMember<IntWrapper>, WeakMember<IntWrapper>> WeakWeak;
  typedef HeapHashSet<WeakMember<IntWrapper>> WeakSet;
  typedef HeapHashCountedSet<WeakMember<IntWrapper>> WeakCountedSet;

  Persistent<WeakStrong> weak_strong = MakeGarbageCollected<WeakStrong>();
  Persistent<StrongWeak> strong_weak = MakeGarbageCollected<StrongWeak>();
  Persistent<WeakWeak> weak_weak = MakeGarbageCollected<WeakWeak>();
  Persistent<WeakSet> weak_set = MakeGarbageCollected<WeakSet>();
  Persistent<WeakCountedSet> weak_counted_set =
      MakeGarbageCollected<WeakCountedSet>();

  Persistent<IntWrapper> two = MakeGarbageCollected<IntWrapper>(2);

  keep_numbers_alive->push_back(MakeGarbageCollected<IntWrapper>(103));
  keep_numbers_alive->push_back(MakeGarbageCollected<IntWrapper>(10));

  {
    weak_strong->insert(MakeGarbageCollected<IntWrapper>(1), two);
    strong_weak->insert(two, MakeGarbageCollected<IntWrapper>(1));
    weak_weak->insert(two, MakeGarbageCollected<IntWrapper>(42));
    weak_weak->insert(MakeGarbageCollected<IntWrapper>(42), two);
    weak_set->insert(MakeGarbageCollected<IntWrapper>(0));
    weak_set->insert(two);
    weak_set->insert(keep_numbers_alive->at(0));
    weak_set->insert(keep_numbers_alive->at(1));
    weak_counted_set->insert(MakeGarbageCollected<IntWrapper>(0));
    weak_counted_set->insert(two);
    weak_counted_set->insert(two);
    weak_counted_set->insert(two);
    weak_counted_set->insert(keep_numbers_alive->at(0));
    weak_counted_set->insert(keep_numbers_alive->at(1));
    EXPECT_EQ(1u, weak_strong->size());
    EXPECT_EQ(1u, strong_weak->size());
    EXPECT_EQ(2u, weak_weak->size());
    EXPECT_EQ(4u, weak_set->size());
    EXPECT_EQ(4u, weak_counted_set->size());
    EXPECT_EQ(3u, weak_counted_set->find(two)->value);
    weak_counted_set->erase(two);
    EXPECT_EQ(2u, weak_counted_set->find(two)->value);
  }

  keep_numbers_alive->at(0) = nullptr;

  PreciselyCollectGarbage();

  EXPECT_EQ(0u, weak_strong->size());
  EXPECT_EQ(0u, strong_weak->size());
  EXPECT_EQ(0u, weak_weak->size());
  EXPECT_EQ(2u, weak_set->size());
  EXPECT_EQ(2u, weak_counted_set->size());
}

template <typename Set>
void OrderedSetHelper(bool strong) {
  IntWrapper::destructor_calls_ = 0;

  Persistent<HeapVector<Member<IntWrapper>>> keep_numbers_alive =
      MakeGarbageCollected<HeapVector<Member<IntWrapper>>>();

  Persistent<Set> set1 = MakeGarbageCollected<Set>();
  Persistent<Set> set2 = MakeGarbageCollected<Set>();

  const Set& const_set = *set1.Get();

  keep_numbers_alive->push_back(MakeGarbageCollected<IntWrapper>(2));
  keep_numbers_alive->push_back(MakeGarbageCollected<IntWrapper>(103));
  keep_numbers_alive->push_back(MakeGarbageCollected<IntWrapper>(10));

  set1->insert(MakeGarbageCollected<IntWrapper>(0));
  set1->insert(keep_numbers_alive->at(0));
  set1->insert(keep_numbers_alive->at(1));
  set1->insert(keep_numbers_alive->at(2));

  set2->clear();
  set2->insert(MakeGarbageCollected<IntWrapper>(42));
  set2->clear();

  EXPECT_EQ(4u, set1->size());
  typename Set::iterator it(set1->begin());
  typename Set::reverse_iterator reverse(set1->rbegin());
  typename Set::const_iterator cit(const_set.begin());
  typename Set::const_reverse_iterator creverse(const_set.rbegin());

  EXPECT_EQ(0, (*it)->Value());
  EXPECT_EQ(0, (*cit)->Value());
  ++it;
  ++cit;
  EXPECT_EQ(2, (*it)->Value());
  EXPECT_EQ(2, (*cit)->Value());
  --it;
  --cit;
  EXPECT_EQ(0, (*it)->Value());
  EXPECT_EQ(0, (*cit)->Value());
  ++it;
  ++cit;
  ++it;
  ++cit;
  EXPECT_EQ(103, (*it)->Value());
  EXPECT_EQ(103, (*cit)->Value());
  ++it;
  ++cit;
  EXPECT_EQ(10, (*it)->Value());
  EXPECT_EQ(10, (*cit)->Value());
  ++it;
  ++cit;

  EXPECT_EQ(10, (*reverse)->Value());
  EXPECT_EQ(10, (*creverse)->Value());
  ++reverse;
  ++creverse;
  EXPECT_EQ(103, (*reverse)->Value());
  EXPECT_EQ(103, (*creverse)->Value());
  --reverse;
  --creverse;
  EXPECT_EQ(10, (*reverse)->Value());
  EXPECT_EQ(10, (*creverse)->Value());
  ++reverse;
  ++creverse;
  ++reverse;
  ++creverse;
  EXPECT_EQ(2, (*reverse)->Value());
  EXPECT_EQ(2, (*creverse)->Value());
  ++reverse;
  ++creverse;
  EXPECT_EQ(0, (*reverse)->Value());
  EXPECT_EQ(0, (*creverse)->Value());
  ++reverse;
  ++creverse;

  EXPECT_EQ(set1->end(), it);
  EXPECT_EQ(const_set.end(), cit);
  EXPECT_EQ(set1->rend(), reverse);
  EXPECT_EQ(const_set.rend(), creverse);

  typename Set::iterator i_x(set2->begin());
  EXPECT_EQ(set2->end(), i_x);

  if (strong)
    set1->erase(keep_numbers_alive->at(0));

  keep_numbers_alive->at(0) = nullptr;

  TestSupportingGC::PreciselyCollectGarbage();

  EXPECT_EQ(2u + (strong ? 1u : 0u), set1->size());

  EXPECT_EQ(2 + (strong ? 0 : 1), IntWrapper::destructor_calls_);

  typename Set::iterator i2(set1->begin());
  if (strong) {
    EXPECT_EQ(0, (*i2)->Value());
    ++i2;
    EXPECT_NE(set1->end(), i2);
  }
  EXPECT_EQ(103, (*i2)->Value());
  ++i2;
  EXPECT_NE(set1->end(), i2);
  EXPECT_EQ(10, (*i2)->Value());
  ++i2;
  EXPECT_EQ(set1->end(), i2);
}

TEST_F(HeapTest, HeapWeakLinkedHashSet) {
  ClearOutOldGarbage();
  OrderedSetHelper<HeapListHashSet<Member<IntWrapper>>>(true);
  ClearOutOldGarbage();
  OrderedSetHelper<HeapLinkedHashSet<Member<IntWrapper>>>(true);
  ClearOutOldGarbage();
  OrderedSetHelper<HeapLinkedHashSet<WeakMember<IntWrapper>>>(false);
}

template <typename Set>
class SetOwner final : public GarbageCollected<SetOwner<Set>> {
 public:
  SetOwner() = default;
  bool operator==(const SetOwner& other) const { return false; }

  void Trace(Visitor* visitor) const {
    visitor->RegisterWeakCallbackMethod<SetOwner,
                                        &SetOwner::ProcessCustomWeakness>(this);
    visitor->Trace(set_);
  }

  void ProcessCustomWeakness(const LivenessBroker& info) { set_.clear(); }

  Set set_;
};

template <typename Set>
void ClearInWeakProcessingHelper() {
  Persistent<SetOwner<Set>> set = MakeGarbageCollected<SetOwner<Set>>();
  TestSupportingGC::PreciselyCollectGarbage();
}

TEST_F(HeapTest, ClearInWeakProcessing) {
  ClearOutOldGarbage();
  ClearInWeakProcessingHelper<HeapLinkedHashSet<Member<IntWrapper>>>();
  ClearOutOldGarbage();
  ClearInWeakProcessingHelper<HeapLinkedHashSet<WeakMember<IntWrapper>>>();
}

class ThingWithDestructor {
  DISALLOW_NEW();

 public:
  ThingWithDestructor() : x_(kEmptyValue) { live_things_with_destructor_++; }

  ThingWithDestructor(int x) : x_(x) { live_things_with_destructor_++; }

  ThingWithDestructor(const ThingWithDestructor& other) {
    *this = other;
    live_things_with_destructor_++;
  }

  ~ThingWithDestructor() { live_things_with_destructor_--; }

  int Value() { return x_; }

  static int live_things_with_destructor_;

  unsigned GetHash() { return IntHash<int>::GetHash(x_); }

 private:
  static const int kEmptyValue = 0;
  int x_;
};

int ThingWithDestructor::live_things_with_destructor_;

static void HeapMapDestructorHelper(bool clear_maps) {
  ThingWithDestructor::live_things_with_destructor_ = 0;

  typedef HeapHashMap<WeakMember<IntWrapper>,
                      Member<RefCountedAndGarbageCollected>>
      RefMap;

  typedef HeapHashMap<WeakMember<IntWrapper>, ThingWithDestructor,
                      DefaultHash<WeakMember<IntWrapper>>::Hash,
                      HashTraits<WeakMember<IntWrapper>>>
      Map;

  Persistent<Map> map(MakeGarbageCollected<Map>());
  Persistent<RefMap> ref_map(MakeGarbageCollected<RefMap>());

  Persistent<IntWrapper> luck(MakeGarbageCollected<IntWrapper>(103));

  int base_line, ref_base_line;

  {
    Map stack_map;
    RefMap stack_ref_map;

    TestSupportingGC::PreciselyCollectGarbage();
    TestSupportingGC::PreciselyCollectGarbage();

    stack_map.insert(MakeGarbageCollected<IntWrapper>(42),
                     ThingWithDestructor(1729));
    stack_map.insert(luck, ThingWithDestructor(8128));
    stack_ref_map.insert(MakeGarbageCollected<IntWrapper>(42),
                         MakeGarbageCollected<RefCountedAndGarbageCollected>());
    stack_ref_map.insert(luck,
                         MakeGarbageCollected<RefCountedAndGarbageCollected>());

    base_line = ThingWithDestructor::live_things_with_destructor_;
    ref_base_line = RefCountedAndGarbageCollected::destructor_calls_;

    // Although the heap maps are on-stack, we can't expect prompt
    // finalization of the elements, so when they go out of scope here we
    // will not necessarily have called the relevant destructors.
  }

  // The RefCountedAndGarbageCollected things need an extra GC to discover
  // that they are no longer ref counted.
  TestSupportingGC::PreciselyCollectGarbage();
  TestSupportingGC::PreciselyCollectGarbage();
  EXPECT_EQ(base_line - 2, ThingWithDestructor::live_things_with_destructor_);
  EXPECT_EQ(ref_base_line + 2,
            RefCountedAndGarbageCollected::destructor_calls_);

  // Now use maps kept alive with persistents. Here we don't expect any
  // destructors to be called before there have been GCs.

  map->insert(MakeGarbageCollected<IntWrapper>(42), ThingWithDestructor(1729));
  map->insert(luck, ThingWithDestructor(8128));
  ref_map->insert(MakeGarbageCollected<IntWrapper>(42),
                  MakeGarbageCollected<RefCountedAndGarbageCollected>());
  ref_map->insert(luck, MakeGarbageCollected<RefCountedAndGarbageCollected>());

  base_line = ThingWithDestructor::live_things_with_destructor_;
  ref_base_line = RefCountedAndGarbageCollected::destructor_calls_;

  luck.Clear();
  if (clear_maps) {
    map->clear();      // Clear map.
    ref_map->clear();  // Clear map.
  } else {
    map.Clear();      // Clear Persistent handle, not map.
    ref_map.Clear();  // Clear Persistent handle, not map.
    TestSupportingGC::PreciselyCollectGarbage();
    TestSupportingGC::PreciselyCollectGarbage();
  }

  EXPECT_EQ(base_line - 2, ThingWithDestructor::live_things_with_destructor_);

  // Need a GC to make sure that the RefCountedAndGarbageCollected thing
  // noticies it's been decremented to zero.
  TestSupportingGC::PreciselyCollectGarbage();
  EXPECT_EQ(ref_base_line + 2,
            RefCountedAndGarbageCollected::destructor_calls_);
}

TEST_F(HeapTest, HeapMapDestructor) {
  ClearOutOldGarbage();
  HeapMapDestructorHelper(true);
  ClearOutOldGarbage();
  HeapMapDestructorHelper(false);
}

typedef HeapHashSet<PairWeakStrong> WeakStrongSet;
typedef HeapHashSet<PairWeakUnwrapped> WeakUnwrappedSet;
typedef HeapHashSet<PairStrongWeak> StrongWeakSet;
typedef HeapHashSet<PairUnwrappedWeak> UnwrappedWeakSet;
typedef HeapLinkedHashSet<PairWeakStrong> WeakStrongLinkedSet;
typedef HeapLinkedHashSet<PairWeakUnwrapped> WeakUnwrappedLinkedSet;
typedef HeapLinkedHashSet<PairStrongWeak> StrongWeakLinkedSet;
typedef HeapLinkedHashSet<PairUnwrappedWeak> UnwrappedWeakLinkedSet;
typedef HeapHashCountedSet<PairWeakStrong> WeakStrongCountedSet;
typedef HeapHashCountedSet<PairWeakUnwrapped> WeakUnwrappedCountedSet;
typedef HeapHashCountedSet<PairStrongWeak> StrongWeakCountedSet;
typedef HeapHashCountedSet<PairUnwrappedWeak> UnwrappedWeakCountedSet;

template <typename T>
T& IteratorExtractor(WTF::KeyValuePair<T, unsigned>& pair) {
  return pair.key;
}

template <typename T>
T& IteratorExtractor(T& not_a_pair) {
  return not_a_pair;
}

template <typename WSSet, typename SWSet, typename WUSet, typename UWSet>
void CheckPairSets(Persistent<WSSet>& weak_strong,
                   Persistent<SWSet>& strong_weak,
                   Persistent<WUSet>& weak_unwrapped,
                   Persistent<UWSet>& unwrapped_weak,
                   bool ones,
                   Persistent<IntWrapper>& two) {
  typename WSSet::iterator it_ws = weak_strong->begin();
  typename SWSet::iterator it_sw = strong_weak->begin();
  typename WUSet::iterator it_wu = weak_unwrapped->begin();
  typename UWSet::iterator it_uw = unwrapped_weak->begin();

  EXPECT_EQ(2u, weak_strong->size());
  EXPECT_EQ(2u, strong_weak->size());
  EXPECT_EQ(2u, weak_unwrapped->size());
  EXPECT_EQ(2u, unwrapped_weak->size());

  PairWeakStrong p = IteratorExtractor(*it_ws);
  PairStrongWeak p2 = IteratorExtractor(*it_sw);
  PairWeakUnwrapped p3 = IteratorExtractor(*it_wu);
  PairUnwrappedWeak p4 = IteratorExtractor(*it_uw);
  if (p.first == two && p.second == two)
    ++it_ws;
  if (p2.first == two && p2.second == two)
    ++it_sw;
  if (p3.first == two && p3.second == 2)
    ++it_wu;
  if (p4.first == 2 && p4.second == two)
    ++it_uw;
  p = IteratorExtractor(*it_ws);
  p2 = IteratorExtractor(*it_sw);
  p3 = IteratorExtractor(*it_wu);
  p4 = IteratorExtractor(*it_uw);
  IntWrapper* null_wrapper = nullptr;
  if (ones) {
    EXPECT_EQ(p.first->Value(), 1);
    EXPECT_EQ(p2.second->Value(), 1);
    EXPECT_EQ(p3.first->Value(), 1);
    EXPECT_EQ(p4.second->Value(), 1);
  } else {
    EXPECT_EQ(p.first, null_wrapper);
    EXPECT_EQ(p2.second, null_wrapper);
    EXPECT_EQ(p3.first, null_wrapper);
    EXPECT_EQ(p4.second, null_wrapper);
  }

  EXPECT_EQ(p.second->Value(), 2);
  EXPECT_EQ(p2.first->Value(), 2);
  EXPECT_EQ(p3.second, 2);
  EXPECT_EQ(p4.first, 2);

  EXPECT_TRUE(weak_strong->Contains(PairWeakStrong(&*two, &*two)));
  EXPECT_TRUE(strong_weak->Contains(PairStrongWeak(&*two, &*two)));
  EXPECT_TRUE(weak_unwrapped->Contains(PairWeakUnwrapped(&*two, 2)));
  EXPECT_TRUE(unwrapped_weak->Contains(PairUnwrappedWeak(2, &*two)));
}

TEST_F(HeapTest, HeapWeakCollectionTypes) {
  IntWrapper::destructor_calls_ = 0;

  typedef HeapHashMap<WeakMember<IntWrapper>, Member<IntWrapper>> WeakStrong;
  typedef HeapHashMap<Member<IntWrapper>, WeakMember<IntWrapper>> StrongWeak;
  typedef HeapHashMap<WeakMember<IntWrapper>, WeakMember<IntWrapper>> WeakWeak;
  typedef HeapHashSet<WeakMember<IntWrapper>> WeakSet;
  typedef HeapLinkedHashSet<WeakMember<IntWrapper>> WeakOrderedSet;

  ClearOutOldGarbage();

  const int kWeakStrongIndex = 0;
  const int kStrongWeakIndex = 1;
  const int kWeakWeakIndex = 2;
  const int kNumberOfMapIndices = 3;
  const int kWeakSetIndex = 3;
  const int kWeakOrderedSetIndex = 4;
  const int kNumberOfCollections = 5;

  for (int test_run = 0; test_run < 4; test_run++) {
    for (int collection_number = 0; collection_number < kNumberOfCollections;
         collection_number++) {
      bool delete_afterwards = (test_run == 1);
      bool add_afterwards = (test_run == 2);
      bool test_that_iterators_make_strong = (test_run == 3);

      // The test doesn't work for strongWeak with deleting because we lost
      // the key from the keepNumbersAlive array, so we can't do the lookup.
      if (delete_afterwards && collection_number == kStrongWeakIndex)
        continue;

      unsigned added = add_afterwards ? 100 : 0;

      Persistent<WeakStrong> weak_strong = MakeGarbageCollected<WeakStrong>();
      Persistent<StrongWeak> strong_weak = MakeGarbageCollected<StrongWeak>();
      Persistent<WeakWeak> weak_weak = MakeGarbageCollected<WeakWeak>();

      Persistent<WeakSet> weak_set = MakeGarbageCollected<WeakSet>();
      Persistent<WeakOrderedSet> weak_ordered_set =
          MakeGarbageCollected<WeakOrderedSet>();

      Persistent<HeapVector<Member<IntWrapper>>> keep_numbers_alive =
          MakeGarbageCollected<HeapVector<Member<IntWrapper>>>();
      for (int i = 0; i < 128; i += 2) {
        auto* wrapped = MakeGarbageCollected<IntWrapper>(i);
        auto* wrapped2 = MakeGarbageCollected<IntWrapper>(i + 1);
        keep_numbers_alive->push_back(wrapped);
        keep_numbers_alive->push_back(wrapped2);
        weak_strong->insert(wrapped, wrapped2);
        strong_weak->insert(wrapped2, wrapped);
        weak_weak->insert(wrapped, wrapped2);
        weak_set->insert(wrapped);
        weak_ordered_set->insert(wrapped);
      }

      EXPECT_EQ(64u, weak_strong->size());
      EXPECT_EQ(64u, strong_weak->size());
      EXPECT_EQ(64u, weak_weak->size());
      EXPECT_EQ(64u, weak_set->size());
      EXPECT_EQ(64u, weak_ordered_set->size());

      // Collect garbage. This should change nothing since we are keeping
      // alive the IntWrapper objects.
      PreciselyCollectGarbage();

      EXPECT_EQ(64u, weak_strong->size());
      EXPECT_EQ(64u, strong_weak->size());
      EXPECT_EQ(64u, weak_weak->size());
      EXPECT_EQ(64u, weak_set->size());
      EXPECT_EQ(64u, weak_ordered_set->size());

      for (int i = 0; i < 128; i += 2) {
        IntWrapper* wrapped = keep_numbers_alive->at(i);
        IntWrapper* wrapped2 = keep_numbers_alive->at(i + 1);
        EXPECT_EQ(wrapped2, weak_strong->at(wrapped));
        EXPECT_EQ(wrapped, strong_weak->at(wrapped2));
        EXPECT_EQ(wrapped2, weak_weak->at(wrapped));
        EXPECT_TRUE(weak_set->Contains(wrapped));
        EXPECT_TRUE(weak_ordered_set->Contains(wrapped));
      }

      for (int i = 0; i < 128; i += 3)
        keep_numbers_alive->at(i) = nullptr;

      if (collection_number != kWeakStrongIndex)
        weak_strong->clear();
      if (collection_number != kStrongWeakIndex)
        strong_weak->clear();
      if (collection_number != kWeakWeakIndex)
        weak_weak->clear();
      if (collection_number != kWeakSetIndex)
        weak_set->clear();
      if (collection_number != kWeakOrderedSetIndex)
        weak_ordered_set->clear();

      if (test_that_iterators_make_strong) {
        WeakStrong::iterator it1 = weak_strong->begin();
        StrongWeak::iterator it2 = strong_weak->begin();
        WeakWeak::iterator it3 = weak_weak->begin();
        WeakSet::iterator it4 = weak_set->begin();
        WeakOrderedSet::iterator it5 = weak_ordered_set->begin();
        // Collect garbage. This should change nothing since the
        // iterators make the collections strong.
        ConservativelyCollectGarbage();
        if (collection_number == kWeakStrongIndex) {
          EXPECT_EQ(64u, weak_strong->size());
          MapIteratorCheck(it1, weak_strong->end(), 64);
        } else if (collection_number == kStrongWeakIndex) {
          EXPECT_EQ(64u, strong_weak->size());
          MapIteratorCheck(it2, strong_weak->end(), 64);
        } else if (collection_number == kWeakWeakIndex) {
          EXPECT_EQ(64u, weak_weak->size());
          MapIteratorCheck(it3, weak_weak->end(), 64);
        } else if (collection_number == kWeakSetIndex) {
          EXPECT_EQ(64u, weak_set->size());
          SetIteratorCheck(it4, weak_set->end(), 64);
        } else if (collection_number == kWeakOrderedSetIndex) {
          EXPECT_EQ(64u, weak_ordered_set->size());
          SetIteratorCheck(it5, weak_ordered_set->end(), 64);
        }
      } else {
        // Collect garbage. This causes weak processing to remove
        // things from the collections.
        PreciselyCollectGarbage();
        unsigned count = 0;
        for (int i = 0; i < 128; i += 2) {
          bool first_alive = keep_numbers_alive->at(i);
          bool second_alive = keep_numbers_alive->at(i + 1);
          if (first_alive && (collection_number == kWeakStrongIndex ||
                              collection_number == kStrongWeakIndex))
            second_alive = true;
          if (first_alive && second_alive &&
              collection_number < kNumberOfMapIndices) {
            if (collection_number == kWeakStrongIndex) {
              if (delete_afterwards) {
                EXPECT_EQ(
                    i + 1,
                    weak_strong->Take(keep_numbers_alive->at(i))->Value());
              }
            } else if (collection_number == kStrongWeakIndex) {
              if (delete_afterwards) {
                EXPECT_EQ(
                    i,
                    strong_weak->Take(keep_numbers_alive->at(i + 1))->Value());
              }
            } else if (collection_number == kWeakWeakIndex) {
              if (delete_afterwards) {
                EXPECT_EQ(i + 1,
                          weak_weak->Take(keep_numbers_alive->at(i))->Value());
              }
            }
            if (!delete_afterwards)
              count++;
          } else if (collection_number == kWeakSetIndex && first_alive) {
            ASSERT_TRUE(weak_set->Contains(keep_numbers_alive->at(i)));
            if (delete_afterwards)
              weak_set->erase(keep_numbers_alive->at(i));
            else
              count++;
          } else if (collection_number == kWeakOrderedSetIndex && first_alive) {
            ASSERT_TRUE(weak_ordered_set->Contains(keep_numbers_alive->at(i)));
            if (delete_afterwards)
              weak_ordered_set->erase(keep_numbers_alive->at(i));
            else
              count++;
          }
        }
        if (add_afterwards) {
          for (int i = 1000; i < 1100; i++) {
            auto* wrapped = MakeGarbageCollected<IntWrapper>(i);
            keep_numbers_alive->push_back(wrapped);
            weak_strong->insert(wrapped, wrapped);
            strong_weak->insert(wrapped, wrapped);
            weak_weak->insert(wrapped, wrapped);
            weak_set->insert(wrapped);
            weak_ordered_set->insert(wrapped);
          }
        }
        if (collection_number == kWeakStrongIndex)
          EXPECT_EQ(count + added, weak_strong->size());
        else if (collection_number == kStrongWeakIndex)
          EXPECT_EQ(count + added, strong_weak->size());
        else if (collection_number == kWeakWeakIndex)
          EXPECT_EQ(count + added, weak_weak->size());
        else if (collection_number == kWeakSetIndex)
          EXPECT_EQ(count + added, weak_set->size());
        else if (collection_number == kWeakOrderedSetIndex)
          EXPECT_EQ(count + added, weak_ordered_set->size());
        WeakStrong::iterator it1 = weak_strong->begin();
        StrongWeak::iterator it2 = strong_weak->begin();
        WeakWeak::iterator it3 = weak_weak->begin();
        WeakSet::iterator it4 = weak_set->begin();
        WeakOrderedSet::iterator it5 = weak_ordered_set->begin();
        MapIteratorCheck(
            it1, weak_strong->end(),
            (collection_number == kWeakStrongIndex ? count : 0) + added);
        MapIteratorCheck(
            it2, strong_weak->end(),
            (collection_number == kStrongWeakIndex ? count : 0) + added);
        MapIteratorCheck(
            it3, weak_weak->end(),
            (collection_number == kWeakWeakIndex ? count : 0) + added);
        SetIteratorCheck(
            it4, weak_set->end(),
            (collection_number == kWeakSetIndex ? count : 0) + added);
        SetIteratorCheck(
            it5, weak_ordered_set->end(),
            (collection_number == kWeakOrderedSetIndex ? count : 0) + added);
      }
      for (unsigned i = 0; i < 128 + added; i++)
        keep_numbers_alive->at(i) = nullptr;
      PreciselyCollectGarbage();
      EXPECT_EQ(0u, weak_strong->size());
      EXPECT_EQ(0u, strong_weak->size());
      EXPECT_EQ(0u, weak_weak->size());
      EXPECT_EQ(0u, weak_set->size());
      EXPECT_EQ(0u, weak_ordered_set->size());
    }
  }
}

TEST_F(HeapTest, HeapHashCountedSetToVector) {
  HeapHashCountedSet<Member<IntWrapper>> set;
  HeapVector<Member<IntWrapper>> vector;
  set.insert(MakeGarbageCollected<IntWrapper>(1));
  set.insert(MakeGarbageCollected<IntWrapper>(1));
  set.insert(MakeGarbageCollected<IntWrapper>(2));

  CopyToVector(set, vector);
  EXPECT_EQ(3u, vector.size());

  Vector<int> int_vector;
  for (const auto& i : vector)
    int_vector.push_back(i->Value());
  std::sort(int_vector.begin(), int_vector.end());
  ASSERT_EQ(3u, int_vector.size());
  EXPECT_EQ(1, int_vector[0]);
  EXPECT_EQ(1, int_vector[1]);
  EXPECT_EQ(2, int_vector[2]);
}

TEST_F(HeapTest, WeakHeapHashCountedSetToVector) {
  HeapHashCountedSet<WeakMember<IntWrapper>> set;
  HeapVector<Member<IntWrapper>> vector;
  set.insert(MakeGarbageCollected<IntWrapper>(1));
  set.insert(MakeGarbageCollected<IntWrapper>(1));
  set.insert(MakeGarbageCollected<IntWrapper>(2));

  CopyToVector(set, vector);
  EXPECT_LE(3u, vector.size());
  for (const auto& i : vector)
    EXPECT_TRUE(i->Value() == 1 || i->Value() == 2);
}

TEST_F(HeapTest, RefCountedGarbageCollected) {
  RefCountedAndGarbageCollected::destructor_calls_ = 0;
  {
    scoped_refptr<RefCountedAndGarbageCollected> ref_ptr3;
    {
      Persistent<RefCountedAndGarbageCollected> persistent;
      {
        Persistent<RefCountedAndGarbageCollected> ref_ptr1 =
            MakeGarbageCollected<RefCountedAndGarbageCollected>();
        Persistent<RefCountedAndGarbageCollected> ref_ptr2 =
            MakeGarbageCollected<RefCountedAndGarbageCollected>();
        PreciselyCollectGarbage();
        EXPECT_EQ(0, RefCountedAndGarbageCollected::destructor_calls_);
        persistent = ref_ptr1.Get();
      }
      // Reference count is zero for both objects but one of
      // them is kept alive by a persistent handle.
      PreciselyCollectGarbage();
      EXPECT_EQ(1, RefCountedAndGarbageCollected::destructor_calls_);
      ref_ptr3 = persistent.Get();
    }
    // The persistent handle is gone but the ref count has been
    // increased to 1.
    PreciselyCollectGarbage();
    EXPECT_EQ(1, RefCountedAndGarbageCollected::destructor_calls_);
  }
  // Both persistent handle is gone and ref count is zero so the
  // object can be collected.
  PreciselyCollectGarbage();
  EXPECT_EQ(2, RefCountedAndGarbageCollected::destructor_calls_);
}

TEST_F(HeapTest, WeakMembers) {
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

TEST_F(HeapTest, FinalizationObserver) {
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

TEST_F(HeapTest, PreFinalizer) {
  Observable::will_finalize_was_called_ = false;
  { MakeGarbageCollected<Observable>(MakeGarbageCollected<Bar>()); }
  PreciselyCollectGarbage();
  EXPECT_TRUE(Observable::will_finalize_was_called_);
}

TEST_F(HeapTest, PreFinalizerUnregistersItself) {
  ObservableWithPreFinalizer::dispose_was_called_ = false;
  MakeGarbageCollected<ObservableWithPreFinalizer>();
  PreciselyCollectGarbage();
  EXPECT_TRUE(ObservableWithPreFinalizer::dispose_was_called_);
  // Don't crash, and assertions don't fail.
}

TEST_F(HeapTest, NestedPreFinalizer) {
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

TEST_F(HeapTest, Comparisons) {
  Persistent<Bar> bar_persistent = MakeGarbageCollected<Bar>();
  Persistent<Foo> foo_persistent = MakeGarbageCollected<Foo>(bar_persistent);
  EXPECT_TRUE(bar_persistent != foo_persistent);
  bar_persistent = foo_persistent;
  EXPECT_TRUE(bar_persistent == foo_persistent);
}

namespace {

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

TEST_F(HeapTest, CheckAndMarkPointer) {
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

TEST_F(HeapTest, CollectionNesting) {
  ClearOutOldGarbage();
  int k;
  int* key = &k;
  IntWrapper::destructor_calls_ = 0;
  typedef HeapVector<Member<IntWrapper>> IntVector;
  typedef HeapDeque<Member<IntWrapper>> IntDeque;
  HeapHashMap<void*, Member<IntVector>>* map =
      MakeGarbageCollected<HeapHashMap<void*, Member<IntVector>>>();
  HeapHashMap<void*, Member<IntDeque>>* map2 =
      MakeGarbageCollected<HeapHashMap<void*, Member<IntDeque>>>();
  static_assert(WTF::IsTraceable<IntVector>::value,
                "Failed to recognize HeapVector as traceable");
  static_assert(WTF::IsTraceable<IntDeque>::value,
                "Failed to recognize HeapDeque as traceable");

  map->insert(key, MakeGarbageCollected<IntVector>());
  map2->insert(key, MakeGarbageCollected<IntDeque>());

  HeapHashMap<void*, Member<IntVector>>::iterator it = map->find(key);
  EXPECT_EQ(0u, map->at(key)->size());

  HeapHashMap<void*, Member<IntDeque>>::iterator it2 = map2->find(key);
  EXPECT_EQ(0u, map2->at(key)->size());

  it->value->push_back(MakeGarbageCollected<IntWrapper>(42));
  EXPECT_EQ(1u, map->at(key)->size());

  it2->value->push_back(MakeGarbageCollected<IntWrapper>(42));
  EXPECT_EQ(1u, map2->at(key)->size());

  Persistent<HeapHashMap<void*, Member<IntVector>>> keep_alive(map);
  Persistent<HeapHashMap<void*, Member<IntDeque>>> keep_alive2(map2);

  for (int i = 0; i < 100; i++) {
    map->insert(key + 1 + i, MakeGarbageCollected<IntVector>());
    map2->insert(key + 1 + i, MakeGarbageCollected<IntDeque>());
  }

  PreciselyCollectGarbage();

  EXPECT_EQ(1u, map->at(key)->size());
  EXPECT_EQ(1u, map2->at(key)->size());
  EXPECT_EQ(0, IntWrapper::destructor_calls_);

  keep_alive = nullptr;
  PreciselyCollectGarbage();
  EXPECT_EQ(1, IntWrapper::destructor_calls_);
}

namespace {
class DisableHeapVerificationScope {
 public:
  DisableHeapVerificationScope(const char*) {
    ThreadState::Current()->EnterNoHeapVerificationScopeForTesting();
  }
  ~DisableHeapVerificationScope() {
    ThreadState::Current()->LeaveNoHeapVerificationScopeForTesting();
  }
};
}  // namespace

TEST_F(HeapTest, GarbageCollectedMixin) {
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

TEST_F(HeapTest, CollectionNesting2) {
  ClearOutOldGarbage();
  void* key = &IntWrapper::destructor_calls_;
  IntWrapper::destructor_calls_ = 0;
  typedef HeapHashSet<Member<IntWrapper>> IntSet;
  HeapHashMap<void*, Member<IntSet>>* map =
      MakeGarbageCollected<HeapHashMap<void*, Member<IntSet>>>();

  map->insert(key, MakeGarbageCollected<IntSet>());

  HeapHashMap<void*, Member<IntSet>>::iterator it = map->find(key);
  EXPECT_EQ(0u, map->at(key)->size());

  it->value->insert(MakeGarbageCollected<IntWrapper>(42));
  EXPECT_EQ(1u, map->at(key)->size());

  Persistent<HeapHashMap<void*, Member<IntSet>>> keep_alive(map);
  PreciselyCollectGarbage();
  EXPECT_EQ(1u, map->at(key)->size());
  EXPECT_EQ(0, IntWrapper::destructor_calls_);
}

TEST_F(HeapTest, CollectionNesting3) {
  ClearOutOldGarbage();
  IntWrapper::destructor_calls_ = 0;
  typedef HeapVector<Member<IntWrapper>> IntVector;
  HeapVector<IntVector>* vector = MakeGarbageCollected<HeapVector<IntVector>>();

  vector->push_back(IntVector());

  HeapVector<IntVector>::iterator it = vector->begin();
  EXPECT_EQ(0u, it->size());

  it->push_back(MakeGarbageCollected<IntWrapper>(42));
  EXPECT_EQ(1u, it->size());

  Persistent<HeapVector<IntVector>> keep_alive(vector);
  PreciselyCollectGarbage();
  EXPECT_EQ(1u, it->size());
  EXPECT_EQ(0, IntWrapper::destructor_calls_);
}

TEST_F(HeapTest, EmbeddedInVector) {
  ClearOutOldGarbage();
  SimpleFinalizedObject::destructor_calls_ = 0;
  {
    Persistent<HeapVector<VectorObject, 2>> inline_vector =
        MakeGarbageCollected<HeapVector<VectorObject, 2>>();
    Persistent<HeapVector<VectorObject>> outline_vector =
        MakeGarbageCollected<HeapVector<VectorObject>>();
    VectorObject i1, i2;
    inline_vector->push_back(i1);
    inline_vector->push_back(i2);

    VectorObject o1, o2;
    outline_vector->push_back(o1);
    outline_vector->push_back(o2);

    Persistent<HeapVector<VectorObjectInheritedTrace>> vector_inherited_trace =
        MakeGarbageCollected<HeapVector<VectorObjectInheritedTrace>>();
    VectorObjectInheritedTrace it1, it2;
    vector_inherited_trace->push_back(it1);
    vector_inherited_trace->push_back(it2);

    PreciselyCollectGarbage();
    EXPECT_EQ(0, SimpleFinalizedObject::destructor_calls_);
  }
  PreciselyCollectGarbage();
  EXPECT_EQ(6, SimpleFinalizedObject::destructor_calls_);
}

class InlinedVectorObject {
  DISALLOW_NEW();

 public:
  InlinedVectorObject() = default;
  ~InlinedVectorObject() { destructor_calls_++; }
  void Trace(Visitor* visitor) const {}

  static int destructor_calls_;
};

int InlinedVectorObject::destructor_calls_ = 0;

class InlinedVectorObjectWithVtable {
  DISALLOW_NEW();

 public:
  InlinedVectorObjectWithVtable() = default;
  virtual ~InlinedVectorObjectWithVtable() { destructor_calls_++; }
  virtual void VirtualMethod() {}
  void Trace(Visitor* visitor) const {}

  static int destructor_calls_;
};

int InlinedVectorObjectWithVtable::destructor_calls_ = 0;

}  // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::InlinedVectorObject)

namespace blink {

class InlinedVectorObjectWrapper final
    : public GarbageCollected<InlinedVectorObjectWrapper> {
 public:
  InlinedVectorObjectWrapper() {
    InlinedVectorObject i1, i2;
    vector1_.push_back(i1);
    vector1_.push_back(i2);
    vector2_.push_back(i1);
    vector2_.push_back(i2);  // This allocates an out-of-line buffer.
    vector3_.push_back(i1);
    vector3_.push_back(i2);
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(vector1_);
    visitor->Trace(vector2_);
    visitor->Trace(vector3_);
  }

 private:
  HeapVector<InlinedVectorObject> vector1_;
  HeapVector<InlinedVectorObject, 1> vector2_;
  HeapVector<InlinedVectorObject, 2> vector3_;
};

class InlinedVectorObjectWithVtableWrapper final
    : public GarbageCollected<InlinedVectorObjectWithVtableWrapper> {
 public:
  InlinedVectorObjectWithVtableWrapper() {
    InlinedVectorObjectWithVtable i1, i2;
    vector1_.push_back(i1);
    vector1_.push_back(i2);
    vector2_.push_back(i1);
    vector2_.push_back(i2);  // This allocates an out-of-line buffer.
    vector3_.push_back(i1);
    vector3_.push_back(i2);
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(vector1_);
    visitor->Trace(vector2_);
    visitor->Trace(vector3_);
  }

 private:
  HeapVector<InlinedVectorObjectWithVtable> vector1_;
  HeapVector<InlinedVectorObjectWithVtable, 1> vector2_;
  HeapVector<InlinedVectorObjectWithVtable, 2> vector3_;
};

TEST_F(HeapTest, VectorDestructors) {
  ClearOutOldGarbage();
  InlinedVectorObject::destructor_calls_ = 0;
  {
    HeapVector<InlinedVectorObject> vector;
    InlinedVectorObject i1, i2;
    vector.push_back(i1);
    vector.push_back(i2);
  }
  PreciselyCollectGarbage();
  // This is not EXPECT_EQ but EXPECT_LE because a HeapVectorBacking calls
  // destructors for all elements in (not the size but) the capacity of
  // the vector. Thus the number of destructors called becomes larger
  // than the actual number of objects in the vector.
  EXPECT_LE(4, InlinedVectorObject::destructor_calls_);

  InlinedVectorObject::destructor_calls_ = 0;
  {
    HeapVector<InlinedVectorObject, 1> vector;
    InlinedVectorObject i1, i2;
    vector.push_back(i1);
    vector.push_back(i2);  // This allocates an out-of-line buffer.
  }
  PreciselyCollectGarbage();
  EXPECT_LE(4, InlinedVectorObject::destructor_calls_);

  InlinedVectorObject::destructor_calls_ = 0;
  {
    HeapVector<InlinedVectorObject, 2> vector;
    InlinedVectorObject i1, i2;
    vector.push_back(i1);
    vector.push_back(i2);
  }
  PreciselyCollectGarbage();
  EXPECT_LE(4, InlinedVectorObject::destructor_calls_);

  InlinedVectorObject::destructor_calls_ = 0;
  {
    Persistent<InlinedVectorObjectWrapper> vector_wrapper =
        MakeGarbageCollected<InlinedVectorObjectWrapper>();
    ConservativelyCollectGarbage();
    EXPECT_EQ(2, InlinedVectorObject::destructor_calls_);
  }
  PreciselyCollectGarbage();
  EXPECT_LE(8, InlinedVectorObject::destructor_calls_);
}

// TODO(Oilpan): when Vector.h's contiguous container support no longer disables
// Vector<>s with inline capacity, enable this test.
#if !defined(ANNOTATE_CONTIGUOUS_CONTAINER)
TEST_F(HeapTest, VectorDestructorsWithVtable) {
  ClearOutOldGarbage();
  InlinedVectorObjectWithVtable::destructor_calls_ = 0;
  {
    HeapVector<InlinedVectorObjectWithVtable> vector;
    InlinedVectorObjectWithVtable i1, i2;
    vector.push_back(i1);
    vector.push_back(i2);
  }
  PreciselyCollectGarbage();
  EXPECT_EQ(4, InlinedVectorObjectWithVtable::destructor_calls_);

  InlinedVectorObjectWithVtable::destructor_calls_ = 0;
  {
    HeapVector<InlinedVectorObjectWithVtable, 1> vector;
    InlinedVectorObjectWithVtable i1, i2;
    vector.push_back(i1);
    vector.push_back(i2);  // This allocates an out-of-line buffer.
  }
  PreciselyCollectGarbage();
  EXPECT_EQ(5, InlinedVectorObjectWithVtable::destructor_calls_);

  InlinedVectorObjectWithVtable::destructor_calls_ = 0;
  {
    HeapVector<InlinedVectorObjectWithVtable, 2> vector;
    InlinedVectorObjectWithVtable i1, i2;
    vector.push_back(i1);
    vector.push_back(i2);
  }
  PreciselyCollectGarbage();
  EXPECT_EQ(4, InlinedVectorObjectWithVtable::destructor_calls_);

  InlinedVectorObjectWithVtable::destructor_calls_ = 0;
  {
    Persistent<InlinedVectorObjectWithVtableWrapper> vector_wrapper =
        MakeGarbageCollected<InlinedVectorObjectWithVtableWrapper>();
    ConservativelyCollectGarbage();
    EXPECT_EQ(3, InlinedVectorObjectWithVtable::destructor_calls_);
  }
  PreciselyCollectGarbage();
  EXPECT_EQ(9, InlinedVectorObjectWithVtable::destructor_calls_);
}
#endif

TEST_F(HeapTest, AllocationDuringFinalization) {
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

class SimpleClassWithDestructor {
 public:
  SimpleClassWithDestructor() = default;
  ~SimpleClassWithDestructor() { was_destructed_ = true; }
  static bool was_destructed_;
};

bool SimpleClassWithDestructor::was_destructed_;

class RefCountedWithDestructor : public RefCounted<RefCountedWithDestructor> {
 public:
  RefCountedWithDestructor() = default;
  ~RefCountedWithDestructor() { was_destructed_ = true; }
  static bool was_destructed_;
};

bool RefCountedWithDestructor::was_destructed_;

template <typename Set>
void DestructorsCalledOnGC(bool add_lots) {
  RefCountedWithDestructor::was_destructed_ = false;
  {
    Set set;
    RefCountedWithDestructor* has_destructor = new RefCountedWithDestructor();
    set.Add(base::AdoptRef(has_destructor));
    EXPECT_FALSE(RefCountedWithDestructor::was_destructed_);

    if (add_lots) {
      for (int i = 0; i < 1000; i++) {
        set.Add(base::AdoptRef(new RefCountedWithDestructor()));
      }
    }

    EXPECT_FALSE(RefCountedWithDestructor::was_destructed_);
    TestSupportingGC::ConservativelyCollectGarbage();
    EXPECT_FALSE(RefCountedWithDestructor::was_destructed_);
  }
  // The destructors of the sets don't call the destructors of the elements
  // in the heap sets. You have to actually remove the elments, call clear()
  // or have a GC to get the destructors called.
  EXPECT_FALSE(RefCountedWithDestructor::was_destructed_);
  TestSupportingGC::PreciselyCollectGarbage();
  EXPECT_TRUE(RefCountedWithDestructor::was_destructed_);
}

template <typename Set>
void DestructorsCalledOnClear(bool add_lots) {
  RefCountedWithDestructor::was_destructed_ = false;
  Set set;
  RefCountedWithDestructor* has_destructor = new RefCountedWithDestructor();
  set.Add(base::AdoptRef(has_destructor));
  EXPECT_FALSE(RefCountedWithDestructor::was_destructed_);

  if (add_lots) {
    for (int i = 0; i < 1000; i++) {
      set.Add(base::AdoptRef(new RefCountedWithDestructor()));
    }
  }

  EXPECT_FALSE(RefCountedWithDestructor::was_destructed_);
  set.Clear();
  EXPECT_TRUE(RefCountedWithDestructor::was_destructed_);
}

TEST_F(HeapTest, DestructorsCalled) {
  HeapHashMap<Member<IntWrapper>, std::unique_ptr<SimpleClassWithDestructor>>
      map;
  SimpleClassWithDestructor* has_destructor = new SimpleClassWithDestructor();
  map.insert(MakeGarbageCollected<IntWrapper>(1),
             base::WrapUnique(has_destructor));
  SimpleClassWithDestructor::was_destructed_ = false;
  map.clear();
  EXPECT_TRUE(SimpleClassWithDestructor::was_destructed_);
}

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

static const bool kIsMixinTrue = IsGarbageCollectedMixin<MultipleMixins>::value;
static const bool kIsMixinFalse = IsGarbageCollectedMixin<IntWrapper>::value;

TEST_F(HeapTest, MultipleMixins) {
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

TEST_F(HeapTest, DerivedMultipleMixins) {
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

class MixinInstanceWithoutTrace
    : public GarbageCollected<MixinInstanceWithoutTrace>,
      public MixinA {
 public:
  MixinInstanceWithoutTrace() = default;
};

TEST_F(HeapTest, MixinInstanceWithoutTrace) {
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

TEST_F(HeapTest, NeedsAdjustPointer) {
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

static void AddElementsToWeakMap(
    HeapHashMap<int, WeakMember<IntWrapper>>* map) {
  // Key cannot be zero in hashmap.
  for (int i = 1; i < 11; i++)
    map->insert(i, MakeGarbageCollected<IntWrapper>(i));
}

// crbug.com/402426
// If it doesn't assert a concurrent modification to the map, then it's passing.
TEST_F(HeapTest, RegressNullIsStrongified) {
  Persistent<HeapHashMap<int, WeakMember<IntWrapper>>> map =
      MakeGarbageCollected<HeapHashMap<int, WeakMember<IntWrapper>>>();
  AddElementsToWeakMap(map);
  HeapHashMap<int, WeakMember<IntWrapper>>::AddResult result =
      map->insert(800, nullptr);
  ConservativelyCollectGarbage();
  result.stored_value->value = MakeGarbageCollected<IntWrapper>(42);
}

TEST_F(HeapTest, Bind) {
  base::OnceClosure closure =
      WTF::Bind(static_cast<void (Bar::*)(Visitor*) const>(&Bar::Trace),
                WrapPersistent(MakeGarbageCollected<Bar>()), nullptr);
  // OffHeapInt* should not make Persistent.
  base::OnceClosure closure2 =
      WTF::Bind(&OffHeapInt::VoidFunction, OffHeapInt::Create(1));
  PreciselyCollectGarbage();
  // The closure should have a persistent handle to the Bar.
  EXPECT_EQ(1u, Bar::live_);

  UseMixin::trace_count_ = 0;
  auto* mixin = MakeGarbageCollected<UseMixin>();
  base::OnceClosure mixin_closure =
      WTF::Bind(static_cast<void (Mixin::*)(Visitor*) const>(&Mixin::Trace),
                WrapPersistent(mixin), nullptr);
  {
    DisableHeapVerificationScope scope(
        "Avoid tracing UseMixin during verification");
    PreciselyCollectGarbage();
  }
  // The closure should have a persistent handle to the mixin.
  EXPECT_EQ(1, UseMixin::trace_count_);
}

typedef HeapHashSet<WeakMember<IntWrapper>> WeakSet;

TEST_F(HeapTest, EphemeronsInEphemerons) {
  typedef HeapHashMap<WeakMember<IntWrapper>, Member<IntWrapper>> InnerMap;
  typedef HeapHashMap<WeakMember<IntWrapper>, Member<InnerMap>> OuterMap;

  for (int keep_outer_alive = 0; keep_outer_alive <= 1; keep_outer_alive++) {
    for (int keep_inner_alive = 0; keep_inner_alive <= 1; keep_inner_alive++) {
      Persistent<OuterMap> outer = MakeGarbageCollected<OuterMap>();
      Persistent<IntWrapper> one = MakeGarbageCollected<IntWrapper>(1);
      Persistent<IntWrapper> two = MakeGarbageCollected<IntWrapper>(2);
      outer->insert(one, MakeGarbageCollected<InnerMap>());
      outer->begin()->value->insert(two, MakeGarbageCollected<IntWrapper>(3));
      EXPECT_EQ(1u, outer->at(one)->size());
      if (!keep_outer_alive)
        one.Clear();
      if (!keep_inner_alive)
        two.Clear();
      PreciselyCollectGarbage();
      if (keep_outer_alive) {
        const InnerMap* inner = outer->at(one);
        if (keep_inner_alive) {
          EXPECT_EQ(1u, inner->size());
          IntWrapper* three = inner->at(two);
          EXPECT_EQ(3, three->Value());
        } else {
          EXPECT_EQ(0u, inner->size());
        }
      } else {
        EXPECT_EQ(0u, outer->size());
      }
      outer->clear();
      Persistent<IntWrapper> deep = MakeGarbageCollected<IntWrapper>(42);
      Persistent<IntWrapper> home = MakeGarbageCollected<IntWrapper>(103);
      Persistent<IntWrapper> composite = MakeGarbageCollected<IntWrapper>(91);
      Persistent<HeapVector<Member<IntWrapper>>> keep_alive =
          MakeGarbageCollected<HeapVector<Member<IntWrapper>>>();
      for (int i = 0; i < 10000; i++) {
        auto* value = MakeGarbageCollected<IntWrapper>(i);
        keep_alive->push_back(value);
        OuterMap::AddResult new_entry =
            outer->insert(value, MakeGarbageCollected<InnerMap>());
        new_entry.stored_value->value->insert(deep, home);
        new_entry.stored_value->value->insert(composite, home);
      }
      composite.Clear();
      PreciselyCollectGarbage();
      EXPECT_EQ(10000u, outer->size());
      for (int i = 0; i < 10000; i++) {
        IntWrapper* value = keep_alive->at(i);
        EXPECT_EQ(1u,
                  outer->at(value)
                      ->size());  // Other one was deleted by weak handling.
        if (i & 1)
          keep_alive->at(i) = nullptr;
      }
      PreciselyCollectGarbage();
      EXPECT_EQ(5000u, outer->size());
    }
  }
}

class EphemeronWrapper : public GarbageCollected<EphemeronWrapper> {
 public:
  void Trace(Visitor* visitor) const { visitor->Trace(map_); }

  typedef HeapHashMap<WeakMember<IntWrapper>, Member<EphemeronWrapper>> Map;
  Map& GetMap() { return map_; }

 private:
  Map map_;
};

TEST_F(HeapTest, EphemeronsPointToEphemerons) {
  Persistent<IntWrapper> key = MakeGarbageCollected<IntWrapper>(42);
  Persistent<IntWrapper> key2 = MakeGarbageCollected<IntWrapper>(103);

  Persistent<EphemeronWrapper> chain;
  for (int i = 0; i < 100; i++) {
    EphemeronWrapper* old_head = chain;
    chain = MakeGarbageCollected<EphemeronWrapper>();
    if (i == 50)
      chain->GetMap().insert(key2, old_head);
    else
      chain->GetMap().insert(key, old_head);
    chain->GetMap().insert(MakeGarbageCollected<IntWrapper>(103),
                           MakeGarbageCollected<EphemeronWrapper>());
  }

  PreciselyCollectGarbage();

  EphemeronWrapper* wrapper = chain;
  for (int i = 0; i < 100; i++) {
    EXPECT_EQ(1u, wrapper->GetMap().size());
    if (i == 49)
      wrapper = wrapper->GetMap().at(key2);
    else
      wrapper = wrapper->GetMap().at(key);
  }
  EXPECT_EQ(nullptr, wrapper);

  key2.Clear();
  PreciselyCollectGarbage();

  wrapper = chain;
  for (int i = 0; i < 50; i++) {
    EXPECT_EQ(i == 49 ? 0u : 1u, wrapper->GetMap().size());
    wrapper = wrapper->GetMap().at(key);
  }
  EXPECT_EQ(nullptr, wrapper);

  key.Clear();
  PreciselyCollectGarbage();
  EXPECT_EQ(0u, chain->GetMap().size());
}

TEST_F(HeapTest, Ephemeron) {
  typedef HeapHashSet<WeakMember<IntWrapper>> Set;

  Persistent<Set> set = MakeGarbageCollected<Set>();

  Persistent<IntWrapper> wp1 = MakeGarbageCollected<IntWrapper>(1);
  Persistent<IntWrapper> wp2 = MakeGarbageCollected<IntWrapper>(2);
  Persistent<IntWrapper> pw1 = MakeGarbageCollected<IntWrapper>(3);
  Persistent<IntWrapper> pw2 = MakeGarbageCollected<IntWrapper>(4);

  set->insert(wp1);
  set->insert(wp2);
  set->insert(pw1);
  set->insert(pw2);

  PreciselyCollectGarbage();

  EXPECT_EQ(4u, set->size());

  wp2.Clear();  // Kills all entries in the weakPairMaps except the first.
  pw2.Clear();  // Kills all entries in the pairWeakMaps except the first.

  for (int i = 0; i < 2; i++) {
    PreciselyCollectGarbage();

    EXPECT_EQ(2u, set->size());  // wp1 and pw1.
  }

  wp1.Clear();
  pw1.Clear();

  PreciselyCollectGarbage();

  EXPECT_EQ(0u, set->size());
}

class Link1 : public GarbageCollected<Link1> {
 public:
  Link1(IntWrapper* link) : link_(link) {}

  void Trace(Visitor* visitor) const { visitor->Trace(link_); }

  IntWrapper* Link() { return link_; }

 private:
  Member<IntWrapper> link_;
};

TEST_F(HeapTest, IndirectStrongToWeak) {
  typedef HeapHashMap<WeakMember<IntWrapper>, Member<Link1>> Map;
  Persistent<Map> map = MakeGarbageCollected<Map>();
  Persistent<IntWrapper> dead_object = MakeGarbageCollected<IntWrapper>(
      100);  // Named for "Drowning by Numbers" (1988).
  Persistent<IntWrapper> life_object = MakeGarbageCollected<IntWrapper>(42);
  map->insert(dead_object, MakeGarbageCollected<Link1>(dead_object));
  map->insert(life_object, MakeGarbageCollected<Link1>(life_object));
  EXPECT_EQ(2u, map->size());
  PreciselyCollectGarbage();
  EXPECT_EQ(2u, map->size());
  EXPECT_EQ(dead_object, map->at(dead_object)->Link());
  EXPECT_EQ(life_object, map->at(life_object)->Link());
  dead_object.Clear();  // Now it can live up to its name.
  PreciselyCollectGarbage();
  EXPECT_EQ(1u, map->size());
  EXPECT_EQ(life_object, map->at(life_object)->Link());
  life_object.Clear();  // Despite its name.
  PreciselyCollectGarbage();
  EXPECT_EQ(0u, map->size());
}

static bool AllocateAndReturnBool() {
  TestSupportingGC::ConservativelyCollectGarbage();
  return true;
}

template <typename T>
class TraceIfNeededTester final
    : public GarbageCollected<TraceIfNeededTester<T>> {
 public:
  TraceIfNeededTester() = default;
  explicit TraceIfNeededTester(const T& obj) : obj_(obj) {}

  void Trace(Visitor* visitor) const { TraceIfNeeded<T>::Trace(visitor, obj_); }
  T& Obj() { return obj_; }
  ~TraceIfNeededTester() = default;

 private:
  T obj_;
};

class PartObject {
  DISALLOW_NEW();

 public:
  PartObject() : obj_(MakeGarbageCollected<SimpleObject>()) {}
  void Trace(Visitor* visitor) const { visitor->Trace(obj_); }

 private:
  Member<SimpleObject> obj_;
};

class AllocatesOnAssignment : public GarbageCollected<AllocatesOnAssignment> {
 public:
  AllocatesOnAssignment(std::nullptr_t) : value_(nullptr) {}
  AllocatesOnAssignment(int x) : value_(MakeGarbageCollected<IntWrapper>(x)) {}
  AllocatesOnAssignment(IntWrapper* x) : value_(x) {}

  AllocatesOnAssignment& operator=(const AllocatesOnAssignment x) {
    value_ = x.value_;
    return *this;
  }

  enum DeletedMarker { kDeletedValue };

  AllocatesOnAssignment(const AllocatesOnAssignment& other) {
    if (!ThreadState::Current()->IsGCForbidden())
      TestSupportingGC::ConservativelyCollectGarbage();
    value_ = MakeGarbageCollected<IntWrapper>(other.value_->Value());
  }

  AllocatesOnAssignment(DeletedMarker) : value_(WTF::kHashTableDeletedValue) {}

  inline bool IsDeleted() const { return value_.IsHashTableDeletedValue(); }

  void Trace(Visitor* visitor) const { visitor->Trace(value_); }

  int Value() { return value_->Value(); }

 private:
  Member<IntWrapper> value_;

  friend bool operator==(const AllocatesOnAssignment&,
                         const AllocatesOnAssignment&);
  friend void swap(AllocatesOnAssignment&, AllocatesOnAssignment&);
};

bool operator==(const AllocatesOnAssignment& a,
                const AllocatesOnAssignment& b) {
  if (a.value_)
    return b.value_ && a.value_->Value() == b.value_->Value();
  return !b.value_;
}

void swap(AllocatesOnAssignment& a, AllocatesOnAssignment& b) {
  std::swap(a.value_, b.value_);
}

}  // namespace blink

namespace blink {

TEST_F(HeapTest, GCInHashMapOperations) {
  typedef HeapHashMap<Member<AllocatesOnAssignment>,
                      Member<AllocatesOnAssignment>>
      Map;
  Persistent<Map> map = MakeGarbageCollected<Map>();
  IntWrapper* key = MakeGarbageCollected<IntWrapper>(42);
  AllocatesOnAssignment* object =
      MakeGarbageCollected<AllocatesOnAssignment>(key);
  map->insert(object, MakeGarbageCollected<AllocatesOnAssignment>(103));
  map->erase(object);
  for (int i = 0; i < 10; i++) {
    map->insert(MakeGarbageCollected<AllocatesOnAssignment>(i),
                MakeGarbageCollected<AllocatesOnAssignment>(i));
  }
  for (Map::iterator it = map->begin(); it != map->end(); ++it)
    EXPECT_EQ(it->key->Value(), it->value->Value());
}

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

TEST_F(HeapTest, PartObjectWithVirtualMethod) {
  ObjectWithVirtualPartObject* object =
      MakeGarbageCollected<ObjectWithVirtualPartObject>();
  EXPECT_TRUE(object);
}

class AllocInSuperConstructorArgumentSuper
    : public GarbageCollected<AllocInSuperConstructorArgumentSuper> {
 public:
  AllocInSuperConstructorArgumentSuper(bool value) : value_(value) {}
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

// Regression test for crbug.com/404511. Tests conservative marking of
// an object with an uninitialized vtable.
TEST_F(HeapTest, AllocationInSuperConstructorArgument) {
  AllocInSuperConstructorArgument* object =
      MakeGarbageCollected<AllocInSuperConstructorArgument>();
  EXPECT_TRUE(object);
  ThreadState::Current()->CollectAllGarbageForTesting();
}

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

TEST_F(HeapTest, NonNodeAllocatingNodeInDestructor) {
  MakeGarbageCollected<NonNodeAllocatingNodeInDestructor>();
  PreciselyCollectGarbage();
  EXPECT_EQ(10, (*NonNodeAllocatingNodeInDestructor::node_)->Value());
  delete NonNodeAllocatingNodeInDestructor::node_;
  NonNodeAllocatingNodeInDestructor::node_ = nullptr;
}

class DeepEagerly final : public GarbageCollected<DeepEagerly> {
 public:
  DeepEagerly(DeepEagerly* next) : next_(next) {}

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

TEST_F(HeapTest, TraceDeepEagerly) {
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

TEST_F(HeapTest, DequeExpand) {
  // Test expansion of a HeapDeque<>'s buffer.

  typedef HeapDeque<Member<IntWrapper>> IntDeque;

  Persistent<IntDeque> deque = MakeGarbageCollected<IntDeque>();

  // Append a sequence, bringing about repeated expansions of the
  // deque's buffer.
  int i = 0;
  for (; i < 60; ++i)
    deque->push_back(MakeGarbageCollected<IntWrapper>(i));

  EXPECT_EQ(60u, deque->size());
  i = 0;
  for (const auto& int_wrapper : *deque) {
    EXPECT_EQ(i, int_wrapper->Value());
    i++;
  }

  // Remove most of the queued objects and have the buffer's start index
  // 'point' somewhere into the buffer, just behind the end index.
  for (i = 0; i < 50; ++i)
    deque->TakeFirst();

  EXPECT_EQ(10u, deque->size());
  i = 0;
  for (const auto& int_wrapper : *deque) {
    EXPECT_EQ(50 + i, int_wrapper->Value());
    i++;
  }

  // Append even more, eventually causing an expansion of the underlying
  // buffer once the end index wraps around and reaches the start index.
  for (i = 0; i < 70; ++i)
    deque->push_back(MakeGarbageCollected<IntWrapper>(60 + i));

  // Verify that the final buffer expansion copied the start and end segments
  // of the old buffer to both ends of the expanded buffer, along with
  // re-adjusting both start&end indices in terms of that expanded buffer.
  EXPECT_EQ(80u, deque->size());
  i = 0;
  for (const auto& int_wrapper : *deque) {
    EXPECT_EQ(i + 50, int_wrapper->Value());
    i++;
  }
}

class SimpleRefValue : public RefCounted<SimpleRefValue> {
 public:
  static scoped_refptr<SimpleRefValue> Create(int i) {
    return base::AdoptRef(new SimpleRefValue(i));
  }

  int Value() const { return value_; }

 private:
  explicit SimpleRefValue(int value) : value_(value) {}

  int value_;
};

class PartObjectWithRef {
  DISALLOW_NEW();

 public:
  PartObjectWithRef(int i) : value_(SimpleRefValue::Create(i)) {}

  void Trace(Visitor* visitor) const {}

  int Value() const { return value_->Value(); }

 private:
  scoped_refptr<SimpleRefValue> value_;
};

}  // namespace blink

WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::PartObjectWithRef)

namespace blink {

TEST_F(HeapTest, HeapVectorPartObjects) {
  HeapVector<PartObjectWithRef> vector1;
  HeapVector<PartObjectWithRef> vector2;

  for (int i = 0; i < 10; ++i) {
    vector1.push_back(PartObjectWithRef(i));
    vector2.push_back(PartObjectWithRef(i));
  }

  vector1.ReserveCapacity(150);
  EXPECT_LE(150u, vector1.capacity());
  EXPECT_EQ(10u, vector1.size());

  vector2.ReserveCapacity(100);
  EXPECT_LE(100u, vector2.capacity());
  EXPECT_EQ(10u, vector2.size());

  for (int i = 0; i < 4; ++i) {
    vector1.push_back(PartObjectWithRef(10 + i));
    vector2.push_back(PartObjectWithRef(10 + i));
    vector2.push_back(PartObjectWithRef(10 + i));
  }

  // Shrinking heap vector backing stores always succeeds,
  // so these two will not currently exercise the code path
  // where shrinking causes copying into a new, small buffer.
  vector2.ShrinkToReasonableCapacity();
  EXPECT_EQ(18u, vector2.size());

  vector1.ShrinkToReasonableCapacity();
  EXPECT_EQ(14u, vector1.size());
}

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

TEST_F(HeapTest, NestedMixinConstruction) {
  TestMixinAllocationC* object = MakeGarbageCollected<TestMixinAllocationC>();
  EXPECT_TRUE(object);
}

class ObjectWithLargeAmountsOfAllocationInConstructor {
 public:
  ObjectWithLargeAmountsOfAllocationInConstructor(
      size_t number_of_large_objects_to_allocate,
      ClassWithMember* member) {
    // Should a constructor allocate plenty in its constructor,
    // and it is a base of GC mixin, GCs will remain locked out
    // regardless, as we cannot safely trace the leftmost GC
    // mixin base.
    DCHECK(ThreadState::Current()->IsGCForbidden());
    for (size_t i = 0; i < number_of_large_objects_to_allocate; i++) {
      auto* large_object = MakeGarbageCollected<LargeHeapObject>();
      EXPECT_TRUE(large_object);
      EXPECT_EQ(0, member->TraceCount());
    }
  }
};

class WeakPersistentHolder final {
 public:
  explicit WeakPersistentHolder(IntWrapper* object) : object_(object) {}
  IntWrapper* Object() const { return object_; }

 private:
  WeakPersistent<IntWrapper> object_;
};

TEST_F(HeapTest, WeakPersistent) {
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

class ThreadedClearOnShutdownTester : public ThreadedTesterBase {
 public:
  static void Test() {
    IntWrapper::destructor_calls_ = 0;
    ThreadedTesterBase::Test(new ThreadedClearOnShutdownTester);
    EXPECT_EQ(kNumberOfThreads, IntWrapper::destructor_calls_);
  }

 private:
  void RunWhileAttached();

  void RunThread() override {
    EXPECT_EQ(42, ThreadSpecificIntWrapper().Value());
    RunWhileAttached();
  }

  class HeapObject;
  friend class HeapObject;

  using WeakHeapObjectSet = HeapHashSet<WeakMember<HeapObject>>;

  static WeakHeapObjectSet& GetWeakHeapObjectSet();

  using HeapObjectSet = HeapHashSet<Member<HeapObject>>;
  static HeapObjectSet& GetHeapObjectSet();

  static IntWrapper& ThreadSpecificIntWrapper() {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<Persistent<IntWrapper>>,
                                    int_wrapper, ());
    Persistent<IntWrapper>& handle = *int_wrapper;
    if (!handle) {
      handle = MakeGarbageCollected<IntWrapper>(42);
      handle.RegisterAsStaticReference();
    }
    return *handle;
  }
};

class ThreadedClearOnShutdownTester::HeapObject final
    : public GarbageCollected<ThreadedClearOnShutdownTester::HeapObject> {
 public:
  explicit HeapObject(bool test_destructor)
      : test_destructor_(test_destructor) {}
  ~HeapObject() {
    if (!test_destructor_)
      return;

    // Verify that the weak reference is gone.
    EXPECT_FALSE(GetWeakHeapObjectSet().Contains(this));

    // Add a new member to the static singleton; this will
    // re-initializes the persistent node of the collection
    // object. Done while terminating the test thread, so
    // verify that this brings about the release of the
    // persistent also.
    GetHeapObjectSet().insert(MakeGarbageCollected<HeapObject>(false));
  }

  void Trace(Visitor* visitor) const {}

 private:
  bool test_destructor_;
};

ThreadedClearOnShutdownTester::WeakHeapObjectSet&
ThreadedClearOnShutdownTester::GetWeakHeapObjectSet() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<Persistent<WeakHeapObjectSet>>,
                                  singleton, ());
  Persistent<WeakHeapObjectSet>& singleton_persistent = *singleton;
  if (!singleton_persistent) {
    singleton_persistent = MakeGarbageCollected<WeakHeapObjectSet>();
    singleton_persistent.RegisterAsStaticReference();
  }
  return *singleton_persistent;
}

ThreadedClearOnShutdownTester::HeapObjectSet&
ThreadedClearOnShutdownTester::GetHeapObjectSet() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<Persistent<HeapObjectSet>>,
                                  singleton, ());
  Persistent<HeapObjectSet>& singleton_persistent = *singleton;
  if (!singleton_persistent) {
    singleton_persistent = MakeGarbageCollected<HeapObjectSet>();
    singleton_persistent.RegisterAsStaticReference();
  }
  return *singleton_persistent;
}

void ThreadedClearOnShutdownTester::RunWhileAttached() {
  EXPECT_EQ(42, ThreadSpecificIntWrapper().Value());
  // Creates a thread-specific singleton to a weakly held object.
  GetWeakHeapObjectSet().insert(MakeGarbageCollected<HeapObject>(true));
}

}  // namespace

TEST_F(HeapTest, TestClearOnShutdown) {
  ThreadedClearOnShutdownTester::Test();
}

// Verify that WeakMember<const T> compiles and behaves as expected.
class WithWeakConstObject final : public GarbageCollected<WithWeakConstObject> {
 public:
  WithWeakConstObject(const IntWrapper* int_wrapper) : wrapper_(int_wrapper) {}

  void Trace(Visitor* visitor) const { visitor->Trace(wrapper_); }

  const IntWrapper* Value() const { return wrapper_; }

 private:
  WeakMember<const IntWrapper> wrapper_;
};

TEST_F(HeapTest, TestWeakConstObject) {
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

class EmptyMixin : public GarbageCollectedMixin {};
class UseMixinFromLeftmostInherited : public UseMixin, public EmptyMixin {
 public:
  ~UseMixinFromLeftmostInherited() = default;
};

TEST_F(HeapTest, IsGarbageCollected) {
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

TEST_F(HeapTest, HeapHashMapCallsDestructor) {
  String string = "string";
  EXPECT_TRUE(string.Impl()->HasOneRef());

  HeapHashMap<KeyWithCopyingMoveConstructor, Member<IntWrapper>> map;

  EXPECT_TRUE(string.Impl()->HasOneRef());

  for (int i = 1; i <= 100; ++i) {
    KeyWithCopyingMoveConstructor key(i, string);
    map.insert(key, MakeGarbageCollected<IntWrapper>(i));
  }

  EXPECT_FALSE(string.Impl()->HasOneRef());
  map.clear();

  EXPECT_TRUE(string.Impl()->HasOneRef());
}

TEST_F(HeapTest, ShrinkVector) {
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

TEST_F(HeapTest, GarbageCollectedInConstruction) {
  using O = ObjectWithCallbackBeforeInitializer<IntWrapper>;
  MakeGarbageCollected<O>(base::BindOnce([](O* thiz) {
    CHECK(HeapObjectHeader::FromPayload(thiz)->IsInConstruction());
  }));
}

TEST_F(HeapTest, GarbageCollectedMixinInConstruction) {
  using O = ObjectWithMixinWithCallbackBeforeInitializer<IntWrapper>;
  MakeGarbageCollected<O>(base::BindOnce([](O::Mixin* thiz) {
    const HeapObjectHeader* const header =
        HeapObjectHeader::FromInnerAddress(reinterpret_cast<Address>(thiz));
    CHECK(header->IsInConstruction());
  }));
}

TEST_F(HeapTest, PersistentAssignsDeletedValue) {
  // Regression test: https://crbug.com/982313

  Persistent<IntWrapper> deleted(WTF::kHashTableDeletedValue);
  Persistent<IntWrapper> pre_initialized(MakeGarbageCollected<IntWrapper>(1));
  pre_initialized = deleted;
  PreciselyCollectGarbage();
}

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

TEST_F(HeapTest, AccessDeletedBackingStore) {
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

TEST_F(HeapTest, CallMostDerivedFinalizer) {
  MakeGarbageCollected<GCDerived>();
  PreciselyCollectGarbage();
  EXPECT_EQ(1, GCDerived::destructor_called);
}

#if defined(ADDRESS_SANITIZER)
TEST(HeapDeathTest, DieOnPoisonedObjectHeaderAccess) {
  auto* ptr = MakeGarbageCollected<IntWrapper>(1);
  HeapObjectHeader* header = HeapObjectHeader::FromPayload(ptr);
  auto* low = reinterpret_cast<uint16_t*>(header);
  auto access = [low] {
    volatile uint16_t half = WTF::AsAtomicPtr(low)->load();
    WTF::AsAtomicPtr(low)->store(half);
  };
  EXPECT_DEATH(access(), "");
}

TEST_F(HeapTest, SuccessfulUnsanitizedAccessToObjectHeader) {
  auto* ptr = MakeGarbageCollected<IntWrapper>(1);
  HeapObjectHeader* header = HeapObjectHeader::FromPayload(ptr);
  auto* low = reinterpret_cast<uint16_t*>(header);
  volatile uint16_t half = internal::AsUnsanitizedAtomic(low)->load();
  internal::AsUnsanitizedAtomic(low)->store(half);
}
#endif  // ADDRESS_SANITIZER

}  // namespace blink
