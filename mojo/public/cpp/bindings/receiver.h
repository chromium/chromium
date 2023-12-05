// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_RECEIVER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_RECEIVER_H_

#include <memory>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/async_flusher.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/bindings/connection_group.h"
#include "mojo/public/cpp/bindings/lib/binding_state.h"
#include "mojo/public/cpp/bindings/pending_flush.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/raw_ptr_impl_ref_traits.h"
#include "mojo/public/cpp/bindings/runtime_features.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

// A Receiver is used to receive and dispatch Interface method calls to a local
// implementation of Interface. Every Receiver object is permanently linked to
// an implementation of Interface at construction time. The Receiver begins
// receiving and scheduling method calls to the implementation once it becomes
// bound either by consuming a PendingReceiver (at construction time or via
// |Bind()|) or by calling |BindNewPipeAndPassRemote()|.
//
// Receiver is NOT thread- or sequence- safe and must be used from a single
// (but otherwise arbitrary) sequence. All bound Receiver objects are associated
// with a base::SequencedTaskRunner which the Receiver uses exclusively to
// schedule incoming method calls and disconnection notifications.
//
// IMPORTANT: In the name of memory safety, Interface method calls and
// disconnection notifications scheduled by a Receiver object will NEVER run
// beyond the lifetime of the Receiver object.
template <typename Interface,
          typename ImplRefTraits = RawPtrImplRefTraits<Interface>>
class Receiver {
 public:
  // Typically (and by default) a Receiver uses a raw pointer to reference its
  // linked Interface implementation object, because typically that
  // implementation object owns the Receiver. An alternative |ImplRefTraits| may
  // be provided as a second Receiver template argument in order to use a
  // different reference type.
  using ImplPointerType = typename ImplRefTraits::PointerType;

  // Constructs an unbound Receiver linked to |impl| for the duration of the
  // Receiver's lifetime. The Receiver can be bound later by calling |Bind()| or
  // |BindNewPipeAndPassRemote()|. An unbound Receiver does not schedule any
  // asynchronous tasks.
  explicit Receiver(ImplPointerType impl) : internal_state_(std::move(impl)) {}

  // Constructs a bound Receiver by consuming |pending_receiver|. The Receiver
  // is permanently linked to |impl| and will schedule incoming |impl| method
  // and disconnection notifications on the default SequencedTaskRunner (i.e.
  // base::SequencedTaskRunner::GetCurrentDefault() at construction time).
  Receiver(ImplPointerType impl, PendingReceiver<Interface> pending_receiver)
      : Receiver(std::move(impl), std::move(pending_receiver), nullptr) {}

  // Similar to above but the constructed Receiver schedules all tasks via
  // |task_runner| instead of the default SequencedTaskRunner. |task_runner|
  // must run tasks on the same sequence that owns this Receiver.
  Receiver(ImplPointerType impl,
           PendingReceiver<Interface> pending_receiver,
           scoped_refptr<base::SequencedTaskRunner> task_runner)
      : internal_state_(std::move(impl)) {
    Bind(std::move(pending_receiver), std::move(task_runner));
  }

  Receiver(const Receiver&) = delete;
  Receiver& operator=(const Receiver&) = delete;

  ~Receiver() = default;

  // Indicates whether this Receiver is bound, meaning it may continue to
  // receive Interface method calls from a remote caller.
  //
  // NOTE: A Receiver is NEVER passively unbound. The only way for it to become
  // unbound is to explicitly call |reset()| or |Unbind()|.
  bool is_bound() const { return internal_state_.is_bound(); }

  // Sets a OnceClosure to be invoked if this Receiver is cut off from its
  // Remote (or PendingRemote). This can happen if the corresponding Remote (or
  // unconsumed PendingRemote) has been destroyed, or if the Remote sends a
  // malformed message. Must only be called on a bound Receiver object, and only
  // remains set as long as the Receiver is both bound and connected.
  //
  // If ever invoked, |handler| will be scheduled asynchronously on the
  // Receiver's bound SequencedTaskRunner.
  void set_disconnect_handler(base::OnceClosure handler) {
    internal_state_.set_connection_error_handler(std::move(handler));
  }

  // Like above but if this callback is set instead of the above, it can receive
  // additional details about why the remote endpoint was closed, if provided.
  void set_disconnect_with_reason_handler(
      ConnectionErrorWithReasonCallback error_handler) {
    DCHECK(is_bound());
    internal_state_.set_connection_error_with_reason_handler(
        std::move(error_handler));
  }

