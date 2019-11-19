// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_CALLBACK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_CALLBACK_H_

#include <memory>

#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// DawnCallback<Callback> is a heap-allocated version of
// base::OnceCallback or base::RepeatingCallback.
// It is allocated on the heap so that it can be reinterpret_cast to/from
// void* and passed to Dawn C callbacks.
//
// Example:
//   DawnCallback<F>* callback =
//     CreateDawnCallback(WTF::Bind(func, arg1));
//
//   // |someDawnFunction| expects callback function with arguments:
//   //    Args... args, void* userdata.
//   // When it is called, it will forward to func(arg1, args...).
//   GetProcs().someDawnFunction(
//     callback->UnboundCallback(), callback->AsUserdata());
template <typename Callback>
class DawnCallback;

template <template <typename> class BaseCallbackTemplate,
          typename R,
          typename... Args>
class DawnCallback<BaseCallbackTemplate<R(Args...)>> {
  using BaseCallback = BaseCallbackTemplate<R(Args...)>;
  using UnboundCallbackFunction = R (*)(Args..., void*);

  static_assert(
      std::is_same<BaseCallback, base::OnceCallback<R(Args...)>>::value ||
          std::is_same<BaseCallback,
                       base::RepeatingCallback<R(Args...)>>::value,
      "Callback must be base::OnceCallback or base::RepeatingCallback");

 public:
  explicit DawnCallback(BaseCallback callback)
      : callback_(std::move(callback)) {}

  R Run(Args... args) && {
    return std::move(callback_).Run(std::forward<Args>(args)...);
  }

  R Run(Args... args) const& {
    return callback_.Run(std::forward<Args>(args)...);
  }

  void Reset() { callback_.Reset(); }

  static R CallUnboundCallback(Args... args, void* handle) {
    // After this non-repeating callback is run, it should delete itself.
    auto callback =
        std::unique_ptr<DawnCallback>(DawnCallback::FromUserdata(handle));
    return std::move(*callback).Run(std::forward<Args>(args)...);
  }

  static R CallUnboundRepeatingCallback(Args... args, void* handle) {
    return DawnCallback::FromUserdata(handle)->Run(std::forward<Args>(args)...);
  }

  UnboundCallbackFunction UnboundCallback() { return CallUnboundCallback; }

  UnboundCallbackFunction UnboundRepeatingCallback() {
    return CallUnboundRepeatingCallback;
  }

  void* AsUserdata() { return static_cast<void*>(this); }

  static DawnCallback* FromUserdata(void* userdata) {
    return static_cast<DawnCallback*>(userdata);
  }

 private:
  BaseCallback callback_;
};

template <typename CallbackType>
DawnCallback<CallbackType>* CreateDawnCallback(CallbackType cb) {
  return new DawnCallback<CallbackType>(std::move(cb));
}

template <typename FunctionType, typename... BoundParameters>
auto BindDawnCallback(FunctionType&& function,
                      BoundParameters&&... bound_parameters) {
  return CreateDawnCallback(
      WTF::Bind(std::forward<FunctionType>(function),
                std::forward<BoundParameters>(bound_parameters)...));
}

template <typename FunctionType, typename... BoundParameters>
auto BindRepeatingDawnCallback(FunctionType&& function,
                               BoundParameters&&... bound_parameters) {
  return CreateDawnCallback(
      WTF::BindRepeating(std::forward<FunctionType>(function),
                         std::forward<BoundParameters>(bound_parameters)...));
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_CALLBACK_H_
