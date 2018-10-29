/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LIFECYCLE_NOTIFIER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LIFECYCLE_NOTIFIER_H_

#include "base/auto_reset.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"

namespace blink {

class LifecycleObserverBase;

template <typename T, typename Observer>
class LifecycleNotifier : public GarbageCollectedMixin {
 public:
  virtual ~LifecycleNotifier();

  void AddObserver(LifecycleObserverBase*);
  void RemoveObserver(LifecycleObserverBase*);

  // NotifyContextDestroyed() should be explicitly dispatched from an
  // observed context to detach its observers and, if the observer kind
  // requires it, notify each observer by invoking ContextDestroyed().
  //
  // When ContextDestroyed() is called, it is supplied the context as
  // an argument, but the observer's LifecycleContext() is still valid
  // and safe to use while handling the notification.
  virtual void NotifyContextDestroyed();

  void Trace(blink::Visitor* visitor) override { visitor->Trace(observers_); }

  bool IsIteratingOverObservers() const {
    return iteration_state_ != kNotIterating;
  }

 protected:
  LifecycleNotifier() : iteration_state_(kNotIterating) {}

  T* Context() { return static_cast<T*>(this); }

  // Safely iterate over the registered lifecycle observers.
  //
  // Adding or removing observers is not allowed during iteration. The callable
  // will only be called synchronously inside ForEachObserver().
  //
  // Sample usage:
  //     ForEachObserver([](ObserverType* observer) {
  //       observer->SomeMethod();
  //     });
  template <typename ForEachCallable>
  void ForEachObserver(const ForEachCallable& callable) const {
    base::AutoReset<IterationState> scope(&iteration_state_, kAllowingNone);
    for (LifecycleObserverBase* observer_base : observers_) {
      Observer* observer = static_cast<Observer*>(observer_base);
      callable(observer);
    }
  }

 private:
  using ObserverSet = HeapLinkedHashSet<WeakMember<LifecycleObserverBase>>;

  enum IterationState {
    kAllowingNone = 0,
    kAllowingAddition = 1,
    kAllowingRemoval = 2,
    kNotIterating = kAllowingAddition | kAllowingRemoval,
  };

  // Iteration state is recorded while iterating the observer set,
  // optionally barring add or remove mutations.
  mutable IterationState iteration_state_;
  ObserverSet observers_;
};

template <typename T, typename Observer>
inline LifecycleNotifier<T, Observer>::~LifecycleNotifier() {
  // FIXME: Enable the following ASSERT. Also see a FIXME in
  // Document::detachLayoutTree().
  // DCHECK(!m_observers.size());
}

// Determine if |contextDestroyed(Observer*) is a public method on
// class type |Observer|, or any of the class types it derives from.
template <typename Observer, typename T>
class HasContextDestroyed {
  using YesType = char;
  using NoType = int;

  template <typename V>
  static YesType CheckHasContextDestroyedMethod(
      V* observer,
      T* context = nullptr,
      typename std::enable_if<
          std::is_same<decltype(observer->ContextDestroyed(context)),
                       void>::value>::type* g = nullptr);
  template <typename V>
  static NoType CheckHasContextDestroyedMethod(...);

 public:
  static_assert(sizeof(Observer), "Observer's class declaration not in scope");
  static const bool value =
      sizeof(YesType) ==
      sizeof(CheckHasContextDestroyedMethod<Observer>(nullptr));
};

// If |Observer::contextDestroyed()| is present, invoke it.
template <typename Observer,
          typename T,
          bool = HasContextDestroyed<Observer, T>::value>
class ContextDestroyedNotifier {
  STATIC_ONLY(ContextDestroyedNotifier);

 public:
  static void Call(Observer* observer, T* context) {
    observer->ContextDestroyed(context);
  }
};

template <typename Observer, typename T>
class ContextDestroyedNotifier<Observer, T, false> {
  STATIC_ONLY(ContextDestroyedNotifier);

 public:
  static void Call(Observer*, T*) {}
};

template <typename T, typename Observer>
inline void LifecycleNotifier<T, Observer>::NotifyContextDestroyed() {
  // Observer unregistration is allowed, but effectively a no-op.
  base::AutoReset<IterationState> scope(&iteration_state_, kAllowingRemoval);
  ObserverSet observers;
  observers_.Swap(observers);
  for (LifecycleObserverBase* observer_base : observers) {
    Observer* observer = static_cast<Observer*>(observer_base);
    DCHECK(observer->LifecycleContext() == Context());
    ContextDestroyedNotifier<Observer, T>::Call(observer, Context());
    observer->ClearContext();
  }
}

template <typename T, typename Observer>
inline void LifecycleNotifier<T, Observer>::AddObserver(
    LifecycleObserverBase* observer) {
  CHECK(iteration_state_ & kAllowingAddition);
  observers_.insert(observer);
}

template <typename T, typename Observer>
inline void LifecycleNotifier<T, Observer>::RemoveObserver(
    LifecycleObserverBase* observer) {
  CHECK(iteration_state_ & kAllowingRemoval);
  observers_.erase(observer);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LIFECYCLE_NOTIFIER_H_
