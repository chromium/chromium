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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_VISITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_VISITOR_H_

#include <memory>
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

namespace base {
class Location;
}

namespace v8 {
class Value;
}

namespace blink {

template <typename T>
class GarbageCollected;
class WeakCallbackInfo;
template <typename T>
struct TraceTrait;
class ThreadState;
class Visitor;
template <typename T>
class TraceWrapperV8Reference;

// The TraceMethodDelegate is used to convert a trace method for type T to a
// TraceCallback.  This allows us to pass a type's trace method as a parameter
// to the PersistentNode constructor. The PersistentNode constructor needs the
// specific trace method due an issue with the Windows compiler which
// instantiates even unused variables. This causes problems
// in header files where we have only forward declarations of classes.
//
// This interface is safe to use on concurrent threads. All accesses (reads)
// from member are done atomically.
template <typename T, void (T::*method)(Visitor*)>
struct TraceMethodDelegate {
  STATIC_ONLY(TraceMethodDelegate);
  static void Trampoline(Visitor* visitor, void* self) {
    (reinterpret_cast<T*>(self)->*method)(visitor);
  }
};

template <typename T, void (T::*method)(const WeakCallbackInfo&)>
struct WeakCallbackMethodDelegate {
  STATIC_ONLY(WeakCallbackMethodDelegate);
  static void Trampoline(const WeakCallbackInfo& info, void* self) {
    (reinterpret_cast<T*>(self)->*method)(info);
  }
};

// Visitor is used to traverse Oilpan's object graph.
class PLATFORM_EXPORT Visitor {
  USING_FAST_MALLOC(Visitor);

 public:
  explicit Visitor(ThreadState* state) : state_(state) {}
  virtual ~Visitor() = default;

  inline ThreadState* State() const { return state_; }
  inline ThreadHeap& Heap() const { return state_->Heap(); }

  // Static visitor implementation forwarding to dynamic interface.

  template <typename T>
  void TraceRoot(const T* t, const base::Location& location) {
    static_assert(sizeof(T), "T must be fully defined");
    static_assert(IsGarbageCollectedType<T>::value,
                  "T needs to be a garbage collected object");
    if (!t)
      return;
    VisitRoot(const_cast<T*>(t), TraceDescriptorFor(t), location);
  }

  template <typename T>
  void Trace(const Member<T>& t) {
    DCHECK(!t.IsHashTableDeletedValueSafe());
    Trace(t.GetSafe());
  }

  // Fallback methods used only when we need to trace raw pointers of T. This is
  // the case when a member is a union where we do not support members.
  template <typename T>
  void Trace(const T* t) {
    Trace(const_cast<T*>(t));
  }

  template <typename T>
  void Trace(T* t) {
    static_assert(sizeof(T), "T must be fully defined");
    static_assert(IsGarbageCollectedType<T>::value,
                  "T needs to be a garbage collected object");
    if (!t)
      return;
    Visit(t, TraceDescriptorFor(t));
  }

  template <typename T>
  void TraceBackingStoreStrongly(T* backing_store, T** backing_store_slot) {
    static_assert(sizeof(T), "T must be fully defined");
    static_assert(IsGarbageCollectedType<T>::value,
                  "T needs to be a garbage collected object");

    VisitBackingStoreStrongly(backing_store,
                              reinterpret_cast<void**>(backing_store_slot),
                              TraceDescriptorFor(backing_store));
  }

  template <typename HashTable, typename T>
  void TraceBackingStoreWeakly(T* backing_store,
                               T** backing_store_slot,
                               WeakCallback weak_callback,
                               void* weak_callback_parameter) {
    static_assert(sizeof(T), "T must be fully defined");
    static_assert(IsGarbageCollectedType<T>::value,
                  "T needs to be a garbage collected object");

    VisitBackingStoreWeakly(backing_store,
                            reinterpret_cast<void**>(backing_store_slot),
                            TraceDescriptorFor(backing_store),
                            WeakTraceDescriptorFor(backing_store),
                            weak_callback, weak_callback_parameter);
  }

  template <typename T>
  void TraceBackingStoreOnly(T* backing_store, T** backing_store_slot) {
    static_assert(sizeof(T), "T must be fully defined");
    static_assert(IsGarbageCollectedType<T>::value,
                  "T needs to be a garbage collected object");

    VisitBackingStoreOnly(backing_store,
                          reinterpret_cast<void**>(backing_store_slot));
  }

