// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_SIMPLE_WATCHER_H_
#define MOJO_PUBLIC_CPP_SYSTEM_SIMPLE_WATCHER_H_

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/handle_signals_state.h"
#include "mojo/public/cpp/system/system_export.h"
#include "mojo/public/cpp/system/trap.h"

namespace base {
class SequencedTaskRunner;
}

namespace mojo {

// This provides a convenient sequence-bound watcher implementation to safely
// watch a single handle, dispatching state change notifications to an arbitrary
// SequencedTaskRunner running on the same sequence as the SimpleWatcher.
//
// SimpleWatcher exposes the concept of "arming" from the low-level Watcher API.
// In general, a SimpleWatcher must be "armed" in order to dispatch a single
// notification, and must then be rearmed before it will dispatch another. For
// more details, see the documentation for ArmingPolicy and the Arm() and
// ArmOrNotify() methods below.
class MOJO_CPP_SYSTEM_EXPORT SimpleWatcher {
 public:
  // A callback to be called any time a watched handle changes state in some
  // interesting way. The |result| argument indicates one of the following
  // conditions depending on its value:
  //
  //   |MOJO_RESULT_OK|: One or more of the signals being watched is satisfied.
  //
  //   |MOJO_RESULT_FAILED_PRECONDITION|: None of the signals being watched can
  //       ever be satisfied again.
  //
  //   |MOJO_RESULT_CANCELLED|: The watched handle has been closed. No further
  //       notifications will be fired, as this equivalent to an implicit
  //       CancelWatch().
  //
  // Note that unlike the first two conditions, this callback may be invoked
  // with |MOJO_RESULT_CANCELLED| even while the SimpleWatcher is disarmed.
  using ReadyCallback = base::RepeatingCallback<void(MojoResult result)>;

  // Like above but also receives the last known handle signal state at the time
  // of the notification.
  using ReadyCallbackWithState =
      base::RepeatingCallback<void(MojoResult result,
                                   const HandleSignalsState& state)>;

  // Selects how this SimpleWatcher is to be armed.
  enum class ArmingPolicy {
    // The SimpleWatcher is armed automatically on Watch() and rearmed again
    // after every invocation of the ReadyCallback. There is no need to manually
    // call Arm() on a SimpleWatcher using this policy. This mode is equivalent
    // to calling ArmOrNotify() once after Watch() and once again after every
    // dispatched notification in MANUAL mode.
    //
    // This provides a reasonable approximation of edge-triggered behavior,
    // mitigating (but not completely eliminating) the potential for redundant
    // notifications.
    //
    // NOTE: It is important when using AUTOMATIC policy that your ReadyCallback
    // always attempt to change the state of the handle (e.g. read available
    // messages on a message pipe.) Otherwise this will result in a potentially
    // large number of avoidable redundant tasks.
    //
    // For perfect edge-triggered behavior, use MANUAL policy and manually Arm()
    // the SimpleWatcher as soon as it becomes possible to do so again.
    AUTOMATIC,

    // The SimpleWatcher is never armed automatically. Arm() or ArmOrNotify()
    // must be called manually before any non-cancellation notification can be
    // dispatched to the ReadyCallback. See the documentation for Arm() and
    // ArmNotify() methods below for more details.
    MANUAL,
  };

  SimpleWatcher(const base::Location& from_here,
                ArmingPolicy arming_policy,
                scoped_refptr<base::SequencedTaskRunner> runner =
                    base::SequencedTaskRunnerHandle::Get());
  ~SimpleWatcher();

  // Indicates if the SimpleWatcher is currently watching a handle.
  bool IsWatching() const;

  // Starts watching |handle|. A SimpleWatcher may only watch one handle at a
  // time, but it is safe to call this more than once as long as the previous
  // watch has been cancelled (i.e. |IsWatching()| returns |false|.)
  //
  // If |handle| is not a valid watchable (message or data pipe) handle or
  // |signals| is not a valid set of signals to watch, this returns
  // |MOJO_RESULT_INVALID_ARGUMENT|.
  //
  // Otherwise |MOJO_RESULT_OK| is returned and the handle will be watched until
  // either |handle| is closed, the SimpleWatcher is destroyed, or Cancel() is
  // explicitly called.
  //
  // Once the watch is started, |callback| may be called at any time on the
  // current sequence until |Cancel()| is called or the handle is closed. Note
  // that |callback| can be called for results other than
  // |MOJO_RESULT_CANCELLED| only if the SimpleWatcher is currently armed. Use
  // ArmingPolicy to configure how a SimpleWatcher is armed.
  //
  // |MOJO_RESULT_CANCELLED| may be dispatched even while the SimpleWatcher
  // is disarmed, and no further notifications will be dispatched after that.
  //
  // Destroying the SimpleWatcher implicitly calls |Cancel()|.
  MojoResult Watch(Handle handle,
                   MojoHandleSignals signals,
                   MojoTriggerCondition condition,
                   ReadyCallbackWithState callback);

