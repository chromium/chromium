// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_RECEIVER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_RECEIVER_H_

#include <stdint.h>

#include <memory>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/lib/sync_method_traits.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/raw_ptr_impl_ref_traits.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/runtime_features.h"

namespace mojo {

class MessageFilter;
class MessageReceiver;

namespace internal {

// Base class containing common code for various AssociatedReceiver template
// expansions to reduce code size.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) AssociatedReceiverBase {
 public:
  AssociatedReceiverBase();

  void SetFilter(std::unique_ptr<MessageFilter> filter);

  void reset();
  void ResetWithReason(uint32_t custom_reason, std::string_view description);

  void set_disconnect_handler(base::OnceClosure error_handler);
  void set_disconnect_with_reason_handler(
      ConnectionErrorWithReasonCallback error_handler);
  void reset_on_disconnect();

  bool is_bound() const { return !!endpoint_client_; }
  explicit operator bool() const { return !!endpoint_client_; }

  void FlushForTesting();

  // Please see comments on the same method of InterfaceEndpointClient.
  void ResetFromAnotherSequenceUnsafe() {
    if (endpoint_client_)
      endpoint_client_->ResetFromAnotherSequenceUnsafe();
  }

 protected:
  ~AssociatedReceiverBase();

  // TODO(dcheng): should probably document this thing.
  void BindImpl(ScopedInterfaceEndpointHandle handle,
                MessageReceiverWithResponderStatus* receiver,
                std::unique_ptr<MessageReceiver> payload_validator,
                base::span<const uint32_t> sync_method_ordinals,
                scoped_refptr<base::SequencedTaskRunner> runner,
                uint32_t interface_version,
                const char* interface_name,
                MessageToMethodInfoCallback method_info_callback,
                MessageToMethodNameCallback method_name_callback);

  std::unique_ptr<InterfaceEndpointClient> endpoint_client_;
};

}  // namespace internal

// An AssociatedReceiver is similar to a Receiver (see receiver.h): it receives
// incoming mojom interface method calls (sent over a message pipe from its
// entangled AssociatedRemote) and dispatches them to a concrete C++
// implementation.
//
// An AssociatedReceiver is needed when it is important to preserve the relative
// ordering of calls with another mojom interface. This is implemented by
// sharing the underlying message pipe between the mojom interfaces where
// ordering must be preserved.
//
// Because of this, an AssociatedReceiver will not receive any mojom interface
// method calls until one of its endpoints (either the AssociatedRemote itself
// or its entangled AssociatedReceiver) is sent over a Remote/Receiver pair
// or an already-established AssociatedRemote/AssociatedReceiver pair.
template <typename Interface,
          typename ImplRefTraits = RawPtrImplRefTraits<Interface>>
class AssociatedReceiver : public internal::AssociatedReceiverBase {
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
  explicit AssociatedReceiver(ImplPointerType impl) {
    stub_.set_sink(std::move(impl));
  }

  // Constructs a bound AssociatedReceiver by consuming |pending_receiver|. The
  // AssociatedReceiver is permanently linked to |impl| and will schedule
  // incoming |impl| method and disconnection notifications on the default
  // SequencedTaskRunner (i.e. base::SequencedTaskRunner::GetCurrentDefault() at
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
      : AssociatedReceiver(std::move(impl)) {
    Bind(std::move(pending_receiver), std::move(task_runner));
  }

  AssociatedReceiver(const AssociatedReceiver&) = delete;
  AssociatedReceiver& operator=(const AssociatedReceiver&) = delete;

  ~AssociatedReceiver() = default;

  // Indicates whether this AssociatedReceiver is bound, meaning it may continue
  // to receive Interface method calls from a remote caller.
  //
  // NOTE: An AssociatedReceiver is NEVER passively unbound. The only way for it
  // to become unbound is to explicitly call |reset()| or |Unbind()|.
  using AssociatedReceiverBase::is_bound;