  // WeakMember version of the templated trace method. It doesn't keep
  // the traced thing alive, but will write null to the WeakMember later
  // if the pointed-to object is dead. It's lying for this to be const,
  // but the overloading resolver prioritizes constness too high when
  // picking the correct overload, so all these trace methods have to have
  // the same constness on their argument to allow the type to decide.
  template <typename T>
  void Trace(const WeakMember<T>& const_weak_member) {
    static_assert(sizeof(T), "T must be fully defined");
    static_assert(IsGarbageCollectedType<T>::value,
                  "T needs to be a garbage collected object");

    WeakMember<T>& weak_member = const_cast<WeakMember<T>&>(const_weak_member);
    std::remove_const_t<T>* value =
        const_cast<std::remove_const_t<T>*>(weak_member.GetSafe());

    if (!value)
      return;

    DCHECK(!weak_member.IsHashTableDeletedValueSafe());
    VisitWeak(value, &weak_member, TraceDescriptorFor(value),
              &HandleWeakCell<T>);
  }

  // Fallback trace method for part objects to allow individual trace methods
  // to trace through a part object with visitor->trace(m_partObject). This
  // takes a const argument, because otherwise it will match too eagerly: a
  // non-const argument would match a non-const Vector<T>& argument better
  // than the specialization that takes const Vector<T>&. For a similar reason,
  // the other specializations take a const argument even though they are
  // usually used with non-const arguments, otherwise this function would match
  // too well.
  template <typename T>
  void Trace(const T& t) {
    static_assert(sizeof(T), "T must be fully defined");
    if (std::is_polymorphic<T>::value) {
      intptr_t vtable = *reinterpret_cast<const intptr_t*>(&t);
      if (!vtable)
        return;
    }
    TraceTrait<T>::Trace(this, &const_cast<T&>(t));
  }

  // Registers an instance method using |RegisterWeakCallback|. See description
  // below.
  template <typename T, void (T::*method)(const WeakCallbackInfo&)>
  void RegisterWeakCallbackMethod(const T* obj) {
    RegisterWeakCallback(&WeakCallbackMethodDelegate<T, method>::Trampoline,
                         const_cast<T*>(obj));
  }

  // Cross-component tracing interface.

  template <typename V8Type>
  void Trace(const TraceWrapperV8Reference<V8Type>& v8reference) {
    Visit(v8reference.template Cast<v8::Value>());
  }

  // Dynamic visitor interface.

  virtual void VisitRoot(void* t, TraceDescriptor desc, const base::Location&) {
    Visit(t, desc);
  }

  // Visits an object through a strong reference.
  virtual void Visit(void*, TraceDescriptor) = 0;

  // Visits an object through a weak reference.
  virtual void VisitWeak(void*, void*, TraceDescriptor, WeakCallback) = 0;

  // Visitors for collection backing stores.
  virtual void VisitBackingStoreStrongly(void*, void**, TraceDescriptor) = 0;
  virtual void VisitBackingStoreWeakly(void*,
                                       void**,
                                       TraceDescriptor,
                                       TraceDescriptor,
                                       WeakCallback,
                                       void*) = 0;
  virtual void VisitBackingStoreOnly(void*, void**) = 0;

  // Visits ephemeron pairs which are a combination of weak and strong keys and
  // values.
  using EphemeronTracingCallback = bool (*)(Visitor*, void*);
  virtual bool VisitEphemeronKeyValuePair(
      void* key,
      void* value,
      EphemeronTracingCallback key_trace_callback,
      EphemeronTracingCallback value_trace_callback) {
    return true;
  }

  // Visits cross-component references to V8.

  virtual void Visit(const TraceWrapperV8Reference<v8::Value>&) = 0;

  // Registers backing store pointers so that they can be moved and properly
  // updated.
  virtual void RegisterBackingStoreCallback(void* backing,
                                            MovingObjectCallback) = 0;

  // Adds a |callback| that is invoked with |parameter| after liveness has been
  // computed on the whole object graph. The |callback| may use the provided
  // |WeakCallbackInfo| to determine whether an object is considered alive or
  // dead.
  //
  // - Upon returning from the callback all references to dead objects must have
  //   been cleared.
  // - Any operation that extends the object graph, including allocation
  //   or reviving objects, is prohibited.
  // - Clearing out pointers is allowed.
  // - Removing elements from heap collections is allowed as these collections
  //   are aware of custom weakness and won't resize their backings.
  virtual void RegisterWeakCallback(WeakCallback callback, void* parameter) = 0;

 protected:
  template <typename T>
  static inline TraceDescriptor TraceDescriptorFor(const T* traceable) {
    return TraceTrait<T>::GetTraceDescriptor(const_cast<T*>(traceable));
  }

  template <typename T>
  static inline TraceDescriptor WeakTraceDescriptorFor(const T* traceable) {
    return TraceTrait<T>::GetWeakTraceDescriptor(const_cast<T*>(traceable));
  }

 private:
  template <typename T>
  static void HandleWeakCell(const WeakCallbackInfo&, void*);

  ThreadState* const state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_VISITOR_H_
