// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_PERSISTENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_PERSISTENT_H_

#include "base/bind.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/heap_compact.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/persistent_node.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/atomics.h"

namespace blink {

// Marker used to annotate persistent objects and collections with,
// so as to enable reliable testing for persistent references via
// a type trait (see TypeTraits.h's IsPersistentReferenceType<>.)
#define IS_PERSISTENT_REFERENCE_TYPE()         \
 public:                                       \
  using IsPersistentReferenceTypeMarker = int; \
                                               \
 private:

enum WeaknessPersistentConfiguration {
  kNonWeakPersistentConfiguration,
  kWeakPersistentConfiguration
};

enum CrossThreadnessPersistentConfiguration {
  kSingleThreadPersistentConfiguration,
  kCrossThreadPersistentConfiguration
};

template <typename T,
          WeaknessPersistentConfiguration weaknessConfiguration,
          CrossThreadnessPersistentConfiguration crossThreadnessConfiguration>
class PersistentBase {
  USING_FAST_MALLOC(PersistentBase);
  IS_PERSISTENT_REFERENCE_TYPE();

 public:
  PersistentBase() : raw_(nullptr) {
    SaveCreationThreadHeap();
    Initialize();
  }

  PersistentBase(std::nullptr_t) : raw_(nullptr) {
    SaveCreationThreadHeap();
    Initialize();
  }

  PersistentBase(T* raw) : raw_(raw) {
    SaveCreationThreadHeap();
    Initialize();
    CheckPointer();
  }

  PersistentBase(T& raw) : raw_(&raw) {
    SaveCreationThreadHeap();
    Initialize();
    CheckPointer();
  }

  PersistentBase(const PersistentBase& other) : raw_(other) {
    SaveCreationThreadHeap();
    Initialize();
    CheckPointer();
  }

  template <typename U>
  PersistentBase(const PersistentBase<U,
                                      weaknessConfiguration,
                                      crossThreadnessConfiguration>& other)
      : raw_(other) {
    SaveCreationThreadHeap();
    Initialize();
    CheckPointer();
  }

  template <typename U>
  PersistentBase(const Member<U>& other) : raw_(other) {
    SaveCreationThreadHeap();
    Initialize();
    CheckPointer();
  }

  PersistentBase(WTF::HashTableDeletedValueType)
      : raw_(reinterpret_cast<T*>(-1)) {
    SaveCreationThreadHeap();
    Initialize();
    CheckPointer();
  }

  ~PersistentBase() {
    Uninitialize();
    raw_ = nullptr;
  }

  bool IsHashTableDeletedValue() const {
    return raw_ == reinterpret_cast<T*>(-1);
  }

  T* Release() {
    T* result = raw_;
    Assign(nullptr);
    return result;
  }

  void Clear() { Assign(nullptr); }
  T& operator*() const {
    CheckPointer();
    return *raw_;
  }
  explicit operator bool() const { return raw_; }
  // TODO(https://crbug.com/653394): Consider returning a thread-safe best
  // guess of validity.
  bool MaybeValid() const { return true; }
  operator T*() const {
    CheckPointer();
    return raw_;
  }
  T* operator->() const { return *this; }

  T* Get() const {
    CheckPointer();
    return raw_;
  }

  template <typename U>
  PersistentBase& operator=(U* other) {
    Assign(other);
    return *this;
  }

  PersistentBase& operator=(std::nullptr_t) {
    Assign(nullptr);
    return *this;
  }

  PersistentBase& operator=(const PersistentBase& other) {
    Assign(other);
    return *this;
  }

  template <typename U>
  PersistentBase& operator=(
      const PersistentBase<U,
                           weaknessConfiguration,
                           crossThreadnessConfiguration>& other) {
    Assign(other);
    return *this;
  }

  template <typename U>
  PersistentBase& operator=(const Member<U>& other) {
    Assign(other);
    return *this;
  }

