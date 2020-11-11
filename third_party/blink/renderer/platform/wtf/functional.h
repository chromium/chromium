/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_FUNCTIONAL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_FUNCTIONAL_H_

#include <utility>
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

namespace blink {
template <typename T>
class Member;
template <typename T>
class WeakMember;
}

namespace WTF {

// Functional.h provides a very simple way to bind a function pointer and
// arguments together into a function object that can be stored, copied and
// invoked, similar to boost::bind and std::bind in C++11.

// To create a same-thread callback, use WTF::Bind() or WTF::BindRepeating().
// Use the former to create a callback that's called only once, and use the
// latter for a callback that may be called multiple times.
//
// WTF::Bind() and WTF::BindRepeating() returns base::OnceCallback and
// base::RepeatingCallback respectively. See //docs/callback.md for how to use
// those types.

// Thread Safety:
//
// WTF::Bind(), WTF::BindRepeating and base::{Once,Repeating}Callback should
// be used for same-thread closures only, i.e. the closures must be created,
// executed and destructed on the same thread.
//
// Use CrossThreadBindOnce() and CrossThreadBindRepeating() if the function/task
// is called or destructed on a (potentially) different thread from the current
// thread. See cross_thread_functional.h for more details.

// WTF::Bind() / WTF::BindRepeating() and move semantics
// =====================================================
//
// For unbound parameters, there are two ways to pass movable arguments:
//
//     1) Pass by rvalue reference.
//
//            void YourFunction(Argument&& argument) { ... }
//            base::OnceCallback<void(Argument&&)> functor =
//                Bind(&YourFunction);
//
//     2) Pass by value.
//
//            void YourFunction(Argument argument) { ... }
//            base::OnceCallback<void(Argument)> functor = Bind(&YourFunction);
//
// Note that with the latter there will be *two* move constructions happening,
// because there needs to be at least one intermediary function call taking an
// argument of type "Argument" (i.e. passed by value). The former case does not
// require any move constructions inbetween.
//
// For bound parameters (arguments supplied on the creation of a functor), you
// can move your argument into the internal storage of the functor by supplying
// an rvalue to that argument (this is done in wrap() of ParamStorageTraits).
// However, to make the functor be able to get called multiple times, the
// stored object does not get moved out automatically when the underlying
// function is actually invoked. If you want to make an argument "auto-passed",
// you can do so by wrapping your bound argument with WTF::Passed() function, as
// shown below:
//
//     void YourFunction(Argument argument)
//     {
//         // |argument| is passed from the internal storage of functor.
//         ...
//     }
//
//     ...
//     base::OnceClosure functor = Bind(&YourFunction, WTF::Passed(Argument()));
//     ...
//     std::move(functor).Run();
//
// The underlying function must receive the argument wrapped by WTF::Passed() by
// rvalue reference or by value.
//
// Obviously, if you create a functor this way, you shouldn't call the functor
// twice or more; after the second call, the passed argument may be invalid.

template <typename T>
class PassedWrapper final {
 public:
  explicit PassedWrapper(T&& scoper) : scoper_(std::move(scoper)) {}
  PassedWrapper(PassedWrapper&& other) : scoper_(std::move(other.scoper_)) {}
  T MoveOut() const { return std::move(scoper_); }

 private:
  mutable T scoper_;
};

template <typename T>
PassedWrapper<T> Passed(T&& value) {
  static_assert(
      !std::is_reference<T>::value,
      "You must pass an rvalue to WTF::passed() so it can be moved. Add "
      "std::move() if necessary.");
  static_assert(!std::is_const<T>::value,
                "|value| must not be const so it can be moved.");
  return PassedWrapper<T>(std::move(value));
}

template <typename T>
class RetainedRefWrapper final {
 public:
  explicit RetainedRefWrapper(T* ptr) : ptr_(ptr) {}
  explicit RetainedRefWrapper(scoped_refptr<T> ptr) : ptr_(std::move(ptr)) {}
  T* get() const { return ptr_.get(); }

