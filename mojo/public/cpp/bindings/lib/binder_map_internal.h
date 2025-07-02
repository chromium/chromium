// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDER_MAP_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDER_MAP_INTERNAL_H_

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace mojo {
namespace internal {

// A wrapper around a string literal that can be used to verify at compile time
// that the string has a static lifetime. This is achieved by using a
// `consteval` constructor, which ensures that the constructor is evaluated at
// compile time. Since the constructor takes a `const char*`, the compiler can
// only evaluate it if the provided string is a literal with a static lifetime.
// If a string with a dynamic lifetime is passed, the compilation will fail.
class StaticString {
 public:
  explicit consteval StaticString(const char* str) : str_(str) {}

  explicit operator std::string_view() const { return str_; }

 private:
  const std::string_view str_;
};

template <typename ContextType>
struct BinderContextTraits {
  using ValueType = ContextType;

  using GenericBinderType =
      base::RepeatingCallback<void(ContextType, mojo::ScopedMessagePipeHandle)>;

  template <typename Interface>
  using BinderType =
      base::RepeatingCallback<void(ContextType,
                                   mojo::PendingReceiver<Interface> receiver)>;

  template <typename Interface>
  using FuncType = void(ContextType, mojo::PendingReceiver<Interface> receiver);

  template <typename Interface>
  static GenericBinderType MakeGenericBinder(BinderType<Interface> binder) {
    return base::BindRepeating(&BindGenericReceiver<Interface>,
                               std::move(binder));
  }

  template <typename Interface>
  static void BindGenericReceiver(const BinderType<Interface>& binder,
                                  ContextType context,
                                  mojo::ScopedMessagePipeHandle receiver_pipe) {
    binder.Run(std::move(context),
               mojo::PendingReceiver<Interface>(std::move(receiver_pipe)));
  }

  template <typename Interface>
  static GenericBinderType MakeGenericBinder(FuncType<Interface>* func) {
    return base::BindRepeating(&BindGenericFunctor<Interface>, func);
  }

  template <typename Interface>
  static void BindGenericFunctor(FuncType<Interface>* func,
                                 ContextType context,
                                 mojo::ScopedMessagePipeHandle receiver_pipe) {
    func(std::move(context),
         mojo::PendingReceiver<Interface>(std::move(receiver_pipe)));
  }
};

template <>
struct BinderContextTraits<void> {
  // Not used, but exists so that BinderMapWithContext can define a compilable
  // variant of |TryBind()| which ostensibly takes a context value even for
  // maps with void context. The implementation will always fail a static_assert
  // at compile time if actually used on such maps. See |BindMap::TryBind()|.
  using ValueType = bool;

  using GenericBinderType =
      base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle)>;

  template <typename Interface>
  using BinderType =
      base::RepeatingCallback<void(mojo::PendingReceiver<Interface> receiver)>;

  template <typename Interface>
  using FuncType = void(mojo::PendingReceiver<Interface> receiver);

  template <typename Interface>
  static GenericBinderType MakeGenericBinder(BinderType<Interface> binder) {
    return base::BindRepeating(&BindGenericReceiver<Interface>,
                               std::move(binder));
  }

  template <typename Interface>
  static void BindGenericReceiver(const BinderType<Interface>& binder,
                                  mojo::ScopedMessagePipeHandle receiver_pipe) {
    binder.Run(mojo::PendingReceiver<Interface>(std::move(receiver_pipe)));
  }

  template <typename Interface>
  static GenericBinderType MakeGenericBinder(FuncType<Interface>* func) {
    return base::BindRepeating(&BindGenericFunctor<Interface>, func);
  }

  template <typename Interface>
  static void BindGenericFunctor(FuncType<Interface>* func,
                                 mojo::ScopedMessagePipeHandle receiver_pipe) {
    func(mojo::PendingReceiver<Interface>(std::move(receiver_pipe)));
  }
};

template <typename ContextType>
class GenericCallbackBinderWithContext {
 public:
  using Traits = BinderContextTraits<ContextType>;
  using ContextValueType = typename Traits::ValueType;
  using GenericBinderType = typename Traits::GenericBinderType;

  GenericCallbackBinderWithContext(
      GenericBinderType callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : callback_(std::move(callback)), task_runner_(std::move(task_runner)) {}

  GenericCallbackBinderWithContext(const GenericCallbackBinderWithContext&) =
      delete;
  GenericCallbackBinderWithContext(GenericCallbackBinderWithContext&&) =
      default;
  GenericCallbackBinderWithContext& operator=(
      const GenericCallbackBinderWithContext&) = delete;
  GenericCallbackBinderWithContext& operator=(
      GenericCallbackBinderWithContext&&) = default;

  ~GenericCallbackBinderWithContext() = default;

  void BindInterface(ContextValueType context,
                     mojo::ScopedMessagePipeHandle receiver_pipe) {
    if (task_runner_) {
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &GenericCallbackBinderWithContext::RunCallbackWithContext,
              callback_, std::move(context), std::move(receiver_pipe)));
      return;
    }
    RunCallbackWithContext(callback_, std::move(context),
                           std::move(receiver_pipe));
  }

  void BindInterface(mojo::ScopedMessagePipeHandle receiver_pipe) {
    if (task_runner_) {
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&GenericCallbackBinderWithContext::RunCallback,
                         callback_, std::move(receiver_pipe)));
      return;
    }
    RunCallback(callback_, std::move(receiver_pipe));
  }

 private:
  static void RunCallbackWithContext(const GenericBinderType& callback,
                                     ContextValueType context,
                                     mojo::ScopedMessagePipeHandle handle) {
    callback.Run(std::move(context), std::move(handle));
  }

  static void RunCallback(const GenericBinderType& callback,
                          mojo::ScopedMessagePipeHandle handle) {
    callback.Run(std::move(handle));
  }

  const GenericBinderType callback_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDER_MAP_INTERNAL_H_