  // Register the persistent node as a 'static reference',
  // belonging to the current thread and a persistent that must
  // be cleared when the ThreadState itself is cleared out and
  // destructed.
  //
  // Static singletons arrange for this to happen, either to ensure
  // clean LSan leak reports or to register a thread-local persistent
  // needing to be cleared out before the thread is terminated.
  PersistentBase* RegisterAsStaticReference() {
    CHECK_EQ(weaknessConfiguration, kNonWeakPersistentConfiguration);
    if (persistent_node_) {
      DCHECK(ThreadState::Current());
      ThreadState::Current()->RegisterStaticPersistentNode(persistent_node_,
                                                           nullptr);
      LEAK_SANITIZER_IGNORE_OBJECT(this);
    }
    return this;
  }

  NO_SANITIZE_ADDRESS
  void ClearWithLockHeld() {
    static_assert(
        crossThreadnessConfiguration == kCrossThreadPersistentConfiguration,
        "This Persistent does not require the cross-thread lock.");
#if DCHECK_IS_ON()
    ProcessHeap::CrossThreadPersistentMutex().AssertAcquired();
#endif
    raw_ = nullptr;
    CrossThreadPersistentRegion& region =
        weaknessConfiguration == kWeakPersistentConfiguration
            ? ProcessHeap::GetCrossThreadWeakPersistentRegion()
            : ProcessHeap::GetCrossThreadPersistentRegion();
    region.FreePersistentNode(persistent_node_);
  }

 protected:
  NO_SANITIZE_ADDRESS
  T* AtomicGet() {
    return reinterpret_cast<T*>(AcquireLoad(reinterpret_cast<void* volatile*>(
        const_cast<typename std::remove_const<T>::type**>(&raw_))));
  }

 private:
  NO_SANITIZE_ADDRESS
  void Assign(T* ptr) {
    if (crossThreadnessConfiguration == kCrossThreadPersistentConfiguration) {
      MutexLocker persistent_lock(ProcessHeap::CrossThreadPersistentMutex());
      raw_ = ptr;
    } else {
      raw_ = ptr;
    }
    CheckPointer();
    if (raw_) {
      if (!persistent_node_)
        Initialize();
      return;
    }
    Uninitialize();
  }

  template <typename VisitorDispatcher>
  void TracePersistent(VisitorDispatcher visitor) {
    static_assert(sizeof(T), "T must be fully defined");
    static_assert(IsGarbageCollectedType<T>::value,
                  "T needs to be a garbage collected object");
    if (weaknessConfiguration == kWeakPersistentConfiguration) {
      visitor->RegisterWeakCallback(this, HandleWeakPersistent);
    } else {
      visitor->Trace(raw_);
    }
  }

  NO_SANITIZE_ADDRESS
  void Initialize() {
    DCHECK(!persistent_node_);
    if (!raw_ || IsHashTableDeletedValue())
      return;

    TraceCallback trace_callback =
        TraceMethodDelegate<PersistentBase,
                            &PersistentBase::TracePersistent>::Trampoline;
    if (crossThreadnessConfiguration == kCrossThreadPersistentConfiguration) {
      CrossThreadPersistentRegion& region =
          weaknessConfiguration == kWeakPersistentConfiguration
              ? ProcessHeap::GetCrossThreadWeakPersistentRegion()
              : ProcessHeap::GetCrossThreadPersistentRegion();
      MutexLocker lock(ProcessHeap::CrossThreadPersistentMutex());
      region.AllocatePersistentNode(persistent_node_, this, trace_callback);
      return;
    }
    ThreadState* state =
        ThreadStateFor<ThreadingTrait<T>::kAffinity>::GetState();
    DCHECK(state->CheckThread());
    PersistentRegion* region =
        weaknessConfiguration == kWeakPersistentConfiguration
            ? state->GetWeakPersistentRegion()
            : state->GetPersistentRegion();
    persistent_node_ = region->AllocatePersistentNode(this, trace_callback);
#if DCHECK_IS_ON()
    state_ = state;
#endif
  }

