// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_GARBAGE_COLLECTED_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_GARBAGE_COLLECTED_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

namespace blink {

template <typename T>
class GarbageCollected;
class HeapObjectHeader;

// GC_PLUGIN_IGNORE is used to make the plugin ignore a particular class or
// field when checking for proper usage.  When using GC_PLUGIN_IGNORE
// a bug-number should be provided as an argument where the bug describes
// what needs to happen to remove the GC_PLUGIN_IGNORE again.
#if defined(__clang__)
#define GC_PLUGIN_IGNORE(bug) \
  __attribute__((annotate("blink_gc_plugin_ignore")))
#else
#define GC_PLUGIN_IGNORE(bug)
#endif

// Template to determine if a class is a GarbageCollectedMixin by checking if it
// has IsGarbageCollectedMixinMarker
template <typename T>
struct IsGarbageCollectedMixin {
 private:
  typedef char YesType;
  struct NoType {
    char padding[8];
  };

  template <typename U>
  static YesType CheckMarker(typename U::IsGarbageCollectedMixinMarker*);
  template <typename U>
  static NoType CheckMarker(...);

 public:
  static const bool value = sizeof(CheckMarker<T>(nullptr)) == sizeof(YesType);
};

// TraceDescriptor is used to describe how to trace an object.
struct TraceDescriptor {
  STACK_ALLOCATED();

