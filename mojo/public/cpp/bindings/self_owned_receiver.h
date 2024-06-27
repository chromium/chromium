// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_SELF_OWNED_RECEIVER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_SELF_OWNED_RECEIVER_H_

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/runtime_features.h"

namespace mojo {

namespace internal {

template <typename Interface>
class SelfOwnedReceiver;

}  // namespace internal

template <typename Interface>
using SelfOwnedReceiverRef =
    base::WeakPtr<internal::SelfOwnedReceiver<Interface>>;

namespace internal {

template <typename Interface>
class SelfOwnedReceiver {
 public:
  // Create a new SelfOwnedReceiver instance. The instance owns itself, cleaning
  // up only in the event of a pipe connection error. Returns a WeakPtr to the
  // new SelfOwnedReceiver instance.
  static SelfOwnedReceiverRef<Interface> Create(
      std::unique_ptr<Interface> impl,
      PendingReceiver<Interface> receiver,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr) {
    if (!internal::GetRuntimeFeature_ExpectEnabled<Interface>()) {
      return nullptr;
    }
    SelfOwnedReceiver* self_owned = new SelfOwnedReceiver(
        std::move(impl), std::move(receiver), std::move(task_runner));
    return self_owned->weak_factory_.GetWeakPtr();
  }

  SelfOwnedReceiver(const SelfOwnedReceiver&) = delete;
  SelfOwnedReceiver& operator=(const SelfOwnedReceiver&) = delete;

  // Note: The error handler must not delete the interface implementation.
  //
  // This method may only be called after this SelfOwnedReceiver has been bound
  // to a message pipe.
  //
  // TODO(dcheng): Consider renaming this eventually.
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

  // Stops processing incoming messages until
  // ResumeIncomingMethodCallProcessing().
  // Outgoing messages are still sent.
  //
  // No errors are detected on the message pipe while paused.
  //
  // This method may only be called if the object has been bound to a message
  // pipe and there are no associated interfaces running.
  void PauseIncomingMethodCallProcessing() { receiver_.Pause(); }
  void ResumeIncomingMethodCallProcessing() { receiver_.Resume(); }

  // Forces the binding to close. This destroys the SelfOwnedReceiver instance.
  void Close() { delete this; }

  Interface* impl() { return impl_.get(); }

  // Sends a message on the underlying message pipe and runs the current
  // message loop until its response is received. This can be used in tests to
  // verify that no message was sent on a message pipe in response to some
  // stimulus.
  void FlushForTesting() { receiver_.FlushForTesting(); }

  // Reports the currently dispatching message as bad. This destroys the
  // SelfOwnedReceiver instance.
  void ReportBadMessage(std::string_view error) {
    receiver_.ReportBadMessage(error);
    Close();
  }

 private:
  SelfOwnedReceiver(std::unique_ptr<Interface> impl,
                    PendingReceiver<Interface> receiver,
                    scoped_refptr<base::SequencedTaskRunner> task_runner)
      : impl_(std::move(impl)),
        receiver_(impl_.get(), std::move(receiver), std::move(task_runner)) {
    receiver_.set_disconnect_with_reason_handler(base::BindOnce(
        &SelfOwnedReceiver::OnDisconnect, base::Unretained(this)));
  }

  ~SelfOwnedReceiver() = default;

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
  Receiver<Interface> receiver_;
  base::WeakPtrFactory<SelfOwnedReceiver> weak_factory_{this};
};

}  // namespace internal

// Binds the lifetime of an interface implementation to the lifetime of the
// Receiver. When the Receiver is disconnected (typically by the remote end
// closing the entangled Remote), the implementation will be deleted.
//
// Any incoming method calls or disconnection notifications will be scheduled
// to run on |task_runner|. If |task_runner| is null, this defaults to the
// current SequencedTaskRunner.
//
// Note: self-owned receivers are unsafe to use when the interface
// implementation has lifetime dependencies outside of its control.
template <typename Interface, typename Impl>
SelfOwnedReceiverRef<Interface> MakeSelfOwnedReceiver(
    std::unique_ptr<Impl> impl,
    PendingReceiver<Interface> receiver,
    scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr) {
  return internal::SelfOwnedReceiver<Interface>::Create(
      std::move(impl), std::move(receiver), std::move(task_runner));
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_SELF_OWNED_RECEIVER_H_