  void Uninitialize() {
    if (crossThreadnessConfiguration == kCrossThreadPersistentConfiguration) {
      if (AcquireLoad(reinterpret_cast<void* volatile*>(&persistent_node_))) {
        CrossThreadPersistentRegion& region =
            weaknessConfiguration == kWeakPersistentConfiguration
                ? ProcessHeap::GetCrossThreadWeakPersistentRegion()
                : ProcessHeap::GetCrossThreadPersistentRegion();
        MutexLocker lock(ProcessHeap::CrossThreadPersistentMutex());
        region.FreePersistentNode(persistent_node_);
      }
      return;
    }

    if (!persistent_node_)
      return;
    ThreadState* state =
        ThreadStateFor<ThreadingTrait<T>::kAffinity>::GetState();
    DCHECK(state->CheckThread());
    // Persistent handle must be created and destructed in the same thread.
#if DCHECK_IS_ON()
    DCHECK_EQ(state_, state);
#endif
    PersistentRegion* region =
        weaknessConfiguration == kWeakPersistentConfiguration
            ? state->GetWeakPersistentRegion()
            : state->GetPersistentRegion();
    state->FreePersistentNode(region, persistent_node_);
    persistent_node_ = nullptr;
  }

  void CheckPointer() const {
#if DCHECK_IS_ON()
    if (!raw_ || IsHashTableDeletedValue())
      return;

    if (crossThreadnessConfiguration != kCrossThreadPersistentConfiguration) {
      ThreadState* current = ThreadState::Current();
      DCHECK(current);
      // m_creationThreadState may be null when this is used in a heap
      // collection which initialized the Persistent with memset and the
      // constructor wasn't called.
      if (creation_thread_state_) {
        // Member should point to objects that belong in the same ThreadHeap.
        DCHECK_EQ(&ThreadState::FromObject(raw_)->Heap(),
                  &creation_thread_state_->Heap());
        // Member should point to objects that belong in the same ThreadHeap.
        DCHECK_EQ(&current->Heap(), &creation_thread_state_->Heap());
      }
    }
#endif
  }

  void SaveCreationThreadHeap() {
#if DCHECK_IS_ON()
    if (crossThreadnessConfiguration == kCrossThreadPersistentConfiguration) {
      creation_thread_state_ = nullptr;
    } else {
      creation_thread_state_ = ThreadState::Current();
      DCHECK(creation_thread_state_);
    }
#endif
  }

  static void HandleWeakPersistent(Visitor* self, void* persistent_pointer) {
    using Base =
        PersistentBase<typename std::remove_const<T>::type,
                       weaknessConfiguration, crossThreadnessConfiguration>;
    Base* persistent = reinterpret_cast<Base*>(persistent_pointer);
    T* object = persistent->Get();
    if (object && !ObjectAliveTrait<T>::IsHeapObjectAlive(object))
      ClearWeakPersistent(persistent);
  }

  static void ClearWeakPersistent(
      PersistentBase<std::remove_const_t<T>,
                     kWeakPersistentConfiguration,
                     kCrossThreadPersistentConfiguration>* persistent) {
#if DCHECK_IS_ON()
    ProcessHeap::CrossThreadPersistentMutex().AssertAcquired();
#endif
    persistent->ClearWithLockHeld();
  }

  static void ClearWeakPersistent(
      PersistentBase<std::remove_const_t<T>,
                     kWeakPersistentConfiguration,
                     kSingleThreadPersistentConfiguration>* persistent) {
    persistent->Clear();
  }

  template <typename BadPersistent>
  static void ClearWeakPersistent(BadPersistent* non_weak_persistent) {
    NOTREACHED();
  }