 private:
  scoped_refptr<T> ptr_;
};

template <typename T>
RetainedRefWrapper<T> RetainedRef(T* ptr) {
  return RetainedRefWrapper<T>(ptr);
}

template <typename T>
RetainedRefWrapper<T> RetainedRef(scoped_refptr<T> ptr) {
  return RetainedRefWrapper<T>(std::move(ptr));
}

template <typename T>
class UnretainedWrapper final {
 public:
  explicit UnretainedWrapper(T* ptr) : ptr_(ptr) {}
  T* Value() const { return ptr_; }

 private:
  T* ptr_;
};

template <typename T>
class CrossThreadUnretainedWrapper final {
 public:
  explicit CrossThreadUnretainedWrapper(T* ptr) : ptr_(ptr) {}
  T* Value() const { return ptr_; }

 private:
  T* ptr_;
};

template <typename T>
UnretainedWrapper<T> Unretained(T* value) {
  static_assert(!WTF::IsGarbageCollectedType<T>::value,
                "WTF::Unretained() + GCed type is forbidden");
  return UnretainedWrapper<T>(value);
}

template <typename T>
CrossThreadUnretainedWrapper<T> CrossThreadUnretained(T* value) {
  static_assert(!WTF::IsGarbageCollectedType<T>::value,
                "CrossThreadUnretained() + GCed type is forbidden");
  return CrossThreadUnretainedWrapper<T>(value);
}

namespace internal {

template <size_t, typename T>
struct CheckGCedTypeRestriction {
  static_assert(!std::is_pointer<T>::value,
                "Raw pointers are not allowed to bind into WTF::Function. Wrap "
                "it with either WrapPersistent, WrapWeakPersistent, "
                "WrapCrossThreadPersistent, WrapCrossThreadWeakPersistent, "
                "RefPtr or unretained.");
  static_assert(!IsSubclassOfTemplate<T, blink::Member>::value &&
                    !IsSubclassOfTemplate<T, blink::WeakMember>::value,
                "Member and WeakMember are not allowed to bind into "
                "WTF::Function. Wrap it with either WrapPersistent, "
                "WrapWeakPersistent, WrapCrossThreadPersistent or "
                "WrapCrossThreadWeakPersistent.");
  static_assert(!WTF::IsGarbageCollectedType<T>::value,
                "GCed types are forbidden as bound parameters.");
  static_assert(!WTF::IsStackAllocatedType<T>::value,
                "Stack allocated types are forbidden as bound parameters.");
  static_assert(
      !(WTF::IsDisallowNew<T>::value && WTF::IsTraceable<T>::value),
      "Traceable disallow new types are forbidden as bound parameters.");
};

template <typename Index, typename... Args>
struct CheckGCedTypeRestrictions;

template <size_t... Ns, typename... Args>
struct CheckGCedTypeRestrictions<std::index_sequence<Ns...>, Args...>
    : CheckGCedTypeRestriction<Ns, Args>... {
  static constexpr bool ok = true;
};

}  // namespace internal

#if DCHECK_IS_ON()

template <typename CallbackType,
          typename RunType = typename CallbackType::RunType>
class ThreadCheckingCallbackWrapper;

// This class wraps a callback and applies thread checking on its construction,
// destruction and invocation (on Run()).
template <typename CallbackType, typename R, typename... Args>
class ThreadCheckingCallbackWrapper<CallbackType, R(Args...)> {
 public:
  explicit ThreadCheckingCallbackWrapper(CallbackType callback)
      : callback_(std::move(callback)) {}

  ~ThreadCheckingCallbackWrapper() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  R Run(Args... args) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return RunInternal(&callback_, std::forward<Args>(args)...);
  }

  bool IsCancelled() const { return callback_.IsCancelled(); }

  bool MaybeValid() const { return callback_.MaybeValid(); }