 public:
  // The adjusted base pointer of the object that should be traced.
  void* base_object_payload;
  // A callback for tracing the object.
  TraceCallback callback;
};

// The GarbageCollectedMixin interface and helper macro
// USING_GARBAGE_COLLECTED_MIXIN can be used to automatically define
// TraceTrait/ObjectAliveTrait on non-leftmost deriving classes
// which need to be garbage collected.
//
// Consider the following case:
// class B {};
// class A : public GarbageCollected, public B {};
//
// We can't correctly handle "Member<B> p = &a" as we can't compute addr of
// object header statically. This can be solved by using GarbageCollectedMixin:
// class B : public GarbageCollectedMixin {};
// class A : public GarbageCollected, public B {
//   USING_GARBAGE_COLLECTED_MIXIN(A);
// };
//
// The classes involved and the helper macros allow for properly handling
// definitions for Member<B> and friends. The mechanisms for handling Member<B>
// involve dynamic dispatch. Note that for Member<A> all methods and pointers
// are statically computed and no dynamic dispatch is involved.
//
// Note that garbage collections are allowed during mixin construction as
// conservative scanning of objects does not rely on the Trace method but rather
// scans the object field by field.
class PLATFORM_EXPORT GarbageCollectedMixin {
 public:
  typedef int IsGarbageCollectedMixinMarker;
  virtual void Trace(Visitor*) {}
  // Provide default implementations that indicate that the vtable is not yet
  // set up properly. This way it is possible to get infos about mixins so that
  // these objects can processed later on. This is necessary as
  // not-fully-constructed mixin objects potentially require being processed
  // as part emitting a write barrier for incremental marking. See
  // |IncrementalMarkingTest::WriteBarrierDuringMixinConstruction| as an
  // example.
  //
  // The not-fully-constructed objects are handled as follows:
  //   1. Write barrier or marking of not fully constructed mixin gets called.
  //   2. Default implementation of GetTraceDescriptor (and friends) returns
  //      kNotFullyConstructedObject as object base payload.
  //   3. Visitor (e.g. MarkingVisitor) can intercept that value and delay
  //      processing that object until the atomic pause.
  //   4. In the atomic phase, mark all not-fully-constructed objects that have
  //      found in the step 1.-3. conservatively.
  //
  // In general, delaying is required as write barriers are omitted in certain
  // scenarios, e.g., initializing stores. As a result, we cannot depend on the
  // write barriers for catching writes to member fields and thus have to
  // process the object (instead of just marking only the header).
  virtual HeapObjectHeader* GetHeapObjectHeader() const {
    return reinterpret_cast<HeapObjectHeader*>(
        BlinkGC::kNotFullyConstructedObject);
  }
  virtual TraceDescriptor GetTraceDescriptor() const {
    return {BlinkGC::kNotFullyConstructedObject, nullptr};
  }
};

#define DEFINE_GARBAGE_COLLECTED_MIXIN_METHODS(TYPE)                         \
 public:                                                                     \
  HeapObjectHeader* GetHeapObjectHeader() const override {                   \
    static_assert(                                                           \
        WTF::IsSubclassOfTemplate<typename std::remove_const<TYPE>::type,    \
                                  blink::GarbageCollected>::value,           \
        "only garbage collected objects can have garbage collected mixins"); \
    return HeapObjectHeader::FromPayload(static_cast<const TYPE*>(this));    \
  }                                                                          \
                                                                             \
  TraceDescriptor GetTraceDescriptor() const override {                      \
    return {const_cast<TYPE*>(static_cast<const TYPE*>(this)),               \
            TraceTrait<TYPE>::Trace};                                        \
  }                                                                          \
                                                                             \
 private:

// The Oilpan GC plugin checks for proper usages of the
// USING_GARBAGE_COLLECTED_MIXIN macro using a typedef marker.
#define DEFINE_GARBAGE_COLLECTED_MIXIN_CONSTRUCTOR_MARKER(TYPE) \
 public:                                                        \
  typedef int HasUsingGarbageCollectedMixinMacro;               \
                                                                \
 private:

// The USING_GARBAGE_COLLECTED_MIXIN macro defines all methods and markers
// needed for handling mixins.
#define USING_GARBAGE_COLLECTED_MIXIN(TYPE)               \
  DEFINE_GARBAGE_COLLECTED_MIXIN_CONSTRUCTOR_MARKER(TYPE) \
  DEFINE_GARBAGE_COLLECTED_MIXIN_METHODS(TYPE)            \
  IS_GARBAGE_COLLECTED_TYPE()

// Merge two or more Mixins into one:
//
//  class A : public GarbageCollectedMixin {};
//  class B : public GarbageCollectedMixin {};
//  class C : public A, public B {
//    // C::GetTraceDescriptor is now ambiguous because there are two
//    // candidates: A::GetTraceDescriptor and B::GetTraceDescriptor.  Ditto for
//    // other functions.
//
//    MERGE_GARBAGE_COLLECTED_MIXINS();
//    // The macro defines C::GetTraceDescriptor, similar to
//    GarbageCollectedMixin,
//    // so that they are no longer ambiguous.
//    // USING_GARBAGE_COLLECTED_MIXIN(TYPE) overrides them later and provides
//    // the implementations.
//  };
#define MERGE_GARBAGE_COLLECTED_MIXINS()                   \
 public:                                                   \
  HeapObjectHeader* GetHeapObjectHeader() const override { \
    return reinterpret_cast<HeapObjectHeader*>(            \
        BlinkGC::kNotFullyConstructedObject);              \
  }                                                        \
  TraceDescriptor GetTraceDescriptor() const override {    \
    return {BlinkGC::kNotFullyConstructedObject, nullptr}; \
  }                                                        \
                                                           \
 private:                                                  \
  using merge_garbage_collected_mixins_requires_semicolon = void

// Base class for objects allocated in the Blink garbage-collected heap.
//
// Instances of GarbageCollected will be finalized if they are non-trivially
// destructible.
template <typename T>
class GarbageCollected;

template <typename T,
          bool = WTF::IsSubclassOfTemplate<typename std::remove_const<T>::type,
                                           GarbageCollected>::value>
class NeedsAdjustPointer;

template <typename T>
class NeedsAdjustPointer<T, true> {
  static_assert(sizeof(T), "T must be fully defined");

 public:
  static const bool value = false;
};

template <typename T>
class NeedsAdjustPointer<T, false> {
  static_assert(sizeof(T), "T must be fully defined");

 public:
  static const bool value =
      IsGarbageCollectedMixin<typename std::remove_const<T>::type>::value;
};

// TODO(sof): migrate to wtf/TypeTraits.h
template <typename T>
class IsFullyDefined {
  using TrueType = char;
  struct FalseType {
    char dummy[2];
  };

  template <typename U, size_t sz = sizeof(U)>
  static TrueType IsSizeofKnown(U*);
  static FalseType IsSizeofKnown(...);
  static T& t_;

 public:
  static const bool value = sizeof(TrueType) == sizeof(IsSizeofKnown(&t_));
};

}  // namespace blink

#endif