  // Resets this Receiver to an unbound state. An unbound Receiver will NEVER
  // schedule method calls or disconnection notifications, and any pending tasks
  // which were scheduled prior to unbinding are effectively cancelled.
  void reset() { internal_state_.Close(); }

  // Similar to the method above, but also specifies a disconnect reason.
  void ResetWithReason(uint32_t custom_reason_code,
                       std::string_view description) {
    internal_state_.CloseWithReason(custom_reason_code, description);
  }

  // Binds this Receiver, connecting it to a new PendingRemote which is
  // returned for transmission elsewhere (typically to a Remote who will consume
  // it to start making calls).
  //
  // The Receiver will schedule incoming |impl| method calls and disconnection
  // notifications on the default SequencedTaskRunner (i.e.
  // base::SequencedTaskRunner::GetCurrentDefault() at the time of this call).
  // Must only be called on an unbound Receiver.
  [[nodiscard]] PendingRemote<Interface> BindNewPipeAndPassRemote() {
    return BindNewPipeAndPassRemote(nullptr);
  }

  // Like above, but the Receiver will schedule incoming |impl| method calls and
  // disconnection notifications on |task_runner| rather than on the default
  // SequencedTaskRunner. Must only be called on an unbound Receiver.
  // |task_runner| must run tasks on the same sequence that owns this Receiver.
  [[nodiscard]] PendingRemote<Interface> BindNewPipeAndPassRemote(
      scoped_refptr<base::SequencedTaskRunner> task_runner) {
    DCHECK(!is_bound()) << "Receiver for " << Interface::Name_
                        << " is already bound";
    PendingRemote<Interface> remote;
    if (!internal::GetRuntimeFeature_ExpectEnabled<Interface>()) {
      reset();
      return remote;
    }
    Bind(remote.InitWithNewPipeAndPassReceiver(), std::move(task_runner));
    return remote;
  }

  // Like above, but the returned PendingRemote has the version annotated.
  [[nodiscard]] PendingRemote<Interface> BindNewPipeAndPassRemoteWithVersion(
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr) {
    auto remote = BindNewPipeAndPassRemote(task_runner);
    remote.internal_state()->version = Interface::Version_;
    return remote;
  }

  // Binds this Receiver by consuming |pending_receiver|, which must be valid.
  // Must only be called on an unbound Receiver.
  //
  // The newly bound Receiver will schedule incoming |impl| method calls and
  // disconnection notifications on the default SequencedTaskRunner (i.e.
  // base::SequencedTaskRunner::GetCurrentDefault() at the time of this call).
  void Bind(PendingReceiver<Interface> pending_receiver) {
    DCHECK(!is_bound()) << "Receiver for " << Interface::Name_
                        << " is already bound";
    Bind(std::move(pending_receiver), nullptr);
  }

  // Like above, but the newly bound Receiver will schedule incoming |impl|
  // method calls and disconnection notifications on |task_runner| instead of
  // the default SequencedTaskRunner. Must only be called on an unbound
  // Receiver. |task_runner| must run tasks on the same sequence that owns this
  // Receiver.
  void Bind(PendingReceiver<Interface> pending_receiver,
            scoped_refptr<base::SequencedTaskRunner> task_runner) {
    DCHECK(!is_bound()) << "Receiver for " << Interface::Name_
                        << " is already bound";
    if (!pending_receiver) {
      reset();
      return;
    }
    if (!internal::GetRuntimeFeature_ExpectEnabled<Interface>()) {
      reset();
      return;
    }
    internal_state_.Bind(pending_receiver.internal_state(),
                         std::move(task_runner));
  }

  // Unbinds this Receiver, preventing any further |impl| method calls or
  // disconnection notifications from being scheduled by it. Any such tasks that
  // were scheduled prior to unbinding are effectively cancelled.
  //
  // Returns a PendingReceiver which remains connected to this receiver's
  // Remote and which may be transferred elsewhere and consumed by another
  // Receiver. Any messages received but not actually dispatched by this
  // Receiver remain intact within the returned PendingReceiver and can be
  // dispatched by whomever binds with it later.
  //
  // Note that a Receiver should not be unbound while there are still living
  // response callbacks that haven't been invoked, as once the Receiver is
  // unbound those response callbacks are no longer valid and the Remote will
  // never be able to receive its expected responses.
  [[nodiscard]] PendingReceiver<Interface> Unbind() {
    DCHECK(is_bound());
    CHECK(!internal_state_.HasAssociatedInterfaces());
    return internal_state_.Unbind();
  }

