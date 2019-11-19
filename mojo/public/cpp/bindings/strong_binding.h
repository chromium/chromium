// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRONG_BINDING_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STRONG_BINDING_H_

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {

template <typename Interface>
class StrongBinding;

template <typename Interface>
using StrongBindingPtr = base::WeakPtr<StrongBinding<Interface>>;

// This connects an interface implementation strongly to a pipe. When a
// connection error is detected the implementation is deleted. If the task
// runner that a StrongBinding is bound on is stopped, the connection error
// handler will not be invoked and the implementation will not be deleted.
//
// To use, call StrongBinding<T>::Create() (see below) or the helper
// MakeStrongBinding function:
//
//   mojo::MakeStrongBinding(std::make_unique<FooImpl>(),
//                           std::move(foo_request));
//
template <typename Interface>
class StrongBinding {
 public:
  // Create a new StrongBinding instance. The instance owns itself, cleaning up
  // only in the event of a pipe connection error. Returns a WeakPtr to the new
  // StrongBinding instance.
  static StrongBindingPtr<Interface> Create(
      std::unique_ptr<Interface> impl,
      InterfaceRequest<Interface> request,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr) {
    StrongBinding* binding = new StrongBinding(
        std::move(impl), std::move(request), std::move(task_runner));
    return binding->weak_factory_.GetWeakPtr();
  }

  // Note: The error handler must not delete the interface implementation.
  //
  // This method may only be called after this StrongBinding has been bound to a
  // message pipe.
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

  // Stops processing incoming messages until
  // ResumeIncomingMethodCallProcessing().
  // Outgoing messages are still sent.
  //
  // No errors are detected on the message pipe while paused.
  //
  // This method may only be called if the object has been bound to a message
  // pipe and there are no associated interfaces running.
  void PauseIncomingMethodCallProcessing() {
    binding_.PauseIncomingMethodCallProcessing();
  }
  void ResumeIncomingMethodCallProcessing() {
    binding_.ResumeIncomingMethodCallProcessing();
  }

  // Forces the binding to close. This destroys the StrongBinding instance.
  void Close() { delete this; }

  Interface* impl() { return impl_.get(); }

  // Sends a message on the underlying message pipe and runs the current
  // message loop until its response is received. This can be used in tests to
  // verify that no message was sent on a message pipe in response to some
  // stimulus.
  void FlushForTesting() { binding_.FlushForTesting(); }

 private:
  StrongBinding(std::unique_ptr<Interface> impl,
                InterfaceRequest<Interface> request,
                scoped_refptr<base::SequencedTaskRunner> task_runner)
      : impl_(std::move(impl)),
        binding_(impl_.get(), std::move(request), std::move(task_runner)) {
    binding_.set_connection_error_with_reason_handler(base::BindOnce(
        &StrongBinding::OnConnectionError, base::Unretained(this)));
  }

  ~StrongBinding() {}

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
  Binding<Interface> binding_;
  base::WeakPtrFactory<StrongBinding> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(StrongBinding);
};

template <typename Interface, typename Impl>
StrongBindingPtr<Interface> MakeStrongBinding(
    std::unique_ptr<Impl> impl,
    InterfaceRequest<Interface> request,
    scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr) {
  return StrongBinding<Interface>::Create(std::move(impl), std::move(request),
                                          std::move(task_runner));
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STRONG_BINDING_H_
