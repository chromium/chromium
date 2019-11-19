// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_EXECUTION_CONTEXT_CONNECTION_TRACKER_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_EXECUTION_CONTEXT_CONNECTION_TRACKER_H_

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message.h"

namespace content {

// Tracks the number of connections (pending or active) for an execution
// context. Notifies content::LockObserver when the number switches between zero
// and non-zero.
//
// There should be one IndexedDBExecutionContextConnectionTracker per execution
// context that uses IndexedDB. Each connection (pending or active) should own a
// Handle obtained from that IndexedDBExecutionContextConnectionTracker.
//
// TODO(https://crbug.com/980533): Currently, this only notifies LockObserver
// for connections held by frame. We should also notify for connections held by
// worker. This requires finding a unique type to identify a frame or a worker.
class CONTENT_EXPORT IndexedDBExecutionContextConnectionTracker {
 private:
  struct State;

 public:
  // Each connection (pending or active) should own a non-null Handle that
  // allows keeping track of the number of connections for the parent execution
  // context.
  class CONTENT_EXPORT Handle {
   public:
    Handle(scoped_refptr<State> state);
    Handle(Handle&& other);
    Handle& operator=(Handle&& other);
    ~Handle();

    // Creates a dummy non-null Handle for testing.
    static Handle CreateForTesting();

    // Returns true if this Handle is null. A connection should be associated
    // with a non-null Handle.
    bool is_null() const { return !state_; }

    // Returns the routing id of the process hosting the execution context for
    // which this Handle tracks connections.
    int render_process_id() const {
      DCHECK(!is_null());
      return state_->render_process_id;
    }

   private:
    scoped_refptr<State> state_;

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle& other) = delete;
  };

  // |render_process_id| identifies the process hosting the execution context.
  // |render_frame_id| identifies the frame if the execution context is a frame,
  // or is MSG_ROUTING_NONE if the execution context is a worker.
  IndexedDBExecutionContextConnectionTracker(int render_process_id,
                                             int render_frame_id);

  IndexedDBExecutionContextConnectionTracker(
      IndexedDBExecutionContextConnectionTracker&& other);

  ~IndexedDBExecutionContextConnectionTracker();

  // Creates a handle that will be owned by a connection (pending or active) in
  // the execution context.
  Handle CreateHandle() const;

 private:
  struct State : public base::RefCounted<State> {
    State(int render_process_id, int render_frame_id);

    bool IsFrame() const { return render_frame_id != MSG_ROUTING_NONE; }

    const int render_process_id;
    const int render_frame_id;
    int num_connections = 0;

   private:
    friend class base::RefCounted<State>;
    ~State() = default;
  };

  scoped_refptr<State> state_;

  IndexedDBExecutionContextConnectionTracker(
      const IndexedDBExecutionContextConnectionTracker&) = delete;
  IndexedDBExecutionContextConnectionTracker& operator=(
      const IndexedDBExecutionContextConnectionTracker& other) = delete;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_EXECUTION_CONTEXT_CONNECTION_TRACKER_H_
