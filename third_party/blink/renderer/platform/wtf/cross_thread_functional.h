// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_CROSS_THREAD_FUNCTIONAL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_CROSS_THREAD_FUNCTIONAL_H_

#include <type_traits>
#include <utility>

#include "base/functional/bind.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/functional_internal.h"

namespace blink {

// `CrossThreadBindOnce()` and `CrossThreadBindRepeating()` are the Blink
// equivalents of `base::BindOnce()` and `base::BindRepeating()` for creating
// a callback that is run or destroyed on a different thread.
//
// Unlike `base::RepeatingCallback`, a repeatable cross-thread function is
// *not* copyable. This is historical; prior to https://crbug.com/40692434,
// `blink::String` was thread-hostile, making any type that transitively held a
// `blink::String` thread-hostile as well. Making the bound function object
// non-copyable somewhat mitigateg this sharp edge: the bound arguments would
// be copied/moved in a way that guaranteed the function object itself held
// sole, unique ownership of thread-hostile arguments, allowing safe transfer
// to another thread.
//
// TODO(crbug.com/963574): Deprecate `CrossThreadBindRepeating()`.
//
// Example:
// // Given the prototype:
// // void MyFunction(const CopyAndMovableObject&, int);
// CopyableAndMovableObject obj;
// CrossThreadOnceFunction<void(int)> f =
//     CrossThreadBindOnce(&MyFunction, obj);
// std::move(f).Run(42);  // Calls MyFunction(<copy of `obj`>, 42);
//
// // Moves ownership of `obj` into `g`.
// CrossThreadOnceFunction<void(int)> g =
//     CrossThreadBindOnce(&MyFunction, std::move(obj));
// std::move(g).Run(42);  // Calls MyFunction(<`obj` that was moved>, 42);

namespace internal {

template <typename Signature>
auto MakeCrossThreadFunction(base::RepeatingCallback<Signature> callback) {
  return CrossThreadFunction<Signature>(std::move(callback));
}

template <typename Signature>
auto MakeCrossThreadOnceFunction(base::OnceCallback<Signature> callback) {
  return CrossThreadOnceFunction<Signature>(std::move(callback));
}

// Insertion of coercion for specific types; transparent forwarding otherwise.

template <typename T>
decltype(auto) CoerceFunctorForCrossThreadBind(T&& functor) {
  return std::forward<T>(functor);
}

template <typename Signature>
base::RepeatingCallback<Signature> CoerceFunctorForCrossThreadBind(
    CrossThreadFunction<Signature>&& functor) {
  return ConvertToBaseRepeatingCallback(std::move(functor));
}

template <typename Signature>
base::OnceCallback<Signature> CoerceFunctorForCrossThreadBind(
    CrossThreadOnceFunction<Signature>&& functor) {
  return ConvertToBaseOnceCallback(std::move(functor));
}

}  // namespace internal

template <typename FunctionType, typename... Ps>
auto CrossThreadBindRepeating(FunctionType&& function, Ps&&... parameters) {
  static_assert(functional_internal::CheckGCedTypeRestrictions<
                    std::index_sequence_for<Ps...>, std::decay_t<Ps>...>::ok,
                "A bound argument uses a bad pattern.");
  static_assert(
      functional_internal::kCheckNoThreadUnsafeRefCounted<std::decay_t<Ps>...>);

  return internal::MakeCrossThreadFunction(
      base::BindRepeating(internal::CoerceFunctorForCrossThreadBind(
                              std::forward<FunctionType>(function)),
                          std::forward<Ps>(parameters)...));
}

template <typename FunctionType, typename... Ps>
auto CrossThreadBindOnce(FunctionType&& function, Ps&&... parameters) {
  static_assert(functional_internal::CheckGCedTypeRestrictions<
                    std::index_sequence_for<Ps...>, std::decay_t<Ps>...>::ok,
                "A bound argument uses a bad pattern.");
  static_assert(
      functional_internal::kCheckNoThreadUnsafeRefCounted<std::decay_t<Ps>...>);

  return internal::MakeCrossThreadOnceFunction(
      base::BindOnce(internal::CoerceFunctorForCrossThreadBind(
                         std::forward<FunctionType>(function)),
                     std::forward<Ps>(parameters)...));
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_CROSS_THREAD_FUNCTIONAL_H_
