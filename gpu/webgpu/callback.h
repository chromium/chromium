// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_WEBGPU_CALLBACK_H_
#define GPU_WEBGPU_CALLBACK_H_

#include <memory>

#include "base/functional/callback.h"

namespace gpu::webgpu {

// WGPUCallback<Callback> is a heap-allocated version of
// base::OnceCallback or base::RepeatingCallback.
// It is allocated on the heap so that it can be reinterpret_cast to/from
// void* and passed to WGPU C callbacks.
//
// Example:
//   WGPUOnceCallback<F>* callback =
//     BindWGPUOnceCallback(func, arg1);
//
//   // |someWGPUFunction| expects callback function with arguments:
//   //    Args... args, void* userdata.
//   // When it is called, it will forward to func(arg1, args...).
//   GetProcs().someWGPUFunction(
//     callback->UnboundCallback(), callback->AsUserdata());
template <typename Callback>
class WGPUCallbackBase;

template <typename Callback>
class WGPUOnceCallback;

template <typename Callback>
class WGPURepeatingCallback;

template <template <typename> class BaseCallbackTemplate,
          typename R,
          typename... Args>
class WGPUCallbackBase<BaseCallbackTemplate<R(Args...)>> {
  using BaseCallback = BaseCallbackTemplate<R(Args...)>;

  static constexpr bool is_once_callback =
      std::is_same<BaseCallback, base::OnceCallback<R(Args...)>>::value;
  static constexpr bool is_repeating_callback =
      std::is_same<BaseCallback, base::RepeatingCallback<R(Args...)>>::value;
  static_assert(
      is_once_callback || is_repeating_callback,
      "Callback must be base::OnceCallback or base::RepeatingCallback");

 public:
  explicit WGPUCallbackBase(BaseCallback callback)
      : callback_(std::move(callback)) {}

  void* AsUserdata() { return static_cast<void*>(this); }

 protected:
  using UnboundCallbackFunction = R (*)(Args..., void*);

  static WGPUCallbackBase* FromUserdata(void* userdata) {
    return static_cast<WGPUCallbackBase*>(userdata);
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
class WGPUOnceCallback<R(Args...)>
    : public WGPUCallbackBase<base::OnceCallback<R(Args...)>> {
  using BaseCallback = base::OnceCallback<R(Args...)>;

 public:
  using WGPUCallbackBase<BaseCallback>::WGPUCallbackBase;

  typename WGPUCallbackBase<BaseCallback>::UnboundCallbackFunction
  UnboundCallback() {
    return CallUnboundOnceCallback;
  }

 private:
  static R CallUnboundOnceCallback(Args... args, void* handle) {
    // After this non-repeating callback is run, it should delete itself.
    auto callback =
        std::unique_ptr<WGPUOnceCallback>(static_cast<WGPUOnceCallback*>(
            WGPUCallbackBase<BaseCallback>::FromUserdata(handle)));
    return std::move(*callback).Run(std::forward<Args>(args)...);
  }
};

template <typename R, typename... Args>
class WGPURepeatingCallback<R(Args...)>
    : public WGPUCallbackBase<base::RepeatingCallback<R(Args...)>> {
  using BaseCallback = base::RepeatingCallback<R(Args...)>;

 public:
  using WGPUCallbackBase<BaseCallback>::WGPUCallbackBase;

  typename WGPUCallbackBase<BaseCallback>::UnboundCallbackFunction
  UnboundCallback() {
    return CallUnboundRepeatingCallback;
  }

 private:
  static R CallUnboundRepeatingCallback(Args... args, void* handle) {
    return static_cast<WGPURepeatingCallback*>(
               WGPUCallbackBase<BaseCallback>::FromUserdata(handle))
        ->Run(std::forward<Args>(args)...);
  }
};

template <typename CallbackType>
auto MakeWGPUOnceCallback(CallbackType&& cb) {
  static_assert(
      std::is_same<CallbackType,
                   base::OnceCallback<typename CallbackType::RunType>>::value,
      "Callback must be base::OnceCallback");
  return new gpu::webgpu::WGPUOnceCallback<typename CallbackType::RunType>(
      std::move(cb));
}

template <typename FunctionType, typename... BoundParameters>
auto BindWGPUOnceCallback(FunctionType&& function,
                          BoundParameters&&... bound_parameters) {
  static constexpr bool is_method =
      base::internal::MakeFunctorTraits<FunctionType>::is_method;
  static constexpr bool is_weak_method =
      base::internal::IsWeakMethod<is_method, BoundParameters...>();
  static_assert(!is_weak_method,
                "BindWGPUOnceCallback cannot be used with weak methods");

  return MakeWGPUOnceCallback(
      base::BindOnce(std::forward<FunctionType>(function),
                     std::forward<BoundParameters>(bound_parameters)...));
}

template <typename CallbackType>
auto MakeWGPURepeatingCallback(CallbackType&& cb) {
  static_assert(
      std::is_same<CallbackType, base::RepeatingCallback<
                                     typename CallbackType::RunType>>::value,
      "Callback must be base::RepeatingCallback");
  return new gpu::webgpu::WGPURepeatingCallback<typename CallbackType::RunType>(
      std::move(cb));
}

template <typename FunctionType, typename... BoundParameters>
auto BindWGPURepeatingCallback(FunctionType&& function,
                               BoundParameters&&... bound_parameters) {
  return MakeWGPURepeatingCallback(
      base::BindRepeating(std::forward<FunctionType>(function),
                          std::forward<BoundParameters>(bound_parameters)...));
}

}  // namespace gpu::webgpu

#endif  // GPU_WEBGPU_CALLBACK_H_