  // m_raw is accessed most, so put it at the first field.
  T* raw_;
  PersistentNode* persistent_node_ = nullptr;
#if DCHECK_IS_ON()
  ThreadState* state_ = nullptr;
  const ThreadState* creation_thread_state_;
#endif
};

// Persistent is a way to create a strong pointer from an off-heap object
// to another on-heap object. As long as the Persistent handle is alive
// the GC will keep the object pointed to alive. The Persistent handle is
// always a GC root from the point of view of the GC.
//
// We have to construct and destruct Persistent in the same thread.
template <typename T>
class Persistent : public PersistentBase<T,
                                         kNonWeakPersistentConfiguration,
                                         kSingleThreadPersistentConfiguration> {
  typedef PersistentBase<T,
                         kNonWeakPersistentConfiguration,
                         kSingleThreadPersistentConfiguration>
      Parent;

 public:
  Persistent() : Parent() {}
  Persistent(std::nullptr_t) : Parent(nullptr) {}
  Persistent(T* raw) : Parent(raw) {}
  Persistent(T& raw) : Parent(raw) {}
  Persistent(const Persistent& other) : Parent(other) {}
  template <typename U>
  Persistent(const Persistent<U>& other) : Parent(other) {}
  template <typename U>
  Persistent(const Member<U>& other) : Parent(other) {}
  Persistent(WTF::HashTableDeletedValueType x) : Parent(x) {}

  template <typename U>
  Persistent& operator=(U* other) {
    Parent::operator=(other);
    return *this;
  }

  Persistent& operator=(std::nullptr_t) {
    Parent::operator=(nullptr);
    return *this;
  }

  Persistent& operator=(const Persistent& other) {
    Parent::operator=(other);
    return *this;
  }

  template <typename U>
  Persistent& operator=(const Persistent<U>& other) {
    Parent::operator=(other);
    return *this;
  }

  template <typename U>
  Persistent& operator=(const Member<U>& other) {
    Parent::operator=(other);
    return *this;
  }
};

// WeakPersistent is a way to create a weak pointer from an off-heap object
// to an on-heap object. The m_raw is automatically cleared when the pointee
// gets collected.
//
// We have to construct and destruct WeakPersistent in the same thread.
//
// Note that collections of WeakPersistents are not supported. Use a collection
// of WeakMembers instead.
//
//   HashSet<WeakPersistent<T>> m_set; // wrong
//   Persistent<HeapHashSet<WeakMember<T>>> m_set; // correct
template <typename T>
class WeakPersistent
    : public PersistentBase<T,
                            kWeakPersistentConfiguration,
                            kSingleThreadPersistentConfiguration> {
  typedef PersistentBase<T,
                         kWeakPersistentConfiguration,
                         kSingleThreadPersistentConfiguration>
      Parent;

 public:
  WeakPersistent() : Parent() {}
  WeakPersistent(std::nullptr_t) : Parent(nullptr) {}
  WeakPersistent(T* raw) : Parent(raw) {}
  WeakPersistent(T& raw) : Parent(raw) {}
  WeakPersistent(const WeakPersistent& other) : Parent(other) {}
  template <typename U>
  WeakPersistent(const WeakPersistent<U>& other) : Parent(other) {}
  template <typename U>
  WeakPersistent(const Member<U>& other) : Parent(other) {}

  template <typename U>
  WeakPersistent& operator=(U* other) {
    Parent::operator=(other);
    return *this;
  }

  WeakPersistent& operator=(std::nullptr_t) {
    Parent::operator=(nullptr);
    return *this;
  }

  WeakPersistent& operator=(const WeakPersistent& other) {
    Parent::operator=(other);
    return *this;
  }

  template <typename U>
  WeakPersistent& operator=(const WeakPersistent<U>& other) {
    Parent::operator=(other);
    return *this;
  }

  template <typename U>
  WeakPersistent& operator=(const Member<U>& other) {
    Parent::operator=(other);
    return *this;
  }
};

// Unlike Persistent, we can destruct a CrossThreadPersistent in a thread
// different from the construction thread.
template <typename T>
class CrossThreadPersistent
    : public PersistentBase<T,
                            kNonWeakPersistentConfiguration,
                            kCrossThreadPersistentConfiguration> {
  typedef PersistentBase<T,
                         kNonWeakPersistentConfiguration,
                         kCrossThreadPersistentConfiguration>
      Parent;

 public:
  CrossThreadPersistent() : Parent() {}
  CrossThreadPersistent(std::nullptr_t) : Parent(nullptr) {}
  CrossThreadPersistent(T* raw) : Parent(raw) {}
  CrossThreadPersistent(T& raw) : Parent(raw) {}
  CrossThreadPersistent(const CrossThreadPersistent& other) : Parent(other) {}
  template <typename U>
  CrossThreadPersistent(const CrossThreadPersistent<U>& other)
      : Parent(other) {}
  template <typename U>
  CrossThreadPersistent(const Member<U>& other) : Parent(other) {}
  CrossThreadPersistent(WTF::HashTableDeletedValueType x) : Parent(x) {}

  T* AtomicGet() { return Parent::AtomicGet(); }

  // Instead of using release(), assign then clear() instead.
  // Using release() with per thread heap enabled can cause the object to be
  // destroyed before assigning it to a new handle.
  T* Release() = delete;

  template <typename U>
  CrossThreadPersistent& operator=(U* other) {
    Parent::operator=(other);
    return *this;
  }

  CrossThreadPersistent& operator=(std::nullptr_t) {
    Parent::operator=(nullptr);
    return *this;
  }

  CrossThreadPersistent& operator=(const CrossThreadPersistent& other) {
    Parent::operator=(other);
    return *this;
  }

  template <typename U>
  CrossThreadPersistent& operator=(const CrossThreadPersistent<U>& other) {
    Parent::operator=(other);
    return *this;
  }

  template <typename U>
  CrossThreadPersistent& operator=(const Member<U>& other) {
    Parent::operator=(other);
    return *this;
  }
};

// Combines the behavior of CrossThreadPersistent and WeakPersistent.
template <typename T>
class CrossThreadWeakPersistent
    : public PersistentBase<T,
                            kWeakPersistentConfiguration,
                            kCrossThreadPersistentConfiguration> {
  typedef PersistentBase<T,
                         kWeakPersistentConfiguration,
                         kCrossThreadPersistentConfiguration>
      Parent;

 public:
  CrossThreadWeakPersistent() : Parent() {}
  CrossThreadWeakPersistent(std::nullptr_t) : Parent(nullptr) {}
  CrossThreadWeakPersistent(T* raw) : Parent(raw) {}
  CrossThreadWeakPersistent(T& raw) : Parent(raw) {}
  CrossThreadWeakPersistent(const CrossThreadWeakPersistent& other)
      : Parent(other) {}
  template <typename U>
  CrossThreadWeakPersistent(const CrossThreadWeakPersistent<U>& other)
      : Parent(other) {}
  template <typename U>
  CrossThreadWeakPersistent(const Member<U>& other) : Parent(other) {}

  template <typename U>
  CrossThreadWeakPersistent& operator=(U* other) {
    Parent::operator=(other);
    return *this;
  }

  CrossThreadWeakPersistent& operator=(std::nullptr_t) {
    Parent::operator=(nullptr);
    return *this;
  }

  CrossThreadWeakPersistent& operator=(const CrossThreadWeakPersistent& other) {
    Parent::operator=(other);
    return *this;
  }

  template <typename U>
  CrossThreadWeakPersistent& operator=(
      const CrossThreadWeakPersistent<U>& other) {
    Parent::operator=(other);
    return *this;
  }

  template <typename U>
  CrossThreadWeakPersistent& operator=(const Member<U>& other) {
    Parent::operator=(other);
    return *this;
  }
};

template <typename T>
Persistent<T> WrapPersistent(T* value) {
  // There is no technical need to require a complete type here. However, types
  // that support wrapper-tracing are not suitable with WrapPersistent because
  // Persistent<T> does not perform wrapper-tracing. We'd like to delete such
  // overloads for sure. Thus, we require a complete type here so that it makes
  // sure that an appropriate header is included and such an overload is
  // deleted.
  static_assert(sizeof(T), "T must be fully defined");

  return Persistent<T>(value);
}

template <typename T,
          typename = std::enable_if_t<WTF::IsGarbageCollectedType<T>::value>>
Persistent<T> WrapPersistentIfNeeded(T* value) {
  return Persistent<T>(value);
}

template <typename T>
T& WrapPersistentIfNeeded(T& value) {
  return value;
}

template <typename T>
WeakPersistent<T> WrapWeakPersistent(T* value) {
  return WeakPersistent<T>(value);
}

template <typename T>
CrossThreadPersistent<T> WrapCrossThreadPersistent(T* value) {
  return CrossThreadPersistent<T>(value);
}

template <typename T>
CrossThreadWeakPersistent<T> WrapCrossThreadWeakPersistent(T* value) {
  return CrossThreadWeakPersistent<T>(value);
}

// Comparison operators between (Weak)Members, Persistents, and UntracedMembers.
template <typename T, typename U>
inline bool operator==(const Member<T>& a, const Member<U>& b) {
  return a.Get() == b.Get();
}
template <typename T, typename U>
inline bool operator!=(const Member<T>& a, const Member<U>& b) {
  return a.Get() != b.Get();
}
template <typename T, typename U>
inline bool operator==(const Persistent<T>& a, const Persistent<U>& b) {
  return a.Get() == b.Get();
}
template <typename T, typename U>
inline bool operator!=(const Persistent<T>& a, const Persistent<U>& b) {
  return a.Get() != b.Get();
}

template <typename T, typename U>
inline bool operator==(const Member<T>& a, const Persistent<U>& b) {
  return a.Get() == b.Get();
}
template <typename T, typename U>
inline bool operator!=(const Member<T>& a, const Persistent<U>& b) {
  return a.Get() != b.Get();
}
template <typename T, typename U>
inline bool operator==(const Persistent<T>& a, const Member<U>& b) {
  return a.Get() == b.Get();
}
template <typename T, typename U>
inline bool operator!=(const Persistent<T>& a, const Member<U>& b) {
  return a.Get() != b.Get();
}

}  // namespace blink

