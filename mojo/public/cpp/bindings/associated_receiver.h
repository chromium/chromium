// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_RECEIVER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_RECEIVER_H_

#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/lib/binding_state.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/raw_ptr_impl_ref_traits.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

// An AssociatedReceiver is used to receive and dispatch Interface method calls
// to a local implementation of Interface. Every AssociatedReceiver object is
// permanently linked to an implementation of Interface at construction time.
//
// Unlike Receiver, an AssociatedReceiver cannot immediately begin receiving
// messages from its entangled AssociatedRemote. One of the two endpoints must
// be transmitted across a concrete Remote first, at which point the endpoints
// begin piggybacking on that Remote's interface pipe.
template <typename Interface,
          typename ImplRefTraits = RawPtrImplRefTraits<Interface>>
class AssociatedReceiver {
 public:
  // Typically (and by default) an AssociatedReceiver uses a raw pointer to
  // reference its linked Interface implementation object, because typically
  // that implementation object owns the AssociatedReceiver. An alternative
  // |ImplRefTraits| may be provided as a second AssociatedReceiver template
  // argument in order to use a different reference type.
  using ImplPointerType = typename ImplRefTraits::PointerType;

  // Constructs an unbound AssociatedReceiver linked to |impl| for the duration
  // of the AssociatedReceiver's lifetime. The AssociatedReceiver can be bound
  // later by calling |Bind()| or |BindNewEndpointAndPassRemote()|. An unbound
  // AssociatedReceiver does not schedule any asynchronous tasks.
  explicit AssociatedReceiver(ImplPointerType impl)
      : binding_(std::move(impl)) {}

  // Constructs a bound AssociatedReceiver by consuming |pending_receiver|. The
  // AssociatedReceiver is permanently linked to |impl| and will schedule
  // incoming |impl| method and disconnection notifications on the default
  // SequencedTaskRunner (i.e. base::SequencedTaskRunnerHandle::Get() at
  // construction time).
  AssociatedReceiver(ImplPointerType impl,
                     PendingAssociatedReceiver<Interface> pending_receiver)
      : AssociatedReceiver(std::move(impl),
                           std::move(pending_receiver),
                           nullptr) {}

  // Similar to above but the constructed AssociatedReceiver schedules all tasks
  // via |task_runner| instead of the default SequencedTaskRunner. |task_runner|
  // must run tasks on the same sequence that owns this AssociatedReceiver.
  AssociatedReceiver(ImplPointerType impl,
                     PendingAssociatedReceiver<Interface> pending_receiver,
                     scoped_refptr<base::SequencedTaskRunner> task_runner)
      : binding_(std::move(impl)) {
    Bind(std::move(pending_receiver), std::move(task_runner));
  }

  ~AssociatedReceiver() = default;

  // Indicates whether this AssociatedReceiver is bound, meaning it may continue
  // to receive Interface method calls from a remote caller.
  //
  // NOTE: An AssociatedReceiver is NEVER passively unbound. The only way for it
  // to become unbound is to explicitly call |reset()| or |Unbind()|.
  bool is_bound() const { return binding_.is_bound(); }

  // Sets a OnceClosure to be invoked if this AssociatedReceiver is cut off from
  // its AssociatedRemote (or PendingAssociatedRemote). This can happen if the
  // corresponding AssociatedRemote (or unconsumed PendingAssociatedRemote) has
  // been destroyed, or if the AssociatedRemote sends a malformed message. Must
  // only be called on a bound AssociatedReceiver object, and only remains set
  // as long as the AssociatedReceiver is both bound and connected.
  //
  // If ever invoked, |handler| will be scheduled asynchronously on the
  // AssociatedReceiver's bound SequencedTaskRunner.
  void set_disconnect_handler(base::OnceClosure handler) {
    binding_.set_connection_error_handler(std::move(handler));
  }

  // Like above but when invoked |handler| will receive additional metadata
  // about why the remote endpoint was closed, if provided.
  void set_disconnect_with_reason_handler(
      ConnectionErrorWithReasonCallback handler) {
    binding_.set_connection_error_with_reason_handler(std::move(handler));
  }

  // Resets this AssociatedReceiver to an unbound state. An unbound
  // AssociatedReceiver will NEVER schedule method calls or disconnection
  // notifications, and any pending tasks which were scheduled prior to
  // unbinding are effectively cancelled.
  void reset() { binding_.Close(); }

  // Similar to above but provides additional information to the remote endpoint
  // about why this end is hanging up.
  void ResetWithReason(uint32_t custom_reason, const std::string& description) {
    binding_.CloseWithReason(custom_reason, description);
  }

