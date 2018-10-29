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
#include <memory>
#include <utility>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/heap/address_cache.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_linked_stack.h"
#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"
#include "third_party/blink/renderer/platform/heap/heap_terminated_array_builder.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/marking_visitor.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/heap/stack_frame_depth.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"

namespace blink {

namespace {

class IntWrapper : public GarbageCollectedFinalized<IntWrapper> {
 public:
  static IntWrapper* Create(int x) { return new IntWrapper(x); }

  virtual ~IntWrapper() { AtomicIncrement(&destructor_calls_); }

  static int destructor_calls_;
  void Trace(blink::Visitor* visitor) {}

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
  void* pointer_[4];
};

static_assert(sizeof(Persistent<IntWrapper>) <= sizeof(SameSizeAsPersistent),
              "Persistent handle should stay small");

class ThreadMarker {
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

typedef std::pair<Member<IntWrapper>, WeakMember<IntWrapper>> StrongWeakPair;

struct PairWithWeakHandling : public StrongWeakPair {
  DISALLOW_NEW();

 public:
  // Regular constructor.
  PairWithWeakHandling(IntWrapper* one, IntWrapper* two)
      : StrongWeakPair(one, two) {
    DCHECK(one);  // We use null first field to indicate empty slots in the hash
                  // table.
  }

  // The HashTable (via the HashTrait) calls this constructor with a
  // placement new to mark slots in the hash table as being deleted. We will
  // never call trace or the destructor on these slots. We mark ourselves
  // deleted
  // with a pointer to -1 in the first field.
  PairWithWeakHandling(WTF::HashTableDeletedValueType)
      : StrongWeakPair(WTF::kHashTableDeletedValue, nullptr) {}

  // Used by the HashTable (via the HashTrait) to skip deleted slots in the
  // table. Recognizes objects that were 'constructed' using the above
  // constructor.
  bool IsHashTableDeletedValue() const {
    return first.IsHashTableDeletedValue();
  }

  bool IsAlive() { return ThreadHeap::IsHeapObjectAlive(second); }

  // Since we don't allocate independent objects of this type, we don't need
  // a regular trace method. Instead, we use a traceInCollection method. If
  // the entry should be deleted from the collection we return true and don't
  // trace the strong pointer.
  template <typename VisitorDispatcher>
  bool TraceInCollection(VisitorDispatcher visitor,
                         WTF::WeakHandlingFlag weakness) {
    HashTraits<WeakMember<IntWrapper>>::TraceInCollection(visitor, second,
                                                          weakness);
    if (!ThreadHeap::IsHeapObjectAlive(second))
      return true;

    visitor->Trace(first);
    return false;
  }

  // Incremental marking requires that these objects have a regular tracing
  // method that is used for eagerly tracing through them in case they are
  // in-place constructed in a container. In this case, we only care about
  // strong fields.
  void Trace(blink::Visitor* visitor) { visitor->Trace(first); }
};

template <typename T>
struct WeakHandlingHashTraits : WTF::SimpleClassHashTraits<T> {
  // We want to treat the object as a weak object in the sense that it can
  // disappear from hash sets and hash maps.
  static const WTF::WeakHandlingFlag kWeakHandlingFlag = WTF::kWeakHandling;
  // Normally whether or not an object needs tracing is inferred
  // automatically from the presence of the trace method, but we don't
  // necessarily have a trace method, and we may not need one because T
  // can perhaps only be allocated inside collections, never as independent
  // objects.  Explicitly mark this as needing tracing and it will be traced
  // in collections using the traceInCollection method, which it must have.
  template <typename U = void>
  struct IsTraceableInCollection {
    static const bool value = true;
  };
  // The traceInCollection method traces differently depending on whether we
  // are strongifying the trace operation.  We strongify the trace operation
  // when there are active iterators on the object.  In this case all
  // WeakMembers are marked like strong members so that elements do not
  // suddenly disappear during iteration.  Returns true if weak pointers to
  // dead objects were found: In this case any strong pointers were not yet
  // traced and the entry should be removed from the collection.
  template <typename VisitorDispatcher>
  static bool TraceInCollection(VisitorDispatcher visitor,
                                T& t,
                                WTF::WeakHandlingFlag weakness) {
    return t.TraceInCollection(visitor, weakness);
  }

  static bool IsAlive(T& t) { return t.IsAlive(); }
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

// The hash algorithm for our custom pair class is just the standard double
// hash for pairs. Note that this means you can't mutate either of the parts of
// the pair while they are in the hash table, as that would change their hash
// code and thus their preferred placement in the table.
template <>
struct DefaultHash<blink::PairWithWeakHandling> {
  typedef PairHash<blink::Member<blink::IntWrapper>,
                   blink::WeakMember<blink::IntWrapper>>
      Hash;
};

// Custom traits for the pair. These are weakness handling traits, which means
// PairWithWeakHandling must implement the traceInCollection method.
// In addition, these traits are concerned with the two magic values for the
// object, that represent empty and deleted slots in the hash table. The
// SimpleClassHashTraits allow empty slots in the table to be initialzed with
// memset to zero, and we use -1 in the first part of the pair to represent
// deleted slots.
template <>
struct HashTraits<blink::PairWithWeakHandling>
    : blink::WeakHandlingHashTraits<blink::PairWithWeakHandling> {
  static const bool kHasIsEmptyValueFunction = true;
  static bool IsEmptyValue(const blink::PairWithWeakHandling& value) {
    return !value.first;
  }
  static void ConstructDeletedValue(blink::PairWithWeakHandling& slot, bool) {
    new (NotNull, &slot) blink::PairWithWeakHandling(kHashTableDeletedValue);
  }
  static bool IsDeletedValue(const blink::PairWithWeakHandling& value) {
    return value.IsHashTableDeletedValue();
  }
};

template <>
struct IsTraceable<blink::PairWithWeakHandling> {
  static const bool value = IsTraceable<blink::StrongWeakPair>::value;
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
 public:
  explicit TestGCCollectGarbageScope(BlinkGC::StackState state) {
    DCHECK(ThreadState::Current()->CheckThread());
  }

  ~TestGCCollectGarbageScope() { ThreadState::Current()->CompleteSweep(); }
};

class TestGCScope : public TestGCCollectGarbageScope {
 public:
  explicit TestGCScope(BlinkGC::StackState state)
      : TestGCCollectGarbageScope(state),
        atomic_pause_scope_(ThreadState::Current()) {
    ThreadState::Current()->Heap().stats_collector()->NotifyMarkingStarted(
        BlinkGC::GCReason::kTesting);
    ThreadState::Current()->AtomicPausePrologue(state, BlinkGC::kAtomicMarking,
                                                BlinkGC::GCReason::kPreciseGC);
  }
  ~TestGCScope() {
    ThreadState::Current()->MarkPhaseEpilogue(BlinkGC::kAtomicMarking);
    ThreadState::Current()->AtomicPauseEpilogue(BlinkGC::kAtomicMarking,
                                                BlinkGC::kEagerSweeping);
  }

 private:
  ThreadState::AtomicPauseScope atomic_pause_scope_;
};

class SimpleObject : public GarbageCollected<SimpleObject> {
 public:
  static SimpleObject* Create() { return new SimpleObject(); }
  void Trace(blink::Visitor* visitor) {}
  char GetPayload(int i) { return payload[i]; }
  // This virtual method is unused but it is here to make sure
  // that this object has a vtable. This object is used
  // as the super class for objects that also have garbage
  // collected mixins and having a virtual here makes sure
  // that adjustment is needed both for marking and for isAlive
  // checks.
  virtual void VirtualMethod() {}

 protected:
  SimpleObject() = default;
  char payload[64];
};

class HeapTestSuperClass
    : public GarbageCollectedFinalized<HeapTestSuperClass> {
 public:
  static HeapTestSuperClass* Create() { return new HeapTestSuperClass(); }

  virtual ~HeapTestSuperClass() { ++destructor_calls_; }

  static int destructor_calls_;
  void Trace(blink::Visitor* visitor) {}

 protected:
  HeapTestSuperClass() = default;
};

int HeapTestSuperClass::destructor_calls_ = 0;

class HeapTestOtherSuperClass {
 public:
  int payload;
};

static const size_t kClassMagic = 0xABCDDBCA;

class HeapTestSubClass : public HeapTestOtherSuperClass,
                         public HeapTestSuperClass {
 public:
  static HeapTestSubClass* Create() { return new HeapTestSubClass(); }

  ~HeapTestSubClass() override {
    EXPECT_EQ(kClassMagic, magic_);
    ++destructor_calls_;
  }

  static int destructor_calls_;

 private:
  HeapTestSubClass() : magic_(kClassMagic) {}

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
  void Trace(blink::Visitor* visitor) {}

 private:
  static const int kArraySize = 1000;
  int8_t array_[kArraySize];
};

class OffHeapInt : public RefCounted<OffHeapInt> {
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

int IntWrapper::destructor_calls_ = 0;
int OffHeapInt::destructor_calls_ = 0;

class ThreadedTesterBase {
 protected:
  static void Test(ThreadedTesterBase* tester) {
    Vector<std::unique_ptr<Thread>, kNumberOfThreads> threads;
    for (int i = 0; i < kNumberOfThreads; i++) {
      threads.push_back(Platform::Current()->CreateThread(
          ThreadCreationParams(WebThreadType::kTestThread)
              .SetThreadNameForTest("blink gc testing thread")));
      PostCrossThreadTask(
          *threads.back()->GetTaskRunner(), FROM_HERE,
          CrossThreadBind(ThreadFunc, CrossThreadUnretained(tester)));
    }
    while (AcquireLoad(&tester->threads_to_finish_)) {
      test::YieldCurrentThread();
    }
    delete tester;
  }

  virtual void RunThread() = 0;

 protected:
  static const int kNumberOfThreads = 10;
  static const int kGcPerThread = 5;
  static const int kNumberOfAllocations = 50;

  ThreadedTesterBase() : gc_count_(0), threads_to_finish_(kNumberOfThreads) {}

  virtual ~ThreadedTesterBase() = default;

  inline bool Done() const {
    return AcquireLoad(&gc_count_) >= kNumberOfThreads * kGcPerThread;
  }

  volatile int gc_count_;
  volatile int threads_to_finish_;

 private:
  static void ThreadFunc(void* data) {
    reinterpret_cast<ThreadedTesterBase*>(data)->RunThread();
  }
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
        IntWrapper::Create(value));
  }

  void AddGlobalPersistent() {
    MutexLocker lock(mutex_);
    cross_persistents_.push_back(CreateGlobalPersistent(0x2a2a2a2a));
  }

  void RunThread() override {
    ThreadState::AttachCurrentThread();

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
          wrapper = IntWrapper::Create(0x0bbac0de);
          if (!(i % 10)) {
            global_persistent = CreateGlobalPersistent(0x0ed0cabb);
          }
          test::YieldCurrentThread();
        }

        if (gc_count < kGcPerThread) {
          PreciselyCollectGarbage();
          gc_count++;
          AtomicIncrement(&gc_count_);
        }

        // Taking snapshot shouldn't have any bad side effect.
        // TODO(haraken): This snapshot GC causes crashes, so disable
        // it at the moment. Fix the crash and enable it.
        // ThreadHeap::collectGarbage(BlinkGC::NoHeapPointersOnStack,
        //                            BlinkGC::TakeSnapshot, BlinkGC::ForcedGC);
        PreciselyCollectGarbage();
        EXPECT_EQ(wrapper->Value(), 0x0bbac0de);
        EXPECT_EQ((*global_persistent)->Value(), 0x0ed0cabb);
      }
      test::YieldCurrentThread();
    }

    ThreadState::DetachCurrentThread();
    AtomicDecrement(&threads_to_finish_);
  }
};

class ThreadedWeaknessTester : public ThreadedTesterBase {
 public:
  static void Test() { ThreadedTesterBase::Test(new ThreadedWeaknessTester); }

 private:
  void RunThread() override {
    ThreadState::AttachCurrentThread();

    int gc_count = 0;
    while (!Done()) {
      {
        Persistent<HeapHashMap<ThreadMarker, WeakMember<IntWrapper>>> weak_map =
            new HeapHashMap<ThreadMarker, WeakMember<IntWrapper>>;

        for (int i = 0; i < kNumberOfAllocations; i++) {
          weak_map->insert(static_cast<unsigned>(i), IntWrapper::Create(0));
          test::YieldCurrentThread();
        }

        if (gc_count < kGcPerThread) {
          PreciselyCollectGarbage();
          gc_count++;
          AtomicIncrement(&gc_count_);
        }

        // Taking snapshot shouldn't have any bad side effect.
        // TODO(haraken): This snapshot GC causes crashes, so disable
        // it at the moment. Fix the crash and enable it.
        // ThreadHeap::collectGarbage(BlinkGC::NoHeapPointersOnStack,
        //                            BlinkGC::TakeSnapshot, BlinkGC::ForcedGC);
        PreciselyCollectGarbage();
        EXPECT_TRUE(weak_map->IsEmpty());
      }
      test::YieldCurrentThread();
    }
    ThreadState::DetachCurrentThread();
    AtomicDecrement(&threads_to_finish_);
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

    void Trace(blink::Visitor* visitor) {}
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
        persistent_chain_ = PersistentChain::Create(count);
      }
    }

    Persistent<PersistentChain> persistent_chain_;
  };

  class PersistentChain : public GarbageCollectedFinalized<PersistentChain> {
   public:
    static PersistentChain* Create(int count) {
      return new PersistentChain(count);
    }

    void Trace(blink::Visitor* visitor) {}

   private:
    explicit PersistentChain(int count) {
      ref_counted_chain_ = base::AdoptRef(RefCountedChain::Create(count));
    }

    scoped_refptr<RefCountedChain> ref_counted_chain_;
  };

  void RunThread() override {
    ThreadState::AttachCurrentThread();

    PersistentChain::Create(100);

    // Upon thread detach, GCs will run until all persistents have been
    // released. We verify that the draining of persistents proceeds
    // as expected by dropping one Persistent<> per GC until there
    // are none left.
    ThreadState::DetachCurrentThread();
    AtomicDecrement(&threads_to_finish_);
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

class TraceCounter : public GarbageCollectedFinalized<TraceCounter> {
 public:
  static TraceCounter* Create() { return new TraceCounter(); }

  void Trace(blink::Visitor* visitor) { trace_count_++; }
  int TraceCount() const { return trace_count_; }

 private:
  TraceCounter() : trace_count_(0) {}

  int trace_count_;
};

TEST(HeapTest, IsHeapObjectAliveForConstPointer) {
  // See http://crbug.com/661363.
  SimpleObject* object = SimpleObject::Create();
  HeapObjectHeader* header = HeapObjectHeader::FromPayload(object);
  header->Mark();
  EXPECT_TRUE(ThreadHeap::IsHeapObjectAlive(object));
  const SimpleObject* const_object = const_cast<const SimpleObject*>(object);
  EXPECT_TRUE(ThreadHeap::IsHeapObjectAlive(const_object));
}

class ClassWithMember : public GarbageCollected<ClassWithMember> {
 public:
  static ClassWithMember* Create() { return new ClassWithMember(); }

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(trace_counter_);
  }
  int TraceCount() const { return trace_counter_->TraceCount(); }

 private:
  ClassWithMember() : trace_counter_(TraceCounter::Create()) {}

  Member<TraceCounter> trace_counter_;
};

class SimpleFinalizedObject
    : public GarbageCollectedFinalized<SimpleFinalizedObject> {
 public:
  static SimpleFinalizedObject* Create() { return new SimpleFinalizedObject(); }

  ~SimpleFinalizedObject() { ++destructor_calls_; }

  static int destructor_calls_;

  void Trace(blink::Visitor* visitor) {}

 private:
  SimpleFinalizedObject() = default;
};

int SimpleFinalizedObject::destructor_calls_ = 0;

class IntNode : public GarbageCollected<IntNode> {
 public:
  // IntNode is used to test typed heap allocation. Instead of
  // redefining blink::Node to our test version, we keep it separate
  // so as to avoid possible warnings about linker duplicates.
  // Override operator new to allocate IntNode subtype objects onto
  // the dedicated heap for blink::Node.
  //
  // TODO(haraken): untangling the heap unit tests from Blink would
  // simplify and avoid running into this problem - http://crbug.com/425381
  GC_PLUGIN_IGNORE("crbug.com/443854")
  void* operator new(size_t size) {
    ThreadState* state = ThreadState::Current();
    const char* type_name = WTF_HEAP_PROFILER_TYPE_NAME(IntNode);
    return state->Heap().AllocateOnArenaIndex(
        state, size, BlinkGC::kNodeArenaIndex, GCInfoTrait<IntNode>::Index(),
        type_name);
  }

  static IntNode* Create(int i) { return new IntNode(i); }

  void Trace(blink::Visitor* visitor) {}

  int Value() { return value_; }

 private:
  IntNode(int i) : value_(i) {}
  int value_;
};

class Bar : public GarbageCollectedFinalized<Bar> {
 public:
  static Bar* Create() { return new Bar(); }

  void FinalizeGarbageCollectedObject() {
    EXPECT_TRUE(magic_ == kMagic);
    magic_ = 0;
    live_--;
  }
  bool HasBeenFinalized() const { return !magic_; }

  virtual void Trace(blink::Visitor* visitor) {}
  static unsigned live_;

 protected:
  static const int kMagic = 1337;
  int magic_;

  Bar() : magic_(kMagic) { live_++; }
};

WILL_NOT_BE_EAGERLY_TRACED_CLASS(Bar);

unsigned Bar::live_ = 0;

class Baz : public GarbageCollected<Baz> {
 public:
  static Baz* Create(Bar* bar) { return new Baz(bar); }

  void Trace(blink::Visitor* visitor) { visitor->Trace(bar_); }

  void Clear() { bar_.Release(); }

  // willFinalize is called by FinalizationObserver.
  void WillFinalize() { EXPECT_TRUE(!bar_->HasBeenFinalized()); }

 private:
  explicit Baz(Bar* bar) : bar_(bar) {}

  Member<Bar> bar_;
};

class Foo : public Bar {
 public:
  static Foo* Create(Bar* bar) { return new Foo(bar); }

  static Foo* Create(Foo* foo) { return new Foo(foo); }

  void Trace(blink::Visitor* visitor) override {
    if (points_to_foo_)
      visitor->Trace(static_cast<Foo*>(bar_));
    else
      visitor->Trace(bar_);
  }

 private:
  Foo(Bar* bar) : Bar(), bar_(bar), points_to_foo_(false) {}

  Foo(Foo* foo) : Bar(), bar_(foo), points_to_foo_(true) {}

  Bar* bar_;
  bool points_to_foo_;
};

WILL_NOT_BE_EAGERLY_TRACED_CLASS(Foo);

class Bars : public Bar {
 public:
  static Bars* Create() { return new Bars(); }

  void Trace(blink::Visitor* visitor) override {
    for (unsigned i = 0; i < width_; i++)
      visitor->Trace(bars_[i]);
  }

  unsigned GetWidth() const { return width_; }

  static const unsigned kWidth = 7500;

 private:
  Bars() : width_(0) {
    for (unsigned i = 0; i < kWidth; i++) {
      bars_[i] = Bar::Create();
      width_++;
    }
  }

  unsigned width_;
  Member<Bar> bars_[kWidth];
};

WILL_NOT_BE_EAGERLY_TRACED_CLASS(Bars);

class ConstructorAllocation : public GarbageCollected<ConstructorAllocation> {
 public:
  static ConstructorAllocation* Create() { return new ConstructorAllocation(); }

  void Trace(blink::Visitor* visitor) { visitor->Trace(int_wrapper_); }

 private:
  ConstructorAllocation() { int_wrapper_ = IntWrapper::Create(42); }

  Member<IntWrapper> int_wrapper_;
};

class LargeHeapObject : public GarbageCollectedFinalized<LargeHeapObject> {
 public:
  ~LargeHeapObject() { destructor_calls_++; }
  static LargeHeapObject* Create() { return new LargeHeapObject(); }
  char Get(size_t i) { return data_[i]; }
  void Set(size_t i, char c) { data_[i] = c; }
  size_t length() { return kLength; }
  void Trace(blink::Visitor* visitor) { visitor->Trace(int_wrapper_); }
  static int destructor_calls_;

 private:
  static const size_t kLength = 1024 * 1024;
  LargeHeapObject() { int_wrapper_ = IntWrapper::Create(23); }
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
class RefCountedAndGarbageCollected
    : public GarbageCollectedFinalized<RefCountedAndGarbageCollected> {
 public:
  static RefCountedAndGarbageCollected* Create() {
    return new RefCountedAndGarbageCollected;
  }

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

  void Trace(blink::Visitor* visitor) {}

  static int destructor_calls_;

 private:
  RefCountedAndGarbageCollected() : ref_count_(0) {}

  int ref_count_;
  SelfKeepAlive<RefCountedAndGarbageCollected> keep_alive_;
};

int RefCountedAndGarbageCollected::destructor_calls_ = 0;

class RefCountedAndGarbageCollected2
    : public HeapTestOtherSuperClass,
      public GarbageCollectedFinalized<RefCountedAndGarbageCollected2> {
 public:
  static RefCountedAndGarbageCollected2* Create() {
    return new RefCountedAndGarbageCollected2;
  }

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

  void Trace(blink::Visitor* visitor) {}

  static int destructor_calls_;

 private:
  RefCountedAndGarbageCollected2() : ref_count_(0) {}

  int ref_count_;
  SelfKeepAlive<RefCountedAndGarbageCollected2> keep_alive_;
};

int RefCountedAndGarbageCollected2::destructor_calls_ = 0;

class Weak : public Bar {
 public:
  static Weak* Create(Bar* strong, Bar* weak) { return new Weak(strong, weak); }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(strong_bar_);
    visitor->template RegisterWeakMembers<Weak, &Weak::ZapWeakMembers>(this);
  }

