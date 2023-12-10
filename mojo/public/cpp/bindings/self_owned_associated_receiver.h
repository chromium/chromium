// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_SELF_OWNED_ASSOCIATED_RECEIVER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_SELF_OWNED_ASSOCIATED_RECEIVER_H_

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/runtime_features.h"

namespace mojo {

namespace internal {

template <typename Interface>
class SelfOwnedAssociatedReceiver;

}  // namespace internal

template <typename Interface>
using SelfOwnedAssociatedReceiverRef =
    base::WeakPtr<internal::SelfOwnedAssociatedReceiver<Interface>>;

namespace internal {

template <typename Interface>
class SelfOwnedAssociatedReceiver {
 public:
  // Create a new SelfOwnedAssociatedReceiver instance. The instance owns
  // itself, cleaning up only in the event of a pipe connection error. Returns a
  // WeakPtr to the new SelfOwnedAssociatedReceiver instance.
  static SelfOwnedAssociatedReceiverRef<Interface> Create(
      std::unique_ptr<Interface> impl,
      PendingAssociatedReceiver<Interface> receiver,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr) {
    if (!internal::GetRuntimeFeature_ExpectEnabled<Interface>()) {
      return nullptr;
    }
    SelfOwnedAssociatedReceiver* self_owned = new SelfOwnedAssociatedReceiver(
        std::move(impl), std::move(receiver), std::move(task_runner));
    return self_owned->weak_factory_.GetWeakPtr();
  }

  SelfOwnedAssociatedReceiver(const SelfOwnedAssociatedReceiver&) = delete;
  SelfOwnedAssociatedReceiver& operator=(const SelfOwnedAssociatedReceiver&) =
      delete;

  // Note: The error handler must not delete the interface implementation.
  //
  // This method may only be called after this SelfOwnedAssociatedReceiver has
  // been bound to a message pipe.
  void set_connection_error_handler(base::OnceClosure error_handler) {
    DCHECK(receiver_.is_bound());
    connection_error_handler_ = std::move(error_handler);
    connection_error_with_reason_handler_.Reset();
  }

  void set_connection_error_with_reason_handler(
      ConnectionErrorWithReasonCallback error_handler) {
    DCHECK(receiver_.is_bound());
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
  void FlushForTesting() { receiver_.FlushForTesting(); }

  // Allows test code to swap the interface implementation.
  //
  // Returns the existing interface implementation to the caller.
  //
  // The caller needs to guarantee that `new_impl` will live longer than `this`
  // SelfOwnedAssociatedReceiver.  One way to achieve this is to store the
  // returned old impl and swap it back in when `new_impl` is getting destroyed.
  // Test code should prefer using `mojo::test::ScopedSwapImplForTesting` if
  // possible.
  [[nodiscard]] std::unique_ptr<Interface> SwapImplForTesting(
      std::unique_ptr<Interface> new_impl) {
    // impl_ and receiver_ point to the same thing so the return value can
    // safely be discarded here as it's returned below.
    std::ignore = receiver_.SwapImplForTesting(new_impl.get());
    impl_.swap(new_impl);
    return new_impl;
  }

 private:
  SelfOwnedAssociatedReceiver(
      std::unique_ptr<Interface> impl,
      PendingAssociatedReceiver<Interface> receiver,
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : impl_(std::move(impl)),
        receiver_(impl_.get(), std::move(receiver), std::move(task_runner)) {
    receiver_.set_disconnect_with_reason_handler(base::BindOnce(
        &SelfOwnedAssociatedReceiver::OnDisconnect, base::Unretained(this)));
  }

  ~SelfOwnedAssociatedReceiver() = default;

  void OnDisconnect(uint32_t custom_reason, const std::string& description) {
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
  AssociatedReceiver<Interface> receiver_;
  base::WeakPtrFactory<SelfOwnedAssociatedReceiver> weak_factory_{this};
};

}  // namespace internal

// Binds the lifetime of an interface implementation to the lifetime of the
// AssociatedReceiver. When the AssociatedReceiver is disconnected (typically by
// the remote end closing the entangled AssociatedRemote), the implementation
// will be deleted.
//
// Any incoming method calls or disconnection notifications will be scheduled
// to run on |task_runner|. If |task_runner| is null, this defaults to the
// current SequencedTaskRunner.
template <typename Interface, typename Impl>
SelfOwnedAssociatedReceiverRef<Interface> MakeSelfOwnedAssociatedReceiver(
    std::unique_ptr<Impl> impl,
    PendingAssociatedReceiver<Interface> receiver,
    scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr) {
  return internal::SelfOwnedAssociatedReceiver<Interface>::Create(
      std::move(impl), std::move(receiver), std::move(task_runner));
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_SELF_OWNED_ASSOCIATED_RECEIVER_H_
