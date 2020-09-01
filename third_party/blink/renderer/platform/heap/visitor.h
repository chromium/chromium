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
#include "third_party/blink/renderer/platform/heap/blink_gc.h"
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
class LivenessBroker;
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
template <typename T, void (T::*method)(Visitor*) const>
struct TraceMethodDelegate {
  STATIC_ONLY(TraceMethodDelegate);
  static void Trampoline(Visitor* visitor, const void* self) {
    (reinterpret_cast<const T*>(self)->*method)(visitor);
  }
};

template <typename T, void (T::*method)(const LivenessBroker&)>
struct WeakCallbackMethodDelegate {
  STATIC_ONLY(WeakCallbackMethodDelegate);
  static void Trampoline(const LivenessBroker& info, const void* self) {
    (reinterpret_cast<T*>(const_cast<void*>(self))->*method)(info);
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
    VisitRoot(t, TraceDescriptorFor(t), location);
  }

  template <typename T>
  void Trace(const Member<T>& t) {
    const T* value = t.GetSafe();

    DCHECK(!Member<T>::IsMemberHashTableDeletedValue(value));

    Trace(value);
  }

  template <typename T>
  ALWAYS_INLINE void TraceMaybeDeleted(const Member<T>& t) {
    const T* value = t.GetSafe();

    if (Member<T>::IsMemberHashTableDeletedValue(value))
      return;

    Trace<T>(value);
  }

  // TraceMayBeDeleted strongifies WeakMembers.
  template <typename T>
  ALWAYS_INLINE void TraceMaybeDeleted(const WeakMember<T>& t) {
    const T* value = t.GetSafe();

    if (WeakMember<T>::IsMemberHashTableDeletedValue(value))
      return;

    Trace<T>(value);
  }

  // Fallback methods used only when we need to trace raw pointers of T. This is
  // the case when a member is a union where we do not support members.
  template <typename T>
  void Trace(T* t) {
    Trace(const_cast<const T*>(t));
  }
  template <typename T>
  void Trace(const T* t) {
    static_assert(sizeof(T), "T must be fully defined");
    static_assert(IsGarbageCollectedType<T>::value,
                  "T needs to be a garbage collected object");
    if (!t)
      return;
    Visit(t, TraceDescriptorFor(t));
  }

