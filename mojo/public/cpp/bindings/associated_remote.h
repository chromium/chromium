// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_REMOTE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_REMOTE_H_

#include <cstdint>
#include <utility>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr_info.h"
#include "mojo/public/cpp/bindings/lib/associated_interface_ptr_state.h"
#include "mojo/public/cpp/bindings/lib/multiplex_router.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

// An AssociatedRemote is similar to a Remote (see remote.h) in that it is used
// to transmit mojom interface method calls to a remote (Associated) Receiver.
//
// Unlike Remote, an entangled AssociatedRemote/AssociatedReceiver pair cannot
// operate on its own and requires a concrete Remote/Receiver pair upon which
// to piggyback.
template <typename Interface>
class AssociatedRemote {
 public:
  using InterfaceType = Interface;
  using PendingType = PendingAssociatedRemote<Interface>;
  using Proxy = typename Interface::Proxy_;

  // Constructs an unbound AssociatedRemote. This object cannot issue Interface
  // method calls and does not schedule any tasks.
  AssociatedRemote() = default;
  AssociatedRemote(AssociatedRemote&& other) noexcept {
    *this = std::move(other);
  }

  // Constructs a new AssociatedRemote which is bound from |pending_remote| and
  // which schedules response callbacks and disconnection notifications on the
  // default SequencedTaskRunner (i.e., base::SequencedTaskRunnerHandle::Get()
  // at construction time).
  explicit AssociatedRemote(PendingAssociatedRemote<Interface> pending_remote)
      : AssociatedRemote(std::move(pending_remote), nullptr) {}

  // Constructs a new AssociatedRemote which is bound from |pending_remote| and
  // which schedules response callbacks and disconnection notifications on
  // |task_runner|. |task_runner| must run tasks on the same sequence that owns
  // this AssociatedRemote.
  AssociatedRemote(PendingAssociatedRemote<Interface> pending_remote,
                   scoped_refptr<base::SequencedTaskRunner> task_runner) {
    Bind(std::move(pending_remote), std::move(task_runner));
  }

  ~AssociatedRemote() = default;

  AssociatedRemote& operator=(AssociatedRemote&& other) noexcept {
    internal_state_.Swap(&other.internal_state_);
    return *this;
  }

  // Exposes access to callable Interface methods directed at this
  // AssociatedRemote's receiver. Must only be called on a bound
  // AssociatedRemote.
  typename Interface::Proxy_* get() const {
    DCHECK(is_bound())
        << "Cannot issue Interface method calls on an unbound AssociatedRemote";
    return internal_state_.instance();
  }

  // Shorthand form of |get()|. See above.
  typename Interface::Proxy_* operator->() const { return get(); }
  typename Interface::Proxy_& operator*() const { return *get(); }

  // Indicates whether this AssociatedRemote is bound and thus can issue
  // Interface method calls via the above accessors.
  //
  // NOTE: The state of being "bound" should not be confused with the state of
  // being "connected" (see |is_connected()| below). An AssociatedRemote is
  // NEVER passively unbound and the only way for it to become unbound is to
  // explicitly call |reset()| or |Unbind()|. As such, unless you make explicit
  // calls to those methods, it is always safe to assume that a AssociatedRemote
  // you've bound will remain bound and callable.
  bool is_bound() const { return internal_state_.is_bound(); }
  explicit operator bool() const { return is_bound(); }

  // Indicates whether this AssociatedRemote is connected to a receiver. Must
  // only be called on a bound AssociatedRemote. If this returns |true|, method
  // calls made by this AssociatedRemote may eventually end up at the connected
  // receiver (though it's of course possible for this call to race with
  // disconnection). If this returns |false| however, all future Interface
  // method calls on this AssociatedRemote will be silently dropped.
  //
  // A bound AssociatedRemote becomes disconnected automatically either when its
  // receiver is destroyed, or when it receives a malformed or otherwise
  // unexpected response message from the receiver.
  //
  // NOTE: The state of being "bound" should not be confused with the state of
  // being "connected". See |is_bound()| above.
  bool is_connected() const {
    DCHECK(is_bound());
    return !internal_state_.encountered_error();
  }