  void ZapWeakMembers(Visitor* visitor) {
    if (!ThreadHeap::IsHeapObjectAlive(weak_bar_))
      weak_bar_ = nullptr;
  }

  bool StrongIsThere() { return !!strong_bar_; }
  bool WeakIsThere() { return !!weak_bar_; }

 private:
  Weak(Bar* strong_bar, Bar* weak_bar)
      : Bar(), strong_bar_(strong_bar), weak_bar_(weak_bar) {}

  Member<Bar> strong_bar_;
  Bar* weak_bar_;
};

WILL_NOT_BE_EAGERLY_TRACED_CLASS(Weak);

class WithWeakMember : public Bar {
 public:
  static WithWeakMember* Create(Bar* strong, Bar* weak) {
    return new WithWeakMember(strong, weak);
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(strong_bar_);
    visitor->Trace(weak_bar_);
  }

  bool StrongIsThere() { return !!strong_bar_; }
  bool WeakIsThere() { return !!weak_bar_; }

 private:
  WithWeakMember(Bar* strong_bar, Bar* weak_bar)
      : Bar(), strong_bar_(strong_bar), weak_bar_(weak_bar) {}

  Member<Bar> strong_bar_;
  WeakMember<Bar> weak_bar_;
};

WILL_NOT_BE_EAGERLY_TRACED_CLASS(WithWeakMember);

class Observable : public GarbageCollectedFinalized<Observable> {
  USING_PRE_FINALIZER(Observable, WillFinalize);

 public:
  static Observable* Create(Bar* bar) { return new Observable(bar); }
  ~Observable() { was_destructed_ = true; }
  void Trace(blink::Visitor* visitor) { visitor->Trace(bar_); }

  // willFinalize is called by FinalizationObserver. willFinalize can touch
  // other on-heap objects.
  void WillFinalize() {
    EXPECT_FALSE(was_destructed_);
    EXPECT_FALSE(bar_->HasBeenFinalized());
    will_finalize_was_called_ = true;
  }
  static bool will_finalize_was_called_;

 private:
  explicit Observable(Bar* bar) : bar_(bar), was_destructed_(false) {}

  Member<Bar> bar_;
  bool was_destructed_;
};

bool Observable::will_finalize_was_called_ = false;

class ObservableWithPreFinalizer
    : public GarbageCollectedFinalized<ObservableWithPreFinalizer> {
  USING_PRE_FINALIZER(ObservableWithPreFinalizer, Dispose);

 public:
  static ObservableWithPreFinalizer* Create() {
    return new ObservableWithPreFinalizer();
  }
  ~ObservableWithPreFinalizer() { was_destructed_ = true; }
  void Trace(blink::Visitor* visitor) {}
  void Dispose() {
    EXPECT_FALSE(was_destructed_);
    dispose_was_called_ = true;
  }
  static bool dispose_was_called_;

 protected:
  ObservableWithPreFinalizer() : was_destructed_(false) {}

  bool was_destructed_;
};

bool ObservableWithPreFinalizer::dispose_was_called_ = false;

bool g_dispose_was_called_for_pre_finalizer_base = false;
bool g_dispose_was_called_for_pre_finalizer_mixin = false;
bool g_dispose_was_called_for_pre_finalizer_sub_class = false;

class PreFinalizerBase : public GarbageCollectedFinalized<PreFinalizerBase> {
  USING_PRE_FINALIZER(PreFinalizerBase, Dispose);

 public:
  static PreFinalizerBase* Create() { return new PreFinalizerBase(); }
  virtual ~PreFinalizerBase() { was_destructed_ = true; }
  virtual void Trace(blink::Visitor* visitor) {}
  void Dispose() {
    EXPECT_FALSE(g_dispose_was_called_for_pre_finalizer_base);
    EXPECT_TRUE(g_dispose_was_called_for_pre_finalizer_sub_class);
    EXPECT_TRUE(g_dispose_was_called_for_pre_finalizer_mixin);
    EXPECT_FALSE(was_destructed_);
    g_dispose_was_called_for_pre_finalizer_base = true;
  }

 protected:
  PreFinalizerBase() : was_destructed_(false) {}
  bool was_destructed_;
};

class PreFinalizerMixin : public GarbageCollectedMixin {
  USING_PRE_FINALIZER(PreFinalizerMixin, Dispose);

 public:
  ~PreFinalizerMixin() { was_destructed_ = true; }
  void Trace(blink::Visitor* visitor) override {}
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
  USING_GARBAGE_COLLECTED_MIXIN(PreFinalizerSubClass);
  USING_PRE_FINALIZER(PreFinalizerSubClass, Dispose);

 public:
  static PreFinalizerSubClass* Create() { return new PreFinalizerSubClass(); }
  ~PreFinalizerSubClass() override { was_destructed_ = true; }
  void Trace(blink::Visitor* visitor) override {}
  void Dispose() {
    EXPECT_FALSE(g_dispose_was_called_for_pre_finalizer_base);
    EXPECT_FALSE(g_dispose_was_called_for_pre_finalizer_sub_class);
    EXPECT_FALSE(g_dispose_was_called_for_pre_finalizer_mixin);
    EXPECT_FALSE(was_destructed_);
    g_dispose_was_called_for_pre_finalizer_sub_class = true;
  }

 protected:
  PreFinalizerSubClass() : was_destructed_(false) {}
  bool was_destructed_;
};

template <typename T>
class FinalizationObserver : public GarbageCollected<FinalizationObserver<T>> {
 public:
  static FinalizationObserver* Create(T* data) {
    return new FinalizationObserver(data);
  }
  bool DidCallWillFinalize() const { return did_call_will_finalize_; }

  void Trace(blink::Visitor* visitor) {
    visitor->template RegisterWeakMembers<
        FinalizationObserver<T>, &FinalizationObserver<T>::ZapWeakMembers>(
        this);
  }

  void ZapWeakMembers(Visitor* visitor) {
    if (data_ && !ThreadHeap::IsHeapObjectAlive(data_)) {
      data_->WillFinalize();
      data_ = nullptr;
      did_call_will_finalize_ = true;
    }
  }

 private:
  FinalizationObserver(T* data) : data_(data), did_call_will_finalize_(false) {}

  WeakMember<T> data_;
  bool did_call_will_finalize_;
};

class FinalizationObserverWithHashMap {
 public:
  typedef HeapHashMap<WeakMember<Observable>,
                      std::unique_ptr<FinalizationObserverWithHashMap>>
      ObserverMap;

  explicit FinalizationObserverWithHashMap(Observable& target)
      : target_(target) {}
  ~FinalizationObserverWithHashMap() {
    target_.WillFinalize();
    did_call_will_finalize_ = true;
  }

  static ObserverMap& Observe(Observable& target) {
    ObserverMap& map = Observers();
    ObserverMap::AddResult result = map.insert(&target, nullptr);
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
    if (!observer_map_)
      observer_map_ = new Persistent<ObserverMap>(new ObserverMap());
    return **observer_map_;
  }

  Observable& target_;
  static Persistent<ObserverMap>* observer_map_;
};

bool FinalizationObserverWithHashMap::did_call_will_finalize_ = false;
Persistent<FinalizationObserverWithHashMap::ObserverMap>*
    FinalizationObserverWithHashMap::observer_map_;

class SuperClass;

class PointsBack : public GarbageCollectedFinalized<PointsBack> {
 public:
  static PointsBack* Create() { return new PointsBack; }

  ~PointsBack() { --alive_count_; }

  void SetBackPointer(SuperClass* back_pointer) {
    back_pointer_ = back_pointer;
  }

  SuperClass* BackPointer() const { return back_pointer_; }

  void Trace(blink::Visitor* visitor) { visitor->Trace(back_pointer_); }

  static int alive_count_;

 private:
  PointsBack() : back_pointer_(nullptr) { ++alive_count_; }

  WeakMember<SuperClass> back_pointer_;
};

int PointsBack::alive_count_ = 0;

class SuperClass : public GarbageCollectedFinalized<SuperClass> {
 public:
  static SuperClass* Create(PointsBack* points_back) {
    return new SuperClass(points_back);
  }

  virtual ~SuperClass() { --alive_count_; }

  void DoStuff(SuperClass* target,
               PointsBack* points_back,
               int super_class_count) {
    ConservativelyCollectGarbage();
    EXPECT_EQ(points_back, target->GetPointsBack());
    EXPECT_EQ(super_class_count, SuperClass::alive_count_);
  }

  virtual void Trace(blink::Visitor* visitor) { visitor->Trace(points_back_); }

  PointsBack* GetPointsBack() const { return points_back_.Get(); }

  static int alive_count_;

 protected:
  explicit SuperClass(PointsBack* points_back) : points_back_(points_back) {
    points_back_->SetBackPointer(this);
    ++alive_count_;
  }

 private:
  Member<PointsBack> points_back_;
};

int SuperClass::alive_count_ = 0;
class SubData : public GarbageCollectedFinalized<SubData> {
 public:
  SubData() { ++alive_count_; }
  ~SubData() { --alive_count_; }

  void Trace(blink::Visitor* visitor) {}

  static int alive_count_;
};

int SubData::alive_count_ = 0;

class SubClass : public SuperClass {
 public:
  static SubClass* Create(PointsBack* points_back) {
    return new SubClass(points_back);
  }

  ~SubClass() override { --alive_count_; }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(data_);
    SuperClass::Trace(visitor);
  }

  static int alive_count_;

 private:
  explicit SubClass(PointsBack* points_back)
      : SuperClass(points_back), data_(new SubData) {
    ++alive_count_;
  }

 private:
  Member<SubData> data_;
};

int SubClass::alive_count_ = 0;

class Mixin : public GarbageCollectedMixin {
 public:
  void Trace(blink::Visitor* visitor) override {}

  virtual char GetPayload(int i) { return padding_[i]; }

 protected:
  int padding_[8];
};

class UseMixin : public SimpleObject, public Mixin {
  USING_GARBAGE_COLLECTED_MIXIN(UseMixin)
 public:
  static UseMixin* Create() { return new UseMixin(); }

  static int trace_count_;
  void Trace(blink::Visitor* visitor) override {
    SimpleObject::Trace(visitor);
    Mixin::Trace(visitor);
    ++trace_count_;
  }

 private:
  UseMixin() {
    // Verify that WTF::IsGarbageCollectedType<> works as expected for mixins.
    static_assert(WTF::IsGarbageCollectedType<UseMixin>::value,
                  "IsGarbageCollectedType<> sanity check failed for GC mixin.");
    trace_count_ = 0;
  }
};

int UseMixin::trace_count_ = 0;

class VectorObject {
  DISALLOW_NEW();

 public:
  VectorObject() { value_ = SimpleFinalizedObject::Create(); }

  void Trace(blink::Visitor* visitor) { visitor->Trace(value_); }

 private:
  Member<SimpleFinalizedObject> value_;
};

class VectorObjectInheritedTrace : public VectorObject {};

class VectorObjectNoTrace {
  DISALLOW_NEW();

 public:
  VectorObjectNoTrace() { value_ = SimpleFinalizedObject::Create(); }

 private:
  Member<SimpleFinalizedObject> value_;
};

class TerminatedArrayItem {
  DISALLOW_NEW();

 public:
  TerminatedArrayItem(IntWrapper* payload)
      : payload_(payload), is_last_(false) {}

  void Trace(blink::Visitor* visitor) { visitor->Trace(payload_); }

  bool IsLastInArray() const { return is_last_; }
  void SetLastInArray(bool value) { is_last_ = value; }

  IntWrapper* Payload() const { return payload_; }

 private:
  Member<IntWrapper> payload_;
  bool is_last_;
};

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::TerminatedArrayItem);
WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::VectorObject);
WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(
    blink::VectorObjectInheritedTrace);
WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::VectorObjectNoTrace);

namespace blink {

class OneKiloByteObject : public GarbageCollectedFinalized<OneKiloByteObject> {
 public:
  ~OneKiloByteObject() { destructor_calls_++; }
  char* Data() { return data_; }
  void Trace(blink::Visitor* visitor) {}
  static int destructor_calls_;

 private:
  static const size_t kLength = 1024;
  char data_[kLength];
};

int OneKiloByteObject::destructor_calls_ = 0;

class DynamicallySizedObject : public GarbageCollected<DynamicallySizedObject> {
 public:
  static DynamicallySizedObject* Create(size_t size) {
    void* slot = ThreadHeap::Allocate<DynamicallySizedObject>(size);
    return new (slot) DynamicallySizedObject();
  }

  void* operator new(std::size_t, void* location) { return location; }

  uint8_t Get(int i) { return *(reinterpret_cast<uint8_t*>(this) + i); }

  void Trace(blink::Visitor* visitor) {}

