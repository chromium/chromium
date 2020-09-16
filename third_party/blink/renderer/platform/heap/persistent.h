// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_PERSISTENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_PERSISTENT_H_

#include "base/bind.h"
#include "base/location.h"
#include "third_party/blink/renderer/platform/bindings/buildflags.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/heap_compact.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/persistent_node.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"

namespace blink {

template <typename T>
class CrossThreadWeakPersistent;

// Wrapping type to force callers to go through macros that expand or drop
// base::Location. This is needed to avoid adding the strings when not needed.
// The type can be dropped once http://crbug.com/760702 is resolved and
// ENABLE_LOCATION_SOURCE is disabled for release builds.
class PersistentLocation final {
 public:
  PersistentLocation() = default;
  explicit PersistentLocation(const base::Location& location)
      : location_(location) {}
  PersistentLocation(const PersistentLocation& other) = default;

  const base::Location& get() const { return location_; }

 private:
  base::Location location_;
};

#if !BUILDFLAG(FROM_HERE_USES_LOCATION_BUILTINS) && \
    BUILDFLAG(RAW_HEAP_SNAPSHOTS)
#if !BUILDFLAG(ENABLE_LOCATION_SOURCE)
#define PERSISTENT_FROM_HERE \
  PersistentLocation(::base::Location::CreateFromHere(__FILE__))
#else
#define PERSISTENT_FROM_HERE \
  PersistentLocation(        \
      ::base::Location::CreateFromHere(__func__, __FILE__, __LINE__))
#endif
#else
#define PERSISTENT_FROM_HERE PersistentLocation()
#endif  // BUILDFLAG(RAW_HEAP_SNAPSHOTS)

template <typename T,
          WeaknessPersistentConfiguration weaknessConfiguration,
          CrossThreadnessPersistentConfiguration crossThreadnessConfiguration>
class PersistentBase {
  USING_FAST_MALLOC(PersistentBase);

 public:
  bool IsHashTableDeletedValue() const {
    return raw_ == reinterpret_cast<T*>(-1);
  }

  T* Release() {
    T* result = raw_;
    AssignSafe(nullptr);
    return result;
  }

  void Clear() {
    // Note that this also frees up related data in the backend.
    AssignSafe(nullptr);
  }

  T* Get() const {
    CheckPointer();
    return raw_;
  }

  // TODO(https://crbug.com/653394): Consider returning a thread-safe best
  // guess of validity.
  bool MaybeValid() const { return true; }

  explicit operator bool() const { return Get(); }
  T& operator*() const { return *Get(); }
  operator T*() const { return Get(); }
  T* operator->() const { return Get(); }

  // Register the persistent node as a 'static reference',
  // belonging to the current thread and a persistent that must
  // be cleared when the ThreadState itself is cleared out and
  // destructed.
  //
  // Static singletons arrange for this to happen, either to ensure
  // clean LSan leak reports or to register a thread-local persistent
  // needing to be cleared out before the thread is terminated.
  PersistentBase* RegisterAsStaticReference() {
    static_assert(weaknessConfiguration == kNonWeakPersistentConfiguration,
                  "Can only register non-weak Persistent references as static "
                  "references.");
    if (PersistentNode* node = persistent_node_.Get()) {
      ThreadState::Current()->RegisterStaticPersistentNode(node);
      LEAK_SANITIZER_IGNORE_OBJECT(this);
    }
    return this;
  }

  NO_SANITIZE_ADDRESS
  void ClearWithLockHeld() {
    static_assert(
        crossThreadnessConfiguration == kCrossThreadPersistentConfiguration,
        "This Persistent does not require the cross-thread lock.");
    PersistentMutexTraits<crossThreadnessConfiguration>::AssertAcquired();
    raw_ = nullptr;
    persistent_node_.ClearWithLockHeld();
  }

  void UpdateLocation(const PersistentLocation& other) {
#if BUILDFLAG(RAW_HEAP_SNAPSHOTS)
    location_ = other;
#endif  // BUILDFLAG(RAW_HEAP_SNAPSHOTS)
  }