  // Sets a OnceClosure to be invoked if this AssociatedReceiver is cut off from
  // its AssociatedRemote (or PendingAssociatedRemote). This can happen if the
  // corresponding AssociatedRemote (or unconsumed PendingAssociatedRemote) has
  // been destroyed, or if the AssociatedRemote sends a malformed message. Must
  // only be called on a bound AssociatedReceiver object, and only remains set
  // as long as the AssociatedReceiver is both bound and connected.
  //
  // If ever invoked, |handler| will be scheduled asynchronously on the
  // AssociatedReceiver's bound SequencedTaskRunner.
  using AssociatedReceiverBase::set_disconnect_handler;

  // Like above but when invoked |handler| will receive additional metadata
  // about why the remote endpoint was closed, if provided.
  using AssociatedReceiverBase::set_disconnect_with_reason_handler;

  // Resets this AssociatedReceiver on disconnect. Note that this replaces any
  // previously set disconnection handler. Must be called on a bound
  // AssociatedReceiver object, and only remains set as long as the
  // AssociatedReceiver is both bound and connected.
  using AssociatedReceiverBase::reset_on_disconnect;

  // Resets this AssociatedReceiver to an unbound state. An unbound
  // AssociatedReceiver will NEVER schedule method calls or disconnection
  // notifications, and any pending tasks which were scheduled prior to
  // unbinding are effectively cancelled.
  using AssociatedReceiverBase::reset;

  // Similar to above but provides additional information to the remote endpoint
  // about why this end is hanging up.
  using AssociatedReceiverBase::ResetWithReason;

  // Helpers for binding and unbinding the AssociatedReceiver. Only an unbound
  // AssociatedReceiver (i.e. |is_bound()| is false) may be bound. Similarly,
  // only a bound AssociatedReceiver may be unbound.

  // Binds this AssociatedReceiver with the returned PendingAssociatedRemote.
  // Mojom interface method calls made via the returned remote will be routed
  // and dispatched to |impl()|.
  //
  // Any incoming method calls or disconnection notifications will be scheduled
  // to run on |task_runner|. If |task_runner| is null, this defaults to the
  // current SequencedTaskRunner.
  [[nodiscard]] PendingAssociatedRemote<Interface> BindNewEndpointAndPassRemote(
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr) {
    DCHECK(!is_bound()) << "AssociatedReceiver for " << Interface::Name_
                        << " is already bound";
    PendingAssociatedRemote<Interface> remote;
    if (!internal::GetRuntimeFeature_ExpectEnabled<Interface>()) {
      return remote;
    }
    Bind(remote.InitWithNewEndpointAndPassReceiver(), std::move(task_runner));
    return remote;
  }

  // Binds this AssociatedReceiver by consuming |pending_receiver|.
  //
  // Any incoming method calls or disconnection notifications will be scheduled
  // to run on |task_runner|. If |task_runner| is null, this defaults to the
  // current SequencedTaskRunner.
  void Bind(PendingAssociatedReceiver<Interface> pending_receiver,
            scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr) {
    DCHECK(!is_bound()) << "AssociatedReceiver for " << Interface::Name_
                        << " is already bound";
    if (!pending_receiver) {
      reset();
      return;
    }
    if (!internal::GetRuntimeFeature_ExpectEnabled<Interface>()) {
      reset();
      return;
    }

    BindImpl(pending_receiver.PassHandle(), &stub_,
             base::WrapUnique(new typename Interface::RequestValidator_()),
             internal::SyncMethodTraits<Interface>::GetOrdinals(),
             std::move(task_runner), Interface::Version_, Interface::Name_,
             Interface::MessageToMethodInfo_, Interface::MessageToMethodName_);
  }