  // Sets a Closure to be invoked if this AssociatedRemote is cut off from its
  // receiver. This can happen if the corresponding AssociatedReceiver (or
  // unconsumed PendingAssociatedReceiver) is destroyed, or if the
  // AssociatedReceiver sends a malformed or otherwise unexpected response
  // message to this AssociatedRemote. Must only be called
  // on a bound AssociatedRemote object, and only remains set as long as the
  // AssociatedRemote is both bound and connected.
  //
  // If invoked at all, |handler| will be scheduled asynchronously using the
  // AssociatedRemote's bound SequencedTaskRunner.
  void set_disconnect_handler(base::OnceClosure handler) {
    if (is_connected())
      internal_state_.set_connection_error_handler(std::move(handler));
  }

  // Similar to above but the handler receives additional metadata if provided
  // by the receiving endpoint when closing itself.
  void set_disconnect_with_reason_handler(
      ConnectionErrorWithReasonCallback handler) {
    internal_state_.set_connection_error_with_reason_handler(
        std::move(handler));
  }

  // Resets this AssociatedRemote to an unbound state. To reset the
  // AssociatedRemote and recover an PendingAssociatedRemote that can be bound
  // again later, use |Unbind()| instead.
  void reset() {
    State doomed_state;
    internal_state_.Swap(&doomed_state);
  }

  // Similar to the method above, but also specifies a disconnect reason.
  void ResetWithReason(uint32_t custom_reason, const std::string& description) {
    if (internal_state_.is_bound())
      internal_state_.CloseWithReason(custom_reason, description);
    reset();
  }

  // Binds this AssociatedRemote, connecting it to a new
  // PendingAssociatedReceiver which is returned for transmission to some
  // AssociatedReceiver which can bind it. The AssociatedRemote will schedule
  // any response callbacks or disconnection notifications on the default
  // SequencedTaskRunner (i.e. base::SequencedTaskRunnerHandle::Get() at the
  // time of this call). Must only be called on an unbound AssociatedRemote.
  PendingAssociatedReceiver<Interface> BindNewEndpointAndPassReceiver()
      WARN_UNUSED_RESULT {
    return BindNewEndpointAndPassReceiver(nullptr);
  }

  // Like above, but the AssociatedRemote will schedule response callbacks and
  // disconnection notifications on |task_runner| instead of the default
  // SequencedTaskRunner. |task_runner| must run tasks on the same sequence that
  // owns this AssociatedRemote.
  PendingAssociatedReceiver<Interface> BindNewEndpointAndPassReceiver(
      scoped_refptr<base::SequencedTaskRunner> task_runner) WARN_UNUSED_RESULT {
    ScopedInterfaceEndpointHandle remote_handle;
    ScopedInterfaceEndpointHandle receiver_handle;
    ScopedInterfaceEndpointHandle::CreatePairPendingAssociation(
        &remote_handle, &receiver_handle);
    Bind(PendingAssociatedRemote<Interface>(std::move(remote_handle), 0),
         std::move(task_runner));
    return PendingAssociatedReceiver<Interface>(std::move(receiver_handle));
  }

  // Like BindNewEndpointAndPassReceiver() above, but it creates a dedicated
  // message pipe. The returned receiver can be bound directly to an
  // implementation, without being first passed through a message pipe endpoint.
  //
  // For testing, where the returned request is bound to e.g. a mock and there
  // are no other interfaces involved.
  PendingAssociatedReceiver<Interface>
  BindNewEndpointAndPassDedicatedReceiverForTesting() WARN_UNUSED_RESULT {
    MessagePipe pipe;
    scoped_refptr<internal::MultiplexRouter> router0 =
        new internal::MultiplexRouter(
            std::move(pipe.handle0), internal::MultiplexRouter::MULTI_INTERFACE,
            false, base::SequencedTaskRunnerHandle::Get());
    scoped_refptr<internal::MultiplexRouter> router1 =
        new internal::MultiplexRouter(
            std::move(pipe.handle1), internal::MultiplexRouter::MULTI_INTERFACE,
            true, base::SequencedTaskRunnerHandle::Get());

    ScopedInterfaceEndpointHandle remote_handle;
    ScopedInterfaceEndpointHandle receiver_handle;
    ScopedInterfaceEndpointHandle::CreatePairPendingAssociation(
        &remote_handle, &receiver_handle);
    InterfaceId id = router1->AssociateInterface(std::move(remote_handle));
    remote_handle = router0->CreateLocalEndpointHandle(id);

    Bind(PendingAssociatedRemote<Interface>(std::move(remote_handle), 0),
         nullptr);
    return PendingAssociatedReceiver<Interface>(std::move(receiver_handle));
  }