  // DEPRECATED: Please use the above signature instead.
  //
  // This watches a handle for |signals| to be satisfied, provided with a
  // callback which takes only a MojoResult value corresponding to the result of
  // a notification.
  MojoResult Watch(Handle handle,
                   MojoHandleSignals signals,
                   ReadyCallback callback) {
    return Watch(handle, signals, MOJO_WATCH_CONDITION_SATISFIED,
                 base::BindRepeating(&DiscardReadyState, std::move(callback)));
  }

  // Cancels the current watch. Once this returns, the ReadyCallback previously
  // passed to |Watch()| will never be called again for this SimpleWatcher.
  //
  // Note that when cancelled with an explicit call to |Cancel()| the
  // ReadyCallback will not be invoked with a |MOJO_RESULT_CANCELLED| result.
  void Cancel();

  // Manually arms the SimpleWatcher.
  //
  // Arming the SimpleWatcher allows it to fire a single notification regarding
  // some future relevant change in the watched handle's state. It's only valid
  // to call Arm() while a handle is being watched (see Watch() above.)
  //
  // SimpleWatcher is always disarmed immediately before invoking its
  // ReadyCallback and must be rearmed again before another notification can
  // fire.
  //
  // If the watched handle already meets the watched signaling conditions -
  // i.e., if it would have notified immediately once armed - the SimpleWatcher
  // is NOT armed, and this call fails with a return value of
  // |MOJO_RESULT_FAILED_PRECONDITION|. In that case, what would have been the
  // result code for that immediate notification is instead placed in
  // |*ready_result| if |ready_result| is non-null, and the last known signaling
  // state of the handle is placed in |*ready_state| if |ready_state| is
  // non-null.
  //
  // If the watcher is successfully armed (or was already armed), this returns
  // |MOJO_RESULT_OK| and |ready_result| and |ready_state| are ignored.
  MojoResult Arm(MojoResult* ready_result = nullptr,
                 HandleSignalsState* ready_state = nullptr);

  // Manually arms the SimpleWatcher OR posts a task to invoke the ReadyCallback
  // with the ready result of the failed arming attempt.
  //
  // This is meant as a convenient helper for a common usage of Arm(), and it
  // ensures that the ReadyCallback will be invoked asynchronously again as soon
  // as the watch's conditions are satisfied, assuming the SimpleWatcher isn't
  // cancelled first.
  //
  // Unlike Arm() above, this can never fail.
  void ArmOrNotify();

  Handle handle() const { return handle_; }
  ReadyCallbackWithState ready_callback() const { return callback_; }

  // Sets the tag used by the heap profiler.
  // |tag| must be a const string literal.
  void set_heap_profiler_tag(const char* heap_profiler_tag) {
    heap_profiler_tag_ = heap_profiler_tag;
  }

 private:
  class Context;

  static void DiscardReadyState(const ReadyCallback& callback,
                                MojoResult result,
                                const HandleSignalsState& state) {
    callback.Run(result);
  }

  void OnHandleReady(int watch_id,
                     MojoResult result,
                     const HandleSignalsState& state);

  SEQUENCE_CHECKER(sequence_checker_);

  // The policy used to determine how this SimpleWatcher is armed.
  const ArmingPolicy arming_policy_;

  // The TaskRunner of this SimpleWatcher's owning sequence. This field is safe
  // to access from any sequence.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Whether |task_runner_| is the same as
  // base::SequencedTaskRunnerHandle::Get() for the thread.
  const bool is_default_task_runner_;

  ScopedTrapHandle trap_handle_;

  // A thread-safe context object corresponding to the currently active watch,
  // if any.
  scoped_refptr<Context> context_;

  // Fields below must only be accessed on the SimpleWatcher's owning sequence.

  // The handle currently under watch. Not owned.
  Handle handle_;

  // A simple counter to disambiguate notifications from multiple watch contexts
  // in the event that this SimpleWatcher cancels and watches multiple times.
  int watch_id_ = 0;

  // The callback to call when the handle is signaled.
  ReadyCallbackWithState callback_;

  // Tag used to ID memory allocations that originated from notifications in
  // this watcher.
  const char* heap_profiler_tag_ = nullptr;

  base::WeakPtrFactory<SimpleWatcher> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SimpleWatcher);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_SIMPLE_WATCHER_H_