 private:
  static R RunInternal(base::RepeatingCallback<R(Args...)>* callback,
                       Args&&... args) {
    return callback->Run(std::forward<Args>(args)...);
  }

  static R RunInternal(base::OnceCallback<R(Args...)>* callback,
                       Args&&... args) {
    return std::move(*callback).Run(std::forward<Args>(args)...);
  }

  SEQUENCE_CHECKER(sequence_checker_);
  CallbackType callback_;

  DISALLOW_COPY_AND_ASSIGN(ThreadCheckingCallbackWrapper);
};

}  // namespace WTF

namespace base {

template <typename CallbackType,
          typename R,
          typename... Args,
          typename... BoundArgs>
struct CallbackCancellationTraits<
    R (WTF::ThreadCheckingCallbackWrapper<CallbackType>::*)(Args...),
    std::tuple<
        std::unique_ptr<WTF::ThreadCheckingCallbackWrapper<CallbackType>>,
        BoundArgs...>> {
  static constexpr bool is_cancellable = true;

  template <typename Functor, typename Receiver, typename... RunArgs>
  static bool IsCancelled(const Functor&,
                          const Receiver& receiver,
                          const RunArgs&...) {
    return receiver->IsCancelled();
  }

  template <typename Functor, typename Receiver, typename... RunArgs>
  static bool MaybeValid(const Functor&,
                         const Receiver& receiver,
                         const RunArgs&...) {
    return receiver->MaybeValid();
  }
};

}  // namespace base

namespace WTF {

#endif

template <typename Signature>
class CrossThreadFunction;

template <typename R, typename... Args>
class CrossThreadFunction<R(Args...)> {
  USING_FAST_MALLOC(CrossThreadFunction);

 public:
  CrossThreadFunction() = default;
  explicit CrossThreadFunction(base::RepeatingCallback<R(Args...)> callback)
      : callback_(std::move(callback)) {}
  ~CrossThreadFunction() = default;

  CrossThreadFunction(const CrossThreadFunction&) = delete;
  CrossThreadFunction& operator=(const CrossThreadFunction&) = delete;

  CrossThreadFunction(CrossThreadFunction&& other) = default;
  CrossThreadFunction& operator=(CrossThreadFunction&& other) = default;

  R Run(Args... args) const & {
    return callback_.Run(std::forward<Args>(args)...);
  }

  bool IsCancelled() const { return callback_.IsCancelled(); }
  void Reset() { callback_.Reset(); }
  explicit operator bool() const { return static_cast<bool>(callback_); }

  friend base::RepeatingCallback<R(Args...)> ConvertToBaseRepeatingCallback(
      CrossThreadFunction function) {
    return std::move(function.callback_);
  }

 private:
  base::RepeatingCallback<R(Args...)> callback_;
};

template <typename Signature>
class CrossThreadOnceFunction;

template <typename R, typename... Args>
class CrossThreadOnceFunction<R(Args...)> {
  USING_FAST_MALLOC(CrossThreadOnceFunction);

 public:
  CrossThreadOnceFunction() = default;
  explicit CrossThreadOnceFunction(base::OnceCallback<R(Args...)> callback)
      : callback_(std::move(callback)) {}
  ~CrossThreadOnceFunction() = default;

  CrossThreadOnceFunction(const CrossThreadOnceFunction&) = delete;
  CrossThreadOnceFunction& operator=(const CrossThreadOnceFunction&) = delete;

  CrossThreadOnceFunction(CrossThreadOnceFunction&& other) = default;
  CrossThreadOnceFunction& operator=(CrossThreadOnceFunction&& other) = default;

  R Run(Args... args) && {
    return std::move(callback_).Run(std::forward<Args>(args)...);
  }

  bool IsCancelled() const { return callback_.IsCancelled(); }
  void Reset() { callback_.Reset(); }
  explicit operator bool() const { return static_cast<bool>(callback_); }

