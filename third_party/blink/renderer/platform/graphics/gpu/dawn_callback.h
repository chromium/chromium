// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_DAWN_CALLBACK_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_DAWN_CALLBACK_H_

#include <memory>

#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// DawnCallback<Callback> is a heap-allocated version of
// base::OnceCallback or base::RepeatingCallback.
// It is allocated on the heap so that it can be reinterpret_cast to/from
// void* and passed to Dawn C callbacks.
//
// Example:
//   DawnOnceCallback<F>* callback =
//     BindDawnOnceCallback(func, arg1);
//
//   // |someDawnFunction| expects callback function with arguments:
//   //    Args... args, void* userdata.
//   // When it is called, it will forward to func(arg1, args...).
//   GetProcs().someDawnFunction(
//     callback->UnboundCallback(), callback->AsUserdata());
template <typename Callback>
class DawnCallbackBase;

template <typename Callback>
class DawnOnceCallback;

template <typename Callback>
class DawnRepeatingCallback;

template <template <typename> class BaseCallbackTemplate,
          typename R,
          typename... Args>
class DawnCallbackBase<BaseCallbackTemplate<R(Args...)>> {
  using BaseCallback = BaseCallbackTemplate<R(Args...)>;

  static constexpr bool is_once_callback =
      std::is_same<BaseCallback, base::OnceCallback<R(Args...)>>::value;
  static constexpr bool is_repeating_callback =
      std::is_same<BaseCallback, base::RepeatingCallback<R(Args...)>>::value;
  static_assert(
      is_once_callback || is_repeating_callback,
      "Callback must be base::OnceCallback or base::RepeatingCallback");

 public:
  explicit DawnCallbackBase(BaseCallback callback)
      : callback_(std::move(callback)) {}

  void* AsUserdata() { return static_cast<void*>(this); }

 protected:
  using UnboundCallbackFunction = R (*)(Args..., void*);

  static DawnCallbackBase* FromUserdata(void* userdata) {
    return static_cast<DawnCallbackBase*>(userdata);
  }

  R Run(Args... args) && {
    static_assert(
        is_once_callback,
        "Run on a moved receiver must only be called on a once callback.");
    return std::move(callback_).Run(std::forward<Args>(args)...);
  }

  R Run(Args... args) const& {
    static_assert(is_repeating_callback,
                  "Run on a unmoved receiver must only be called on a "
                  "repeating callback.");
    return callback_.Run(std::forward<Args>(args)...);
  }

 private:
  BaseCallback callback_;
};

template <typename R, typename... Args>
class DawnOnceCallback<R(Args...)>
    : public DawnCallbackBase<base::OnceCallback<R(Args...)>> {
  using BaseCallback = base::OnceCallback<R(Args...)>;

 public:
  using DawnCallbackBase<BaseCallback>::DawnCallbackBase;

  typename DawnCallbackBase<BaseCallback>::UnboundCallbackFunction
  UnboundCallback() {
    return CallUnboundOnceCallback;
  }

 private:
  static R CallUnboundOnceCallback(Args... args, void* handle) {
    // After this non-repeating callback is run, it should delete itself.
    auto callback =
        std::unique_ptr<DawnOnceCallback>(static_cast<DawnOnceCallback*>(
            DawnCallbackBase<BaseCallback>::FromUserdata(handle)));
    return std::move(*callback).Run(std::forward<Args>(args)...);
  }
};

template <typename R, typename... Args>
class DawnRepeatingCallback<R(Args...)>
    : public DawnCallbackBase<base::RepeatingCallback<R(Args...)>> {
  using BaseCallback = base::RepeatingCallback<R(Args...)>;

 public:
  using DawnCallbackBase<BaseCallback>::DawnCallbackBase;

  typename DawnCallbackBase<BaseCallback>::UnboundCallbackFunction
  UnboundCallback() {
    return CallUnboundRepeatingCallback;
  }

 private:
  static R CallUnboundRepeatingCallback(Args... args, void* handle) {
    return static_cast<DawnRepeatingCallback*>(
               DawnCallbackBase<BaseCallback>::FromUserdata(handle))
        ->Run(std::forward<Args>(args)...);
  }
};

template <typename FunctionType, typename... BoundParameters>
auto BindDawnOnceCallback(FunctionType&& function,
                          BoundParameters&&... bound_parameters) {
  static constexpr bool is_method =
      base::internal::MakeFunctorTraits<FunctionType>::is_method;
  static constexpr bool is_weak_method =
      base::internal::IsWeakMethod<is_method, BoundParameters...>();
  static_assert(!is_weak_method,
                "BindDawnOnceCallback cannot be used with weak methods");

  auto cb = WTF::Bind(std::forward<FunctionType>(function),
                      std::forward<BoundParameters>(bound_parameters)...);
  return new DawnOnceCallback<typename decltype(cb)::RunType>(std::move(cb));
}

template <typename FunctionType, typename... BoundParameters>
auto BindDawnRepeatingCallback(FunctionType&& function,
                               BoundParameters&&... bound_parameters) {
  auto cb =
      WTF::BindRepeating(std::forward<FunctionType>(function),
                         std::forward<BoundParameters>(bound_parameters)...);
  return std::make_unique<
      DawnRepeatingCallback<typename decltype(cb)::RunType>>(std::move(cb));
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_DAWN_CALLBACK_H_
