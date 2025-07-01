// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_TRACKED_CALLBACK_H_
#define PPAPI_SHARED_IMPL_TRACKED_CALLBACK_H_

#include <stdint.h>

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"
#include "ppapi/shared_impl/ppb_message_loop_shared.h"

namespace ppapi {

class CallbackTracker;
class MessageLoopShared;
class Resource;

namespace thunk {
namespace subtle {
// For a friend declaration below.
class EnterBase;
}
}

// |TrackedCallback| represents a tracked Pepper callback (from the browser to
// the plugin), typically still pending. Such callbacks have the standard Pepper
// callback semantics. Execution (i.e., completion) of callbacks happens through
// objects of subclasses of |TrackedCallback|. Two things are ensured: (1) that
// the callback is executed at most once, and (2) once a callback is marked to
// be aborted, any subsequent completion is abortive (even if a non-abortive
// completion had previously been scheduled).
//
// The details of non-abortive completion depend on the type of callback (e.g.,
// different parameters may be required), but basic abort functionality is core.
// The ability to post aborts is needed in many situations to ensure that the
// plugin is not re-entered into. (Note that posting a task to just run
// |Abort()| is different and not correct; calling |PostAbort()| additionally
// guarantees that all subsequent completions will be abortive.)
//
// This class is reference counted so that different things can hang on to it,
// and not worry too much about ensuring Pepper callback semantics. Note that
// the "owning" |CallbackTracker| will keep a reference until the callback is
// completed.
//
// A note on threading:
// TrackedCallback is usable on any thread. It is *mostly* only used when
// ppapi::ProxyLock is held. However, it's necessary that Run() can be called
// without the ProxyLock. This is used to allow running the callback from
// the IO thread. In particular, blocking callbacks may not have a message loop
// to which we could post, so Run() must be able to signal the condition
// variable to wake up the thread that's waiting on the blocking callback, and
// Run() must be able to do this while not holding the ProxyLock.
class PPAPI_SHARED_EXPORT TrackedCallback
    : public base::RefCountedThreadSafe<TrackedCallback> {
 public:
  TrackedCallback() = delete;

  // Create a tracked completion callback and register it with the tracker. The
  // resource pointer is not stored. If |resource| is NULL, this callback will
  // not be added to the callback tracker.
  TrackedCallback(Resource* resource, const PP_CompletionCallback& callback);

  TrackedCallback(const TrackedCallback&) = delete;
  TrackedCallback& operator=(const TrackedCallback&) = delete;

  // These run the callback in an abortive manner, or post a task to do so (but
  // immediately marking the callback as to be aborted).
  void Abort();
  void PostAbort();

  // Run the callback with the given result. If the callback had previously been
  // marked as to be aborted (by |PostAbort()|), |result| will be ignored and
  // the callback will be run with result |PP_ERROR_ABORTED|.
  //
  // Run() will invoke the call immediately, if invoked from the target thread
  // (as determined by target_loop_). If invoked on a different thread, the
  // callback will be scheduled to run later on target_loop_.
  void Run(int32_t result);
  void AcquireProxyLockAndRun(int32_t result);
  // PostRun is like Run(), except it guarantees that the callback will be run
  // later. In particular, if you invoke PostRun on the same thread on which the
  // callback is targeted to run, it will *not* be run immediately.
  void PostRun(int32_t result);

  // A task to perform cleanup or write output parameters before the callback
  // returns a result to the plugin. The |result| parameter has the result so
  // far, e.g. whether the callback has been aborted. If the callback hasn't
  // been aborted the return value of the task will become the callback result.
  // The task is always called on the same thread as the callback to the plugin.
  using CompletionTask = base::OnceCallback<int32_t(int32_t /* result */)>;

  // Sets a task that is run just before calling back into the plugin. This
  // should only be called once. Note that the CompletionTask always runs while
  // holding the ppapi::ProxyLock.
  void set_completion_task(CompletionTask completion_task);

  // Returns the ID of the resource which "owns" the callback, or 0 if the
  // callback is not associated with any resource.
  PP_Resource resource_id() const { return resource_id_; }

  // Returns true if this is a blocking callback.
  bool is_blocking() const {
    // This is set on construction and never changes after that, so there is
    // no need to lock.
    return !callback_.func;
  }

  MessageLoopShared* target_loop() const {
    // This is set on construction and never changes after that, so there is
    // no need to lock.
    return target_loop_.get();
  }

  // Determines if the given callback is pending. A callback is pending if it
  // has not completed and has not been aborted. When receiving a plugin call,
  // use this to detect if |callback| represents an operation in progress. When
  // finishing a plugin call, use this to determine whether to write 'out'
  // params and Run |callback|.
  // NOTE: an aborted callback has not necessarily completed, so a false result
  // doesn't imply that the callback has completed.
  // As a convenience, if |callback| is null, this returns false.
  static bool IsPending(const scoped_refptr<TrackedCallback>& callback);

  // Helper to determine if the given callback is scheduled to run on another
  // message loop.
  static bool IsScheduledToRun(const scoped_refptr<TrackedCallback>& callback);

 private:
  bool is_required() {
    return (callback_.func &&
            !(callback_.flags & PP_COMPLETIONCALLBACK_FLAG_OPTIONAL));
  }
  bool has_null_target_loop() const { return target_loop_.get() == NULL; }

  // Same as PostRun(), but lock_ must already be held.
  void PostRunWithLock(int32_t result);

  void SignalBlockingCallback(int32_t result);

  // TrackedCallback and EnterBase work together to provide appropriate behavior
  // for callbacks. Pepper interface implementations and proxies should
  // usually not have to check whether callbacks are required, optional, or
  // blocking. Nor should interface and proxy implementations have to worry
  // about blocking on a callback or marking them complete explicitly.
  //
  // (There are exceptions; e.g. FileIO checks is_blocking() in order to do
  // some operations directly on the calling thread if possible.)
  friend class ppapi::thunk::subtle::EnterBase;

  // Block until the associated operation has completed. Returns the result.
  // This must only be called on a non-main thread on a blocking callback.
  int32_t BlockUntilComplete();

  // Mark this object as complete and remove it from the tracker. This must only
  // be called once. Note that running this may result in this object being
  // deleted (so keep a reference if it'll still be needed).
  void MarkAsCompleted();
  void MarkAsCompletedWithLock();

  // This class is ref counted.
  friend class base::RefCountedThreadSafe<TrackedCallback>;
  ~TrackedCallback();

  mutable base::Lock lock_;

  // Flag used by |PostAbort()| and |PostRun()| to check that we don't schedule
  // the callback more than once.
  bool is_scheduled_;

  scoped_refptr<CallbackTracker> tracker_;
  PP_Resource resource_id_;
  bool completed_;
  bool aborted_;
  PP_CompletionCallback callback_;

  // Task to run just before calling back into the plugin.
  CompletionTask completion_task_;

  // The MessageLoopShared on which this callback should be run. This will be
  // NULL if we're in-process.
  scoped_refptr<MessageLoopShared> target_loop_;

  int32_t result_for_blocked_callback_;
  // Used for pausing/waking the blocked thread if this is a blocking completion
  // callback. Note that in-process, there is no lock, blocking callbacks are
  // not allowed, and therefore this pointer will be NULL.
  std::unique_ptr<base::ConditionVariable> operation_completed_condvar_;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_TRACKED_CALLBACK_H_