  friend base::OnceCallback<R(Args...)> ConvertToBaseOnceCallback(
      CrossThreadOnceFunction function) {
    return std::move(function.callback_);
  }

 private:
  base::OnceCallback<R(Args...)> callback_;
};

// Note: now there is WTF::Bind()and WTF::BindRepeating(). See the comment block
// above for the correct usage of those.
template <typename FunctionType, typename... BoundParameters>
base::OnceCallback<base::MakeUnboundRunType<FunctionType, BoundParameters...>>
Bind(FunctionType&& function, BoundParameters&&... bound_parameters) {
  static_assert(internal::CheckGCedTypeRestrictions<
                    std::index_sequence_for<BoundParameters...>,
                    std::decay_t<BoundParameters>...>::ok,
                "A bound argument uses a bad pattern.");
  auto cb = base::BindOnce(std::forward<FunctionType>(function),
                           std::forward<BoundParameters>(bound_parameters)...);
#if DCHECK_IS_ON()
  using UnboundRunType =
      base::MakeUnboundRunType<FunctionType, BoundParameters...>;
  using WrapperType =
      ThreadCheckingCallbackWrapper<base::OnceCallback<UnboundRunType>>;
  cb = base::BindOnce(&WrapperType::Run,
                      std::make_unique<WrapperType>(std::move(cb)));
#endif
  return cb;
}

template <typename FunctionType, typename... BoundParameters>
base::RepeatingCallback<
    base::MakeUnboundRunType<FunctionType, BoundParameters...>>
BindRepeating(FunctionType function, BoundParameters&&... bound_parameters) {
  static_assert(internal::CheckGCedTypeRestrictions<
                    std::index_sequence_for<BoundParameters...>,
                    std::decay_t<BoundParameters>...>::ok,
                "A bound argument uses a bad pattern.");
  auto cb = base::BindRepeating(
      function, std::forward<BoundParameters>(bound_parameters)...);
#if DCHECK_IS_ON()
  using UnboundRunType =
      base::MakeUnboundRunType<FunctionType, BoundParameters...>;
  using WrapperType =
      ThreadCheckingCallbackWrapper<base::RepeatingCallback<UnboundRunType>>;
  cb = base::BindRepeating(&WrapperType::Run,
                           std::make_unique<WrapperType>(std::move(cb)));
#endif
  return cb;
}

template <typename T>
using CrossThreadRepeatingFunction = CrossThreadFunction<T>;
using CrossThreadRepeatingClosure = CrossThreadFunction<void()>;
using CrossThreadClosure = CrossThreadFunction<void()>;

using CrossThreadOnceClosure = CrossThreadOnceFunction<void()>;

}  // namespace WTF

namespace base {

template <typename T>
struct BindUnwrapTraits<WTF::RetainedRefWrapper<T>> {
  static T* Unwrap(const WTF::RetainedRefWrapper<T>& wrapped) {
    return wrapped.get();
  }
};

template <typename T>
struct BindUnwrapTraits<WTF::PassedWrapper<T>> {
  static T Unwrap(const WTF::PassedWrapper<T>& wrapped) {
    return wrapped.MoveOut();
  }
};

template <typename T>
struct BindUnwrapTraits<WTF::UnretainedWrapper<T>> {
  static T* Unwrap(const WTF::UnretainedWrapper<T>& wrapped) {
    return wrapped.Value();
  }
};

template <typename T>
struct BindUnwrapTraits<WTF::CrossThreadUnretainedWrapper<T>> {
  static T* Unwrap(const WTF::CrossThreadUnretainedWrapper<T>& wrapped) {
    return wrapped.Value();
  }
};

}  // namespace base

using WTF::CrossThreadUnretained;

using WTF::CrossThreadFunction;
using WTF::CrossThreadClosure;

using WTF::CrossThreadOnceClosure;
using WTF::CrossThreadOnceFunction;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_FUNCTIONAL_H_