 private:
  DynamicallySizedObject() = default;
};

class FinalizationAllocator
    : public GarbageCollectedFinalized<FinalizationAllocator> {
 public:
  FinalizationAllocator(Persistent<IntWrapper>* wrapper) : wrapper_(wrapper) {}

  ~FinalizationAllocator() {
    for (int i = 0; i < 10; ++i)
      *wrapper_ = IntWrapper::Create(42);
    for (int i = 0; i < 512; ++i)
      new OneKiloByteObject();
    for (int i = 0; i < 32; ++i)
      LargeHeapObject::Create();
  }

  void Trace(blink::Visitor* visitor) {}

 private:
  Persistent<IntWrapper>* wrapper_;
};

class PreFinalizationAllocator
    : public GarbageCollectedFinalized<PreFinalizationAllocator> {
  USING_PRE_FINALIZER(PreFinalizationAllocator, Dispose);

 public:
  PreFinalizationAllocator(Persistent<IntWrapper>* wrapper)
      : wrapper_(wrapper) {}

  void Dispose() {
    for (int i = 0; i < 10; ++i)
      *wrapper_ = IntWrapper::Create(42);
    for (int i = 0; i < 512; ++i)
      new OneKiloByteObject();
    for (int i = 0; i < 32; ++i)
      LargeHeapObject::Create();
  }

  void Trace(blink::Visitor* visitor) {}

 private:
  Persistent<IntWrapper>* wrapper_;
};

class PreFinalizerBackingShrinkForbidden
    : public GarbageCollectedFinalized<PreFinalizerBackingShrinkForbidden> {
  USING_PRE_FINALIZER(PreFinalizerBackingShrinkForbidden, Dispose);

 public:
  PreFinalizerBackingShrinkForbidden() {
    for (int i = 0; i < 32; ++i) {
      vector_.push_back(new IntWrapper(i));
    }
    EXPECT_LT(31ul, vector_.capacity());

    for (int i = 0; i < 32; ++i) {
      map_.insert(i + 1, new IntWrapper(i + 1));
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

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(vector_);
    visitor->Trace(map_);
  }

 private:
  HeapVector<Member<IntWrapper>> vector_;
  HeapHashMap<int, Member<IntWrapper>> map_;
};

TEST(HeapTest, PreFinalizerBackingShrinkForbidden) {
  new PreFinalizerBackingShrinkForbidden();
  PreciselyCollectGarbage();
}

class PreFinalizerVectorBackingExpandForbidden
    : public GarbageCollectedFinalized<
          PreFinalizerVectorBackingExpandForbidden> {
  USING_PRE_FINALIZER(PreFinalizerVectorBackingExpandForbidden, Dispose);

 public:
  PreFinalizerVectorBackingExpandForbidden() {
    vector_.push_back(new IntWrapper(1));
  }

  void Dispose() { EXPECT_DEATH(Test(), ""); }

  void Test() {
    // vector_'s backing will need to expand.
    for (int i = 0; i < 32; ++i) {
      vector_.push_back(nullptr);
    }
  }

  void Trace(blink::Visitor* visitor) { visitor->Trace(vector_); }

 private:
  HeapVector<Member<IntWrapper>> vector_;
};

TEST(HeapDeathTest, PreFinalizerVectorBackingExpandForbidden) {
  new PreFinalizerVectorBackingExpandForbidden();
  PreciselyCollectGarbage();
}

class PreFinalizerHashTableBackingExpandForbidden
    : public GarbageCollectedFinalized<
          PreFinalizerHashTableBackingExpandForbidden> {
  USING_PRE_FINALIZER(PreFinalizerHashTableBackingExpandForbidden, Dispose);

 public:
  PreFinalizerHashTableBackingExpandForbidden() {
    map_.insert(123, new IntWrapper(123));
  }

  void Dispose() { EXPECT_DEATH(Test(), ""); }

  void Test() {
    // map_'s backing will need to expand.
    for (int i = 1; i < 32; ++i) {
      map_.insert(i, nullptr);
    }
  }

  void Trace(blink::Visitor* visitor) { visitor->Trace(map_); }

 private:
  HeapHashMap<int, Member<IntWrapper>> map_;
};

TEST(HeapDeathTest, PreFinalizerHashTableBackingExpandForbidden) {
  new PreFinalizerHashTableBackingExpandForbidden();
  PreciselyCollectGarbage();
}

class LargeMixin : public GarbageCollected<LargeMixin>, public Mixin {
  USING_GARBAGE_COLLECTED_MIXIN(LargeMixin);

 private:
  char data[65536];
};

TEST(HeapDeathTest, LargeGarbageCollectedMixin) {
  EXPECT_DEATH(new LargeMixin(), "");
}

TEST(HeapTest, Transition) {
  {
    RefCountedAndGarbageCollected::destructor_calls_ = 0;
    Persistent<RefCountedAndGarbageCollected> ref_counted =
        RefCountedAndGarbageCollected::Create();
    PreciselyCollectGarbage();
    EXPECT_EQ(0, RefCountedAndGarbageCollected::destructor_calls_);
  }
  PreciselyCollectGarbage();
  EXPECT_EQ(1, RefCountedAndGarbageCollected::destructor_calls_);
  RefCountedAndGarbageCollected::destructor_calls_ = 0;

  Persistent<PointsBack> points_back1 = PointsBack::Create();
  Persistent<PointsBack> points_back2 = PointsBack::Create();
  Persistent<SuperClass> super_class = SuperClass::Create(points_back1);
  Persistent<SubClass> sub_class = SubClass::Create(points_back2);
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

TEST(HeapTest, Threading) {
  ThreadedHeapTester::Test();
}

TEST(HeapTest, ThreadedWeakness) {
  ThreadedWeaknessTester::Test();
}

TEST(HeapTest, ThreadPersistent) {
  ThreadPersistentHeapTester::Test();
}

TEST(HeapTest, BasicFunctionality) {
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

  uint8_t* address = reinterpret_cast<uint8_t*>(
      ThreadHeap::Allocate<DynamicallySizedObject>(100));
  for (int i = 0; i < 100; i++)
    address[i] = i;
  address = reinterpret_cast<uint8_t*>(
      ThreadHeap::Reallocate<DynamicallySizedObject>(address, 100000));
  for (int i = 0; i < 100; i++)
    EXPECT_EQ(address[i], i);
  address = reinterpret_cast<uint8_t*>(
      ThreadHeap::Reallocate<DynamicallySizedObject>(address, 50));
  for (int i = 0; i < 50; i++)
    EXPECT_EQ(address[i], i);
  // This should be equivalent to free(address).
  EXPECT_EQ(reinterpret_cast<uintptr_t>(
                ThreadHeap::Reallocate<DynamicallySizedObject>(address, 0)),
            0ul);
  // This should be equivalent to malloc(0).
  EXPECT_EQ(reinterpret_cast<uintptr_t>(
                ThreadHeap::Reallocate<DynamicallySizedObject>(nullptr, 0)),
            0ul);
}

TEST(HeapTest, SimpleAllocation) {
  ThreadHeap& heap = ThreadState::Current()->Heap();
  ClearOutOldGarbage();
  EXPECT_EQ(0ul, heap.ObjectPayloadSizeForTesting());

  // Allocate an object in the heap.
  HeapAllocatedArray* array = new HeapAllocatedArray();
  EXPECT_TRUE(heap.ObjectPayloadSizeForTesting() >= sizeof(HeapAllocatedArray));

  // Sanity check of the contents in the heap.
  EXPECT_EQ(0, array->at(0));
  EXPECT_EQ(42, array->at(42));
  EXPECT_EQ(0, array->at(128));
  EXPECT_EQ(999 % 128, array->at(999));
}

TEST(HeapTest, SimplePersistent) {
  Persistent<TraceCounter> trace_counter = TraceCounter::Create();
  EXPECT_EQ(0, trace_counter->TraceCount());
  PreciselyCollectGarbage();
  int saved_trace_count = trace_counter->TraceCount();
  EXPECT_LT(0, saved_trace_count);

  Persistent<ClassWithMember> class_with_member = ClassWithMember::Create();
  EXPECT_EQ(0, class_with_member->TraceCount());
  PreciselyCollectGarbage();
  EXPECT_LT(0, class_with_member->TraceCount());
  EXPECT_LT(saved_trace_count, trace_counter->TraceCount());
}

TEST(HeapTest, SimpleFinalization) {
  {
    Persistent<SimpleFinalizedObject> finalized =
        SimpleFinalizedObject::Create();
    EXPECT_EQ(0, SimpleFinalizedObject::destructor_calls_);
    PreciselyCollectGarbage();
    EXPECT_EQ(0, SimpleFinalizedObject::destructor_calls_);
  }

  PreciselyCollectGarbage();
  EXPECT_EQ(1, SimpleFinalizedObject::destructor_calls_);
}

#if DCHECK_IS_ON() || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER)
TEST(HeapTest, FreelistReuse) {
  ClearOutOldGarbage();

  for (int i = 0; i < 100; i++)
    new IntWrapper(i);
  IntWrapper* p1 = new IntWrapper(100);
  PreciselyCollectGarbage();
  // In non-production builds, we delay reusing freed memory for at least
  // one GC cycle.
  for (int i = 0; i < 100; i++) {
    IntWrapper* p2 = new IntWrapper(i);
    EXPECT_NE(p1, p2);
  }

  PreciselyCollectGarbage();
  PreciselyCollectGarbage();
  // Now the freed memory in the first GC should be reused.
  bool reused_memory_found = false;
  for (int i = 0; i < 10000; i++) {
    IntWrapper* p2 = new IntWrapper(i);
    if (p1 == p2) {
      reused_memory_found = true;
      break;
    }
  }
  EXPECT_TRUE(reused_memory_found);
}
#endif

TEST(HeapTest, LazySweepingPages) {
  ClearOutOldGarbage();

  SimpleFinalizedObject::destructor_calls_ = 0;
  EXPECT_EQ(0, SimpleFinalizedObject::destructor_calls_);
  for (int i = 0; i < 1000; i++)
    SimpleFinalizedObject::Create();
  ThreadState::Current()->CollectGarbage(
      BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
      BlinkGC::kLazySweeping, BlinkGC::GCReason::kForcedGC);
  EXPECT_EQ(0, SimpleFinalizedObject::destructor_calls_);
  for (int i = 0; i < 10000; i++)
    SimpleFinalizedObject::Create();
  EXPECT_EQ(1000, SimpleFinalizedObject::destructor_calls_);
  PreciselyCollectGarbage();
  EXPECT_EQ(11000, SimpleFinalizedObject::destructor_calls_);
}

TEST(HeapTest, LazySweepingLargeObjectPages) {
  ClearOutOldGarbage();

  // Create free lists that can be reused for IntWrappers created in
  // LargeHeapObject::create().
  Persistent<IntWrapper> p1 = new IntWrapper(1);
  for (int i = 0; i < 100; i++) {
    new IntWrapper(i);
  }
  Persistent<IntWrapper> p2 = new IntWrapper(2);
  PreciselyCollectGarbage();
  PreciselyCollectGarbage();

  LargeHeapObject::destructor_calls_ = 0;
  EXPECT_EQ(0, LargeHeapObject::destructor_calls_);
  for (int i = 0; i < 10; i++)
    LargeHeapObject::Create();
  ThreadState::Current()->CollectGarbage(
      BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
      BlinkGC::kLazySweeping, BlinkGC::GCReason::kForcedGC);
  EXPECT_EQ(0, LargeHeapObject::destructor_calls_);
  for (int i = 0; i < 10; i++) {
    LargeHeapObject::Create();
    EXPECT_EQ(i + 1, LargeHeapObject::destructor_calls_);
  }
  LargeHeapObject::Create();
  LargeHeapObject::Create();
  EXPECT_EQ(10, LargeHeapObject::destructor_calls_);
  ThreadState::Current()->CollectGarbage(
      BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
      BlinkGC::kLazySweeping, BlinkGC::GCReason::kForcedGC);
  EXPECT_EQ(10, LargeHeapObject::destructor_calls_);
  PreciselyCollectGarbage();
  EXPECT_EQ(22, LargeHeapObject::destructor_calls_);
}

class SimpleFinalizedEagerObjectBase
    : public GarbageCollectedFinalized<SimpleFinalizedEagerObjectBase> {
 public:
  virtual ~SimpleFinalizedEagerObjectBase() = default;
  void Trace(blink::Visitor* visitor) {}

  EAGERLY_FINALIZE();

 protected:
  SimpleFinalizedEagerObjectBase() = default;
};

class SimpleFinalizedEagerObject : public SimpleFinalizedEagerObjectBase {
 public:
  static SimpleFinalizedEagerObject* Create() {
    return new SimpleFinalizedEagerObject();
  }

  ~SimpleFinalizedEagerObject() override { ++destructor_calls_; }

  static int destructor_calls_;

 private:
  SimpleFinalizedEagerObject() = default;
};

template <typename T>
class ParameterizedButEmpty {
 public:
  EAGERLY_FINALIZE();
};

class SimpleFinalizedObjectInstanceOfTemplate final
    : public GarbageCollectedFinalized<SimpleFinalizedObjectInstanceOfTemplate>,
      public ParameterizedButEmpty<SimpleFinalizedObjectInstanceOfTemplate> {
 public:
  static SimpleFinalizedObjectInstanceOfTemplate* Create() {
    return new SimpleFinalizedObjectInstanceOfTemplate();
  }
  ~SimpleFinalizedObjectInstanceOfTemplate() { ++destructor_calls_; }

  void Trace(blink::Visitor* visitor) {}

  static int destructor_calls_;

 private:
  SimpleFinalizedObjectInstanceOfTemplate() = default;
};

int SimpleFinalizedEagerObject::destructor_calls_ = 0;
int SimpleFinalizedObjectInstanceOfTemplate::destructor_calls_ = 0;

TEST(HeapTest, EagerlySweepingPages) {
  ClearOutOldGarbage();

  SimpleFinalizedObject::destructor_calls_ = 0;
  SimpleFinalizedEagerObject::destructor_calls_ = 0;
  SimpleFinalizedObjectInstanceOfTemplate::destructor_calls_ = 0;
  EXPECT_EQ(0, SimpleFinalizedObject::destructor_calls_);
  EXPECT_EQ(0, SimpleFinalizedEagerObject::destructor_calls_);
  for (int i = 0; i < 1000; i++)
    SimpleFinalizedObject::Create();
  for (int i = 0; i < 100; i++)
    SimpleFinalizedEagerObject::Create();
  for (int i = 0; i < 100; i++)
    SimpleFinalizedObjectInstanceOfTemplate::Create();
  ThreadState::Current()->CollectGarbage(
      BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
      BlinkGC::kLazySweeping, BlinkGC::GCReason::kForcedGC);
  EXPECT_EQ(0, SimpleFinalizedObject::destructor_calls_);
  EXPECT_EQ(100, SimpleFinalizedEagerObject::destructor_calls_);
  EXPECT_EQ(100, SimpleFinalizedObjectInstanceOfTemplate::destructor_calls_);
}

TEST(HeapTest, Finalization) {
  {
    HeapTestSubClass* t1 = HeapTestSubClass::Create();
    HeapTestSubClass* t2 = HeapTestSubClass::Create();
    HeapTestSuperClass* t3 = HeapTestSuperClass::Create();
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

TEST(HeapTest, TypedArenaSanity) {
  // We use TraceCounter for allocating an object on the general heap.
  Persistent<TraceCounter> general_heap_object = TraceCounter::Create();
  Persistent<IntNode> typed_heap_object = IntNode::Create(0);
  EXPECT_NE(PageFromObject(general_heap_object.Get()),
            PageFromObject(typed_heap_object.Get()));
}

TEST(HeapTest, NoAllocation) {
  ThreadState* state = ThreadState::Current();
  EXPECT_TRUE(state->IsAllocationAllowed());
  {
    // Disallow allocation
    ThreadState::NoAllocationScope no_allocation_scope(state);
    EXPECT_FALSE(state->IsAllocationAllowed());
  }
  EXPECT_TRUE(state->IsAllocationAllowed());
}

TEST(HeapTest, Members) {
  Bar::live_ = 0;
  {
    Persistent<Baz> h1;
    Persistent<Baz> h2;
    {
      h1 = Baz::Create(Bar::Create());
      PreciselyCollectGarbage();
      EXPECT_EQ(1u, Bar::live_);
      h2 = Baz::Create(Bar::Create());
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

TEST(HeapTest, MarkTest) {
  {
    Bar::live_ = 0;
    Persistent<Bar> bar = Bar::Create();
#if DCHECK_IS_ON()
    DCHECK(ThreadState::Current()->Heap().FindPageFromAddress(bar));
#endif
    EXPECT_EQ(1u, Bar::live_);
    {
      Foo* foo = Foo::Create(bar);
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

TEST(HeapTest, DeepTest) {
  const unsigned kDepth = 100000;
  Bar::live_ = 0;
  {
    Bar* bar = Bar::Create();
#if DCHECK_IS_ON()
    DCHECK(ThreadState::Current()->Heap().FindPageFromAddress(bar));
#endif
    Foo* foo = Foo::Create(bar);
#if DCHECK_IS_ON()
    DCHECK(ThreadState::Current()->Heap().FindPageFromAddress(foo));
#endif
    EXPECT_EQ(2u, Bar::live_);
    for (unsigned i = 0; i < kDepth; i++) {
      Foo* foo2 = Foo::Create(foo);
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

TEST(HeapTest, WideTest) {
  Bar::live_ = 0;
  {
    Bars* bars = Bars::Create();
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

TEST(HeapTest, HashMapOfMembers) {
  ThreadHeap& heap = ThreadState::Current()->Heap();
  IntWrapper::destructor_calls_ = 0;

  ClearOutOldGarbage();
  size_t initial_object_payload_size = heap.ObjectPayloadSizeForTesting();
  {
    typedef HeapHashMap<Member<IntWrapper>, Member<IntWrapper>,
                        DefaultHash<Member<IntWrapper>>::Hash,
                        HashTraits<Member<IntWrapper>>,
                        HashTraits<Member<IntWrapper>>>
        HeapObjectIdentityMap;

    Persistent<HeapObjectIdentityMap> map = new HeapObjectIdentityMap();

    map->clear();
    size_t after_set_was_created = heap.ObjectPayloadSizeForTesting();
    EXPECT_TRUE(after_set_was_created > initial_object_payload_size);

    PreciselyCollectGarbage();
    size_t after_gc = heap.ObjectPayloadSizeForTesting();
    EXPECT_EQ(after_gc, after_set_was_created);

    // If the additions below cause garbage collections, these
    // pointers should be found by conservative stack scanning.
    IntWrapper* one(IntWrapper::Create(1));
    IntWrapper* another_one(IntWrapper::Create(1));

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
      IntWrapper* i_wrapper(IntWrapper::Create(i));
      IntWrapper* i_squared(IntWrapper::Create(i * i));
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

TEST(HeapTest, NestedAllocation) {
  ThreadHeap& heap = ThreadState::Current()->Heap();
  ClearOutOldGarbage();
  size_t initial_object_payload_size = heap.ObjectPayloadSizeForTesting();
  {
    Persistent<ConstructorAllocation> constructor_allocation =
        ConstructorAllocation::Create();
  }
  ClearOutOldGarbage();
  size_t after_free = heap.ObjectPayloadSizeForTesting();
  EXPECT_TRUE(initial_object_payload_size == after_free);
}

TEST(HeapTest, LargeHeapObjects) {
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
    Persistent<LargeHeapObject> object = LargeHeapObject::Create();
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
        object = LargeHeapObject::Create();
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
TEST(HeapTest, MAYBE_LargeHashMap) {
  ClearOutOldGarbage();

  // Try to allocate a HashTable larger than kMaxHeapObjectSize
  // (crbug.com/597953).
  wtf_size_t size = kMaxHeapObjectSize /
                    sizeof(HeapHashMap<int, Member<IntWrapper>>::ValueType);
  Persistent<HeapHashMap<int, Member<IntWrapper>>> map =
      new HeapHashMap<int, Member<IntWrapper>>();
  map->ReserveCapacityForSize(size);
  EXPECT_LE(size, map->Capacity());
}

TEST(HeapTest, LargeVector) {
  ClearOutOldGarbage();

  // Try to allocate a HeapVectors larger than kMaxHeapObjectSize
  // (crbug.com/597953).
  wtf_size_t size = kMaxHeapObjectSize / sizeof(int);
  Persistent<HeapVector<int>> vector = new HeapVector<int>(size);
  EXPECT_LE(size, vector->capacity());
}

typedef std::pair<Member<IntWrapper>, int> PairWrappedUnwrapped;
typedef std::pair<int, Member<IntWrapper>> PairUnwrappedWrapped;
typedef std::pair<WeakMember<IntWrapper>, Member<IntWrapper>> PairWeakStrong;
typedef std::pair<Member<IntWrapper>, WeakMember<IntWrapper>> PairStrongWeak;
typedef std::pair<WeakMember<IntWrapper>, int> PairWeakUnwrapped;
typedef std::pair<int, WeakMember<IntWrapper>> PairUnwrappedWeak;

class Container : public GarbageCollected<Container> {
 public:
  static Container* Create() { return new Container(); }
  HeapHashMap<Member<IntWrapper>, Member<IntWrapper>> map;
  HeapHashSet<Member<IntWrapper>> set;
  HeapHashSet<Member<IntWrapper>> set2;
  HeapHashCountedSet<Member<IntWrapper>> set3;
  HeapVector<Member<IntWrapper>, 2> vector;
  HeapVector<PairWrappedUnwrapped, 2> vector_wu;
  HeapVector<PairUnwrappedWrapped, 2> vector_uw;
  HeapDeque<Member<IntWrapper>, 0> deque;
  HeapDeque<PairWrappedUnwrapped, 0> deque_wu;
  HeapDeque<PairUnwrappedWrapped, 0> deque_uw;
  void Trace(blink::Visitor* visitor) {
    visitor->Trace(map);
    visitor->Trace(set);
    visitor->Trace(set2);
    visitor->Trace(set3);
    visitor->Trace(vector);
    visitor->Trace(vector_wu);
    visitor->Trace(vector_uw);
    visitor->Trace(deque);
    visitor->Trace(deque_wu);
    visitor->Trace(deque_uw);
  }
};

struct NeedsTracingTrait {
  explicit NeedsTracingTrait(IntWrapper* wrapper) : wrapper_(wrapper) {}
  void Trace(blink::Visitor* visitor) { visitor->Trace(wrapper_); }
  Member<IntWrapper> wrapper_;
};

TEST(HeapTest, HeapVectorFilledWithValue) {
  IntWrapper* val = IntWrapper::Create(1);
  HeapVector<Member<IntWrapper>> vector(10, val);
  EXPECT_EQ(10u, vector.size());
  for (wtf_size_t i = 0; i < vector.size(); i++)
    EXPECT_EQ(val, vector[i]);
}

TEST(HeapTest, HeapVectorWithInlineCapacity) {
  IntWrapper* one = IntWrapper::Create(1);
  IntWrapper* two = IntWrapper::Create(2);
  IntWrapper* three = IntWrapper::Create(3);
  IntWrapper* four = IntWrapper::Create(4);
  IntWrapper* five = IntWrapper::Create(5);
  IntWrapper* six = IntWrapper::Create(6);
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

TEST(HeapTest, HeapVectorShrinkCapacity) {
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

TEST(HeapTest, HeapVectorShrinkInlineCapacity) {
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

TEST(HeapTest, HeapVectorOnStackLargeObjectPageSized) {
  ClearOutOldGarbage();
  // Try to allocate a vector of a size that will end exactly where the
  // LargeObjectPage ends.
  using Container = HeapVector<Member<IntWrapper>>;
  Container vector;
  wtf_size_t size =
      (kLargeObjectSizeThreshold + kBlinkGuardPageSize -
       static_cast<wtf_size_t>(LargeObjectPage::PageHeaderSize()) -
       sizeof(HeapObjectHeader)) /
      sizeof(Container::ValueType);
  vector.ReserveCapacity(size);
  for (unsigned i = 0; i < size; ++i)
    vector.push_back(IntWrapper::Create(i));
  ConservativelyCollectGarbage();
}

template <typename T, wtf_size_t inlineCapacity, typename U>
bool DequeContains(HeapDeque<T, inlineCapacity>& deque, U u) {
  typedef typename HeapDeque<T, inlineCapacity>::iterator iterator;
  for (iterator it = deque.begin(); it != deque.end(); ++it) {
    if (*it == u)
      return true;
  }
  return false;
}

TEST(HeapTest, HeapCollectionTypes) {
  IntWrapper::destructor_calls_ = 0;

  typedef HeapHashMap<Member<IntWrapper>, Member<IntWrapper>> MemberMember;
  typedef HeapHashMap<Member<IntWrapper>, int> MemberPrimitive;
  typedef HeapHashMap<int, Member<IntWrapper>> PrimitiveMember;

  typedef HeapHashSet<Member<IntWrapper>> MemberSet;
  typedef HeapHashCountedSet<Member<IntWrapper>> MemberCountedSet;

  typedef HeapVector<Member<IntWrapper>, 2> MemberVector;
  typedef HeapDeque<Member<IntWrapper>, 0> MemberDeque;

  typedef HeapVector<PairWrappedUnwrapped, 2> VectorWU;
  typedef HeapVector<PairUnwrappedWrapped, 2> VectorUW;
  typedef HeapDeque<PairWrappedUnwrapped, 0> DequeWU;
  typedef HeapDeque<PairUnwrappedWrapped, 0> DequeUW;

  Persistent<MemberMember> member_member = new MemberMember();
  Persistent<MemberMember> member_member2 = new MemberMember();
  Persistent<MemberMember> member_member3 = new MemberMember();
  Persistent<MemberPrimitive> member_primitive = new MemberPrimitive();
  Persistent<PrimitiveMember> primitive_member = new PrimitiveMember();
  Persistent<MemberSet> set = new MemberSet();
  Persistent<MemberSet> set2 = new MemberSet();
  Persistent<MemberCountedSet> set3 = new MemberCountedSet();
  Persistent<MemberVector> vector = new MemberVector();
  Persistent<MemberVector> vector2 = new MemberVector();
  Persistent<VectorWU> vector_wu = new VectorWU();
  Persistent<VectorWU> vector_wu2 = new VectorWU();
  Persistent<VectorUW> vector_uw = new VectorUW();
  Persistent<VectorUW> vector_uw2 = new VectorUW();
  Persistent<MemberDeque> deque = new MemberDeque();
  Persistent<MemberDeque> deque2 = new MemberDeque();
  Persistent<DequeWU> deque_wu = new DequeWU();
  Persistent<DequeWU> deque_wu2 = new DequeWU();
  Persistent<DequeUW> deque_uw = new DequeUW();
  Persistent<DequeUW> deque_uw2 = new DequeUW();
  Persistent<Container> container = Container::Create();

  ClearOutOldGarbage();
  {
    Persistent<IntWrapper> one(IntWrapper::Create(1));
    Persistent<IntWrapper> two(IntWrapper::Create(2));
    Persistent<IntWrapper> one_b(IntWrapper::Create(1));
    Persistent<IntWrapper> two_b(IntWrapper::Create(2));
    Persistent<IntWrapper> one_c(IntWrapper::Create(1));
    Persistent<IntWrapper> one_d(IntWrapper::Create(1));
    Persistent<IntWrapper> one_e(IntWrapper::Create(1));
    Persistent<IntWrapper> one_f(IntWrapper::Create(1));
    {
      IntWrapper* three_b(IntWrapper::Create(3));
      IntWrapper* three_c(IntWrapper::Create(3));
      IntWrapper* three_d(IntWrapper::Create(3));
      IntWrapper* three_e(IntWrapper::Create(3));
      IntWrapper* three_f(IntWrapper::Create(3));
      IntWrapper* three(IntWrapper::Create(3));
      IntWrapper* four_b(IntWrapper::Create(4));
      IntWrapper* four_c(IntWrapper::Create(4));
      IntWrapper* four_d(IntWrapper::Create(4));
      IntWrapper* four_e(IntWrapper::Create(4));
      IntWrapper* four_f(IntWrapper::Create(4));
      IntWrapper* four(IntWrapper::Create(4));
      IntWrapper* five_c(IntWrapper::Create(5));
      IntWrapper* five_d(IntWrapper::Create(5));
      IntWrapper* five_e(IntWrapper::Create(5));
      IntWrapper* five_f(IntWrapper::Create(5));

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
      deque_wu->push_back(PairWrappedUnwrapped(&*one_e, 42));
      vector_wu2->push_back(PairWrappedUnwrapped(&*three_c, 43));
      vector_wu2->push_back(PairWrappedUnwrapped(&*four_c, 44));
      vector_wu2->push_back(PairWrappedUnwrapped(&*five_c, 45));
      deque_wu2->push_back(PairWrappedUnwrapped(&*three_e, 43));
      deque_wu2->push_back(PairWrappedUnwrapped(&*four_e, 44));
      deque_wu2->push_back(PairWrappedUnwrapped(&*five_e, 45));
      vector_uw->push_back(PairUnwrappedWrapped(1, &*one_d));
      vector_uw2->push_back(PairUnwrappedWrapped(103, &*three_d));
      vector_uw2->push_back(PairUnwrappedWrapped(104, &*four_d));
      vector_uw2->push_back(PairUnwrappedWrapped(105, &*five_d));
      deque_uw->push_back(PairUnwrappedWrapped(1, &*one_f));
      deque_uw2->push_back(PairUnwrappedWrapped(103, &*three_f));
      deque_uw2->push_back(PairUnwrappedWrapped(104, &*four_f));
      deque_uw2->push_back(PairUnwrappedWrapped(105, &*five_f));

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
      EXPECT_EQ(1u, deque_wu->size());
      EXPECT_EQ(3u, deque_wu2->size());
      EXPECT_EQ(1u, deque_uw->size());
      EXPECT_EQ(3u, deque_uw2->size());

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

      DequeWU& c_deque_wu = container->deque_wu;
      c_deque_wu.Swap(*deque_wu.Get());
      deque_wu2->Swap(c_deque_wu);
      deque_wu->Swap(c_deque_wu);

      DequeUW& c_deque_uw = container->deque_uw;
      c_deque_uw.Swap(*deque_uw.Get());
      deque_uw2->Swap(c_deque_uw);
      deque_uw->Swap(c_deque_uw);

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
      EXPECT_TRUE(
          DequeContains(*deque_wu, PairWrappedUnwrapped(&*three_e, 43)));
      EXPECT_TRUE(DequeContains(*deque_wu, PairWrappedUnwrapped(&*four_e, 44)));
      EXPECT_TRUE(DequeContains(*deque_wu, PairWrappedUnwrapped(&*five_e, 45)));
      EXPECT_TRUE(DequeContains(*deque_wu2, PairWrappedUnwrapped(&*one_e, 42)));
      EXPECT_FALSE(
          DequeContains(*deque_wu2, PairWrappedUnwrapped(&*three_e, 43)));
      EXPECT_TRUE(
          DequeContains(*deque_uw, PairUnwrappedWrapped(103, &*three_f)));
      EXPECT_TRUE(
          DequeContains(*deque_uw, PairUnwrappedWrapped(104, &*four_f)));
      EXPECT_TRUE(
          DequeContains(*deque_uw, PairUnwrappedWrapped(105, &*five_f)));
      EXPECT_TRUE(DequeContains(*deque_uw2, PairUnwrappedWrapped(1, &*one_f)));
      EXPECT_FALSE(
          DequeContains(*deque_uw2, PairUnwrappedWrapped(103, &*three_f)));
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
    EXPECT_EQ(3u, deque_uw->size());
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
  EXPECT_EQ(3u, deque_wu->size());
  EXPECT_EQ(1u, deque_wu2->size());
  EXPECT_EQ(3u, deque_uw->size());
  EXPECT_EQ(1u, deque_uw2->size());
}

TEST(HeapTest, PersistentVector) {
  IntWrapper::destructor_calls_ = 0;

  typedef Vector<Persistent<IntWrapper>> PersistentVector;

  Persistent<IntWrapper> one(IntWrapper::Create(1));
  Persistent<IntWrapper> two(IntWrapper::Create(2));
  Persistent<IntWrapper> three(IntWrapper::Create(3));
  Persistent<IntWrapper> four(IntWrapper::Create(4));
  Persistent<IntWrapper> five(IntWrapper::Create(5));
  Persistent<IntWrapper> six(IntWrapper::Create(6));
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

TEST(HeapTest, CrossThreadPersistentVector) {
  IntWrapper::destructor_calls_ = 0;

  typedef Vector<CrossThreadPersistent<IntWrapper>> CrossThreadPersistentVector;

  CrossThreadPersistent<IntWrapper> one(IntWrapper::Create(1));
  CrossThreadPersistent<IntWrapper> two(IntWrapper::Create(2));
  CrossThreadPersistent<IntWrapper> three(IntWrapper::Create(3));
  CrossThreadPersistent<IntWrapper> four(IntWrapper::Create(4));
  CrossThreadPersistent<IntWrapper> five(IntWrapper::Create(5));
  CrossThreadPersistent<IntWrapper> six(IntWrapper::Create(6));
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

TEST(HeapTest, PersistentSet) {
  IntWrapper::destructor_calls_ = 0;

  typedef HashSet<Persistent<IntWrapper>> PersistentSet;

  IntWrapper* one_raw = IntWrapper::Create(1);
  Persistent<IntWrapper> one(one_raw);
  Persistent<IntWrapper> one2(one_raw);
  Persistent<IntWrapper> two(IntWrapper::Create(2));
  Persistent<IntWrapper> three(IntWrapper::Create(3));
  Persistent<IntWrapper> four(IntWrapper::Create(4));
  Persistent<IntWrapper> five(IntWrapper::Create(5));
  Persistent<IntWrapper> six(IntWrapper::Create(6));
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

TEST(HeapTest, CrossThreadPersistentSet) {
  IntWrapper::destructor_calls_ = 0;

  typedef HashSet<CrossThreadPersistent<IntWrapper>> CrossThreadPersistentSet;

  IntWrapper* one_raw = IntWrapper::Create(1);
  CrossThreadPersistent<IntWrapper> one(one_raw);
  CrossThreadPersistent<IntWrapper> one2(one_raw);
  CrossThreadPersistent<IntWrapper> two(IntWrapper::Create(2));
  CrossThreadPersistent<IntWrapper> three(IntWrapper::Create(3));
  CrossThreadPersistent<IntWrapper> four(IntWrapper::Create(4));
  CrossThreadPersistent<IntWrapper> five(IntWrapper::Create(5));
  CrossThreadPersistent<IntWrapper> six(IntWrapper::Create(6));
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

class NonTrivialObject final {
  DISALLOW_NEW();

 public:
  NonTrivialObject() = default;
  explicit NonTrivialObject(int num) {
    deque_.push_back(IntWrapper::Create(num));
    vector_.push_back(IntWrapper::Create(num));
  }
  void Trace(blink::Visitor* visitor) {
    visitor->Trace(deque_);
    visitor->Trace(vector_);
  }

 private:
  HeapDeque<Member<IntWrapper>> deque_;
  HeapVector<Member<IntWrapper>> vector_;
};

TEST(HeapTest, HeapHashMapWithInlinedObject) {
  HeapHashMap<int, NonTrivialObject> map;
  for (int num = 1; num < 1000; num++) {
    NonTrivialObject object(num);
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

TEST(HeapTest, HeapWeakCollectionSimple) {
  ClearOutOldGarbage();
  IntWrapper::destructor_calls_ = 0;

  Persistent<HeapVector<Member<IntWrapper>>> keep_numbers_alive =
      new HeapVector<Member<IntWrapper>>;

  typedef HeapHashMap<WeakMember<IntWrapper>, Member<IntWrapper>> WeakStrong;
  typedef HeapHashMap<Member<IntWrapper>, WeakMember<IntWrapper>> StrongWeak;
  typedef HeapHashMap<WeakMember<IntWrapper>, WeakMember<IntWrapper>> WeakWeak;
  typedef HeapHashSet<WeakMember<IntWrapper>> WeakSet;
  typedef HeapHashCountedSet<WeakMember<IntWrapper>> WeakCountedSet;

  Persistent<WeakStrong> weak_strong = new WeakStrong();
  Persistent<StrongWeak> strong_weak = new StrongWeak();
  Persistent<WeakWeak> weak_weak = new WeakWeak();
  Persistent<WeakSet> weak_set = new WeakSet();
  Persistent<WeakCountedSet> weak_counted_set = new WeakCountedSet();

  Persistent<IntWrapper> two = IntWrapper::Create(2);

  keep_numbers_alive->push_back(IntWrapper::Create(103));
  keep_numbers_alive->push_back(IntWrapper::Create(10));

  {
    weak_strong->insert(IntWrapper::Create(1), two);
    strong_weak->insert(two, IntWrapper::Create(1));
    weak_weak->insert(two, IntWrapper::Create(42));
    weak_weak->insert(IntWrapper::Create(42), two);
    weak_set->insert(IntWrapper::Create(0));
    weak_set->insert(two);
    weak_set->insert(keep_numbers_alive->at(0));
    weak_set->insert(keep_numbers_alive->at(1));
    weak_counted_set->insert(IntWrapper::Create(0));
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
  ClearOutOldGarbage();
  IntWrapper::destructor_calls_ = 0;

  Persistent<HeapVector<Member<IntWrapper>>> keep_numbers_alive =
      new HeapVector<Member<IntWrapper>>;

  Persistent<Set> set1 = new Set();
  Persistent<Set> set2 = new Set();

  const Set& const_set = *set1.Get();

  keep_numbers_alive->push_back(IntWrapper::Create(2));
  keep_numbers_alive->push_back(IntWrapper::Create(103));
  keep_numbers_alive->push_back(IntWrapper::Create(10));

  set1->insert(IntWrapper::Create(0));
  set1->insert(keep_numbers_alive->at(0));
  set1->insert(keep_numbers_alive->at(1));
  set1->insert(keep_numbers_alive->at(2));

  set2->clear();
  set2->insert(IntWrapper::Create(42));
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

  PreciselyCollectGarbage();

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

TEST(HeapTest, HeapWeakLinkedHashSet) {
  OrderedSetHelper<HeapLinkedHashSet<Member<IntWrapper>>>(true);
  OrderedSetHelper<HeapLinkedHashSet<WeakMember<IntWrapper>>>(false);
  OrderedSetHelper<HeapListHashSet<Member<IntWrapper>>>(true);
}

class ThingWithDestructor {
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
  ClearOutOldGarbage();
  ThingWithDestructor::live_things_with_destructor_ = 0;

  typedef HeapHashMap<WeakMember<IntWrapper>,
                      Member<RefCountedAndGarbageCollected>>
      RefMap;

  typedef HeapHashMap<WeakMember<IntWrapper>, ThingWithDestructor,
                      DefaultHash<WeakMember<IntWrapper>>::Hash,
                      HashTraits<WeakMember<IntWrapper>>>
      Map;

  Persistent<Map> map(new Map());
  Persistent<RefMap> ref_map(new RefMap());

  Persistent<IntWrapper> luck(IntWrapper::Create(103));

  int base_line, ref_base_line;

  {
    Map stack_map;
    RefMap stack_ref_map;

    PreciselyCollectGarbage();
    PreciselyCollectGarbage();

    stack_map.insert(IntWrapper::Create(42), ThingWithDestructor(1729));
    stack_map.insert(luck, ThingWithDestructor(8128));
    stack_ref_map.insert(IntWrapper::Create(42),
                         RefCountedAndGarbageCollected::Create());
    stack_ref_map.insert(luck, RefCountedAndGarbageCollected::Create());

    base_line = ThingWithDestructor::live_things_with_destructor_;
    ref_base_line = RefCountedAndGarbageCollected::destructor_calls_;

    // Although the heap maps are on-stack, we can't expect prompt
    // finalization of the elements, so when they go out of scope here we
    // will not necessarily have called the relevant destructors.
  }

  // The RefCountedAndGarbageCollected things need an extra GC to discover
  // that they are no longer ref counted.
  PreciselyCollectGarbage();
  PreciselyCollectGarbage();
  EXPECT_EQ(base_line - 2, ThingWithDestructor::live_things_with_destructor_);
  EXPECT_EQ(ref_base_line + 2,
            RefCountedAndGarbageCollected::destructor_calls_);

  // Now use maps kept alive with persistents. Here we don't expect any
  // destructors to be called before there have been GCs.

  map->insert(IntWrapper::Create(42), ThingWithDestructor(1729));
  map->insert(luck, ThingWithDestructor(8128));
  ref_map->insert(IntWrapper::Create(42),
                  RefCountedAndGarbageCollected::Create());
  ref_map->insert(luck, RefCountedAndGarbageCollected::Create());

  base_line = ThingWithDestructor::live_things_with_destructor_;
  ref_base_line = RefCountedAndGarbageCollected::destructor_calls_;

  luck.Clear();
  if (clear_maps) {
    map->clear();      // Clear map.
    ref_map->clear();  // Clear map.
  } else {
    map.Clear();      // Clear Persistent handle, not map.
    ref_map.Clear();  // Clear Persistent handle, not map.
    PreciselyCollectGarbage();
    PreciselyCollectGarbage();
  }

  EXPECT_EQ(base_line - 2, ThingWithDestructor::live_things_with_destructor_);

  // Need a GC to make sure that the RefCountedAndGarbageCollected thing
  // noticies it's been decremented to zero.
  PreciselyCollectGarbage();
  EXPECT_EQ(ref_base_line + 2,
            RefCountedAndGarbageCollected::destructor_calls_);
}

TEST(HeapTest, HeapMapDestructor) {
  HeapMapDestructorHelper(true);
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

template <typename WSSet, typename SWSet, typename WUSet, typename UWSet>
void WeakPairsHelper() {
  IntWrapper::destructor_calls_ = 0;

  Persistent<HeapVector<Member<IntWrapper>>> keep_numbers_alive =
      new HeapVector<Member<IntWrapper>>;

  Persistent<WSSet> weak_strong = new WSSet();
  Persistent<SWSet> strong_weak = new SWSet();
  Persistent<WUSet> weak_unwrapped = new WUSet();
  Persistent<UWSet> unwrapped_weak = new UWSet();

  Persistent<IntWrapper> two = IntWrapper::Create(2);

  weak_strong->insert(PairWeakStrong(IntWrapper::Create(1), &*two));
  weak_strong->insert(PairWeakStrong(&*two, &*two));
  strong_weak->insert(PairStrongWeak(&*two, IntWrapper::Create(1)));
  strong_weak->insert(PairStrongWeak(&*two, &*two));
  weak_unwrapped->insert(PairWeakUnwrapped(IntWrapper::Create(1), 2));
  weak_unwrapped->insert(PairWeakUnwrapped(&*two, 2));
  unwrapped_weak->insert(PairUnwrappedWeak(2, IntWrapper::Create(1)));
  unwrapped_weak->insert(PairUnwrappedWeak(2, &*two));

  CheckPairSets<WSSet, SWSet, WUSet, UWSet>(
      weak_strong, strong_weak, weak_unwrapped, unwrapped_weak, true, two);

  PreciselyCollectGarbage();
  CheckPairSets<WSSet, SWSet, WUSet, UWSet>(
      weak_strong, strong_weak, weak_unwrapped, unwrapped_weak, false, two);
}

TEST(HeapTest, HeapWeakPairs) {
  {
    typedef HeapHashSet<PairWeakStrong> WeakStrongSet;
    typedef HeapHashSet<PairWeakUnwrapped> WeakUnwrappedSet;
    typedef HeapHashSet<PairStrongWeak> StrongWeakSet;
    typedef HeapHashSet<PairUnwrappedWeak> UnwrappedWeakSet;
    WeakPairsHelper<WeakStrongSet, StrongWeakSet, WeakUnwrappedSet,
                    UnwrappedWeakSet>();
  }

  {
    typedef HeapListHashSet<PairWeakStrong> WeakStrongSet;
    typedef HeapListHashSet<PairWeakUnwrapped> WeakUnwrappedSet;
    typedef HeapListHashSet<PairStrongWeak> StrongWeakSet;
    typedef HeapListHashSet<PairUnwrappedWeak> UnwrappedWeakSet;
    WeakPairsHelper<WeakStrongSet, StrongWeakSet, WeakUnwrappedSet,
                    UnwrappedWeakSet>();
  }

  {
    typedef HeapLinkedHashSet<PairWeakStrong> WeakStrongSet;
    typedef HeapLinkedHashSet<PairWeakUnwrapped> WeakUnwrappedSet;
    typedef HeapLinkedHashSet<PairStrongWeak> StrongWeakSet;
    typedef HeapLinkedHashSet<PairUnwrappedWeak> UnwrappedWeakSet;
    WeakPairsHelper<WeakStrongSet, StrongWeakSet, WeakUnwrappedSet,
                    UnwrappedWeakSet>();
  }
}

TEST(HeapTest, HeapWeakCollectionTypes) {
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

      Persistent<WeakStrong> weak_strong = new WeakStrong();
      Persistent<StrongWeak> strong_weak = new StrongWeak();
      Persistent<WeakWeak> weak_weak = new WeakWeak();

      Persistent<WeakSet> weak_set = new WeakSet();
      Persistent<WeakOrderedSet> weak_ordered_set = new WeakOrderedSet();

      Persistent<HeapVector<Member<IntWrapper>>> keep_numbers_alive =
          new HeapVector<Member<IntWrapper>>;
      for (int i = 0; i < 128; i += 2) {
        IntWrapper* wrapped = IntWrapper::Create(i);
        IntWrapper* wrapped2 = IntWrapper::Create(i + 1);
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
            IntWrapper* wrapped = IntWrapper::Create(i);
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

TEST(HeapTest, HeapHashCountedSetToVector) {
  HeapHashCountedSet<Member<IntWrapper>> set;
  HeapVector<Member<IntWrapper>> vector;
  set.insert(new IntWrapper(1));
  set.insert(new IntWrapper(1));
  set.insert(new IntWrapper(2));

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

TEST(HeapTest, WeakHeapHashCountedSetToVector) {
  HeapHashCountedSet<WeakMember<IntWrapper>> set;
  HeapVector<Member<IntWrapper>> vector;
  set.insert(new IntWrapper(1));
  set.insert(new IntWrapper(1));
  set.insert(new IntWrapper(2));

  CopyToVector(set, vector);
  EXPECT_LE(3u, vector.size());
  for (const auto& i : vector)
    EXPECT_TRUE(i->Value() == 1 || i->Value() == 2);
}

TEST(HeapTest, RefCountedGarbageCollected) {
  RefCountedAndGarbageCollected::destructor_calls_ = 0;
  {
    scoped_refptr<RefCountedAndGarbageCollected> ref_ptr3;
    {
      Persistent<RefCountedAndGarbageCollected> persistent;
      {
        Persistent<RefCountedAndGarbageCollected> ref_ptr1 =
            RefCountedAndGarbageCollected::Create();
        Persistent<RefCountedAndGarbageCollected> ref_ptr2 =
            RefCountedAndGarbageCollected::Create();
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

TEST(HeapTest, WeakMembers) {
  Bar::live_ = 0;
  {
    Persistent<Bar> h1 = Bar::Create();
    Persistent<Weak> h4;
    Persistent<WithWeakMember> h5;
    PreciselyCollectGarbage();
    ASSERT_EQ(1u, Bar::live_);  // h1 is live.
    {
      Bar* h2 = Bar::Create();
      Bar* h3 = Bar::Create();
      h4 = Weak::Create(h2, h3);
      h5 = WithWeakMember::Create(h2, h3);
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

TEST(HeapTest, FinalizationObserver) {
  Persistent<FinalizationObserver<Observable>> o;
  {
    Observable* foo = Observable::Create(Bar::Create());
    // |o| observes |foo|.
    o = FinalizationObserver<Observable>::Create(foo);
  }
  // FinalizationObserver doesn't have a strong reference to |foo|. So |foo|
  // and its member will be collected.
  PreciselyCollectGarbage();
  EXPECT_EQ(0u, Bar::live_);
  EXPECT_TRUE(o->DidCallWillFinalize());

  FinalizationObserverWithHashMap::did_call_will_finalize_ = false;
  Observable* foo = Observable::Create(Bar::Create());
  FinalizationObserverWithHashMap::ObserverMap& map =
      FinalizationObserverWithHashMap::Observe(*foo);
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

TEST(HeapTest, PreFinalizer) {
  Observable::will_finalize_was_called_ = false;
  { Observable::Create(Bar::Create()); }
  PreciselyCollectGarbage();
  EXPECT_TRUE(Observable::will_finalize_was_called_);
}

TEST(HeapTest, PreFinalizerUnregistersItself) {
  ObservableWithPreFinalizer::dispose_was_called_ = false;
  ObservableWithPreFinalizer::Create();
  PreciselyCollectGarbage();
  EXPECT_TRUE(ObservableWithPreFinalizer::dispose_was_called_);
  // Don't crash, and assertions don't fail.
}

TEST(HeapTest, NestedPreFinalizer) {
  g_dispose_was_called_for_pre_finalizer_base = false;
  g_dispose_was_called_for_pre_finalizer_sub_class = false;
  g_dispose_was_called_for_pre_finalizer_mixin = false;
  PreFinalizerSubClass::Create();
  PreciselyCollectGarbage();
  EXPECT_TRUE(g_dispose_was_called_for_pre_finalizer_base);
  EXPECT_TRUE(g_dispose_was_called_for_pre_finalizer_sub_class);
  EXPECT_TRUE(g_dispose_was_called_for_pre_finalizer_mixin);
  // Don't crash, and assertions don't fail.
}

TEST(HeapTest, Comparisons) {
  Persistent<Bar> bar_persistent = Bar::Create();
  Persistent<Foo> foo_persistent = Foo::Create(bar_persistent);
  EXPECT_TRUE(bar_persistent != foo_persistent);
  bar_persistent = foo_persistent;
  EXPECT_TRUE(bar_persistent == foo_persistent);
}

#if DCHECK_IS_ON()
namespace {

static size_t g_check_mark_count = 0;

bool ReportMarkedPointer(HeapObjectHeader*) {
  g_check_mark_count++;
  // Do not try to mark the located heap object.
  return true;
}
}
#endif

TEST(HeapTest, CheckAndMarkPointer) {
#if DCHECK_IS_ON()
  ThreadHeap& heap = ThreadState::Current()->Heap();
  ClearOutOldGarbage();

  Vector<Address> object_addresses;
  Vector<Address> end_addresses;
  Address large_object_address;
  Address large_object_end_address;
  for (int i = 0; i < 10; i++) {
    SimpleObject* object = SimpleObject::Create();
    Address object_address = reinterpret_cast<Address>(object);
    object_addresses.push_back(object_address);
    end_addresses.push_back(object_address + sizeof(SimpleObject) - 1);
  }
  LargeHeapObject* large_object = LargeHeapObject::Create();
  large_object_address = reinterpret_cast<Address>(large_object);
  large_object_end_address = large_object_address + sizeof(LargeHeapObject) - 1;

  // This is a low-level test where we call checkAndMarkPointer. This method
  // causes the object start bitmap to be computed which requires the heap
  // to be in a consistent state (e.g. the free allocation area must be put
  // into a free list header). However when we call makeConsistentForGC it
  // also clears out the freelists so we have to rebuild those before trying
  // to allocate anything again. We do this by forcing a GC after doing the
  // checkAndMarkPointer tests.
  {
    TestGCScope scope(BlinkGC::kHeapPointersOnStack);
    MarkingVisitor visitor(ThreadState::Current(),
                           MarkingVisitor::kGlobalMarking);
    heap.address_cache()->EnableLookup();
    heap.address_cache()->Flush();
    for (wtf_size_t i = 0; i < object_addresses.size(); i++) {
      EXPECT_TRUE(heap.CheckAndMarkPointer(&visitor, object_addresses[i],
                                           ReportMarkedPointer));
      EXPECT_TRUE(heap.CheckAndMarkPointer(&visitor, end_addresses[i],
                                           ReportMarkedPointer));
    }
    EXPECT_EQ(object_addresses.size() * 2, g_check_mark_count);
    g_check_mark_count = 0;
    EXPECT_TRUE(heap.CheckAndMarkPointer(&visitor, large_object_address,
                                         ReportMarkedPointer));
    EXPECT_TRUE(heap.CheckAndMarkPointer(&visitor, large_object_end_address,
                                         ReportMarkedPointer));
    EXPECT_EQ(2ul, g_check_mark_count);
    g_check_mark_count = 0ul;
  }
  // This forces a GC without stack scanning which results in the objects
  // being collected. This will also rebuild the above mentioned freelists,
  // however we don't rely on that below since we don't have any allocations.
  ClearOutOldGarbage();
  {
    TestGCScope scope(BlinkGC::kHeapPointersOnStack);
    MarkingVisitor visitor(ThreadState::Current(),
                           MarkingVisitor::kGlobalMarking);
    heap.address_cache()->EnableLookup();
    heap.address_cache()->Flush();
    for (wtf_size_t i = 0; i < object_addresses.size(); i++) {
      // We would like to assert that checkAndMarkPointer returned false
      // here because the pointers no longer point into a valid object
      // (it's been freed by the GCs. But checkAndMarkPointer will return
      // true for any pointer that points into a heap page, regardless of
      // whether it points at a valid object (this ensures the
      // correctness of the page-based on-heap address caches), so we
      // can't make that assert.
      heap.CheckAndMarkPointer(&visitor, object_addresses[i],
                               ReportMarkedPointer);
      heap.CheckAndMarkPointer(&visitor, end_addresses[i], ReportMarkedPointer);
    }
    EXPECT_EQ(0ul, g_check_mark_count);
    heap.CheckAndMarkPointer(&visitor, large_object_address,
                             ReportMarkedPointer);
    heap.CheckAndMarkPointer(&visitor, large_object_end_address,
                             ReportMarkedPointer);
    EXPECT_EQ(0ul, g_check_mark_count);
  }
  // This round of GC is important to make sure that the object start
  // bitmap are cleared out and that the free lists are rebuild.
  ClearOutOldGarbage();
#endif
}

TEST(HeapTest, CollectionNesting) {
  ClearOutOldGarbage();
  int* key = &IntWrapper::destructor_calls_;
  IntWrapper::destructor_calls_ = 0;
  typedef HeapVector<Member<IntWrapper>> IntVector;
  typedef HeapDeque<Member<IntWrapper>> IntDeque;
  HeapHashMap<void*, IntVector>* map = new HeapHashMap<void*, IntVector>();
  HeapHashMap<void*, IntDeque>* map2 = new HeapHashMap<void*, IntDeque>();
  static_assert(WTF::IsTraceable<IntVector>::value,
                "Failed to recognize HeapVector as traceable");
  static_assert(WTF::IsTraceable<IntDeque>::value,
                "Failed to recognize HeapDeque as traceable");

  map->insert(key, IntVector());
  map2->insert(key, IntDeque());

  HeapHashMap<void*, IntVector>::iterator it = map->find(key);
  EXPECT_EQ(0u, map->at(key).size());

  HeapHashMap<void*, IntDeque>::iterator it2 = map2->find(key);
  EXPECT_EQ(0u, map2->at(key).size());

  it->value.push_back(IntWrapper::Create(42));
  EXPECT_EQ(1u, map->at(key).size());

  it2->value.push_back(IntWrapper::Create(42));
  EXPECT_EQ(1u, map2->at(key).size());

  Persistent<HeapHashMap<void*, IntVector>> keep_alive(map);
  Persistent<HeapHashMap<void*, IntDeque>> keep_alive2(map2);

  for (int i = 0; i < 100; i++) {
    map->insert(key + 1 + i, IntVector());
    map2->insert(key + 1 + i, IntDeque());
  }

  PreciselyCollectGarbage();

  EXPECT_EQ(1u, map->at(key).size());
  EXPECT_EQ(1u, map2->at(key).size());
  EXPECT_EQ(0, IntWrapper::destructor_calls_);

  keep_alive = nullptr;
  PreciselyCollectGarbage();
  EXPECT_EQ(1, IntWrapper::destructor_calls_);
}

TEST(HeapTest, GarbageCollectedMixin) {
  ClearOutOldGarbage();

  Persistent<UseMixin> usemixin = UseMixin::Create();
  EXPECT_EQ(0, UseMixin::trace_count_);
  PreciselyCollectGarbage();
  EXPECT_EQ(1, UseMixin::trace_count_);

  Persistent<Mixin> mixin = usemixin;
  usemixin = nullptr;
  PreciselyCollectGarbage();
  EXPECT_EQ(2, UseMixin::trace_count_);

  Persistent<HeapHashSet<WeakMember<Mixin>>> weak_map =
      new HeapHashSet<WeakMember<Mixin>>;
  weak_map->insert(UseMixin::Create());
  PreciselyCollectGarbage();
  EXPECT_EQ(0u, weak_map->size());
}

TEST(HeapTest, CollectionNesting2) {
  ClearOutOldGarbage();
  void* key = &IntWrapper::destructor_calls_;
  IntWrapper::destructor_calls_ = 0;
  typedef HeapHashSet<Member<IntWrapper>> IntSet;
  HeapHashMap<void*, IntSet>* map = new HeapHashMap<void*, IntSet>();

  map->insert(key, IntSet());

  HeapHashMap<void*, IntSet>::iterator it = map->find(key);
  EXPECT_EQ(0u, map->at(key).size());

  it->value.insert(IntWrapper::Create(42));
  EXPECT_EQ(1u, map->at(key).size());

  Persistent<HeapHashMap<void*, IntSet>> keep_alive(map);
  PreciselyCollectGarbage();
  EXPECT_EQ(1u, map->at(key).size());
  EXPECT_EQ(0, IntWrapper::destructor_calls_);
}

TEST(HeapTest, CollectionNesting3) {
  ClearOutOldGarbage();
  IntWrapper::destructor_calls_ = 0;
  typedef HeapVector<Member<IntWrapper>> IntVector;
  typedef HeapDeque<Member<IntWrapper>> IntDeque;
  HeapVector<IntVector>* vector = new HeapVector<IntVector>();
  HeapDeque<IntDeque>* deque = new HeapDeque<IntDeque>();

  vector->push_back(IntVector());
  deque->push_back(IntDeque());

  HeapVector<IntVector>::iterator it = vector->begin();
  HeapDeque<IntDeque>::iterator it2 = deque->begin();
  EXPECT_EQ(0u, it->size());
  EXPECT_EQ(0u, it2->size());

  it->push_back(IntWrapper::Create(42));
  it2->push_back(IntWrapper::Create(42));
  EXPECT_EQ(1u, it->size());
  EXPECT_EQ(1u, it2->size());

  Persistent<HeapVector<IntVector>> keep_alive(vector);
  Persistent<HeapDeque<IntDeque>> keep_alive2(deque);
  PreciselyCollectGarbage();
  EXPECT_EQ(1u, it->size());
  EXPECT_EQ(1u, it2->size());
  EXPECT_EQ(0, IntWrapper::destructor_calls_);
}

TEST(HeapTest, EmbeddedInVector) {
  ClearOutOldGarbage();
  SimpleFinalizedObject::destructor_calls_ = 0;
  {
    Persistent<HeapVector<VectorObject, 2>> inline_vector =
        new HeapVector<VectorObject, 2>;
    Persistent<HeapVector<VectorObject>> outline_vector =
        new HeapVector<VectorObject>;
    VectorObject i1, i2;
    inline_vector->push_back(i1);
    inline_vector->push_back(i2);

    VectorObject o1, o2;
    outline_vector->push_back(o1);
    outline_vector->push_back(o2);

    Persistent<HeapVector<VectorObjectInheritedTrace>> vector_inherited_trace =
        new HeapVector<VectorObjectInheritedTrace>;
    VectorObjectInheritedTrace it1, it2;
    vector_inherited_trace->push_back(it1);
    vector_inherited_trace->push_back(it2);

    PreciselyCollectGarbage();
    EXPECT_EQ(0, SimpleFinalizedObject::destructor_calls_);
  }
  PreciselyCollectGarbage();
  EXPECT_EQ(6, SimpleFinalizedObject::destructor_calls_);
}

TEST(HeapTest, EmbeddedInDeque) {
  ClearOutOldGarbage();
  SimpleFinalizedObject::destructor_calls_ = 0;
  {
    Persistent<HeapDeque<VectorObject, 2>> inline_deque =
        new HeapDeque<VectorObject, 2>;
    Persistent<HeapDeque<VectorObject>> outline_deque =
        new HeapDeque<VectorObject>;
    VectorObject i1, i2;
    inline_deque->push_back(i1);
    inline_deque->push_back(i2);

    VectorObject o1, o2;
    outline_deque->push_back(o1);
    outline_deque->push_back(o2);

    Persistent<HeapDeque<VectorObjectInheritedTrace>> deque_inherited_trace =
        new HeapDeque<VectorObjectInheritedTrace>;
    VectorObjectInheritedTrace it1, it2;
    deque_inherited_trace->push_back(it1);
    deque_inherited_trace->push_back(it2);

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
  void Trace(blink::Visitor* visitor) {}

  static int destructor_calls_;
};

int InlinedVectorObject::destructor_calls_ = 0;

class InlinedVectorObjectWithVtable {
  DISALLOW_NEW();

 public:
  InlinedVectorObjectWithVtable() = default;
  virtual ~InlinedVectorObjectWithVtable() { destructor_calls_++; }
  virtual void VirtualMethod() {}
  void Trace(blink::Visitor* visitor) {}

  static int destructor_calls_;
};

int InlinedVectorObjectWithVtable::destructor_calls_ = 0;

}  // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::InlinedVectorObject);

namespace blink {

class InlinedVectorObjectWrapper final
    : public GarbageCollectedFinalized<InlinedVectorObjectWrapper> {
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

  void Trace(blink::Visitor* visitor) {
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
    : public GarbageCollectedFinalized<InlinedVectorObjectWithVtableWrapper> {
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

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(vector1_);
    visitor->Trace(vector2_);
    visitor->Trace(vector3_);
  }

 private:
  HeapVector<InlinedVectorObjectWithVtable> vector1_;
  HeapVector<InlinedVectorObjectWithVtable, 1> vector2_;
  HeapVector<InlinedVectorObjectWithVtable, 2> vector3_;
};

TEST(HeapTest, VectorDestructors) {
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
        new InlinedVectorObjectWrapper();
    ConservativelyCollectGarbage();
    EXPECT_EQ(2, InlinedVectorObject::destructor_calls_);
  }
  PreciselyCollectGarbage();
  EXPECT_LE(8, InlinedVectorObject::destructor_calls_);
}

// TODO(Oilpan): when Vector.h's contiguous container support no longer disables
// Vector<>s with inline capacity, enable this test.
#if !defined(ANNOTATE_CONTIGUOUS_CONTAINER)
TEST(HeapTest, VectorDestructorsWithVtable) {
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
        new InlinedVectorObjectWithVtableWrapper();
    ConservativelyCollectGarbage();
    EXPECT_EQ(3, InlinedVectorObjectWithVtable::destructor_calls_);
  }
  PreciselyCollectGarbage();
  EXPECT_EQ(9, InlinedVectorObjectWithVtable::destructor_calls_);
}
#endif

template <typename Set>
void RawPtrInHashHelper() {
  Set set;
  set.Add(new int(42));
  set.Add(new int(42));
  EXPECT_EQ(2u, set.size());
  for (typename Set::iterator it = set.begin(); it != set.end(); ++it) {
    EXPECT_EQ(42, **it);
    delete *it;
  }
}

TEST(HeapTest, HeapTerminatedArray) {
  ClearOutOldGarbage();
  IntWrapper::destructor_calls_ = 0;

  HeapTerminatedArray<TerminatedArrayItem>* arr = nullptr;

  const wtf_size_t kPrefixSize = 4;
  const wtf_size_t kSuffixSize = 4;

  {
    HeapTerminatedArrayBuilder<TerminatedArrayItem> builder(arr);
    builder.Grow(kPrefixSize);
    ConservativelyCollectGarbage();
    for (wtf_size_t i = 0; i < kPrefixSize; i++)
      builder.Append(TerminatedArrayItem(IntWrapper::Create(i)));
    arr = builder.Release();
  }

  ConservativelyCollectGarbage();
  EXPECT_EQ(0, IntWrapper::destructor_calls_);
  EXPECT_EQ(kPrefixSize, arr->size());
  for (wtf_size_t i = 0; i < kPrefixSize; i++)
    EXPECT_EQ(i, static_cast<wtf_size_t>(arr->at(i).Payload()->Value()));

  {
    HeapTerminatedArrayBuilder<TerminatedArrayItem> builder(arr);
    builder.Grow(kSuffixSize);
    for (wtf_size_t i = 0; i < kSuffixSize; i++)
      builder.Append(TerminatedArrayItem(IntWrapper::Create(kPrefixSize + i)));
    arr = builder.Release();
  }

  ConservativelyCollectGarbage();
  EXPECT_EQ(0, IntWrapper::destructor_calls_);
  EXPECT_EQ(kPrefixSize + kSuffixSize, arr->size());
  for (wtf_size_t i = 0; i < kPrefixSize + kSuffixSize; i++)
    EXPECT_EQ(i, static_cast<wtf_size_t>(arr->at(i).Payload()->Value()));

  {
    Persistent<HeapTerminatedArray<TerminatedArrayItem>> persistent_arr = arr;
    arr = nullptr;
    PreciselyCollectGarbage();
    arr = persistent_arr.Get();
    EXPECT_EQ(0, IntWrapper::destructor_calls_);
    EXPECT_EQ(kPrefixSize + kSuffixSize, arr->size());
    for (wtf_size_t i = 0; i < kPrefixSize + kSuffixSize; i++)
      EXPECT_EQ(i, static_cast<wtf_size_t>(arr->at(i).Payload()->Value()));
  }

  arr = nullptr;
  PreciselyCollectGarbage();
  EXPECT_EQ(8, IntWrapper::destructor_calls_);
}

TEST(HeapTest, HeapLinkedStack) {
  ClearOutOldGarbage();
  IntWrapper::destructor_calls_ = 0;

  HeapLinkedStack<TerminatedArrayItem>* stack =
      new HeapLinkedStack<TerminatedArrayItem>();

  const wtf_size_t kStackSize = 10;

  for (wtf_size_t i = 0; i < kStackSize; i++)
    stack->Push(TerminatedArrayItem(IntWrapper::Create(i)));

  ConservativelyCollectGarbage();
  EXPECT_EQ(0, IntWrapper::destructor_calls_);
  EXPECT_EQ(kStackSize, stack->size());
  while (!stack->IsEmpty()) {
    EXPECT_EQ(stack->size() - 1,
              static_cast<size_t>(stack->Peek().Payload()->Value()));
    stack->Pop();
  }

  Persistent<HeapLinkedStack<TerminatedArrayItem>> p_stack = stack;

  PreciselyCollectGarbage();
  EXPECT_EQ(kStackSize, static_cast<size_t>(IntWrapper::destructor_calls_));
  EXPECT_EQ(0u, p_stack->size());
}

TEST(HeapTest, AllocationDuringFinalization) {
  ClearOutOldGarbage();
  IntWrapper::destructor_calls_ = 0;
  OneKiloByteObject::destructor_calls_ = 0;
  LargeHeapObject::destructor_calls_ = 0;

  Persistent<IntWrapper> wrapper;
  new FinalizationAllocator(&wrapper);

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

TEST(HeapTest, AllocationDuringPrefinalizer) {
  ClearOutOldGarbage();
  IntWrapper::destructor_calls_ = 0;
  OneKiloByteObject::destructor_calls_ = 0;
  LargeHeapObject::destructor_calls_ = 0;

  Persistent<IntWrapper> wrapper;
  new PreFinalizationAllocator(&wrapper);

  PreciselyCollectGarbage();
  EXPECT_EQ(0, IntWrapper::destructor_calls_);
  EXPECT_EQ(0, OneKiloByteObject::destructor_calls_);
  EXPECT_EQ(0, LargeHeapObject::destructor_calls_);
  // Check that the wrapper allocated during finalization is not
  // swept away and zapped later in the same sweeping phase.
  EXPECT_EQ(42, wrapper->Value());

  wrapper.Clear();
  PreciselyCollectGarbage();
  // The 42 IntWrappers were the ones allocated in the pre-finalizer
  // of PreFinalizationAllocator and the ones allocated in LargeHeapObject.
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
    ConservativelyCollectGarbage();
    EXPECT_FALSE(RefCountedWithDestructor::was_destructed_);
  }
  // The destructors of the sets don't call the destructors of the elements
  // in the heap sets. You have to actually remove the elments, call clear()
  // or have a GC to get the destructors called.
  EXPECT_FALSE(RefCountedWithDestructor::was_destructed_);
  PreciselyCollectGarbage();
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

TEST(HeapTest, DestructorsCalled) {
  HeapHashMap<Member<IntWrapper>, std::unique_ptr<SimpleClassWithDestructor>>
      map;
  SimpleClassWithDestructor* has_destructor = new SimpleClassWithDestructor();
  map.insert(IntWrapper::Create(1), base::WrapUnique(has_destructor));
  SimpleClassWithDestructor::was_destructed_ = false;
  map.clear();
  EXPECT_TRUE(SimpleClassWithDestructor::was_destructed_);
}

class MixinA : public GarbageCollectedMixin {
 public:
  MixinA() : obj_(IntWrapper::Create(100)) {}
  void Trace(blink::Visitor* visitor) override {
    trace_count_++;
    visitor->Trace(obj_);
  }

  static int trace_count_;

  Member<IntWrapper> obj_;
};

int MixinA::trace_count_ = 0;

class MixinB : public GarbageCollectedMixin {
 public:
  MixinB() : obj_(IntWrapper::Create(101)) {}
  void Trace(blink::Visitor* visitor) override { visitor->Trace(obj_); }
  Member<IntWrapper> obj_;
};

class MultipleMixins : public GarbageCollected<MultipleMixins>,
                       public MixinA,
                       public MixinB {
  USING_GARBAGE_COLLECTED_MIXIN(MultipleMixins);

 public:
  MultipleMixins() : obj_(IntWrapper::Create(102)) {}
  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(obj_);
    MixinA::Trace(visitor);
    MixinB::Trace(visitor);
  }
  Member<IntWrapper> obj_;
};

class DerivedMultipleMixins : public MultipleMixins {
 public:
  DerivedMultipleMixins() : obj_(IntWrapper::Create(103)) {}

  void Trace(blink::Visitor* visitor) override {
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

TEST(HeapTest, MultipleMixins) {
  EXPECT_TRUE(kIsMixinTrue);
  EXPECT_FALSE(kIsMixinFalse);

  ClearOutOldGarbage();
  IntWrapper::destructor_calls_ = 0;
  MultipleMixins* obj = new MultipleMixins();
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

TEST(HeapTest, DerivedMultipleMixins) {
  ClearOutOldGarbage();
  IntWrapper::destructor_calls_ = 0;
  DerivedMultipleMixins::trace_called_ = 0;

  DerivedMultipleMixins* obj = new DerivedMultipleMixins();
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
  USING_GARBAGE_COLLECTED_MIXIN(MixinInstanceWithoutTrace);

 public:
  MixinInstanceWithoutTrace() = default;
};

TEST(HeapTest, MixinInstanceWithoutTrace) {
  // Verify that a mixin instance without any traceable
  // references inherits the mixin's trace implementation.
  ClearOutOldGarbage();
  MixinA::trace_count_ = 0;
  MixinInstanceWithoutTrace* obj = new MixinInstanceWithoutTrace();
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

TEST(HeapTest, NeedsAdjustPointer) {
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

template <typename Set>
void SetWithCustomWeaknessHandling() {
  typedef typename Set::iterator Iterator;
  Persistent<IntWrapper> living_int(IntWrapper::Create(42));
  Persistent<Set> set1(new Set());
  {
    Set set2;
    Set* set3 = new Set();
    set2.insert(
        PairWithWeakHandling(IntWrapper::Create(0), IntWrapper::Create(1)));
    set3->insert(
        PairWithWeakHandling(IntWrapper::Create(2), IntWrapper::Create(3)));
    set1->insert(
        PairWithWeakHandling(IntWrapper::Create(4), IntWrapper::Create(5)));
    ConservativelyCollectGarbage();
    // The first set is pointed to from a persistent, so it's referenced, but
    // the weak processing may have taken place.
    if (set1->size()) {
      Iterator i1 = set1->begin();
      EXPECT_EQ(4, i1->first->Value());
      EXPECT_EQ(5, i1->second->Value());
    }
    // The second set is on-stack, so its backing store must be referenced from
    // the stack. That makes the weak references strong.
    Iterator i2 = set2.begin();
    EXPECT_EQ(0, i2->first->Value());
    EXPECT_EQ(1, i2->second->Value());
    // The third set is pointed to from the stack, so it's referenced, but the
    // weak processing may have taken place.
    if (set3->size()) {
      Iterator i3 = set3->begin();
      EXPECT_EQ(2, i3->first->Value());
      EXPECT_EQ(3, i3->second->Value());
    }
  }
  PreciselyCollectGarbage();
  EXPECT_EQ(0u, set1->size());
  set1->insert(PairWithWeakHandling(IntWrapper::Create(103), living_int));
  // This one gets zapped at GC time because nothing holds the 103 alive.
  set1->insert(PairWithWeakHandling(living_int, IntWrapper::Create(103)));
  set1->insert(PairWithWeakHandling(
      IntWrapper::Create(103),
      IntWrapper::Create(103)));  // This one gets zapped too.
  set1->insert(PairWithWeakHandling(living_int, living_int));
  // This one is identical to the previous and doesn't add anything.
  set1->insert(PairWithWeakHandling(living_int, living_int));
  EXPECT_EQ(4u, set1->size());
  PreciselyCollectGarbage();
  EXPECT_EQ(2u, set1->size());
  Iterator i1 = set1->begin();
  EXPECT_TRUE(i1->first->Value() == 103 || i1->first == living_int);
  EXPECT_EQ(living_int, i1->second);
  ++i1;
  EXPECT_TRUE(i1->first->Value() == 103 || i1->first == living_int);
  EXPECT_EQ(living_int, i1->second);
}

TEST(HeapTest, SetWithCustomWeaknessHandling) {
  SetWithCustomWeaknessHandling<HeapHashSet<PairWithWeakHandling>>();
  SetWithCustomWeaknessHandling<HeapLinkedHashSet<PairWithWeakHandling>>();
}

TEST(HeapTest, MapWithCustomWeaknessHandling) {
  typedef HeapHashMap<PairWithWeakHandling, scoped_refptr<OffHeapInt>> Map;
  typedef Map::iterator Iterator;
  ClearOutOldGarbage();
  OffHeapInt::destructor_calls_ = 0;

  Persistent<Map> map1(new Map());
  Persistent<IntWrapper> living_int(IntWrapper::Create(42));
  {
    Map map2;
    Map* map3 = new Map();
    map2.insert(
        PairWithWeakHandling(IntWrapper::Create(0), IntWrapper::Create(1)),
        OffHeapInt::Create(1001));
    map3->insert(
        PairWithWeakHandling(IntWrapper::Create(2), IntWrapper::Create(3)),
        OffHeapInt::Create(1002));
    map1->insert(
        PairWithWeakHandling(IntWrapper::Create(4), IntWrapper::Create(5)),
        OffHeapInt::Create(1003));
    EXPECT_EQ(0, OffHeapInt::destructor_calls_);

    ConservativelyCollectGarbage();
    // The first map2 is pointed to from a persistent, so it's referenced, but
    // the weak processing may have taken place.
    if (map1->size()) {
      Iterator i1 = map1->begin();
      EXPECT_EQ(4, i1->key.first->Value());
      EXPECT_EQ(5, i1->key.second->Value());
      EXPECT_EQ(1003, i1->value->Value());
    }
    // The second map2 is on-stack, so its backing store must be referenced from
    // the stack. That makes the weak references strong.
    Iterator i2 = map2.begin();
    EXPECT_EQ(0, i2->key.first->Value());
    EXPECT_EQ(1, i2->key.second->Value());
    EXPECT_EQ(1001, i2->value->Value());
    // The third map2 is pointed to from the stack, so it's referenced, but the
    // weak processing may have taken place.
    if (map3->size()) {
      Iterator i3 = map3->begin();
      EXPECT_EQ(2, i3->key.first->Value());
      EXPECT_EQ(3, i3->key.second->Value());
      EXPECT_EQ(1002, i3->value->Value());
    }
  }
  PreciselyCollectGarbage();

  EXPECT_EQ(0u, map1->size());
  EXPECT_EQ(3, OffHeapInt::destructor_calls_);

  OffHeapInt::destructor_calls_ = 0;

  map1->insert(PairWithWeakHandling(IntWrapper::Create(103), living_int),
               OffHeapInt::Create(2000));
  map1->insert(PairWithWeakHandling(living_int, IntWrapper::Create(103)),
               OffHeapInt::Create(2001));  // This one gets zapped at GC time
  // because nothing holds the 103 alive.
  map1->insert(
      PairWithWeakHandling(IntWrapper::Create(103), IntWrapper::Create(103)),
      OffHeapInt::Create(2002));  // This one gets zapped too.
  scoped_refptr<OffHeapInt> dupe_int(OffHeapInt::Create(2003));
  map1->insert(PairWithWeakHandling(living_int, living_int), dupe_int);
  map1->insert(
      PairWithWeakHandling(living_int, living_int),
      dupe_int);  // This one is identical to the previous and doesn't add
                  // anything.
  dupe_int = nullptr;

  EXPECT_EQ(0, OffHeapInt::destructor_calls_);
  EXPECT_EQ(4u, map1->size());
  PreciselyCollectGarbage();
  EXPECT_EQ(2, OffHeapInt::destructor_calls_);
  EXPECT_EQ(2u, map1->size());
  Iterator i1 = map1->begin();
  EXPECT_TRUE(i1->key.first->Value() == 103 || i1->key.first == living_int);
  EXPECT_EQ(living_int, i1->key.second);
  ++i1;
  EXPECT_TRUE(i1->key.first->Value() == 103 || i1->key.first == living_int);
  EXPECT_EQ(living_int, i1->key.second);
}

TEST(HeapTest, MapWithCustomWeaknessHandling2) {
  typedef HeapHashMap<scoped_refptr<OffHeapInt>, PairWithWeakHandling> Map;
  typedef Map::iterator Iterator;
  ClearOutOldGarbage();
  OffHeapInt::destructor_calls_ = 0;

  Persistent<Map> map1(new Map());
  Persistent<IntWrapper> living_int(IntWrapper::Create(42));

  {
    Map map2;
    Map* map3 = new Map();
    map2.insert(
        OffHeapInt::Create(1001),
        PairWithWeakHandling(IntWrapper::Create(0), IntWrapper::Create(1)));
    map3->insert(
        OffHeapInt::Create(1002),
        PairWithWeakHandling(IntWrapper::Create(2), IntWrapper::Create(3)));
    map1->insert(
        OffHeapInt::Create(1003),
        PairWithWeakHandling(IntWrapper::Create(4), IntWrapper::Create(5)));
    EXPECT_EQ(0, OffHeapInt::destructor_calls_);

    ConservativelyCollectGarbage();
    // The first map2 is pointed to from a persistent, so it's referenced, but
    // the weak processing may have taken place.
    if (map1->size()) {
      Iterator i1 = map1->begin();
      EXPECT_EQ(4, i1->value.first->Value());
      EXPECT_EQ(5, i1->value.second->Value());
      EXPECT_EQ(1003, i1->key->Value());
    }
    // The second map2 is on-stack, so its backing store must be referenced from
    // the stack. That makes the weak references strong.
    Iterator i2 = map2.begin();
    EXPECT_EQ(0, i2->value.first->Value());
    EXPECT_EQ(1, i2->value.second->Value());
    EXPECT_EQ(1001, i2->key->Value());
    // The third map2 is pointed to from the stack, so it's referenced, but the
    // weak processing may have taken place.
    if (map3->size()) {
      Iterator i3 = map3->begin();
      EXPECT_EQ(2, i3->value.first->Value());
      EXPECT_EQ(3, i3->value.second->Value());
      EXPECT_EQ(1002, i3->key->Value());
    }
  }
  PreciselyCollectGarbage();

  EXPECT_EQ(0u, map1->size());
  EXPECT_EQ(3, OffHeapInt::destructor_calls_);

  OffHeapInt::destructor_calls_ = 0;

  map1->insert(OffHeapInt::Create(2000),
               PairWithWeakHandling(IntWrapper::Create(103), living_int));
  // This one gets zapped at GC time because nothing holds the 103 alive.
  map1->insert(OffHeapInt::Create(2001),
               PairWithWeakHandling(living_int, IntWrapper::Create(103)));
  map1->insert(OffHeapInt::Create(2002),
               PairWithWeakHandling(
                   IntWrapper::Create(103),
                   IntWrapper::Create(103)));  // This one gets zapped too.
  scoped_refptr<OffHeapInt> dupe_int(OffHeapInt::Create(2003));
  map1->insert(dupe_int, PairWithWeakHandling(living_int, living_int));
  // This one is identical to the previous and doesn't add anything.
  map1->insert(dupe_int, PairWithWeakHandling(living_int, living_int));
  dupe_int = nullptr;

  EXPECT_EQ(0, OffHeapInt::destructor_calls_);
  EXPECT_EQ(4u, map1->size());
  PreciselyCollectGarbage();
  EXPECT_EQ(2, OffHeapInt::destructor_calls_);
  EXPECT_EQ(2u, map1->size());
  Iterator i1 = map1->begin();
  EXPECT_TRUE(i1->value.first->Value() == 103 || i1->value.first == living_int);
  EXPECT_EQ(living_int, i1->value.second);
  ++i1;
  EXPECT_TRUE(i1->value.first->Value() == 103 || i1->value.first == living_int);
  EXPECT_EQ(living_int, i1->value.second);
}

static void AddElementsToWeakMap(
    HeapHashMap<int, WeakMember<IntWrapper>>* map) {
  // Key cannot be zero in hashmap.
  for (int i = 1; i < 11; i++)
    map->insert(i, IntWrapper::Create(i));
}

// crbug.com/402426
// If it doesn't assert a concurrent modification to the map, then it's passing.
TEST(HeapTest, RegressNullIsStrongified) {
  Persistent<HeapHashMap<int, WeakMember<IntWrapper>>> map =
      new HeapHashMap<int, WeakMember<IntWrapper>>();
  AddElementsToWeakMap(map);
  HeapHashMap<int, WeakMember<IntWrapper>>::AddResult result =
      map->insert(800, nullptr);
  ConservativelyCollectGarbage();
  result.stored_value->value = IntWrapper::Create(42);
}

TEST(HeapTest, Bind) {
  base::OnceClosure closure =
      WTF::Bind(static_cast<void (Bar::*)(Visitor*)>(&Bar::Trace),
                WrapPersistent(Bar::Create()), nullptr);
  // OffHeapInt* should not make Persistent.
  base::OnceClosure closure2 =
      WTF::Bind(&OffHeapInt::VoidFunction, OffHeapInt::Create(1));
  PreciselyCollectGarbage();
  // The closure should have a persistent handle to the Bar.
  EXPECT_EQ(1u, Bar::live_);

  UseMixin::trace_count_ = 0;
  Mixin* mixin = UseMixin::Create();
  base::OnceClosure mixin_closure =
      WTF::Bind(static_cast<void (Mixin::*)(Visitor*)>(&Mixin::Trace),
                WrapPersistent(mixin), nullptr);
  PreciselyCollectGarbage();
  // The closure should have a persistent handle to the mixin.
  EXPECT_EQ(1, UseMixin::trace_count_);
}

typedef HeapHashSet<WeakMember<IntWrapper>> WeakSet;

// These special traits will remove a set from a map when the set is empty.
struct EmptyClearingHashSetTraits : HashTraits<WeakSet> {
  static const WTF::WeakHandlingFlag kWeakHandlingFlag = WTF::kWeakHandling;

  static bool IsAlive(WeakSet& set) {
    bool live_entries_found = false;
    WeakSet::iterator end = set.end();
    for (WeakSet::iterator it = set.begin(); it != end; ++it) {
      if (ThreadHeap::IsHeapObjectAlive(*it)) {
        live_entries_found = true;
        break;
      }
    }
    return live_entries_found;
  }

  template <typename VisitorDispatcher>
  static bool TraceInCollection(VisitorDispatcher visitor,
                                WeakSet& set,
                                WTF::WeakHandlingFlag weakenss) {
    bool live_entries_found = false;
    WeakSet::iterator end = set.end();
    for (WeakSet::iterator it = set.begin(); it != end; ++it) {
      if (ThreadHeap::IsHeapObjectAlive(*it)) {
        live_entries_found = true;
        break;
      }
    }
    // If there are live entries in the set then the set cannot be removed
    // from the map it is contained in, and we need to mark it (and its
    // backing) live. We just trace normally, which will invoke the normal
    // weak handling for any entries that are not live.
    if (live_entries_found)
      set.Trace(visitor);
    return !live_entries_found;
  }
};

// This is an example to show how you can remove entries from a T->WeakSet map
// when the weak sets become empty. For this example we are using a type that
// is given to use (HeapHashSet) rather than a type of our own. This means:
// 1) We can't just override the HashTrait for the type since this would affect
//    all collections that use this kind of weak set. Instead we have our own
//    traits and use a map with custom traits for the value type. These traits
//    are the 5th template parameter, so we have to supply default values for
//    the 3rd and 4th template parameters
// 2) We can't just inherit from WeakHandlingHashTraits, since that trait
//    assumes we can add methods to the type, but we can't add methods to
//    HeapHashSet.
TEST(HeapTest, RemoveEmptySets) {
  ClearOutOldGarbage();
  OffHeapInt::destructor_calls_ = 0;

  Persistent<IntWrapper> living_int(IntWrapper::Create(42));

  typedef scoped_refptr<OffHeapInt> Key;
  typedef HeapHashMap<Key, WeakSet, WTF::DefaultHash<Key>::Hash,
                      HashTraits<Key>, EmptyClearingHashSetTraits>
      Map;
  Persistent<Map> map(new Map());
  map->insert(OffHeapInt::Create(1), WeakSet());
  {
    WeakSet& set = map->begin()->value;
    set.insert(IntWrapper::Create(103));  // Weak set can't hold this long.
    set.insert(living_int);  // This prevents the set from being emptied.
    EXPECT_EQ(2u, set.size());
  }

  // The set we add here is empty, so the entry will be removed from the map
  // at the next GC.
  map->insert(OffHeapInt::Create(2), WeakSet());
  EXPECT_EQ(2u, map->size());

  PreciselyCollectGarbage();
  EXPECT_EQ(1u, map->size());  // The one with key 2 was removed.
  EXPECT_EQ(1, OffHeapInt::destructor_calls_);
  {
    WeakSet& set = map->begin()->value;
    EXPECT_EQ(1u, set.size());
  }

  living_int.Clear();  // The weak set can no longer keep the '42' alive now.
  PreciselyCollectGarbage();
  EXPECT_EQ(0u, map->size());
}

TEST(HeapTest, EphemeronsInEphemerons) {
  typedef HeapHashMap<WeakMember<IntWrapper>, Member<IntWrapper>> InnerMap;
  typedef HeapHashMap<WeakMember<IntWrapper>, InnerMap> OuterMap;

  for (int keep_outer_alive = 0; keep_outer_alive <= 1; keep_outer_alive++) {
    for (int keep_inner_alive = 0; keep_inner_alive <= 1; keep_inner_alive++) {
      Persistent<OuterMap> outer = new OuterMap();
      Persistent<IntWrapper> one = IntWrapper::Create(1);
      Persistent<IntWrapper> two = IntWrapper::Create(2);
      outer->insert(one, InnerMap());
      outer->begin()->value.insert(two, IntWrapper::Create(3));
      EXPECT_EQ(1u, outer->at(one).size());
      if (!keep_outer_alive)
        one.Clear();
      if (!keep_inner_alive)
        two.Clear();
      PreciselyCollectGarbage();
      if (keep_outer_alive) {
        const InnerMap& inner = outer->at(one);
        if (keep_inner_alive) {
          EXPECT_EQ(1u, inner.size());
          IntWrapper* three = inner.at(two);
          EXPECT_EQ(3, three->Value());
        } else {
          EXPECT_EQ(0u, inner.size());
        }
      } else {
        EXPECT_EQ(0u, outer->size());
      }
      outer->clear();
      Persistent<IntWrapper> deep = IntWrapper::Create(42);
      Persistent<IntWrapper> home = IntWrapper::Create(103);
      Persistent<IntWrapper> composite = IntWrapper::Create(91);
      Persistent<HeapVector<Member<IntWrapper>>> keep_alive =
          new HeapVector<Member<IntWrapper>>();
      for (int i = 0; i < 10000; i++) {
        IntWrapper* value = IntWrapper::Create(i);
        keep_alive->push_back(value);
        OuterMap::AddResult new_entry = outer->insert(value, InnerMap());
        new_entry.stored_value->value.insert(deep, home);
        new_entry.stored_value->value.insert(composite, home);
      }
      composite.Clear();
      PreciselyCollectGarbage();
      EXPECT_EQ(10000u, outer->size());
      for (int i = 0; i < 10000; i++) {
        IntWrapper* value = keep_alive->at(i);
        EXPECT_EQ(1u,
                  outer->at(value)
                      .size());  // Other one was deleted by weak handling.
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
  void Trace(blink::Visitor* visitor) { visitor->Trace(map_); }

  typedef HeapHashMap<WeakMember<IntWrapper>, Member<EphemeronWrapper>> Map;
  Map& GetMap() { return map_; }

 private:
  Map map_;
};

TEST(HeapTest, EphemeronsPointToEphemerons) {
  Persistent<IntWrapper> key = IntWrapper::Create(42);
  Persistent<IntWrapper> key2 = IntWrapper::Create(103);

  Persistent<EphemeronWrapper> chain;
  for (int i = 0; i < 100; i++) {
    EphemeronWrapper* old_head = chain;
    chain = new EphemeronWrapper();
    if (i == 50)
      chain->GetMap().insert(key2, old_head);
    else
      chain->GetMap().insert(key, old_head);
    chain->GetMap().insert(IntWrapper::Create(103), new EphemeronWrapper());
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

TEST(HeapTest, Ephemeron) {
  typedef HeapHashMap<WeakMember<IntWrapper>, PairWithWeakHandling> WeakPairMap;
  typedef HeapHashMap<PairWithWeakHandling, WeakMember<IntWrapper>> PairWeakMap;
  typedef HeapHashSet<WeakMember<IntWrapper>> Set;

  Persistent<WeakPairMap> weak_pair_map = new WeakPairMap();
  Persistent<WeakPairMap> weak_pair_map2 = new WeakPairMap();
  Persistent<WeakPairMap> weak_pair_map3 = new WeakPairMap();
  Persistent<WeakPairMap> weak_pair_map4 = new WeakPairMap();

  Persistent<PairWeakMap> pair_weak_map = new PairWeakMap();
  Persistent<PairWeakMap> pair_weak_map2 = new PairWeakMap();

  Persistent<Set> set = new Set();

  Persistent<IntWrapper> wp1 = IntWrapper::Create(1);
  Persistent<IntWrapper> wp2 = IntWrapper::Create(2);
  Persistent<IntWrapper> pw1 = IntWrapper::Create(3);
  Persistent<IntWrapper> pw2 = IntWrapper::Create(4);

  weak_pair_map->insert(wp1, PairWithWeakHandling(wp1, wp1));
  weak_pair_map->insert(wp2, PairWithWeakHandling(wp1, wp1));
  weak_pair_map2->insert(wp1, PairWithWeakHandling(wp1, wp2));
  weak_pair_map2->insert(wp2, PairWithWeakHandling(wp1, wp2));
  // The map from wp1 to (wp2, wp1) would mark wp2 live, so we skip that.
  weak_pair_map3->insert(wp2, PairWithWeakHandling(wp2, wp1));
  weak_pair_map4->insert(wp1, PairWithWeakHandling(wp2, wp2));
  weak_pair_map4->insert(wp2, PairWithWeakHandling(wp2, wp2));

  pair_weak_map->insert(PairWithWeakHandling(pw1, pw1), pw1);
  pair_weak_map->insert(PairWithWeakHandling(pw1, pw2), pw1);
  // The map from (pw2, pw1) to pw1 would make pw2 live, so we skip that.
  pair_weak_map->insert(PairWithWeakHandling(pw2, pw2), pw1);
  pair_weak_map2->insert(PairWithWeakHandling(pw1, pw1), pw2);
  pair_weak_map2->insert(PairWithWeakHandling(pw1, pw2), pw2);
  pair_weak_map2->insert(PairWithWeakHandling(pw2, pw1), pw2);
  pair_weak_map2->insert(PairWithWeakHandling(pw2, pw2), pw2);

  set->insert(wp1);
  set->insert(wp2);
  set->insert(pw1);
  set->insert(pw2);

  PreciselyCollectGarbage();

  EXPECT_EQ(2u, weak_pair_map->size());
  EXPECT_EQ(2u, weak_pair_map2->size());
  EXPECT_EQ(1u, weak_pair_map3->size());
  EXPECT_EQ(2u, weak_pair_map4->size());

  EXPECT_EQ(3u, pair_weak_map->size());
  EXPECT_EQ(4u, pair_weak_map2->size());

  EXPECT_EQ(4u, set->size());

  wp2.Clear();  // Kills all entries in the weakPairMaps except the first.
  pw2.Clear();  // Kills all entries in the pairWeakMaps except the first.

  for (int i = 0; i < 2; i++) {
    PreciselyCollectGarbage();

    EXPECT_EQ(1u, weak_pair_map->size());
    EXPECT_EQ(0u, weak_pair_map2->size());
    EXPECT_EQ(0u, weak_pair_map3->size());
    EXPECT_EQ(0u, weak_pair_map4->size());

    EXPECT_EQ(1u, pair_weak_map->size());
    EXPECT_EQ(0u, pair_weak_map2->size());

    EXPECT_EQ(2u, set->size());  // wp1 and pw1.
  }

  wp1.Clear();
  pw1.Clear();

  PreciselyCollectGarbage();

  EXPECT_EQ(0u, weak_pair_map->size());
  EXPECT_EQ(0u, pair_weak_map->size());
  EXPECT_EQ(0u, set->size());
}

class Link1 : public GarbageCollected<Link1> {
 public:
  Link1(IntWrapper* link) : link_(link) {}

  void Trace(blink::Visitor* visitor) { visitor->Trace(link_); }

  IntWrapper* Link() { return link_; }

 private:
  Member<IntWrapper> link_;
};

TEST(HeapTest, IndirectStrongToWeak) {
  typedef HeapHashMap<WeakMember<IntWrapper>, Member<Link1>> Map;
  Persistent<Map> map = new Map();
  Persistent<IntWrapper> dead_object =
      IntWrapper::Create(100);  // Named for "Drowning by Numbers" (1988).
  Persistent<IntWrapper> life_object = IntWrapper::Create(42);
  map->insert(dead_object, new Link1(dead_object));
  map->insert(life_object, new Link1(life_object));
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

static Mutex& MainThreadMutex() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, main_mutex, ());
  return main_mutex;
}

static ThreadCondition& MainThreadCondition() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadCondition, main_condition,
                                  (MainThreadMutex()));
  return main_condition;
}

static void ParkMainThread() {
  MainThreadCondition().Wait();
}

static void WakeMainThread() {
  MutexLocker locker(MainThreadMutex());
  MainThreadCondition().Signal();
}

static Mutex& WorkerThreadMutex() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, worker_mutex, ());
  return worker_mutex;
}

static ThreadCondition& WorkerThreadCondition() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadCondition, worker_condition,
                                  (WorkerThreadMutex()));
  return worker_condition;
}

static void ParkWorkerThread() {
  WorkerThreadCondition().Wait();
}

static void WakeWorkerThread() {
  MutexLocker locker(WorkerThreadMutex());
  WorkerThreadCondition().Signal();
}

class ThreadedStrongificationTester {
 public:
  static void Test() {
    IntWrapper::destructor_calls_ = 0;

    MutexLocker locker(MainThreadMutex());
    std::unique_ptr<Thread> worker_thread = Platform::Current()->CreateThread(
        ThreadCreationParams(WebThreadType::kTestThread)
            .SetThreadNameForTest("Test Worker Thread"));
    PostCrossThreadTask(*worker_thread->GetTaskRunner(), FROM_HERE,
                        CrossThreadBind(WorkerThreadMain));

    // Wait for the worker thread initialization. The worker
    // allocates a weak collection where both collection and
    // contents are kept alive via persistent pointers.
    ParkMainThread();

    // Perform two garbage collections where the worker thread does
    // not wake up in between. This will cause us to remove marks
    // and mark unmarked objects dead. The collection on the worker
    // heap is found through the persistent and the backing should
    // be marked.
    PreciselyCollectGarbage();
    PreciselyCollectGarbage();

    // Wake up the worker thread so it can continue. It will sweep
    // and perform another GC where the backing store of its
    // collection should be strongified.
    WakeWorkerThread();

    // Wait for the worker thread to sweep its heaps before checking.
    ParkMainThread();
  }

 private:
  using WeakCollectionType =
      HeapHashMap<WeakMember<IntWrapper>, Member<IntWrapper>>;

  static WeakCollectionType* AllocateCollection() {
    // Create a weak collection that is kept alive by a persistent
    // and keep the contents alive with a persistents as
    // well.
    Persistent<IntWrapper> wrapper1 = IntWrapper::Create(32);
    Persistent<IntWrapper> wrapper2 = IntWrapper::Create(32);
    Persistent<IntWrapper> wrapper3 = IntWrapper::Create(32);
    Persistent<IntWrapper> wrapper4 = IntWrapper::Create(32);
    Persistent<IntWrapper> wrapper5 = IntWrapper::Create(32);
    Persistent<IntWrapper> wrapper6 = IntWrapper::Create(32);
    Persistent<WeakCollectionType> weak_collection = new WeakCollectionType;
    weak_collection->insert(wrapper1, wrapper1);
    weak_collection->insert(wrapper2, wrapper2);
    weak_collection->insert(wrapper3, wrapper3);
    weak_collection->insert(wrapper4, wrapper4);
    weak_collection->insert(wrapper5, wrapper5);
    weak_collection->insert(wrapper6, wrapper6);

    // Signal the main thread that the worker is done with its allocation.
    WakeMainThread();

    // Wait for the main thread to do two GCs without sweeping
    // this thread heap.
    ParkWorkerThread();

    return weak_collection;
  }

  static void WorkerThreadMain() {
    MutexLocker locker(WorkerThreadMutex());

    ThreadState::AttachCurrentThread();

    {
      Persistent<WeakCollectionType> collection = AllocateCollection();
      {
        // Prevent weak processing with an iterator and GC.
        WeakCollectionType::iterator it = collection->begin();
        ConservativelyCollectGarbage();

        // The backing should be strongified because of the iterator.
        EXPECT_EQ(6u, collection->size());
        EXPECT_EQ(32, it->value->Value());
      }

      // Disregarding the iterator but keeping the collection alive
      // with a persistent should lead to weak processing.
      PreciselyCollectGarbage();
      EXPECT_EQ(0u, collection->size());
    }

    WakeMainThread();
    ThreadState::DetachCurrentThread();
  }

  static volatile uintptr_t worker_object_pointer_;
};

TEST(HeapTest, ThreadedStrongification) {
  ThreadedStrongificationTester::Test();
}

class MemberSameThreadCheckTester {
 public:
  void Test() {
    IntWrapper::destructor_calls_ = 0;

    MutexLocker locker(MainThreadMutex());
    std::unique_ptr<Thread> worker_thread = Platform::Current()->CreateThread(
        ThreadCreationParams(WebThreadType::kTestThread)
            .SetThreadNameForTest("Test Worker Thread"));
    PostCrossThreadTask(
        *worker_thread->GetTaskRunner(), FROM_HERE,
        CrossThreadBind(&MemberSameThreadCheckTester::WorkerThreadMain,
                        CrossThreadUnretained(this)));

    ParkMainThread();
  }

 private:
  Member<IntWrapper> wrapper_;

  void WorkerThreadMain() {
    MutexLocker locker(WorkerThreadMutex());

    ThreadState::AttachCurrentThread();

    // Setting an object created on the worker thread to a Member allocated on
    // the main thread is not allowed.
    wrapper_ = IntWrapper::Create(42);

    WakeMainThread();
    ThreadState::DetachCurrentThread();
  }
};

#if DCHECK_IS_ON()
// TODO(keishi) This test is flaky on mac_chromium_rel_ng bot.
// crbug.com/709069
#if !defined(OS_MACOSX)
TEST(HeapDeathTest, MemberSameThreadCheck) {
  EXPECT_DEATH(MemberSameThreadCheckTester().Test(), "");
}
#endif
#endif

class PersistentSameThreadCheckTester {
 public:
  void Test() {
    IntWrapper::destructor_calls_ = 0;

    MutexLocker locker(MainThreadMutex());
    std::unique_ptr<Thread> worker_thread = Platform::Current()->CreateThread(
        ThreadCreationParams(WebThreadType::kTestThread)
            .SetThreadNameForTest("Test Worker Thread"));
    PostCrossThreadTask(
        *worker_thread->GetTaskRunner(), FROM_HERE,
        CrossThreadBind(&PersistentSameThreadCheckTester::WorkerThreadMain,
                        CrossThreadUnretained(this)));

    ParkMainThread();
  }

 private:
  Persistent<IntWrapper> wrapper_;

  void WorkerThreadMain() {
    MutexLocker locker(WorkerThreadMutex());

    ThreadState::AttachCurrentThread();

    // Setting an object created on the worker thread to a Persistent allocated
    // on the main thread is not allowed.
    wrapper_ = IntWrapper::Create(42);

    WakeMainThread();
    ThreadState::DetachCurrentThread();
  }
};

#if DCHECK_IS_ON()
// TODO(keishi) This test is flaky on mac_chromium_rel_ng bot.
// crbug.com/709069
#if !defined(OS_MACOSX)
TEST(HeapDeathTest, PersistentSameThreadCheck) {
  EXPECT_DEATH(PersistentSameThreadCheckTester().Test(), "");
}
#endif
#endif

class MarkingSameThreadCheckTester {
 public:
  void Test() {
    IntWrapper::destructor_calls_ = 0;

    MutexLocker locker(MainThreadMutex());
    std::unique_ptr<Thread> worker_thread = Platform::Current()->CreateThread(
        ThreadCreationParams(WebThreadType::kTestThread)
            .SetThreadNameForTest("Test Worker Thread"));
    Persistent<MainThreadObject> main_thread_object = new MainThreadObject();
    PostCrossThreadTask(
        *worker_thread->GetTaskRunner(), FROM_HERE,
        CrossThreadBind(&MarkingSameThreadCheckTester::WorkerThreadMain,
                        CrossThreadUnretained(this),
                        WrapCrossThreadPersistent(main_thread_object.Get())));
    ParkMainThread();
    // This will try to mark MainThreadObject when it tries to mark IntWrapper
    // it should crash.
    PreciselyCollectGarbage();
  }

 private:
  class MainThreadObject : public GarbageCollectedFinalized<MainThreadObject> {
   public:
    void Trace(blink::Visitor* visitor) { visitor->Trace(wrapper_set_); }
    void AddToSet(IntWrapper* wrapper) { wrapper_set_.insert(42, wrapper); }

   private:
    HeapHashMap<int, Member<IntWrapper>> wrapper_set_;
  };

  void WorkerThreadMain(MainThreadObject* main_thread_object) {
    MutexLocker locker(WorkerThreadMutex());

    ThreadState::AttachCurrentThread();

    // Adding a reference to an object created on the worker thread to a
    // HeapHashMap created on the main thread is not allowed.
    main_thread_object->AddToSet(IntWrapper::Create(42));

    WakeMainThread();
    ThreadState::DetachCurrentThread();
  }
};

#if DCHECK_IS_ON()
// TODO(keishi) This test is flaky on mac_chromium_rel_ng bot.
// crbug.com/709069
#if !defined(OS_MACOSX)
TEST(HeapDeathTest, MarkingSameThreadCheck) {
  // This will crash during marking, at the DCHECK in Visitor::markHeader() or
  // earlier.
  EXPECT_DEATH(MarkingSameThreadCheckTester().Test(), "");
}
#endif
#endif

static bool AllocateAndReturnBool() {
  ConservativelyCollectGarbage();
  return true;
}

static bool CheckGCForbidden() {
  DCHECK(ThreadState::Current()->IsGCForbidden());
  return true;
}

class MixinClass : public GarbageCollectedMixin {
 public:
  MixinClass() : dummy_(CheckGCForbidden()) {}

 private:
  bool dummy_;
};

class ClassWithGarbageCollectingMixinConstructor
    : public GarbageCollected<ClassWithGarbageCollectingMixinConstructor>,
      public MixinClass {
  USING_GARBAGE_COLLECTED_MIXIN(ClassWithGarbageCollectingMixinConstructor);

 public:
  static int trace_called_;

  ClassWithGarbageCollectingMixinConstructor()
      : trace_counter_(TraceCounter::Create()),
        wrapper_(IntWrapper::Create(32)) {}

  void Trace(blink::Visitor* visitor) override {
    trace_called_++;
    visitor->Trace(trace_counter_);
    visitor->Trace(wrapper_);
  }

  void Verify() {
    EXPECT_EQ(32, wrapper_->Value());
    EXPECT_EQ(0, trace_counter_->TraceCount());
    EXPECT_EQ(0, trace_called_);
  }

 private:
  Member<TraceCounter> trace_counter_;
  Member<IntWrapper> wrapper_;
};

int ClassWithGarbageCollectingMixinConstructor::trace_called_ = 0;

// Regression test for out of bounds call through vtable.
// Passes if it doesn't crash.
TEST(HeapTest, GarbageCollectionDuringMixinConstruction) {
  ClassWithGarbageCollectingMixinConstructor* a =
      new ClassWithGarbageCollectingMixinConstructor();
  a->Verify();
}

class DestructorLockingObject
    : public GarbageCollectedFinalized<DestructorLockingObject> {
 public:
  static DestructorLockingObject* Create() {
    return new DestructorLockingObject();
  }

  virtual ~DestructorLockingObject() {
    ++destructor_calls_;
  }

  static int destructor_calls_;
  void Trace(blink::Visitor* visitor) {}

 private:
  DestructorLockingObject() = default;
};

int DestructorLockingObject::destructor_calls_ = 0;

template <typename T>
class TraceIfNeededTester
    : public GarbageCollectedFinalized<TraceIfNeededTester<T>> {
 public:
  static TraceIfNeededTester<T>* Create() {
    return new TraceIfNeededTester<T>();
  }
  static TraceIfNeededTester<T>* Create(const T& obj) {
    return new TraceIfNeededTester<T>(obj);
  }
  void Trace(blink::Visitor* visitor) {
    TraceIfNeeded<T>::Trace(visitor, obj_);
  }
  T& Obj() { return obj_; }
  ~TraceIfNeededTester() = default;

 private:
  TraceIfNeededTester() = default;
  explicit TraceIfNeededTester(const T& obj) : obj_(obj) {}
  T obj_;
};

class PartObject {
  DISALLOW_NEW();

 public:
  PartObject() : obj_(SimpleObject::Create()) {}
  void Trace(blink::Visitor* visitor) { visitor->Trace(obj_); }

 private:
  Member<SimpleObject> obj_;
};

class AllocatesOnAssignment {
 public:
  AllocatesOnAssignment(std::nullptr_t) : value_(nullptr) {}
  AllocatesOnAssignment(int x) : value_(new IntWrapper(x)) {}
  AllocatesOnAssignment(IntWrapper* x) : value_(x) {}

  AllocatesOnAssignment& operator=(const AllocatesOnAssignment x) {
    value_ = x.value_;
    return *this;
  }

  enum DeletedMarker { kDeletedValue };

  AllocatesOnAssignment(const AllocatesOnAssignment& other) {
    if (!ThreadState::Current()->IsGCForbidden())
      ConservativelyCollectGarbage();
    value_ = new IntWrapper(other.value_->Value());
  }

  AllocatesOnAssignment(DeletedMarker) : value_(WTF::kHashTableDeletedValue) {}

  inline bool IsDeleted() const { return value_.IsHashTableDeletedValue(); }

  void Trace(blink::Visitor* visitor) { visitor->Trace(value_); }

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

struct DegenerateHash {
  static unsigned GetHash(const AllocatesOnAssignment&) { return 0; }
  static bool Equal(const AllocatesOnAssignment& a,
                    const AllocatesOnAssignment& b) {
    return !a.IsDeleted() && a == b;
  }
  static const bool safe_to_compare_to_empty_or_deleted = true;
};

struct AllocatesOnAssignmentHashTraits
    : WTF::GenericHashTraits<AllocatesOnAssignment> {
  typedef AllocatesOnAssignment T;
  typedef std::nullptr_t EmptyValueType;
  static EmptyValueType EmptyValue() { return nullptr; }
  static const bool kEmptyValueIsZero =
      false;  // Can't be zero if it has a vtable.
  static void ConstructDeletedValue(T& slot, bool) {
    slot = T(AllocatesOnAssignment::kDeletedValue);
  }
  static bool IsDeletedValue(const T& value) { return value.IsDeleted(); }
};

}  // namespace blink

namespace WTF {

template <>
struct DefaultHash<blink::AllocatesOnAssignment> {
  typedef blink::DegenerateHash Hash;
};

template <>
struct HashTraits<blink::AllocatesOnAssignment>
    : blink::AllocatesOnAssignmentHashTraits {};

}  // namespace WTF

namespace blink {

TEST(HeapTest, GCInHashMapOperations) {
  typedef HeapHashMap<AllocatesOnAssignment, AllocatesOnAssignment> Map;
  Map* map = new Map();
  IntWrapper* key = new IntWrapper(42);
  map->insert(key, AllocatesOnAssignment(103));
  map->erase(key);
  for (int i = 0; i < 10; i++)
    map->insert(AllocatesOnAssignment(i), AllocatesOnAssignment(i));
  for (Map::iterator it = map->begin(); it != map->end(); ++it)
    EXPECT_EQ(it->key.Value(), it->value.Value());
}

class PartObjectWithVirtualMethod {
 public:
  virtual void Trace(blink::Visitor* visitor) {}
};

class ObjectWithVirtualPartObject
    : public GarbageCollected<ObjectWithVirtualPartObject> {
 public:
  ObjectWithVirtualPartObject() : dummy_(AllocateAndReturnBool()) {}
  void Trace(blink::Visitor* visitor) { visitor->Trace(part_); }

 private:
  bool dummy_;
  PartObjectWithVirtualMethod part_;
};

TEST(HeapTest, PartObjectWithVirtualMethod) {
  ObjectWithVirtualPartObject* object = new ObjectWithVirtualPartObject();
  EXPECT_TRUE(object);
}

class AllocInSuperConstructorArgumentSuper
    : public GarbageCollectedFinalized<AllocInSuperConstructorArgumentSuper> {
 public:
  AllocInSuperConstructorArgumentSuper(bool value) : value_(value) {}
  virtual ~AllocInSuperConstructorArgumentSuper() = default;
  virtual void Trace(blink::Visitor* visitor) {}
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
TEST(HeapTest, AllocationInSuperConstructorArgument) {
  AllocInSuperConstructorArgument* object =
      new AllocInSuperConstructorArgument();
  EXPECT_TRUE(object);
  ThreadState::Current()->CollectAllGarbage();
}

class NonNodeAllocatingNodeInDestructor
    : public GarbageCollectedFinalized<NonNodeAllocatingNodeInDestructor> {
 public:
  ~NonNodeAllocatingNodeInDestructor() {
    node_ = new Persistent<IntNode>(IntNode::Create(10));
  }

  void Trace(blink::Visitor* visitor) {}

  static Persistent<IntNode>* node_;
};

Persistent<IntNode>* NonNodeAllocatingNodeInDestructor::node_ = nullptr;

TEST(HeapTest, NonNodeAllocatingNodeInDestructor) {
  new NonNodeAllocatingNodeInDestructor();
  PreciselyCollectGarbage();
  EXPECT_EQ(10, (*NonNodeAllocatingNodeInDestructor::node_)->Value());
  delete NonNodeAllocatingNodeInDestructor::node_;
  NonNodeAllocatingNodeInDestructor::node_ = nullptr;
}

class TraceTypeEagerly1 : public GarbageCollected<TraceTypeEagerly1> {};
class TraceTypeEagerly2 : public TraceTypeEagerly1 {};

class TraceTypeNonEagerly1 {};
WILL_NOT_BE_EAGERLY_TRACED_CLASS(TraceTypeNonEagerly1);
class TraceTypeNonEagerly2 : public TraceTypeNonEagerly1 {};

TEST(HeapTest, TraceTypesEagerly) {
  static_assert(TraceEagerlyTrait<TraceTypeEagerly1>::value, "should be true");
  static_assert(TraceEagerlyTrait<Member<TraceTypeEagerly1>>::value,
                "should be true");
  static_assert(TraceEagerlyTrait<WeakMember<TraceTypeEagerly1>>::value,
                "should be true");
  static_assert(TraceEagerlyTrait<HeapVector<Member<TraceTypeEagerly1>>>::value,
                "should be true");
  static_assert(
      TraceEagerlyTrait<HeapVector<WeakMember<TraceTypeEagerly1>>>::value,
      "should be true");
  static_assert(
      TraceEagerlyTrait<HeapHashSet<Member<TraceTypeEagerly1>>>::value,
      "should be true");
  static_assert(
      TraceEagerlyTrait<HeapHashSet<Member<TraceTypeEagerly1>>>::value,
      "should be true");
  using HashMapIntToObj = HeapHashMap<int, Member<TraceTypeEagerly1>>;
  static_assert(TraceEagerlyTrait<HashMapIntToObj>::value, "should be true");
  using HashMapObjToInt = HeapHashMap<Member<TraceTypeEagerly1>, int>;
  static_assert(TraceEagerlyTrait<HashMapObjToInt>::value, "should be true");

  static_assert(TraceEagerlyTrait<TraceTypeEagerly2>::value, "should be true");
  static_assert(TraceEagerlyTrait<Member<TraceTypeEagerly2>>::value,
                "should be true");

  static_assert(!TraceEagerlyTrait<TraceTypeNonEagerly1>::value,
                "should be false");
  static_assert(TraceEagerlyTrait<TraceTypeNonEagerly2>::value,
                "should be true");
}

class DeepEagerly final : public GarbageCollected<DeepEagerly> {
 public:
  DeepEagerly(DeepEagerly* next) : next_(next) {}

  void Trace(blink::Visitor* visitor) {
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

TEST(HeapTest, TraceDeepEagerly) {
// The allocation & GC overhead is considerable for this test,
// straining debug builds and lower-end targets too much to be
// worth running.
#if !DCHECK_IS_ON() && !defined(OS_ANDROID)
  DeepEagerly* obj = nullptr;
  for (int i = 0; i < 10000000; i++)
    obj = new DeepEagerly(obj);

  Persistent<DeepEagerly> persistent(obj);
  PreciselyCollectGarbage();

  // Verify that the DeepEagerly chain isn't completely unravelled
  // by performing eager trace() calls, but the explicit mark
  // stack is switched once some nesting limit is exceeded.
  EXPECT_GT(DeepEagerly::s_trace_lazy_, 2);
#endif
}

TEST(HeapTest, DequeExpand) {
  // Test expansion of a HeapDeque<>'s buffer.

  typedef HeapDeque<Member<IntWrapper>> IntDeque;

  Persistent<IntDeque> deque = new IntDeque();

  // Append a sequence, bringing about repeated expansions of the
  // deque's buffer.
  int i = 0;
  for (; i < 60; ++i)
    deque->push_back(IntWrapper::Create(i));

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
    deque->push_back(IntWrapper::Create(60 + i));

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

  void Trace(blink::Visitor* visitor) {}

  int Value() const { return value_->Value(); }

 private:
  scoped_refptr<SimpleRefValue> value_;
};

}  // namespace blink

WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::PartObjectWithRef);

namespace blink {

TEST(HeapTest, DequePartObjectsExpand) {
  // Test expansion of HeapDeque<PartObject>

  using PartDeque = HeapDeque<PartObjectWithRef>;

  Persistent<PartDeque> deque = new PartDeque();
  // Auxillary Deque used to prevent 'inline' buffer expansion.
  Persistent<PartDeque> deque_unused = new PartDeque();

  // Append a sequence, bringing about repeated expansions of the
  // deque's buffer.
  int i = 0;
  for (; i < 60; ++i) {
    deque->push_back(PartObjectWithRef(i));
    deque_unused->push_back(PartObjectWithRef(i));
  }

  EXPECT_EQ(60u, deque->size());
  i = 0;
  for (const PartObjectWithRef& part : *deque) {
    EXPECT_EQ(i, part.Value());
    i++;
  }

  // Remove most of the queued objects and have the buffer's start index
  // 'point' somewhere into the buffer, just behind the end index.
  for (i = 0; i < 50; ++i)
    deque->TakeFirst();

  EXPECT_EQ(10u, deque->size());
  i = 0;
  for (const PartObjectWithRef& part : *deque) {
    EXPECT_EQ(50 + i, part.Value());
    i++;
  }

  // Append even more, eventually causing an expansion of the underlying
  // buffer once the end index wraps around and reaches the start index.
  for (i = 0; i < 70; ++i)
    deque->push_back(PartObjectWithRef(60 + i));

  // Verify that the final buffer expansion copied the start and end segments
  // of the old buffer to both ends of the expanded buffer, along with
  // re-adjusting both start&end indices in terms of that expanded buffer.
  EXPECT_EQ(80u, deque->size());
  i = 0;
  for (const PartObjectWithRef& part : *deque) {
    EXPECT_EQ(i + 50, part.Value());
    i++;
  }

  for (i = 0; i < 70; ++i)
    deque->push_back(PartObjectWithRef(130 + i));

  EXPECT_EQ(150u, deque->size());
  i = 0;
  for (const PartObjectWithRef& part : *deque) {
    EXPECT_EQ(i + 50, part.Value());
    i++;
  }
}

TEST(HeapTest, HeapVectorPartObjects) {
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

namespace {

enum GrowthDirection {
  kGrowsTowardsHigher,
  kGrowsTowardsLower,
};

NOINLINE NO_SANITIZE_ADDRESS GrowthDirection StackGrowthDirection() {
  // Disable ASan, otherwise its stack checking (use-after-return) will
  // confuse the direction check.
  static char* previous = nullptr;
  char dummy;
  if (!previous) {
    previous = &dummy;
    GrowthDirection result = StackGrowthDirection();
    previous = nullptr;
    return result;
  }
  DCHECK_NE(&dummy, previous);
  return &dummy < previous ? kGrowsTowardsLower : kGrowsTowardsHigher;
}

}  // namespace

TEST(HeapTest, StackGrowthDirection) {
  // The implementation of marking probes stack usage as it runs,
  // and has a builtin assumption that the stack grows towards
  // lower addresses.
  EXPECT_EQ(kGrowsTowardsLower, StackGrowthDirection());
}

TEST(HeapTest, StackFrameDepthDisabledByDefault) {
  StackFrameDepth depth;
  // Only allow recursion after explicitly enabling the stack limit.
  EXPECT_FALSE(depth.IsSafeToRecurse());
}

TEST(HeapTest, StackFrameDepthEnable) {
  StackFrameDepth depth;
  StackFrameDepthScope scope(&depth);
  // The scope may fail to enable recursion when the stack is close to the
  // limit. In all other cases we should be able to safely recurse.
  EXPECT_TRUE(depth.IsSafeToRecurse() || !depth.IsEnabled());
}

class TestMixinAllocationA : public GarbageCollected<TestMixinAllocationA>,
                             public GarbageCollectedMixin {
  USING_GARBAGE_COLLECTED_MIXIN(TestMixinAllocationA);

 public:
  TestMixinAllocationA() {
    // Completely wrong in general, but test only
    // runs this constructor while constructing another mixin.
    DCHECK(ThreadState::Current()->IsGCForbidden());
  }

  void Trace(blink::Visitor* visitor) override {}
};

class TestMixinAllocationB : public TestMixinAllocationA {
  USING_GARBAGE_COLLECTED_MIXIN(TestMixinAllocationB);

 public:
  TestMixinAllocationB()
      : a_(new TestMixinAllocationA())  // Construct object during a mixin
                                        // construction.
  {
    // Completely wrong in general, but test only
    // runs this constructor while constructing another mixin.
    DCHECK(ThreadState::Current()->IsGCForbidden());
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(a_);
    TestMixinAllocationA::Trace(visitor);
  }

 private:
  Member<TestMixinAllocationA> a_;
};

class TestMixinAllocationC final : public TestMixinAllocationB {
  USING_GARBAGE_COLLECTED_MIXIN(TestMixinAllocationC);

 public:
  TestMixinAllocationC() { DCHECK(!ThreadState::Current()->IsGCForbidden()); }

  void Trace(blink::Visitor* visitor) override {
    TestMixinAllocationB::Trace(visitor);
  }
};

TEST(HeapTest, NestedMixinConstruction) {
  TestMixinAllocationC* object = new TestMixinAllocationC();
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
      LargeHeapObject* large_object = LargeHeapObject::Create();
      EXPECT_TRUE(large_object);
      EXPECT_EQ(0, member->TraceCount());
    }
  }
};

class TestMixinAllocatingObject final
    : public TestMixinAllocationB,
      public ObjectWithLargeAmountsOfAllocationInConstructor {
  USING_GARBAGE_COLLECTED_MIXIN(TestMixinAllocatingObject);

 public:
  static TestMixinAllocatingObject* Create(ClassWithMember* member) {
    return new TestMixinAllocatingObject(member);
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(trace_counter_);
    TestMixinAllocationB::Trace(visitor);
  }

  int TraceCount() const { return trace_counter_->TraceCount(); }

 private:
  TestMixinAllocatingObject(ClassWithMember* member)
      : ObjectWithLargeAmountsOfAllocationInConstructor(600, member),
        trace_counter_(TraceCounter::Create()) {
    DCHECK(!ThreadState::Current()->IsGCForbidden());
    ConservativelyCollectGarbage();
    EXPECT_GT(member->TraceCount(), 0);
    EXPECT_GT(TraceCount(), 0);
  }

  Member<TraceCounter> trace_counter_;
};

TEST(HeapTest, MixinConstructionNoGC) {
  Persistent<ClassWithMember> object = ClassWithMember::Create();
  EXPECT_EQ(0, object->TraceCount());
  TestMixinAllocatingObject* mixin =
      TestMixinAllocatingObject::Create(object.Get());
  EXPECT_TRUE(mixin);
  EXPECT_GT(object->TraceCount(), 0);
  EXPECT_GT(mixin->TraceCount(), 0);
}

class WeakPersistentHolder final {
 public:
  explicit WeakPersistentHolder(IntWrapper* object) : object_(object) {}
  IntWrapper* Object() const { return object_; }

 private:
  WeakPersistent<IntWrapper> object_;
};

TEST(HeapTest, WeakPersistent) {
  Persistent<IntWrapper> object = new IntWrapper(20);
  std::unique_ptr<WeakPersistentHolder> holder =
      std::make_unique<WeakPersistentHolder>(object);
  PreciselyCollectGarbage();
  EXPECT_TRUE(holder->Object());
  object = nullptr;
  PreciselyCollectGarbage();
  EXPECT_FALSE(holder->Object());
}

namespace {

void WorkerThreadMainForCrossThreadWeakPersistentTest(
    DestructorLockingObject** object) {
  // Step 2: Create an object and store the pointer.
  MutexLocker locker(WorkerThreadMutex());
  ThreadState::AttachCurrentThread();
  *object = DestructorLockingObject::Create();
  WakeMainThread();
  ParkWorkerThread();

  // Step 4: Run a GC.
  ThreadState::Current()->CollectGarbage(
      BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
      BlinkGC::kEagerSweeping, BlinkGC::GCReason::kForcedGC);
  WakeMainThread();
  ParkWorkerThread();

  // Step 6: Finish.
  ThreadState::DetachCurrentThread();
  WakeMainThread();
}

}  // anonymous namespace

TEST(HeapTest, CrossThreadWeakPersistent) {
  // Create an object in the worker thread, have a CrossThreadWeakPersistent
  // pointing to it on the main thread, clear the reference in the worker
  // thread, run a GC in the worker thread, and see if the
  // CrossThreadWeakPersistent is cleared.

  DestructorLockingObject::destructor_calls_ = 0;

  // Step 1: Initiate a worker thread, and wait for |object| to get allocated on
  // the worker thread.
  MutexLocker main_thread_mutex_locker(MainThreadMutex());
  std::unique_ptr<Thread> worker_thread = Platform::Current()->CreateThread(
      ThreadCreationParams(WebThreadType::kTestThread)
          .SetThreadNameForTest("Test Worker Thread"));
  DestructorLockingObject* object = nullptr;
  PostCrossThreadTask(
      *worker_thread->GetTaskRunner(), FROM_HERE,
      CrossThreadBind(WorkerThreadMainForCrossThreadWeakPersistentTest,
                      CrossThreadUnretained(&object)));
  ParkMainThread();

  // Step 3: Set up a CrossThreadWeakPersistent.
  ASSERT_TRUE(object);
  CrossThreadWeakPersistent<DestructorLockingObject>
      cross_thread_weak_persistent(object);
  object = nullptr;
  EXPECT_EQ(0, DestructorLockingObject::destructor_calls_);

  // Pretend we have no pointers on stack during the step 4.
  WakeWorkerThread();
  ParkMainThread();

  // Step 5: Make sure the weak persistent is cleared.
  EXPECT_FALSE(cross_thread_weak_persistent.Get());
  EXPECT_EQ(1, DestructorLockingObject::destructor_calls_);

  WakeWorkerThread();
  ParkMainThread();
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
    ThreadState::AttachCurrentThread();
    EXPECT_EQ(42, ThreadSpecificIntWrapper().Value());
    RunWhileAttached();
    ThreadState::DetachCurrentThread();
    AtomicDecrement(&threads_to_finish_);
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
      handle = new IntWrapper(42);
      handle.RegisterAsStaticReference();
    }
    return *handle;
  }
};

class ThreadedClearOnShutdownTester::HeapObject final
    : public GarbageCollectedFinalized<
          ThreadedClearOnShutdownTester::HeapObject> {
 public:
  static HeapObject* Create(bool test_destructor) {
    return new HeapObject(test_destructor);
  }

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
    GetHeapObjectSet().insert(Create(false));
  }

  void Trace(blink::Visitor* visitor) {}

 private:
  explicit HeapObject(bool test_destructor)
      : test_destructor_(test_destructor) {}

  bool test_destructor_;
};

ThreadedClearOnShutdownTester::WeakHeapObjectSet&
ThreadedClearOnShutdownTester::GetWeakHeapObjectSet() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<Persistent<WeakHeapObjectSet>>,
                                  singleton, ());
  Persistent<WeakHeapObjectSet>& singleton_persistent = *singleton;
  if (!singleton_persistent) {
    singleton_persistent = new WeakHeapObjectSet();
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
    singleton_persistent = new HeapObjectSet();
    singleton_persistent.RegisterAsStaticReference();
  }
  return *singleton_persistent;
}

void ThreadedClearOnShutdownTester::RunWhileAttached() {
  EXPECT_EQ(42, ThreadSpecificIntWrapper().Value());
  // Creates a thread-specific singleton to a weakly held object.
  GetWeakHeapObjectSet().insert(HeapObject::Create(true));
}

}  // namespace

TEST(HeapTest, TestClearOnShutdown) {
  ThreadedClearOnShutdownTester::Test();
}

// Verify that WeakMember<const T> compiles and behaves as expected.
class WithWeakConstObject final : public GarbageCollected<WithWeakConstObject> {
 public:
  static WithWeakConstObject* Create(const IntWrapper* int_wrapper) {
    return new WithWeakConstObject(int_wrapper);
  }

  void Trace(blink::Visitor* visitor) { visitor->Trace(wrapper_); }

  const IntWrapper* Value() const { return wrapper_; }

 private:
  WithWeakConstObject(const IntWrapper* int_wrapper) : wrapper_(int_wrapper) {}

  WeakMember<const IntWrapper> wrapper_;
};

TEST(HeapTest, TestWeakConstObject) {
  Persistent<WithWeakConstObject> weak_wrapper;
  {
    const IntWrapper* wrapper = IntWrapper::Create(42);
    weak_wrapper = WithWeakConstObject::Create(wrapper);
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

TEST(HeapTest, IsGarbageCollected) {
  // Static sanity checks covering the correct operation of
  // IsGarbageCollectedType<>.

  static_assert(WTF::IsGarbageCollectedType<SimpleObject>::value,
                "GarbageCollected<>");
  static_assert(WTF::IsGarbageCollectedType<const SimpleObject>::value,
                "const GarbageCollected<>");
  static_assert(WTF::IsGarbageCollectedType<IntWrapper>::value,
                "GarbageCollectedFinalized<>");
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
  static_assert(WTF::IsGarbageCollectedType<
                    HeapTerminatedArray<Member<IntWrapper>>>::value,
                "HeapTerminatedArray");
}

TEST(HeapTest, HeapHashMapCallsDestructor) {
  String string = "string";
  EXPECT_TRUE(string.Impl()->HasOneRef());

  HeapHashMap<KeyWithCopyingMoveConstructor, Member<IntWrapper>> map;

  EXPECT_TRUE(string.Impl()->HasOneRef());

  for (int i = 1; i <= 100; ++i) {
    KeyWithCopyingMoveConstructor key(i, string);
    map.insert(key, IntWrapper::Create(i));
  }

  EXPECT_FALSE(string.Impl()->HasOneRef());
  map.clear();

  EXPECT_TRUE(string.Impl()->HasOneRef());
}

class DoublyLinkedListNodeImpl
    : public GarbageCollectedFinalized<DoublyLinkedListNodeImpl>,
      public DoublyLinkedListNode<DoublyLinkedListNodeImpl> {
 public:
  DoublyLinkedListNodeImpl() = default;
  static DoublyLinkedListNodeImpl* Create() {
    return new DoublyLinkedListNodeImpl();
  }

  static int destructor_calls_;
  ~DoublyLinkedListNodeImpl() { ++destructor_calls_; }

  void Trace(Visitor* visitor) {
    visitor->Trace(prev_);
    visitor->Trace(next_);
  }

 private:
  friend class WTF::DoublyLinkedListNode<DoublyLinkedListNodeImpl>;
  Member<DoublyLinkedListNodeImpl> prev_;
  Member<DoublyLinkedListNodeImpl> next_;
};

int DoublyLinkedListNodeImpl::destructor_calls_ = 0;

template <typename T>
class HeapDoublyLinkedListContainer
    : public GarbageCollected<HeapDoublyLinkedListContainer<T>> {
 public:
  static HeapDoublyLinkedListContainer<T>* Create() {
    return new HeapDoublyLinkedListContainer<T>();
  }
  HeapDoublyLinkedListContainer<T>() = default;
  HeapDoublyLinkedList<T> list_;
  void Trace(Visitor* visitor) { visitor->Trace(list_); }
};

TEST(HeapTest, HeapDoublyLinkedList) {
  Persistent<HeapDoublyLinkedListContainer<DoublyLinkedListNodeImpl>>
      container =
          HeapDoublyLinkedListContainer<DoublyLinkedListNodeImpl>::Create();
  DoublyLinkedListNodeImpl::destructor_calls_ = 0;

  container->list_.Append(DoublyLinkedListNodeImpl::Create());
  container->list_.Append(DoublyLinkedListNodeImpl::Create());

  PreciselyCollectGarbage();
  EXPECT_EQ(DoublyLinkedListNodeImpl::destructor_calls_, 0);

  container->list_.RemoveHead();
  PreciselyCollectGarbage();
  EXPECT_EQ(DoublyLinkedListNodeImpl::destructor_calls_, 1);

  container->list_.RemoveHead();
  PreciselyCollectGarbage();
  EXPECT_EQ(DoublyLinkedListNodeImpl::destructor_calls_, 2);
}

TEST(HeapTest, PromptlyFreeStackAllocatedHeapVector) {
  NormalPageArena* normal_arena;
  Address before;
  {
    HeapVector<Member<IntWrapper>> vector;
    vector.push_back(new IntWrapper(0));
    NormalPage* normal_page =
        static_cast<NormalPage*>(PageFromObject(vector.data()));
    normal_arena = normal_page->ArenaForNormalPage();
    CHECK(normal_arena);
    before = normal_arena->CurrentAllocationPoint();
  }
  Address after = normal_arena->CurrentAllocationPoint();
  // We check the allocation point to see if promptly freed
  EXPECT_NE(after, before);
}

TEST(HeapTest, PromptlyFreeStackAllocatedHeapDeque) {
  NormalPageArena* normal_arena;
  Address before;
  {
    HeapDeque<Member<IntWrapper>> deque;
    deque.push_back(new IntWrapper(0));
    NormalPage* normal_page =
        static_cast<NormalPage*>(PageFromObject(&deque.front()));
    normal_arena = normal_page->ArenaForNormalPage();
    CHECK(normal_arena);
    before = normal_arena->CurrentAllocationPoint();
  }
  Address after = normal_arena->CurrentAllocationPoint();
  // We check the allocation point to see if promptly freed
  EXPECT_NE(after, before);
}

TEST(HeapTest, PromptlyFreeStackAllocatedHeapHashSet) {
  NormalPageArena* normal_arena = static_cast<NormalPageArena*>(
      ThreadState::Current()->Heap().Arena(BlinkGC::kHashTableArenaIndex));
  CHECK(normal_arena);
  Address before;
  {
    HeapHashSet<Member<IntWrapper>> hash_set;
    hash_set.insert(new IntWrapper(0));
    before = normal_arena->CurrentAllocationPoint();
  }
  Address after = normal_arena->CurrentAllocationPoint();
  // We check the allocation point to see if promptly freed
  EXPECT_NE(after, before);
}

TEST(HeapTest, PromptlyFreeStackAllocatedHeapListHashSet) {
  NormalPageArena* normal_arena = static_cast<NormalPageArena*>(
      ThreadState::Current()->Heap().Arena(BlinkGC::kHashTableArenaIndex));
  CHECK(normal_arena);
  Address before;
  {
    HeapListHashSet<Member<IntWrapper>> list_hash_set;
    list_hash_set.insert(new IntWrapper(0));
    before = normal_arena->CurrentAllocationPoint();
  }
  Address after = normal_arena->CurrentAllocationPoint();
  // We check the allocation point to see if promptly freed
  EXPECT_NE(after, before);
}

TEST(HeapTest, PromptlyFreeStackAllocatedHeapLinkedHashSet) {
  NormalPageArena* normal_arena = static_cast<NormalPageArena*>(
      ThreadState::Current()->Heap().Arena(BlinkGC::kHashTableArenaIndex));
  CHECK(normal_arena);
  Address before;
  {
    HeapLinkedHashSet<Member<IntWrapper>> linked_hash_set;
    linked_hash_set.insert(new IntWrapper(0));
    before = normal_arena->CurrentAllocationPoint();
  }
  Address after = normal_arena->CurrentAllocationPoint();
  // We check the allocation point to see if promptly freed
  EXPECT_NE(after, before);
}

TEST(HeapTest, ShrinkVector) {
  // Regression test: https://crbug.com/823289

  HeapVector<Member<IntWrapper>> vector;
  vector.ReserveCapacity(32);
  for (int i = 0; i < 4; i++) {
    vector.push_back(new IntWrapper(i));
  }

  ConservativelyCollectGarbage(BlinkGC::kLazySweeping);

  // The following call tries to promptly free the left overs. In the buggy
  // scenario that would create a free HeapObjectHeader that is assumed to be
  // black which it is not.
  vector.ShrinkToFit();
}

}  // namespace blink
