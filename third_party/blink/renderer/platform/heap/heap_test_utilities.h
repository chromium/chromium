/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_TEST_UTILITIES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_TEST_UTILITIES_H_

#include "base/callback.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/blink_gc.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/impl/trace_traits.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class TestSupportingGC : public testing::Test {
 public:
  // Performs a precise garbage collection with eager sweeping.
  static void PreciselyCollectGarbage(
      BlinkGC::SweepingType sweeping_type = BlinkGC::kEagerSweeping);

  // Performs a conservative garbage collection.
  static void ConservativelyCollectGarbage(
      BlinkGC::SweepingType sweeping_type = BlinkGC::kEagerSweeping);

  ~TestSupportingGC() override;

  // Performs multiple rounds of garbage collections until no more memory can be
  // freed. This is useful to avoid other garbage collections having to deal
  // with stale memory.
  void ClearOutOldGarbage();

  // Completes sweeping if it is currently running.
  void CompleteSweepingIfNeeded();

 protected:
  base::test::TaskEnvironment task_environment_;
};

template <typename T>
class ObjectWithCallbackBeforeInitializer
    : public GarbageCollected<ObjectWithCallbackBeforeInitializer<T>> {
 public:
  ObjectWithCallbackBeforeInitializer(
      base::OnceCallback<void(ObjectWithCallbackBeforeInitializer<T>*)>&& cb,
      T* value)
      : bool_(ExecuteCallbackReturnTrue(this, std::move(cb))), value_(value) {}

  ObjectWithCallbackBeforeInitializer(
      base::OnceCallback<void(ObjectWithCallbackBeforeInitializer<T>*)>&& cb)
      : bool_(ExecuteCallbackReturnTrue(this, std::move(cb))) {}

  virtual void Trace(Visitor* visitor) const { visitor->Trace(value_); }

  T* value() const { return value_.Get(); }

 private:
  static bool ExecuteCallbackReturnTrue(
      ObjectWithCallbackBeforeInitializer* thiz,
      base::OnceCallback<void(ObjectWithCallbackBeforeInitializer<T>*)>&& cb) {
    std::move(cb).Run(thiz);
    return true;
  }

  bool bool_;
  Member<T> value_;
};

template <typename T>
class MixinWithCallbackBeforeInitializer : public GarbageCollectedMixin {
 public:
  MixinWithCallbackBeforeInitializer(
      base::OnceCallback<void(MixinWithCallbackBeforeInitializer<T>*)>&& cb,
      T* value)
      : bool_(ExecuteCallbackReturnTrue(this, std::move(cb))), value_(value) {}

  MixinWithCallbackBeforeInitializer(
      base::OnceCallback<void(MixinWithCallbackBeforeInitializer<T>*)>&& cb)
      : bool_(ExecuteCallbackReturnTrue(this, std::move(cb))) {}

  void Trace(Visitor* visitor) const override { visitor->Trace(value_); }

  T* value() const { return value_.Get(); }

 private:
  static bool ExecuteCallbackReturnTrue(
      MixinWithCallbackBeforeInitializer* thiz,
      base::OnceCallback<void(MixinWithCallbackBeforeInitializer<T>*)>&& cb) {
    std::move(cb).Run(thiz);
    return true;
  }

  bool bool_;
  Member<T> value_;
};

class BoolMixin {
 protected:
  bool bool_ = false;
};

template <typename T>
class ObjectWithMixinWithCallbackBeforeInitializer
    : public GarbageCollected<ObjectWithMixinWithCallbackBeforeInitializer<T>>,
      public BoolMixin,
      public MixinWithCallbackBeforeInitializer<T> {
 public:
  using Mixin = MixinWithCallbackBeforeInitializer<T>;

  ObjectWithMixinWithCallbackBeforeInitializer(
      base::OnceCallback<void(Mixin*)>&& cb,
      T* value)
      : Mixin(std::move(cb), value) {}

  ObjectWithMixinWithCallbackBeforeInitializer(
      base::OnceCallback<void(Mixin*)>&& cb)
      : Mixin(std::move(cb)) {}

  void Trace(Visitor* visitor) const override { Mixin::Trace(visitor); }
};

// Simple linked object to be used in tests.
class LinkedObject : public GarbageCollected<LinkedObject> {
 public:
  LinkedObject() = default;
  explicit LinkedObject(LinkedObject* next) : next_(next) {}

  void set_next(LinkedObject* next) { next_ = next; }
  LinkedObject* next() const { return next_; }
  Member<LinkedObject>& next_ref() { return next_; }

  virtual void Trace(Visitor* visitor) const { visitor->Trace(next_); }

 private:
  Member<LinkedObject> next_;
};

// Test driver for incremental marking. Assumes that no stack handling is
// required.
class IncrementalMarkingTestDriver {
 public:
  explicit IncrementalMarkingTestDriver(ThreadState* thread_state)
      : thread_state_(thread_state) {}
  ~IncrementalMarkingTestDriver();

  void Start();
  bool SingleStep(BlinkGC::StackState stack_state =
                      BlinkGC::StackState::kNoHeapPointersOnStack);
  void FinishSteps(BlinkGC::StackState stack_state =
                       BlinkGC::StackState::kNoHeapPointersOnStack);
  void FinishGC(bool complete_sweep = true);

  // Methods for forcing a concurrent marking step without any assistance from
  // mutator thread (i.e. without incremental marking on the mutator thread).
  bool SingleConcurrentStep(BlinkGC::StackState stack_state =
                                BlinkGC::StackState::kNoHeapPointersOnStack);
  void FinishConcurrentSteps(BlinkGC::StackState stack_state =
                                 BlinkGC::StackState::kNoHeapPointersOnStack);

  size_t GetHeapCompactLastFixupCount() const;

 private:
  ThreadState* const thread_state_;
};

class IntegerObject : public GarbageCollected<IntegerObject> {
 public:
  static std::atomic_int destructor_calls;

  explicit IntegerObject(int x) : x_(x) {}

  virtual ~IntegerObject() {
    destructor_calls.fetch_add(1, std::memory_order_relaxed);
  }

  virtual void Trace(Visitor* visitor) const {}

  int Value() const { return x_; }

  bool operator==(const IntegerObject& other) const {
    return other.Value() == Value();
  }

  unsigned GetHash() { return IntHash<int>::GetHash(x_); }

 private:
  int x_;
};

struct IntegerObjectHash {
  static unsigned GetHash(const IntegerObject& key) {
    return WTF::HashInt(static_cast<uint32_t>(key.Value()));
  }

  static bool Equal(const IntegerObject& a, const IntegerObject& b) {
    return a == b;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_TEST_UTILITIES_H_
