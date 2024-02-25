// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_REMOTE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_REMOTE_H_

#include <cstdint>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/async_flusher.h"
#include "mojo/public/cpp/bindings/lib/interface_ptr_state.h"
#include "mojo/public/cpp/bindings/pending_flush.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/runtime_features.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

// A Remote is used to issue Interface method calls to a single connected
// Receiver or PendingReceiver. The Remote must be bound in order to issue those
// method calls, and it becomes bound by consuming a PendingRemote either at
// construction time or by calling |Bind()|.
//
// Remote is NOT thread- or sequence-safe and must be used on a single
// (but otherwise arbitrary) sequence. All bound Remote objects are associated
// with a base::SequenceTaskRunner which the Remote uses exclusively to schedule
// response callbacks and disconnection notifications.
//
// The most common ways to bind a Remote are to consume a PendingRemote received
// via some IPC, or to call |BindNewPipeAndPassReceiver()| and send the returned
// PendingReceiver somewhere useful (i.e., to a remote Receiver who will consume
// it). For example:
//
//     mojo::Remote<mojom::Widget> widget;
//     widget_factory->CreateWidget(widget.BindNewPipeAndPassReceiver());
//     widget->Click();
//
// IMPORTANT: There are some things to be aware of regarding Interface method
// calls as they relate to Remote object lifetime:
//
//   - Interface method calls issued immediately before the destruction of a
//     Remote ARE guaranteed to be transmitted to the remote's receiver as long
//     as the receiver itself remains alive, either as a Receiver or a
//     PendingReceiver.
//
//   - In the name of memory safety, Interface method response callbacks (and in
//     general ANY tasks which can be scheduled by a Remote) will NEVER
//     be dispatched beyond the lifetime of the Remote object. As such, if
//     you make a call and you need its reply, you must keep the Remote alive
//     until the reply is received.
template <typename Interface>
class Remote {
 public:
  using InterfaceType = Interface;
  using PendingType = PendingRemote<Interface>;

  // Constructs an unbound Remote. This object cannot issue Interface method
  // calls and does not schedule any tasks.
  Remote() = default;
  Remote(Remote&& other) noexcept { *this = std::move(other); }

  // Constructs a new Remote which is bound from |pending_remote| and which
  // schedules response callbacks and disconnection notifications on the default
  // SequencedTaskRunner (i.e., base::SequencedTaskRunner::GetCurrentDefault()
  // at construction time).
  explicit Remote(PendingRemote<Interface> pending_remote)
      : Remote(std::move(pending_remote), nullptr) {}

  // Constructs a new Remote which is bound from |pending_remote| and which
  // schedules response callbacks and disconnection notifications on
  // |task_runner|. |task_runner| must run tasks on the same sequence that owns
  // this Remote.
  Remote(PendingRemote<Interface> pending_remote,
         scoped_refptr<base::SequencedTaskRunner> task_runner) {
    Bind(std::move(pending_remote), std::move(task_runner));
  }

  Remote(const Remote&) = delete;
  Remote& operator=(const Remote&) = delete;

  ~Remote() = default;

  Remote& operator=(Remote&& other) noexcept {
    internal_state_.Swap(&other.internal_state_);
    return *this;
  }

  // Exposes access to callable Interface methods directed at this Remote's
  // receiver. Must only be called on a bound Remote.
  typename Interface::Proxy_* get() const {
    DCHECK(is_bound())
        << "Cannot issue Interface method calls on an unbound Remote";
    return internal_state_.instance();
  }

  // Shorthand form of |get()|. See above.
  typename Interface::Proxy_* operator->() const { return get(); }
  typename Interface::Proxy_& operator*() const { return *get(); }

  // Indicates whether this Remote is bound and thus can issue Interface method
  // calls via the above accessors.
  //
  // NOTE: The state of being "bound" should not be confused with the state of
  // being "connected" (see |is_connected()| below). A Remote is NEVER passively
  // unbound and the only way for it to become unbound is to explicitly call
  // |reset()| or |Unbind()|. As such, unless you make explicit calls to those
  // methods, it is always safe to assume that a Remote you've bound will remain
  // bound and callable.
  bool is_bound() const { return internal_state_.is_bound(); }
  explicit operator bool() const { return is_bound(); }

  // Indicates whether this Remote is connected to a receiver. Must only be
  // called on a bound Remote. If this returns |true|, method calls made by this
  // Remote may eventually end up at the connected receiver (though it's of
  // course possible for this call to race with disconnection). If this returns
  // |false| however, all future Interface method calls on this Remote will be
  // silently dropped.
  //
  // A bound Remote becomes disconnected automatically either when its receiver
  // is destroyed, or when it receives a malformed or otherwise unexpected
  // response message from the receiver.
  //
  // NOTE: The state of being "bound" should not be confused with the state of
  // being "connected". See |is_bound()| above.
  bool is_connected() const {
    DCHECK(is_bound());
    return !internal_state_.encountered_error();
  }