namespace WTF {

template <
    typename T,
    blink::WeaknessPersistentConfiguration weaknessConfiguration,
    blink::CrossThreadnessPersistentConfiguration crossThreadnessConfiguration>
struct VectorTraits<blink::PersistentBase<T,
                                          weaknessConfiguration,
                                          crossThreadnessConfiguration>>
    : VectorTraitsBase<blink::PersistentBase<T,
                                             weaknessConfiguration,
                                             crossThreadnessConfiguration>> {
  STATIC_ONLY(VectorTraits);
  static const bool kNeedsDestruction = true;
  static const bool kCanInitializeWithMemset = true;
  static const bool kCanClearUnusedSlotsWithMemset = false;
  static const bool kCanMoveWithMemcpy = true;
};

template <typename T>
struct HashTraits<blink::Persistent<T>>
    : HandleHashTraits<T, blink::Persistent<T>> {};

template <typename T>
struct HashTraits<blink::CrossThreadPersistent<T>>
    : HandleHashTraits<T, blink::CrossThreadPersistent<T>> {};

template <typename T>
struct DefaultHash<blink::Persistent<T>> {
  STATIC_ONLY(DefaultHash);
  using Hash = MemberHash<T>;
};

template <typename T>
struct DefaultHash<blink::WeakPersistent<T>> {
  STATIC_ONLY(DefaultHash);
  using Hash = MemberHash<T>;
};

template <typename T>
struct DefaultHash<blink::CrossThreadPersistent<T>> {
  STATIC_ONLY(DefaultHash);
  using Hash = MemberHash<T>;
};

template <typename T>
struct DefaultHash<blink::CrossThreadWeakPersistent<T>> {
  STATIC_ONLY(DefaultHash);
  using Hash = MemberHash<T>;
};

}  // namespace WTF

namespace base {

template <typename T>
struct IsWeakReceiver<blink::WeakPersistent<T>> : std::true_type {};

template <typename T>
struct IsWeakReceiver<blink::CrossThreadWeakPersistent<T>> : std::true_type {};

template <typename T>
struct BindUnwrapTraits<blink::CrossThreadWeakPersistent<T>> {
  static blink::CrossThreadPersistent<T> Unwrap(
      const blink::CrossThreadWeakPersistent<T>& wrapped) {
    return blink::CrossThreadPersistent<T>(wrapped.Get());
  }
};
}

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_PERSISTENT_H_
