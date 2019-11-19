// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_execution_context_connection_tracker.h"

#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/lock_observer.h"
#include "content/public/common/content_client.h"

namespace content {

namespace {

LockObserver* GetLockObserver() {
  return GetContentClient()->browser()->GetLockObserver();
}

}  // namespace

IndexedDBExecutionContextConnectionTracker::Handle::Handle(
    scoped_refptr<State> state)
    : state_(std::move(state)) {
  DCHECK(state_);
  int prev_num_connections = state_->num_connections++;
  if (state_->IsFrame() && prev_num_connections == 0) {
    LockObserver* lock_observer = GetLockObserver();
    if (lock_observer) {
      lock_observer->OnFrameStartsHoldingIndexedDBConnections(
          state_->render_process_id, state_->render_frame_id);
    }
  }
}

IndexedDBExecutionContextConnectionTracker::Handle::Handle(Handle&& other) =
    default;

IndexedDBExecutionContextConnectionTracker::Handle&
IndexedDBExecutionContextConnectionTracker::Handle::operator=(Handle&& other) =
    default;

IndexedDBExecutionContextConnectionTracker::Handle::~Handle() {
  if (state_ && state_->IsFrame()) {
    DCHECK_GT(state_->num_connections, 0);
    --state_->num_connections;
    if (state_->num_connections == 0) {
      LockObserver* lock_observer = GetLockObserver();
      if (lock_observer) {
        lock_observer->OnFrameStopsHoldingIndexedDBConnections(
            state_->render_process_id, state_->render_frame_id);
      }
    }
  }
}

// static
IndexedDBExecutionContextConnectionTracker::Handle
IndexedDBExecutionContextConnectionTracker::Handle::CreateForTesting() {
  return Handle(
      base::MakeRefCounted<State>(MSG_ROUTING_NONE, MSG_ROUTING_NONE));
}

IndexedDBExecutionContextConnectionTracker::
    IndexedDBExecutionContextConnectionTracker(int render_process_id,
                                               int render_frame_id)
    : state_(base::MakeRefCounted<State>(render_process_id, render_frame_id)) {}

IndexedDBExecutionContextConnectionTracker::
    IndexedDBExecutionContextConnectionTracker(
        IndexedDBExecutionContextConnectionTracker&& other) = default;

IndexedDBExecutionContextConnectionTracker::
    ~IndexedDBExecutionContextConnectionTracker() {}

IndexedDBExecutionContextConnectionTracker::Handle
IndexedDBExecutionContextConnectionTracker::CreateHandle() const {
  return Handle(state_);
}

IndexedDBExecutionContextConnectionTracker::State::State(int render_process_id,
                                                         int render_frame_id)
    : render_process_id(render_process_id), render_frame_id(render_frame_id) {}

}  // namespace content