 protected:
  ~PersistentBase() {
    UninitializeSafe();
    // Not resetting raw_ as it is not observable.
  }

  PersistentBase() : raw_(nullptr) {
    SaveCreationThreadHeap();
    // No initialization needed for empty handle.
  }
  PersistentBase(const PersistentLocation& location) : PersistentBase() {
    UpdateLocation(location);
  }

  PersistentBase(std::nullptr_t) : raw_(nullptr) {
    SaveCreationThreadHeap();
    // No initialization needed for empty handle.
  }
  PersistentBase(const PersistentLocation& location, std::nullptr_t)
      : PersistentBase(nullptr) {
    UpdateLocation(location);
  }

  PersistentBase(T* raw) : raw_(raw) {
    SaveCreationThreadHeap();
    InitializeSafe();
    CheckPointer();
  }
  PersistentBase(const PersistentLocation& location, T* raw)
      : PersistentBase(raw) {
    UpdateLocation(location);
  }

  PersistentBase(T& raw) : raw_(&raw) {
    SaveCreationThreadHeap();
    InitializeSafe();
    CheckPointer();
  }
  PersistentBase(const PersistentLocation& location, T& raw)
      : PersistentBase(raw) {
    UpdateLocation(location);
  }

  PersistentBase(const PersistentBase& other) : raw_(other) {
    SaveCreationThreadHeap();
    InitializeSafe();
    CheckPointer();
  }
  PersistentBase(const PersistentLocation& location, PersistentBase& other)
      : PersistentBase(other) {
    UpdateLocation(location);
  }

  template <typename U>
  PersistentBase(const PersistentBase<U,
                                      weaknessConfiguration,
                                      crossThreadnessConfiguration>& other)
      : raw_(other) {
    SaveCreationThreadHeap();
    InitializeSafe();
    CheckPointer();
  }
  template <typename U>
  PersistentBase(const PersistentLocation& location,
                 const PersistentBase<U,
                                      weaknessConfiguration,
                                      crossThreadnessConfiguration>& other)
      : PersistentBase(other) {
    UpdateLocation(location);
  }

  template <typename U>
  PersistentBase(const Member<U>& other) : raw_(other) {
    SaveCreationThreadHeap();
    InitializeSafe();
    CheckPointer();
  }
  template <typename U>
  PersistentBase(const PersistentLocation& location, const Member<U>& other)
      : PersistentBase(other) {
    UpdateLocation(location);
  }

  PersistentBase(WTF::HashTableDeletedValueType)
      : raw_(reinterpret_cast<T*>(-1)) {
    SaveCreationThreadHeap();
    // No initialization needed for empty handle.
  }
  PersistentBase(const PersistentLocation& location,
                 WTF::HashTableDeletedValueType)
      : PersistentBase(WTF::kHashTableDeletedValue) {
    UpdateLocation(location);
  }

  template <typename U>
  PersistentBase& operator=(U* other) {
    AssignSafe(other);
    return *this;
  }

  PersistentBase& operator=(std::nullptr_t) {
    AssignSafe(nullptr);
    return *this;
  }

  template <typename U>
  PersistentBase& operator=(const Member<U>& other) {
    AssignSafe(other);
    return *this;
  }

  // Using unsafe operations and assuming that caller acquires the lock for
  // kCrossThreadPersistentConfiguration configuration.
  PersistentBase& operator=(const PersistentBase& other) {
    PersistentMutexTraits<crossThreadnessConfiguration>::AssertAcquired();
    AssignUnsafe(other);
    return *this;
  }

  // Using unsafe operations and assuming that caller acquires the lock for
  // kCrossThreadPersistentConfiguration configuration.
  template <typename U>
  PersistentBase& operator=(
      const PersistentBase<U,
                           weaknessConfiguration,
                           crossThreadnessConfiguration>& other) {
    PersistentMutexTraits<crossThreadnessConfiguration>::AssertAcquired();
    AssignUnsafe(other);
    return *this;
  }

