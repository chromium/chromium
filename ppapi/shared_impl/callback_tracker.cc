// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/callback_tracker.h"

#include <algorithm>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/tracked_callback.h"

namespace ppapi {

// CallbackTracker -------------------------------------------------------------

CallbackTracker::CallbackTracker() : abort_all_called_(false) {}

void CallbackTracker::AbortAll() {
  // Iterate over a copy:
  // 1) because |Abort()| calls |Remove()| (indirectly).
  // 2) So we can drop the lock before calling in to TrackedCallback.
  CallbackSetMap pending_callbacks_copy;
  {
    base::AutoLock acquire(lock_);
    pending_callbacks_copy = pending_callbacks_;
    abort_all_called_ = true;
  }
  for (CallbackSetMap::iterator it1 = pending_callbacks_copy.begin();
       it1 != pending_callbacks_copy.end();
       ++it1) {
    for (CallbackSet::iterator it2 = it1->second.begin();
         it2 != it1->second.end();
         ++it2) {
      (*it2)->Abort();
    }
  }
}

void CallbackTracker::PostAbortForResource(PP_Resource resource_id) {
  // Only TrackedCallbacks with a valid resource should appear in the tracker.
  DCHECK_NE(resource_id, 0);
  CallbackSet callbacks_for_resource;
  {
    base::AutoLock acquire(lock_);
    CallbackSetMap::iterator iter = pending_callbacks_.find(resource_id);
    // The resource may have no callbacks, so it won't be found, and we're done.
    if (iter == pending_callbacks_.end())
      return;
    // Copy the set so we can drop the lock before calling in to
    // TrackedCallback.
    callbacks_for_resource = iter->second;
  }
  for (const auto& iter : callbacks_for_resource) {
    iter->PostAbort();
  }
}

CallbackTracker::~CallbackTracker() {
  // All callbacks must be aborted before destruction.
  CHECK_EQ(0u, pending_callbacks_.size());
}

void CallbackTracker::Add(
    const scoped_refptr<TrackedCallback>& tracked_callback) {
  base::AutoLock acquire(lock_);
  DCHECK(!abort_all_called_);
  PP_Resource resource_id = tracked_callback->resource_id();
  // Only TrackedCallbacks with a valid resource should appear in the tracker.
  DCHECK_NE(resource_id, 0);
  DCHECK(pending_callbacks_[resource_id].find(tracked_callback) ==
         pending_callbacks_[resource_id].end());
  pending_callbacks_[resource_id].insert(tracked_callback);
}

void CallbackTracker::Remove(
    const scoped_refptr<TrackedCallback>& tracked_callback) {
  base::AutoLock acquire(lock_);
  CallbackSetMap::iterator map_it =
      pending_callbacks_.find(tracked_callback->resource_id());
  DCHECK(map_it != pending_callbacks_.end());
  CallbackSet::iterator it = map_it->second.find(tracked_callback);
  DCHECK(it != map_it->second.end());
  map_it->second.erase(it);

  // If there are no pending callbacks left for this ID, get rid of the entry.
  if (map_it->second.empty())
    pending_callbacks_.erase(map_it);
}

}  // namespace ppapi
