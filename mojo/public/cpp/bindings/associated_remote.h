// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_REMOTE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_REMOTE_H_

#include <cstdint>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr_info.h"
#include "mojo/public/cpp/bindings/lib/associated_interface_ptr_state.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/runtime_features.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace mojo {

// An AssociatedRemote is similar to a Remote (see remote.h): it is used to
// issue mojom interface method calls that will be sent over a message pipe to
// be handled by the entangled AssociatedReceiver.
//
// An AssociatedRemote is needed when it is important to preserve the relative
// ordering of calls with another mojom interface. This is implemented by
// sharing the underlying message pipe between the mojom interfaces where
// ordering must be preserved.
//
// Because of this, an AssociatedRemote cannot be used to issue mojom interface
// method calls until one of its endpoints (either the AssociatedRemote itself
// or its entangled AssociatedReceiver) is sent over a Remote/Receiver pair
// or an already-established AssociatedRemote/AssociatedReceiver pair.
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
  // default SequencedTaskRunner (i.e.,
  // base::SequencedTaskRunner::GetCurrentDefault() at construction time).
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

  AssociatedRemote(const AssociatedRemote&) = delete;
  AssociatedRemote& operator=(const AssociatedRemote&) = delete;

  ~AssociatedRemote() = default;

  AssociatedRemote& operator=(AssociatedRemote&& other) noexcept {
    internal_state_.Swap(&other.internal_state_);
    return *this;
  }

  // Exposes access to callable Interface methods directed at this
  // AssociatedRemote's receiver. Must only be called on a bound
  // AssociatedRemote.
  //
  // Please also see comments of |is_bound()| about when it is safe to make
  // calls using the returned pointer.
  typename Interface::Proxy_* get() const {
    DCHECK(is_bound())
        << "Cannot issue Interface method calls on an unbound AssociatedRemote";
    return internal_state_.instance();
  }

  // Shorthand form of |get()|. See above.
  typename Interface::Proxy_* operator->() const { return get(); }
  typename Interface::Proxy_& operator*() const { return *get(); }

  // Indicates whether this AssociatedRemote is bound.
  //
  // NOTE:
  // 1) The state of being "bound" should not be confused with the state of
  // being "connected" (see |is_connected()| below). An AssociatedRemote is
  // NEVER passively unbound and the only way for it to become unbound is to
  // explicitly call |reset()| or |Unbind()|. As such, unless you make explicit
  // calls to those methods, it is always safe to assume that an
  // AssociatedRemote you've bound will remain bound.
  //
  // 2) The state of being "bound" is a necessary but not sufficient condition
  // for Interface methods to be callable. For them to be callable, the
  // AssociatedRemote must also be "associated", which means either one of
  // the following cases:
  //   2-1) Either itself or its entangled AssociatedReceiver must be sent over
  //   a Remote/Receiver pair or an already-established
  //   AssociatedRemote/AssociatedReceiver pair.
  //   2-2) It is bound with a dedicated message pipe. Please see comments of
  //   BindNewEndpointAndPassDedicatedReceiver().
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
  // message to this AssociatedRemote. Must only be called on a bound
  // AssociatedRemote object, and only remains set as long as the
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

  // A convenient helper that resets this AssociatedRemote on disconnect. Note
  // that this replaces any previously set disconnection handler. Must be called
  // on a bound AssociatedRemote object. If the AssociatedRemote is connected,
  // a callback is set to reset it after it is disconnected. If AssociatedRemote
  // is bound but disconnected then reset is called immediately.
  void reset_on_disconnect() {
    if (!is_connected()) {
      reset();
      return;
    }
    set_disconnect_handler(
        base::BindOnce(&AssociatedRemote::reset, base::Unretained(this)));
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

  // Helpers for binding and unbinding the AssociatedRemote. Only an unbound
  // AssociatedRemote (i.e. |is_bound()| is false) may be bound. Similarly, only
  // a bound AssociatedRemote may be unbound.

  // Binds this AssociatedRemote with the returned PendingAssociatedReceiver.
  // Mojom interface method calls made through |this| will be routed to the
  // object that ends up binding the returned PendingAssociatedReceiver.
  //
  // Any response callbacks or disconnection notifications will be scheduled to
  // run on |task_runner|. If |task_runner| is null, defaults to the current
  // SequencedTaskRunner.
  [[nodiscard]] PendingAssociatedReceiver<Interface>
  BindNewEndpointAndPassReceiver(
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr) {
    DCHECK(!is_bound()) << "AssociatedRemote for " << Interface::Name_
                        << " is already bound";
    if (!internal::GetRuntimeFeature_ExpectEnabled<Interface>()) {
      return PendingAssociatedReceiver<Interface>();
    }
    ScopedInterfaceEndpointHandle remote_handle;
    ScopedInterfaceEndpointHandle receiver_handle;
    ScopedInterfaceEndpointHandle::CreatePairPendingAssociation(
        &remote_handle, &receiver_handle);
    Bind(PendingAssociatedRemote<Interface>(std::move(remote_handle), 0),
         std::move(task_runner));
    return PendingAssociatedReceiver<Interface>(std::move(receiver_handle));
  }

  // Binds this AssociatedRemote by consuming |pending_remote|.
  //
  // Any response callbacks or disconnection notifications will be scheduled to
  // run on |task_runner|. If |task_runner| is null, defaults to the current
  // SequencedTaskRunner.
  void Bind(PendingAssociatedRemote<Interface> pending_remote,
            scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr) {
    DCHECK(!is_bound()) << "AssociatedRemote for " << Interface::Name_
                        << " is already bound";

    if (!pending_remote) {
      reset();
      return;
    }
    if (!internal::GetRuntimeFeature_ExpectEnabled<Interface>()) {
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
    std::ignore = internal_state_.instance();
  }

  // Binds this AssociatedRemote with the returned PendingAssociatedReceiver
  // using a dedicated message pipe. This allows the entangled
  // AssociatedReceiver/AssociatedRemote endpoints to be used without ever being
  // associated with any other mojom interfaces.
  //
  // Needless to say, messages sent between the two entangled endpoints will not
  // be ordered with respect to any other mojom interfaces. This is generally
  // useful for ignoring calls on an associated remote or for binding associated
  // endpoints in tests.
  [[nodiscard]] PendingAssociatedReceiver<Interface>
  BindNewEndpointAndPassDedicatedReceiver() {
    DCHECK(!is_bound()) << "AssociatedRemote for " << Interface::Name_
                        << " is already bound";

    PendingAssociatedReceiver<Interface> receiver =
        BindNewEndpointAndPassReceiver();
    if (receiver) {
      receiver.EnableUnassociatedUsage();
    }
    return receiver;
  }

  // Unbinds this AssociatedRemote, rendering it unable to issue further
  // Interface method calls. Returns a PendingAssociatedRemote which may be
  // passed across threads or processes and consumed by another AssociatedRemote
  // elsewhere.
  //
  // Note that it is an error (the bad, crashy kind of error) to attempt to
  // |Unbind()| an AssociatedRemote which is awaiting one or more responses to
  // previously issued Interface method calls. Calling this method should only
  // be considered in cases where satisfaction of that constraint can be proven.
  //
  // Must only be called on a bound AssociatedRemote.
  [[nodiscard]] PendingAssociatedRemote<Interface> Unbind() {
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
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_REMOTE_H_