  // Binds this AssociatedReceiver, connecting it to a new
  // PendingAssociatedRemote which is returned for transmission elsewhere
  // (typically to an AssociatedRemote who will consume it to start making
  // calls).
  //
  // The AssociatedReceiver will schedule incoming |impl| method calls and
  // disconnection notifications on the default SequencedTaskRunner (i.e.
  // base::SequencedTaskRunnerHandle::Get() at the time of this call). Must only
  // be called on an unbound AssociatedReceiver.
  PendingAssociatedRemote<Interface> BindNewEndpointAndPassRemote()
      WARN_UNUSED_RESULT {
    return BindNewEndpointAndPassRemote(nullptr);
  }

  // Like above, but the AssociatedReceiver will schedule incoming |impl| method
  // calls and disconnection notifications on |task_runner| rather than on the
  // default SequencedTaskRunner. Must only be called on an unbound
  // AssociatedReceiver. |task_runner| must run tasks on the same sequence that
  // owns this AssociatedReceiver.
  PendingAssociatedRemote<Interface> BindNewEndpointAndPassRemote(
      scoped_refptr<base::SequencedTaskRunner> task_runner) WARN_UNUSED_RESULT {
    DCHECK(!is_bound()) << "AssociatedReceiver is already bound";
    PendingAssociatedRemote<Interface> remote;
    Bind(remote.InitWithNewEndpointAndPassReceiver(), std::move(task_runner));
    return remote;
  }

  // Binds this AssociatedReceiver by consuming |pending_receiver|. Must only be
  // called on an unbound AssociatedReceiver.
  //
  // The newly bound AssociatedReceiver will schedule incoming |impl| method
  // calls and disconnection notifications on the default SequencedTaskRunner
  // (i.e. base::SequencedTaskRunnerHandle::Get() at the time of this call).
  void Bind(PendingAssociatedReceiver<Interface> pending_receiver) {
    Bind(std::move(pending_receiver), nullptr);
  }

  // Like above, but the newly bound AssociatedReceiver will schedule incoming
  // |impl| method calls and disconnection notifications on |task_runner|
  // instead of the default SequencedTaskRunner. Must only be called on an
  // unbound AssociatedReceiver. |task_runner| must run tasks on the same
  // sequence that owns this AssociatedReceiver.
  void Bind(PendingAssociatedReceiver<Interface> pending_receiver,
            scoped_refptr<base::SequencedTaskRunner> task_runner) {
    if (pending_receiver) {
      binding_.Bind(
          AssociatedInterfaceRequest<Interface>(pending_receiver.PassHandle()),
          std::move(task_runner));
    } else {
      reset();
    }
  }

  // Unbinds this AssociatedReceiver, preventing any further |impl| method calls
  // or disconnection notifications from being scheduled by it. Any such tasks
  // that were scheduled prior to unbinding are effectively cancelled.
  //
  // Returns a PendingAssociatedReceiver which remains connected to this
  // receiver's AssociatedRemote and which may be transferred elsewhere and
  // consumed by another AssociatedReceiver. Any messages received but not
  // actually dispatched by this AssociatedReceiver remain intact within the
  // returned PendingAssociatedReceiver and can be dispatched by whomever binds
  // with it later.
  //
  //
  // Note that an AssociatedReceiver should not be unbound while there are still
  // living response callbacks that haven't been invoked, as once the
  // AssociatedReceiver is unbound those response callbacks are no longer valid
  // and the AssociatedRemote will never be able to receive its expected
  // responses.
  PendingAssociatedReceiver<Interface> Unbind() WARN_UNUSED_RESULT {
    return PendingAssociatedReceiver<Interface>(binding_.Unbind().PassHandle());
  }

  // Sets a message filter to be notified of each incoming message before
  // dispatch. If a filter returns |false| from Accept(), the message is not
  // dispatched and the pipe is closed. Filters cannot be removed once added
  // and only one can be set.
  void SetFilter(std::unique_ptr<MessageFilter> filter) {
    DCHECK(is_bound());
    binding_.SetFilter(std::move(filter));
  }

  // Sends a message on the underlying message pipe and runs the current
  // message loop until its response is received. This can be used in tests to
  // verify that no message was sent on a message pipe in response to some
  // stimulus.
  void FlushForTesting() { binding_.FlushForTesting(); }

  // Returns the interface implementation that was previously specified.
  Interface* impl() { return binding_.impl(); }

  // Allows test code to swap the interface implementation.
  ImplPointerType SwapImplForTesting(ImplPointerType new_impl) {
    return binding_.SwapImplForTesting(new_impl);
  }

 private:
  // TODO(https://crbug.com/875030): Move AssociatedBinding details into this
  // class.
  AssociatedBinding<Interface, ImplRefTraits> binding_;

  DISALLOW_COPY_AND_ASSIGN(AssociatedReceiver);
};

// Constructs an invalid PendingAssociatedReceiver of any arbitrary interface
// type. Useful as short-hand for a default constructed value.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) NullAssociatedReceiver {
 public:
  template <typename Interface>
  operator PendingAssociatedReceiver<Interface>() const {
    return PendingAssociatedReceiver<Interface>();
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_RECEIVER_H_
