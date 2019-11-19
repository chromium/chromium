// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRONG_ASSOCIATED_BINDING_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STRONG_ASSOCIATED_BINDING_H_

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/associated_interface_request.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {

template <typename Interface>
class StrongAssociatedBinding;

template <typename Interface>
using StrongAssociatedBindingPtr =
    base::WeakPtr<StrongAssociatedBinding<Interface>>;

// This connects an interface implementation strongly to an associated pipe.
// When a connection error is detected the implementation is deleted. If the
// task runner that a StrongAssociatedBinding is bound on is stopped, the
// connection error handler will not be invoked and the implementation will not
// be deleted.
//
// To use, call StrongAssociatedBinding<T>::Create() (see below) or the helper
// MakeStrongAssociatedBinding function:
//
//   mojo::MakeStrongAssociatedBinding(std::make_unique<FooImpl>(),
//                                     std::move(foo_request));
//
template <typename Interface>
class StrongAssociatedBinding {
 public:
  // Create a new StrongAssociatedBinding instance. The instance owns itself,
  // cleaning up only in the event of a pipe connection error. Returns a WeakPtr
  // to the new StrongAssociatedBinding instance.
  static StrongAssociatedBindingPtr<Interface> Create(
      std::unique_ptr<Interface> impl,
      AssociatedInterfaceRequest<Interface> request,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr) {
    StrongAssociatedBinding* binding = new StrongAssociatedBinding(
        std::move(impl), std::move(request), std::move(task_runner));
    return binding->weak_factory_.GetWeakPtr();
  }

  // Note: The error handler must not delete the interface implementation.
  //
  // This method may only be called after this StrongAssociatedBinding has been
  // bound to a message pipe.
  void set_connection_error_handler(base::OnceClosure error_handler) {
    DCHECK(binding_.is_bound());
    connection_error_handler_ = std::move(error_handler);
    connection_error_with_reason_handler_.Reset();
  }

  void set_connection_error_with_reason_handler(
      ConnectionErrorWithReasonCallback error_handler) {
    DCHECK(binding_.is_bound());
    connection_error_with_reason_handler_ = std::move(error_handler);
    connection_error_handler_.Reset();
  }

  // Forces the binding to close. This destroys the StrongBinding instance.
  void Close() { delete this; }

  Interface* impl() { return impl_.get(); }

  // Sends a message on the underlying message pipe and runs the current
  // message loop until its response is received. This can be used in tests to
  // verify that no message was sent on a message pipe in response to some
  // stimulus.
  void FlushForTesting() { binding_.FlushForTesting(); }

  // Allows test code to swap the interface implementation.
  std::unique_ptr<Interface> SwapImplForTesting(
      std::unique_ptr<Interface> new_impl) {
    binding_.SwapImplForTesting(new_impl.get());
    impl_.swap(new_impl);
    return new_impl;
  }

 private:
  StrongAssociatedBinding(std::unique_ptr<Interface> impl,
                          AssociatedInterfaceRequest<Interface> request,
                          scoped_refptr<base::SequencedTaskRunner> task_runner)
      : impl_(std::move(impl)),
        binding_(impl_.get(), std::move(request), std::move(task_runner)) {
    binding_.set_connection_error_with_reason_handler(base::BindOnce(
        &StrongAssociatedBinding::OnConnectionError, base::Unretained(this)));
  }

  ~StrongAssociatedBinding() {}

  void OnConnectionError(uint32_t custom_reason,
                         const std::string& description) {
    if (connection_error_handler_) {
      std::move(connection_error_handler_).Run();
    } else if (connection_error_with_reason_handler_) {
      std::move(connection_error_with_reason_handler_)
          .Run(custom_reason, description);
    }
    Close();
  }

  std::unique_ptr<Interface> impl_;
  base::OnceClosure connection_error_handler_;
  ConnectionErrorWithReasonCallback connection_error_with_reason_handler_;
  AssociatedBinding<Interface> binding_;
  base::WeakPtrFactory<StrongAssociatedBinding> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(StrongAssociatedBinding);
};

template <typename Interface, typename Impl>
StrongAssociatedBindingPtr<Interface> MakeStrongAssociatedBinding(
    std::unique_ptr<Impl> impl,
    AssociatedInterfaceRequest<Interface> request,
    scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr) {
  return StrongAssociatedBinding<Interface>::Create(
      std::move(impl), std::move(request), std::move(task_runner));
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STRONG_ASSOCIATED_BINDING_H_