  // Binds this AssociatedReceiver with the returned PendingAssociatedRemote
  // using a dedicated message pipe. This allows the entangled
  // AssociatedReceiver/AssociatedRemote endpoints to be used without ever being
  // associated with any other mojom interfaces.
  //
  // Needless to say, messages sent between the two entangled endpoints will not
  // be ordered with respect to any other mojom interfaces. This is generally
  // useful for ignoring calls on an associated remote or for binding associated
  // endpoints in tests.
  [[nodiscard]] PendingAssociatedRemote<Interface>
  BindNewEndpointAndPassDedicatedRemote() {
    DCHECK(!is_bound()) << "AssociatedReceiver for " << Interface::Name_
                        << " is already bound";
    PendingAssociatedRemote<Interface> remote = BindNewEndpointAndPassRemote();
    if (remote) {
      remote.EnableUnassociatedUsage();
    }
    return remote;
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
  [[nodiscard]] PendingAssociatedReceiver<Interface> Unbind() {
    DCHECK(is_bound());
    // TODO(dcheng): Consider moving implementation into base class:
    //   std::exchange(endpoint_client_, nullptr)->PassHandle();
    PendingAssociatedReceiver<Interface> pending_receiver(
        endpoint_client_->PassHandle());
    endpoint_client_.reset();
    return pending_receiver;
  }

  // Sets a message filter to be notified of each incoming message before
  // dispatch. If a filter returns |false| from Accept(), the message is not
  // dispatched and the pipe is closed. Filters cannot be removed once added
  // and only one can be set.
  using AssociatedReceiverBase::SetFilter;

  // Sends a message on the underlying message pipe and runs the current
  // message loop until its response is received. This can be used in tests to
  // verify that no message was sent on a message pipe in response to some
  // stimulus.
  using AssociatedReceiverBase::FlushForTesting;

  // Returns the interface implementation that was previously specified.
  Interface* impl() { return ImplRefTraits::GetRawPointer(&stub_.sink()); }

  // Allows test code to swap the interface implementation.
  //
  // Returns the existing interface implementation to the caller.
  //
  // The caller needs to guarantee that `new_impl` will live longer than
  // `this` AssociatedReceiver.  One way to achieve this is to store
  // the returned `old_impl` and swap it back in when `new_impl` is getting
  // destroyed.
  // Test code should prefer using `mojo::test::ScopedSwapImplForTesting` if
  // possible.
  [[nodiscard]] ImplPointerType SwapImplForTesting(ImplPointerType new_impl) {
    return std::exchange(stub_.sink(), std::move(new_impl));
  }

  // Reports the currently dispatching message as bad and resets this receiver.
  // Note that this is only legal to call from within the stack frame of a
  // message dispatch. If you need to do asynchronous work before determining
  // the legitimacy of a message, use GetBadMessageCallback() and retain its
  // result until ready to invoke or discard it.
  NOT_TAIL_CALLED void ReportBadMessage(const std::string& error) {
    GetBadMessageCallback().Run(error);
  }

  // Acquires a callback which may be run to report the currently dispatching
  // message as bad and reset this receiver. Note that this is only legal to
  // call from directly within stack frame of a message dispatch, but the
  // returned callback may be called exactly once any time thereafter to report
  // the message as bad. |GetBadMessageCallback()| may only be called once per
  // message, and the returned callback must be run on the same sequence to
  // which this Receiver is bound.
  ReportBadMessageCallback GetBadMessageCallback() {
    return base::BindOnce(
        [](ReportBadMessageCallback inner_callback,
           base::WeakPtr<AssociatedReceiver> receiver, std::string_view error) {
          std::move(inner_callback).Run(error);
          if (receiver)
            receiver->reset();
        },
        mojo::GetBadMessageCallback(), weak_ptr_factory_.GetWeakPtr());
  }

  typename Interface::template Stub_<ImplRefTraits> stub_;

  base::WeakPtrFactory<AssociatedReceiver> weak_ptr_factory_{this};
};

// Associates |handle| with a dedicated and disconnected message pipe.
// Generally, |handle| should be the receiving side of an entangled
// AssociatedReceiver/AssociatedRemote pair, which allows the AssociatedRemote
// to be used to make calls that will be silently dropped.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS)
void AssociateWithDisconnectedPipe(ScopedInterfaceEndpointHandle handle);

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_RECEIVER_H_