  // Binds this AssociatedRemote by consuming |pending_remote|, which must be
  // valid. The AssociatedRemote will schedule any response callbacks or
  // disconnection notifications on the default SequencedTaskRunner (i.e.
  // base::SequencedTaskRunnerHandle::Get() at the time of this call). Must only
  // be called on an unbound AssociatedRemote.
  void Bind(PendingAssociatedRemote<Interface> pending_remote) {
    DCHECK(pending_remote.is_valid());
    Bind(std::move(pending_remote), nullptr);
  }

  // Like above, but the AssociatedRemote will schedule response callbacks and
  // disconnection notifications on |task_runner| instead of the default
  // SequencedTaskRunner. Must only be called on an unbound AssociatedRemote.
  // |task_runner| must run tasks on the same sequence that owns this
  // AssociatedRemote.
  void Bind(PendingAssociatedRemote<Interface> pending_remote,
            scoped_refptr<base::SequencedTaskRunner> task_runner) {
    DCHECK(!is_bound()) << "AssociatedRemote is already bound";
    if (!pending_remote) {
      reset();
      return;
    }

    internal_state_.Bind(
        AssociatedInterfacePtrInfo<Interface>(pending_remote.PassHandle(),
                                              pending_remote.version()),
        std::move(task_runner));

    // Force the internal state to configure its proxy. Unlike InterfacePtr we
    // do not use AssociatedRemote in transit, so binding to a pipe handle can
    // also imply binding to a SequencedTaskRunner and observing pipe handle
    // state.
    ignore_result(internal_state_.instance());
  }

  // Unbinds this AssociatedRemote, rendering it unable to issue further
  // Interface method calls. Returns a PendingAssociatedRemote which may be
  // passed across threads or processes and consumed by another AssociatedRemote
  // elsewhere.
  //
  // Note that it is an error (the bad, crashy kind of error) to attempt to
  // |Unbind()| a AssociatedRemote which is awaiting one or more responses to
  // previously issued Interface method calls. Calling this method should only
  // be considered in cases where satisfaction of that constraint can be proven.
  //
  // Must only be called on a bound AssociatedRemote.
  PendingAssociatedRemote<Interface> Unbind() WARN_UNUSED_RESULT {
    DCHECK(is_bound());
    CHECK(!internal_state_.has_pending_callbacks());
    State state;
    internal_state_.Swap(&state);
    AssociatedInterfacePtrInfo<Interface> info = state.PassInterface();
    return PendingAssociatedRemote<Interface>(info.PassHandle(),
                                              info.version());
  }

  // Sends a message on the underlying message pipe and runs the current
  // message loop until its response is received. This can be used in tests to
  // verify that no message was sent on a message pipe in response to some
  // stimulus.
  void FlushForTesting() { internal_state_.FlushForTesting(); }

  internal::AssociatedInterfacePtrState<Interface>* internal_state() {
    return &internal_state_;
  }

 private:
  using State = internal::AssociatedInterfacePtrState<Interface>;
  mutable State internal_state_;

  DISALLOW_COPY_AND_ASSIGN(AssociatedRemote);
};

// Constructs an invalid PendingAssociatedRemote of any arbitrary interface
// type. Useful as short-hand for a default constructed value.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) NullAssociatedRemote {
 public:
  template <typename Interface>
  operator PendingAssociatedRemote<Interface>() const {
    return PendingAssociatedRemote<Interface>();
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_REMOTE_H_