  // Sets the message filter to be notified of each incoming message before
  // dispatch. If a filter returns |false| from WillDispatch(), the message is
  // not dispatched and the pipe is closed. Filters cannot be removed once
  // added and only one can be set.
  void SetFilter(std::unique_ptr<MessageFilter> filter) {
    DCHECK(is_bound());
    internal_state_.SetFilter(std::move(filter));
  }

  // Pause and resume message dispatch.
  void Pause() {
    CHECK(!internal_state_.HasAssociatedInterfaces());
    internal_state_.PauseIncomingMethodCallProcessing();
  }

  void Resume() { internal_state_.ResumeIncomingMethodCallProcessing(); }

  // Blocks the calling thread until a new message arrives and is dispatched
  // to the bound implementation.
  bool WaitForIncomingCall() {
    return internal_state_.WaitForIncomingMethodCall();
  }

  // Pauses the Remote endpoint, stopping dispatch of callbacks on that end. Any
  // callbacks called prior to this will dispatch before the Remote endpoint is
  // paused; any callbacks called after this will only be called once the flush
  // operation corresponding to |flush| is completed or canceled.
  //
  // See documentation for |FlushAsync()| on Remote and Receiver for how to
  // acquire a PendingFlush object, and documentation on PendingFlush for
  // example usage.
  void PauseRemoteCallbacksUntilFlushCompletes(PendingFlush flush) {
    internal_state_.PauseRemoteCallbacksUntilFlushCompletes(std::move(flush));
  }

  // Flushes the Remote endpoint asynchronously using |flusher|. The
  // corresponding PendingFlush will be notified only once all response
  // callbacks issued prior to this operation have been dispatched at the Remote
  // endpoint.
  //
  // NOTE: It is more common to use |FlushAsync()| defined below. If you really
  // want to provide your own AsyncFlusher using this method, see the
  // single-arugment constructor on PendingFlush. This would typically be used
  // when code executing on the current sequence wishes to immediately pause
  // one of its remote endpoints to wait on a flush operation that needs to be
  // initiated on a separate sequence. Rather than bouncing to the second
  // sequence to initiate a flush and then passing a PendingFlush back to the
  // original sequence, the AsyncFlusher/PendingFlush can be created on the
  // original sequence and a single task can be posted to pass the AsyncFlusher
  // to the second sequence for use with this method.
  void FlushAsyncWithFlusher(AsyncFlusher flusher) {
    internal_state_.FlushAsync(std::move(flusher));
  }

  // Same as above but an AsyncFlusher/PendingFlush pair is created on the
  // caller's behalf. The AsyncFlusher is immediately passed to a
  // |FlushAsyncWithFlusher()| call on this object, while the PendingFlush is
  // returned for use by the caller. See documentation on PendingFlush for
  // example usage.
  PendingFlush FlushAsync() {
    AsyncFlusher flusher;
    PendingFlush flush(&flusher);
    FlushAsyncWithFlusher(std::move(flusher));
    return flush;
  }

  // Flushes any replies previously sent by the Receiver, only unblocking once
  // acknowledgement from the Remote is received.
  void FlushForTesting() { internal_state_.FlushForTesting(); }

  // Exposed for testing, should not generally be used.
  void EnableTestingMode() { internal_state_.EnableTestingMode(); }

  // Allows test code to swap the interface implementation.
  //
  // Returns the existing interface implementation to the caller.
  //
  // The caller needs to guarantee that `new_impl` will live longer than
  // `this` Receiver.  One way to achieve this is to store the returned
  // `old_impl` and swap it back in when `new_impl` is getting destroyed.
  // Test code should prefer using `mojo::test::ScopedSwapImplForTesting` if
  // possible.
  [[nodiscard]] ImplPointerType SwapImplForTesting(ImplPointerType new_impl) {
    return internal_state_.SwapImplForTesting(std::move(new_impl));
  }

  // Reports the currently dispatching message as bad and resets this receiver.
  // Note that this is only legal to call from within the stack frame of a
  // message dispatch. If you need to do asynchronous work before determining
  // the legitimacy of a message, use GetBadMessageCallback() and retain its
  // result until ready to invoke or discard it.
  NOT_TAIL_CALLED void ReportBadMessage(std::string_view error) {
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
    return internal_state_.GetBadMessageCallback();
  }

  // DO NOT USE. Exposed only for internal use and for testing.
  internal::BindingState<Interface, ImplRefTraits>* internal_state() {
    return &internal_state_;
  }

 private:
  internal::BindingState<Interface, ImplRefTraits> internal_state_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_RECEIVER_H_