  // Using unsafe operations and assuming that caller acquires the lock for
  // kCrossThreadPersistentConfiguration configuration.
  template <typename U>
  PersistentBase& operator=(
      PersistentBase<U, weaknessConfiguration, crossThreadnessConfiguration>&&
          other) {
    PersistentMutexTraits<crossThreadnessConfiguration>::AssertAcquired();
    if (persistent_node_.IsInitialized()) {
      // Drop persistent node if present as it's always possible to reuse the
      // node (if present) from |other|.
      persistent_node_.Uninitialize();
    }
    // Explicit cast enabling downcasting.
    raw_ = static_cast<T*>(other.raw_);
    other.raw_ = nullptr;
    // Efficiently move by just rewiring the node pointer.
    persistent_node_ = std::move(other.persistent_node_);
    DCHECK(!other.persistent_node_.Get());
    if (persistent_node_.IsInitialized()) {
      // If |raw_| points to a non-null or deleted value, just reuse the node.
      TraceCallback trace_callback =
          TraceMethodDelegate<PersistentBase,
                              &PersistentBase::TracePersistent>::Trampoline;
      persistent_node_.Get()->Reinitialize(this, trace_callback);
    }
    CheckPointer();
    return *this;
  }

  NO_SANITIZE_ADDRESS
  bool IsNotNull() const { return raw_; }

  NO_SANITIZE_ADDRESS
  void AssignSafe(T* ptr) {
    typename PersistentMutexTraits<crossThreadnessConfiguration>::Locker lock;
    AssignUnsafe(ptr);
  }

  NO_SANITIZE_ADDRESS
  void AssignUnsafe(T* ptr) {
    raw_ = ptr;
    CheckPointer();
    if (raw_ && !IsHashTableDeletedValue()) {
      if (!persistent_node_.IsInitialized())
        InitializeUnsafe();
      return;
    }
    UninitializeUnsafe();
  }

  void TracePersistent(Visitor* visitor) const {
    static_assert(sizeof(T), "T must be fully defined");
    static_assert(IsGarbageCollectedType<T>::value,
                  "T needs to be a garbage collected object");
    DCHECK(!IsHashTableDeletedValue());
    if (weaknessConfiguration == kWeakPersistentConfiguration) {
      visitor->RegisterWeakCallback(HandleWeakPersistent, this);
    } else {
#if BUILDFLAG(RAW_HEAP_SNAPSHOTS)
      visitor->TraceRoot(raw_, location_.get());
#else
      visitor->TraceRoot(raw_, base::Location());
#endif  // BUILDFLAG(RAW_HEAP_SNAPSHOTS)
    }
  }

  NO_SANITIZE_ADDRESS
  void InitializeSafe() {
    DCHECK(!persistent_node_.IsInitialized());
    if (!raw_ || IsHashTableDeletedValue())
      return;

    TraceCallback trace_callback =
        TraceMethodDelegate<PersistentBase,
                            &PersistentBase::TracePersistent>::Trampoline;
    typename PersistentMutexTraits<crossThreadnessConfiguration>::Locker lock;
    persistent_node_.Initialize(this, trace_callback);
  }

  NO_SANITIZE_ADDRESS
  void InitializeUnsafe() {
    DCHECK(!persistent_node_.IsInitialized());
    if (!raw_ || IsHashTableDeletedValue())
      return;

    TraceCallback trace_callback =
        TraceMethodDelegate<PersistentBase,
                            &PersistentBase::TracePersistent>::Trampoline;
    persistent_node_.Initialize(this, trace_callback);
  }

  void UninitializeSafe() {
    if (persistent_node_.IsInitialized()) {
      typename PersistentMutexTraits<crossThreadnessConfiguration>::Locker lock;
      persistent_node_.Uninitialize();
    }
  }