  // Sets a Closure to be invoked if this Remote is cut off from its receiver.
  // This can happen if the corresponding Receiver (or unconsumed
  // PendingReceiver) is destroyed, or if the Receiver sends a malformed or
  // otherwise unexpected response message to this Remote. Must only be called
  // on a bound Remote object, and only remains set as long as the Remote is
  // both bound and connected.
  //
  // If invoked at all, |handler| will be scheduled asynchronously using the
  // Remote's bound SequencedTaskRunner.
  void set_disconnect_handler(base::OnceClosure handler) {
    if (is_connected())
      internal_state_.set_connection_error_handler(std::move(handler));
  }

  // Like above but also receives extra user-defined metadata about why the
  // receiving endpoint was closed.
  void set_disconnect_with_reason_handler(
      ConnectionErrorWithReasonCallback handler) {
    internal_state_.set_connection_error_with_reason_handler(
        std::move(handler));
  }

  // A convenient helper that resets this Remote on disconnect. Note that this
  // replaces any previously set disconnection handler. Must be called on a
  // bound Remote object. If the Remote is connected, a callback is set to reset
  // it after it is disconnected. If Remote is bound but disconnected then reset
  // is called immediately.
  void reset_on_disconnect() {
    if (!is_connected()) {
      reset();
      return;
    }
    set_disconnect_handler(
        base::BindOnce(&Remote::reset, base::Unretained(this)));
  }

  // Sets a Closure to be invoked any time the receiving endpoint reports itself
  // as idle and there are no in-flight messages it has yet to acknowledge, and
  // this state occurs continuously for a duration of at least |timeout|. The
  // first time this is called, it must be called BEFORE sending any interface
  // messages to the receiver. It may be called any number of times after that
  // to reconfigure the idle timeout period or assign a new idle handler.
  //
  // Once called, the interface connection incurs some permanent additional
  // per-message overhead to help track idle state across the interface
  // boundary.
  //
  // Whenever this callback is invoked, the following conditions are guaranteed
  // to hold:
  //
  //   - There are no messages sent on this Remote that have not already been
  //     dispatched by the receiver.
  //   - There are no interfaces which were bound directly or transitively
  //     through this Remote and are still connected.
  //   - The receiver has explicitly notified us that it considers itself to be
  //     "idle."
  //   - The receiver has not dispatched any additional messages since sending
  //     this idle notification.
  //   - The Remote does not have any outstanding reply callbacks that haven't
  //     been called yet.
  //   - All of the above has been true continuously for a duration of at least
  //     |timeout|.
  //
  void set_idle_handler(base::TimeDelta timeout,
                        base::RepeatingClosure handler) {
    internal_state_.set_idle_handler(timeout, std::move(handler));
  }

  // A convenient helper for common idle timeout behavior. This is equivalent to
  // calling |set_idle_handler| with a handler that only resets this Remote.
  void reset_on_idle_timeout(base::TimeDelta timeout) {
    set_idle_handler(
        timeout, base::BindRepeating(&Remote::reset, base::Unretained(this)));
  }

  // Resets this Remote to an unbound state. To reset the Remote and recover an
  // PendingRemote that can be bound again later, use |Unbind()| instead.
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

  // Returns the version of Interface used by this Remote. Defaults to 0 but can
  // be adjusted either at binding time, or by invoking either |QueryVersion()|
  // or |RequireVersion()|.
  uint32_t version() const { return internal_state_.version(); }

  // Binds this Remote, connecting it to a new PendingReceiver which is
  // returned for transmission to some Receiver which can bind it. The Remote
  // will schedule any response callbacks or disconnection notifications on the
  // default SequencedTaskRunner (i.e.
  // base::SequencedTaskRunner::GetCurrentDefault() at the time of this call).
  // Must only be called on an unbound Remote.
  [[nodiscard]] PendingReceiver<Interface> BindNewPipeAndPassReceiver() {
    DCHECK(!is_bound()) << "Remote for " << Interface::Name_
                        << " is already bound";
    return BindNewPipeAndPassReceiver(nullptr);
  }

  // Like above, but the Remote will schedule response callbacks and
  // disconnection notifications on |task_runner| instead of the default
  // SequencedTaskRunner. |task_runner| must run tasks on the same sequence that
  // owns this Remote.
  [[nodiscard]] PendingReceiver<Interface> BindNewPipeAndPassReceiver(
      scoped_refptr<base::SequencedTaskRunner> task_runner) {
    DCHECK(!is_bound()) << "Remote for " << Interface::Name_
                        << " is already bound";
    if (!internal::GetRuntimeFeature_ExpectEnabled<Interface>()) {
      reset();
      return PendingReceiver<Interface>();
    }
    MessagePipe pipe;
    Bind(PendingRemote<Interface>(std::move(pipe.handle0), 0),
         std::move(task_runner));
    return PendingReceiver<Interface>(std::move(pipe.handle1));
  }