  // WeakMember version of the templated trace method. It doesn't keep
  // the traced thing alive, but will write null to the WeakMember later
  // if the pointed-to object is dead. It's lying for this to be const,
  // but the overloading resolver prioritizes constness too high when
  // picking the correct overload, so all these trace methods have to have
  // the same constness on their argument to allow the type to decide.
  template <typename T>
  void Trace(const WeakMember<T>& weak_member) {
    static_assert(sizeof(T), "T must be fully defined");
    static_assert(IsGarbageCollectedType<T>::value,
                  "T needs to be a garbage collected object");

    const T* value = weak_member.GetSafe();

    if (!value)
      return;

    DCHECK(!WeakMember<T>::IsMemberHashTableDeletedValue(value));
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
      const intptr_t vtable = *reinterpret_cast<const intptr_t*>(&t);
      if (!vtable)
        return;
    }
    TraceTrait<T>::Trace(this, &t);
  }

  template <typename T>
  void TraceEphemeron(const WeakMember<T>& key,
                      const void* value,
                      TraceCallback value_trace_callback) {
    const T* t = key.GetSafe();
    if (!t)
      return;
    VisitEphemeron(TraceDescriptorFor(t).base_object_payload, value,
                   value_trace_callback);
  }

  template <typename T>
  void TraceWeakContainer(const T* object,
                          const T* const* slot,
                          TraceDescriptor strong_desc,
                          TraceDescriptor weak_dec,
                          WeakCallback weak_callback,
                          const void* weak_callback_parameter) {
    static_assert(sizeof(T), "T must be fully defined");
    static_assert(IsGarbageCollectedType<T>::value,
                  "T needs to be a garbage collected object");
    VisitWeakContainer(reinterpret_cast<const void*>(object),
                       reinterpret_cast<const void* const*>(slot), strong_desc,
                       weak_dec, weak_callback, weak_callback_parameter);
  }

  template <typename T>
  void TraceMovablePointer(const T* const* slot) {
    RegisterMovableSlot(reinterpret_cast<const void* const*>(slot));
  }

  // Cross-component tracing interface.
  template <typename V8Type>
  void Trace(const TraceWrapperV8Reference<V8Type>& v8reference) {
    Visit(v8reference.template Cast<v8::Value>());
  }

  // Dynamic visitor interface.

  // Adds a |callback| that is invoked with |parameter| after liveness has been
  // computed on the whole object graph. The |callback| may use the provided
  // |LivenessBroker| to determine whether an object is considered alive or
  // dead.
  //
  // - Upon returning from the callback all references to dead objects must have
  //   been cleared.
  // - Any operation that extends the object graph, including allocation
  //   or reviving objects, is prohibited.
  // - Clearing out pointers is allowed.
  // - Removing elements from heap collections is allowed as these collections
  //   are aware of custom weakness and won't resize their backings.
  virtual void RegisterWeakCallback(WeakCallback callback,
                                    const void* parameter) {}

  // Registers an instance method using |RegisterWeakCallback|. See description
  // below.
  template <typename T, void (T::*method)(const LivenessBroker&)>
  void RegisterWeakCallbackMethod(const T* obj) {
    RegisterWeakCallback(&WeakCallbackMethodDelegate<T, method>::Trampoline,
                         obj);
  }

  // Returns whether the visitor is used in a concurrent setting.
  virtual bool IsConcurrent() const { return false; }

  // Defers invoking |desc| to the main thread when running concurrently.
  // Returns true if |desc| has been queued for later processing and false if
  // running in a non-concurrent setting.
  //
  // This can be used to defer processing data structures to the main thread
  // when support for concurrent processing is missing.
  virtual bool DeferredTraceIfConcurrent(TraceDescriptor, size_t) {
    return false;
  }

 protected:
  // Visits an object through a strong reference.
  virtual void Visit(const void*, TraceDescriptor) {}

  // Visits an object through a weak reference.
  virtual void VisitWeak(const void*,
                         const void*,
                         TraceDescriptor,
                         WeakCallback) {}

  // Visits cross-component references to V8.
  virtual void Visit(const TraceWrapperV8Reference<v8::Value>&) {}

  virtual void VisitRoot(const void* t,
                         TraceDescriptor desc,
                         const base::Location&) {
    Visit(t, desc);
  }

  // Visits ephemeron pairs which are a combination of weak and strong keys and
  // values.
  virtual void VisitEphemeron(const void*, const void*, TraceCallback) {}

  // Visits a container |object| holding ephemeron pairs held from |slot|.  The
  // descriptor |strong_desc| can be used to enforce strong treatment of
  // |object|. The |weak_desc| descriptor is invoked repeatedly until no
  // more new objects are found. It is expected that |weak_desc| processing
  // ultimately yields in a call to VisitEphemeron. After marking all reachable
  // objects, |weak_callback| is invoked with |weak_callback_parameter|. It is
  // expected that this callback is used to reset non-live entries in the
  // ephemeron container.
  virtual void VisitWeakContainer(const void* object,
                                  const void* const* slot,
                                  TraceDescriptor strong_desc,
                                  TraceDescriptor weak_desc,
                                  WeakCallback weak_callback,
                                  const void* weak_callback_parameter) {}

  virtual void RegisterMovableSlot(const void* const* slot) {}

  template <typename T>
  static TraceDescriptor TraceDescriptorFor(const T* traceable) {
    return TraceTrait<T>::GetTraceDescriptor(traceable);
  }

 private:
  template <typename T>
  static void HandleWeakCell(const LivenessBroker&, const void*);

  ThreadState* const state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_VISITOR_H_