  void UninitializeUnsafe() {
    if (persistent_node_.IsInitialized())
      persistent_node_.Uninitialize();
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

  static void HandleWeakPersistent(const LivenessBroker& broker,
                                   const void* persistent_pointer) {
    using Base =
        PersistentBase<typename std::remove_const<T>::type,
                       weaknessConfiguration, crossThreadnessConfiguration>;
    Base* persistent =
        reinterpret_cast<Base*>(const_cast<void*>(persistent_pointer));
    T* object = persistent->Get();
    if (object && !broker.IsHeapObjectAlive(object))
      ClearWeakPersistent(persistent);
  }

  static void ClearWeakPersistent(
      PersistentBase<std::remove_const_t<T>,
                     kWeakPersistentConfiguration,
                     kCrossThreadPersistentConfiguration>* persistent) {
    PersistentMutexTraits<crossThreadnessConfiguration>::AssertAcquired();
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

  // raw_ is accessed most, so put it at the first field.
  T* raw_;

  // The pointer to the underlying persistent node.
  //
  // Since accesses are atomics in the cross-thread case, a different type is
  // needed to prevent the compiler producing an error when it encounters
  // operations that are legal on raw pointers but not on atomics, or
  // vice-versa.
  std::conditional_t<
      crossThreadnessConfiguration == kCrossThreadPersistentConfiguration,
      CrossThreadPersistentNodePtr<weaknessConfiguration>,
      PersistentNodePtr<ThreadingTrait<T>::kAffinity, weaknessConfiguration>>
      persistent_node_;

#if BUILDFLAG(RAW_HEAP_SNAPSHOTS)
  PersistentLocation location_;
#endif  // BUILDFLAG(RAW_HEAP_SNAPSHOTS)

#if DCHECK_IS_ON()
  const ThreadState* creation_thread_state_;
#endif

  template <typename F,
            WeaknessPersistentConfiguration,
            CrossThreadnessPersistentConfiguration>
  friend class PersistentBase;
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
  using Parent = PersistentBase<T,
                                kNonWeakPersistentConfiguration,
                                kSingleThreadPersistentConfiguration>;

 public:
  Persistent() : Parent() {}
  Persistent(const PersistentLocation& location) : Parent(location) {}
  Persistent(std::nullptr_t) : Parent(nullptr) {}
  Persistent(const PersistentLocation& location, std::nullptr_t)
      : Parent(location, nullptr) {}
  Persistent(T* raw) : Parent(raw) {}
  Persistent(const PersistentLocation& location, T* raw)
      : Parent(location, raw) {}
  Persistent(T& raw) : Parent(raw) {}
  Persistent(const PersistentLocation& location, T& raw)
      : Parent(location, raw) {}
  Persistent(const Persistent& other) : Parent(other) {}
  Persistent(const PersistentLocation& location, const Persistent& other)
      : Parent(location, other) {}
  template <typename U>
  Persistent(const Persistent<U>& other) : Parent(other) {}
  template <typename U>
  Persistent(const PersistentLocation& location, const Persistent<U>& other)
      : Parent(location, other) {}
  template <typename U>
  Persistent(const Member<U>& other) : Parent(other) {}
  template <typename U>
  Persistent(const PersistentLocation& location, const Member<U>& other)
      : Parent(location, other) {}
  Persistent(WTF::HashTableDeletedValueType x) : Parent(x) {}
  Persistent(const PersistentLocation& location,
             WTF::HashTableDeletedValueType x)
      : Parent(location, x) {}

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
  using Parent = PersistentBase<T,
                                kWeakPersistentConfiguration,
                                kSingleThreadPersistentConfiguration>;

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

  NO_SANITIZE_ADDRESS
  bool IsClearedUnsafe() const { return this->IsNotNull(); }
};

// CrossThreadPersistent allows for holding onto an object strongly on a
// different thread.
//
// Thread-safe operations:
// - Construction
// - Destruction
// - Copy and move construction and assignment
// - Clearing
// - Deref if treated as immutable reference or if externally synchronized (e.g.
//   mutex, task). The current implementation of Get() uses a raw load (on
//   purpose) which prohibits mutation while accessing the reference on a
//   different thread.
template <typename T>
class CrossThreadPersistent
    : public PersistentBase<T,
                            kNonWeakPersistentConfiguration,
                            kCrossThreadPersistentConfiguration> {
  using Parent = PersistentBase<T,
                                kNonWeakPersistentConfiguration,
                                kCrossThreadPersistentConfiguration>;

 public:
  CrossThreadPersistent() : Parent() {}
  CrossThreadPersistent(const PersistentLocation& location)
      : Parent(location) {}
  CrossThreadPersistent(std::nullptr_t) : Parent(nullptr) {}
  CrossThreadPersistent(const PersistentLocation& location, std::nullptr_t)
      : Parent(location, nullptr) {}
  explicit CrossThreadPersistent(T* raw) : Parent(raw) {}
  CrossThreadPersistent(const PersistentLocation& location, T* raw)
      : Parent(location, raw) {}
  explicit CrossThreadPersistent(T& raw) : Parent(raw) {}
  CrossThreadPersistent(const PersistentLocation& location, T& raw)
      : Parent(location, raw) {}
  CrossThreadPersistent(const CrossThreadPersistent& other) { *this = other; }
  CrossThreadPersistent(const PersistentLocation& location,
                        const CrossThreadPersistent& other) {
    *this = other;
  }
  template <typename U>
  CrossThreadPersistent(const CrossThreadPersistent<U>& other) {
    *this = other;
  }
  template <typename U>
  CrossThreadPersistent(const PersistentLocation& location,
                        const CrossThreadPersistent<U>& other) {
    *this = other;
  }
  template <typename U>
  CrossThreadPersistent(const Member<U>& other) : Parent(other) {}
  template <typename U>
  CrossThreadPersistent(const PersistentLocation& location,
                        const Member<U>& other)
      : Parent(location, other) {}
  CrossThreadPersistent(WTF::HashTableDeletedValueType x) : Parent(x) {}
  CrossThreadPersistent(const PersistentLocation& location,
                        WTF::HashTableDeletedValueType x)
      : Parent(location, x) {}
  template <typename U>
  CrossThreadPersistent(const CrossThreadWeakPersistent<U>& other) {
    *this = other;
  }

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
    MutexLocker locker(ProcessHeap::CrossThreadPersistentMutex());
    Parent::operator=(other);
    return *this;
  }

  template <typename U>
  CrossThreadPersistent& operator=(const CrossThreadPersistent<U>& other) {
    MutexLocker locker(ProcessHeap::CrossThreadPersistentMutex());
    Parent::operator=(other);
    return *this;
  }

  template <typename U>
  CrossThreadPersistent& operator=(const CrossThreadWeakPersistent<U>&);
};

// CrossThreadWeakPersistent combines behavior of CrossThreadPersistent and
// WeakPersistent, i.e., it allows holding onto an object weakly on a different
// thread.
//
// Thread-safe operations:
// - Construction
// - Destruction
// - Copy and move construction and assignment
// - Clearing
//
// Note that this does not include dereferencing and using the raw pointer as
// there is no guarantee that the object will be alive at the time it is used.
template <typename T>
class CrossThreadWeakPersistent
    : public PersistentBase<T,
                            kWeakPersistentConfiguration,
                            kCrossThreadPersistentConfiguration> {
  using Parent = PersistentBase<T,
                                kWeakPersistentConfiguration,
                                kCrossThreadPersistentConfiguration>;

 public:
  CrossThreadWeakPersistent() : Parent() {}
  explicit CrossThreadWeakPersistent(T* raw) : Parent(raw) {}
  explicit CrossThreadWeakPersistent(T& raw) : Parent(raw) {}
  CrossThreadWeakPersistent(const CrossThreadWeakPersistent& other) {
    *this = other;
  }
  template <typename U>
  CrossThreadWeakPersistent(const CrossThreadWeakPersistent<U>& other) {
    *this = other;
  }
  CrossThreadWeakPersistent(CrossThreadWeakPersistent&& other) {
    *this = std::move(other);
  }
  template <typename U>
  CrossThreadWeakPersistent(CrossThreadWeakPersistent<U>&& other) {
    *this = std::move(other);
  }

  CrossThreadWeakPersistent& operator=(const CrossThreadWeakPersistent& other) {
    MutexLocker locker(ProcessHeap::CrossThreadPersistentMutex());
    Parent::operator=(other);
    return *this;
  }

  template <typename U>
  CrossThreadWeakPersistent& operator=(
      const CrossThreadWeakPersistent<U>& other) {
    MutexLocker locker(ProcessHeap::CrossThreadPersistentMutex());
    Parent::operator=(other);
    return *this;
  }

  CrossThreadWeakPersistent& operator=(CrossThreadWeakPersistent&& other) {
    MutexLocker locker(ProcessHeap::CrossThreadPersistentMutex());
    Parent::operator=(std::move(other));
    return *this;
  }

  template <typename U>
  CrossThreadWeakPersistent& operator=(CrossThreadWeakPersistent<U>&& other) {
    MutexLocker locker(ProcessHeap::CrossThreadPersistentMutex());
    Parent::operator=(std::move(other));
    return *this;
  }

  template <typename U>
  CrossThreadWeakPersistent& operator=(U* other) {
    Parent::operator=(other);
    return *this;
  }

  // Create a CrossThreadPersistent that keeps the underlying object alive if
  // there is still on set. Can be used to work with an object on a different
  // thread than it was allocated. Note that CTP does not block threads from
  // terminating, in which case the reference would still be invalid.
  const CrossThreadPersistent<T> Lock() const {
    return CrossThreadPersistent<T>(*this);
  }

  // Disallow directly using CrossThreadWeakPersistent. Users must go through
  // CrossThreadPersistent to access the pointee. Note that this does not
  // guarantee that the object is still alive at that point. Users must check
  // the state of CTP manually before invoking any calls.
  T* operator->() const = delete;
  T& operator*() const = delete;
  operator T*() const = delete;
  T* Get() const = delete;

 private:
  template <typename U>
  friend class CrossThreadPersistent;
};

template <typename T>
template <typename U>
CrossThreadPersistent<T>& CrossThreadPersistent<T>::operator=(
    const CrossThreadWeakPersistent<U>& other) {
  MutexLocker locker(ProcessHeap::CrossThreadPersistentMutex());
  using ParentU = PersistentBase<U, kWeakPersistentConfiguration,
                                 kCrossThreadPersistentConfiguration>;
  this->AssignUnsafe(static_cast<const ParentU&>(other).Get());
  return *this;
}

template <typename T>
Persistent<T> WrapPersistentInternal(const PersistentLocation& location,
                                     T* value) {
  return Persistent<T>(location, value);
}

template <typename T>
Persistent<T> WrapPersistentInternal(T* value) {
  return Persistent<T>(value);
}

#if BUILDFLAG(RAW_HEAP_SNAPSHOTS)
#define WrapPersistent(value) \
  WrapPersistentInternal(PERSISTENT_FROM_HERE, value)
#else
#define WrapPersistent(value) WrapPersistentInternal(value)
#endif  // BUILDFLAG(RAW_HEAP_SNAPSHOTS)

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
CrossThreadPersistent<T> WrapCrossThreadPersistentInternal(
    const PersistentLocation& location,
    T* value) {
  return CrossThreadPersistent<T>(location, value);
}

template <typename T>
CrossThreadPersistent<T> WrapCrossThreadPersistentInternal(T* value) {
  return CrossThreadPersistent<T>(value);
}

#if BUILDFLAG(RAW_HEAP_SNAPSHOTS)
#define WrapCrossThreadPersistent(value) \
  WrapCrossThreadPersistentInternal(PERSISTENT_FROM_HERE, value)
#else
#define WrapCrossThreadPersistent(value) \
  WrapCrossThreadPersistentInternal(value)
#endif  // BUILDFLAG(RAW_HEAP_SNAPSHOTS)

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

template <typename T>
struct CrossThreadCopier<blink::CrossThreadPersistent<T>>
    : public CrossThreadCopierPassThrough<blink::CrossThreadPersistent<T>> {
  STATIC_ONLY(CrossThreadCopier);
};

template <typename T>
struct CrossThreadCopier<blink::CrossThreadWeakPersistent<T>>
    : public CrossThreadCopierPassThrough<blink::CrossThreadWeakPersistent<T>> {
  STATIC_ONLY(CrossThreadCopier);
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
    return blink::CrossThreadPersistent<T>(wrapped);
  }
};
}

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_PERSISTENT_H_
