// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_CALLBACK_TRACKER_H_
#define PPAPI_SHARED_IMPL_CALLBACK_TRACKER_H_

#include <map>
#include <set>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

class TrackedCallback;

// Pepper callbacks have the following semantics (unless otherwise specified;
// in particular, the below apply to all completion callbacks):
//  - Callbacks are always run on the thread where the plugin called a Pepper
//    function providing that callback.
//  - Callbacks are always called from the message loop of the thread. In
//    particular, calling into Pepper will not result in the plugin being
//    re-entered via a synchronously-run callback.
//  - Each callback will be executed (a.k.a. completed) exactly once.
//  - Each callback may be *aborted*, which means that it will be executed with
//    result |PP_ERROR_ABORTED| (in the case of completion callbacks). The
//    ABORT counts as the callback's one completion.
//  - Before |PPP_ShutdownModule()| is called, every pending callback (for every
//    instance of that module) will be aborted.
//  - Callbacks are usually associated to a resource, whose "deletion" provides
//    a "cancellation" (or abort) mechanism -- see below.
//  - When a plugin releases its last reference to resource, all callbacks
//    associated to that resource are aborted. Even if a non-abortive completion
//    of such a callback had previously been scheduled (i.e., posted), that
//    callback must now be aborted. The aborts should be scheduled immediately
//    (upon the last reference being given up) and should not rely on anything
//    else (e.g., a background task to complete or further action from the
//    plugin).
//  - Abortive completion gives no information about the status of the
//    asynchronous operation: The operation may have not yet begun, may be in
//    progress, or may be completed (successfully or not). In fact, the
//    operation may still proceed after the callback has been aborted.
//  - Any output data buffers provided to Pepper are associated with a resource.
//    Once that resource is released, no subsequent writes to those buffers
//    will occur. When operations occur on background threads, writing to the
//    plugin's data buffers should be delayed to happen on the callback's thread
//    to ensure that we don't write to the buffers if the callback has been
//    aborted (see TrackedCallback::set_completion_task()).
//
// Thread-safety notes:
// |CallbackTracker| uses a lock to protect its dictionary of callbacks. This
// is primarily to allow the safe removal of callbacks from any thread without
// requiring that the |ProxyLock| is held. Methods that may invoke a callback
// need to have the |ProxyLock| (and those methods assert that it's acquired).
// |TrackedCallback| is thread-safe ref-counted, so objects which live on
// different threads may keep references. Releasing a reference to
// |TrackedCallback| on a different thread (possibly causing destruction) is
// also okay.
//
// |CallbackTracker| tracks pending Pepper callbacks for a single instance. It
// also tracks, for each resource ID, which callbacks are pending. Just before
// a callback is completed, it is removed from the tracker. We use
// |CallbackTracker| for two things: (1) to ensure that all callbacks are
// properly aborted before instance shutdown, and (2) to ensure that all
// callbacks associated with a given resource are aborted when a plugin instance
// releases its last reference to that resource.
class PPAPI_SHARED_EXPORT CallbackTracker
    : public base::RefCountedThreadSafe<CallbackTracker> {
 public:
  CallbackTracker();

  CallbackTracker(const CallbackTracker&) = delete;
  CallbackTracker& operator=(const CallbackTracker&) = delete;

  // Abort all callbacks (synchronously).
  void AbortAll();

  // Abort all callbacks associated to the given resource ID (which must be
  // valid, i.e., nonzero) by posting a task (or tasks).
  void PostAbortForResource(PP_Resource resource_id);

 private:
  friend class base::RefCountedThreadSafe<CallbackTracker>;
  ~CallbackTracker();

  // |TrackedCallback| are expected to automatically add and
  // remove themselves from their provided |CallbackTracker|.
  friend class TrackedCallback;
  void Add(const scoped_refptr<TrackedCallback>& tracked_callback);
  void Remove(const scoped_refptr<TrackedCallback>& tracked_callback);

  // For each resource ID with a pending callback, store a set with its pending
  // callbacks. (Resource ID 0 is used for callbacks not associated to a valid
  // resource.) If a resource ID is re-used for another resource, there may be
  // aborted callbacks corresponding to the original resource in that set; these
  // will be removed when they are completed (abortively).
  typedef std::set<scoped_refptr<TrackedCallback> > CallbackSet;
  typedef std::map<PP_Resource, CallbackSet> CallbackSetMap;
  CallbackSetMap pending_callbacks_;

  // Used to ensure we don't add any callbacks after AbortAll.
  bool abort_all_called_;

  base::Lock lock_;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_CALLBACK_TRACKER_H_