  // Binds this Remote by consuming |pending_remote|, which must be valid. The
  // Remote will schedule any response callbacks or disconnection notifications
  // on the default SequencedTaskRunner (i.e.
  // base::SequencedTaskRunner::GetCurrentDefault() at the time of this call).
  // Must only be called on an unbound Remote.
  void Bind(PendingRemote<Interface> pending_remote) {
    DCHECK(!is_bound()) << "Remote for " << Interface::Name_
                        << " is already bound";
    DCHECK(pending_remote.is_valid());
    Bind(std::move(pending_remote), nullptr);
  }

  // Like above, but the Remote will schedule response callbacks and
  // disconnection notifications on |task_runner| instead of the default
  // SequencedTaskRunner. Must only be called on an unbound Remote.
  // |task_runner| must run tasks on the same sequence that owns this Remote.
  void Bind(PendingRemote<Interface> pending_remote,
            scoped_refptr<base::SequencedTaskRunner> task_runner) {
    DCHECK(!is_bound()) << "Remote for " << Interface::Name_
                        << " is already bound";
    if (!pending_remote) {
      reset();
      return;
    }
    if (!internal::GetRuntimeFeature_ExpectEnabled<Interface>()) {
      reset();
      return;
    }
    internal_state_.Bind(pending_remote.internal_state(),
                         std::move(task_runner));

    // Force the internal state to configure its proxy. Unlike InterfacePtr we
    // do not use Remote in transit, so binding to a pipe handle can also imply
    // binding to a SequencedTaskRunner and observing pipe handle state. This
    // allows for e.g. |is_connected()| to be a more reliable API than
    // |InterfacePtr::encountered_error()|.
    std::ignore = internal_state_.instance();
  }

  // Unbinds this Remote, rendering it unable to issue further Interface method
  // calls. Returns a PendingRemote which may be passed across threads or
  // processes and consumed by another Remote elsewhere.
  //
  // Note that it is an error (the bad, crashy kind of error) to attempt to
  // |Unbind()| a Remote which is awaiting one or more responses to previously
  // issued Interface method calls. Calling this method should only be
  // considered in cases where satisfaction of that constraint can be proven.
  //
  // Must only be called on a bound Remote.
  [[nodiscard]] PendingRemote<Interface> Unbind() {
    DCHECK(is_bound());
    CHECK(!internal_state_.has_pending_callbacks());
    State state;
    internal_state_.Swap(&state);
    internal::PendingRemoteState pending_state = state.Unbind();
    return PendingRemote<Interface>(std::move(pending_state.pipe),
                                    pending_state.version);
  }

  // Queries the max version that the receiving endpoint supports. Once a
  // response is received, |callback| will be invoked with the version number
  // and the version number of this Remote object will also be updated.
  void QueryVersion(base::OnceCallback<void(uint32_t)> callback) {
    internal_state_.QueryVersion(std::move(callback));
  }

  // Requires the receiving endpoint to support at least the specified
  // |version|. If it does not, it will close its end of the connection
  // immediately.
  void RequireVersion(uint32_t version) {
    internal_state_.RequireVersion(version);
  }

  // Pauses the receiving endpoint until the flush corresponding to |flush| has
  // completed. Any calls made on this Remote prior to this call will be
  // dispatched at the receiving endpoint before pausing. The endpoint will not
  // dispatch any subsequent calls until the flush operation corresponding to
  // |flush| has been completed or canceled.
  //
  // See documentation for |FlushAsync()| on Remote and Receiver for how to
  // acquire a PendingFlush object, and documentation on PendingFlush for
  // example usage.
  void PauseReceiverUntilFlushCompletes(PendingFlush flush) {
    internal_state_.PauseReceiverUntilFlushCompletes(std::move(flush));
  }

  // Flushes the receiving endpoint asynchronously using |flusher|. Once all
  // calls made on this Remote prior to this |FlushAsyncWithFlusher()| call have
  // dispatched at the receiving endpoint, |flusher| will signal its
  // corresponding PendingFlush, unblocking any endpoint waiting on the flush
  // operation.
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

  // Sends a no-op message on the underlying message pipe and runs the current
  // message loop until its response is received. This can be used in tests to
  // verify that no message was sent on a message pipe in response to some
  // stimulus.
  void FlushForTesting() { internal_state_.FlushForTesting(); }

  // Same as |FlushForTesting()| but will call |callback| when the flush is
  // complete.
  void FlushAsyncForTesting(base::OnceClosure callback) {
    internal_state_.FlushAsyncForTesting(std::move(callback));
  }

  // Returns the number of unacknowledged messages sent by this Remote. Only
  // non-zero when |set_idle_handler()| has been called.
  unsigned int GetNumUnackedMessagesForTesting() const {
    return internal_state_.GetNumUnackedMessagesForTesting();
  }

  // DO NOT USE. Exposed only for internal use and for testing.
  internal::InterfacePtrState<Interface>* internal_state() {
    return &internal_state_;
  }

 private:
  using State = internal::InterfacePtrState<Interface>;
  mutable State internal_state_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_REMOTE_H_
