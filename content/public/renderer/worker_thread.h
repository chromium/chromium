// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_WORKER_THREAD_H_
#define CONTENT_PUBLIC_RENDERER_WORKER_THREAD_H_

#include "base/functional/callback.h"
#include "content/common/content_export.h"

namespace content {

// Utility functions for worker threads, for example service worker threads.
//
// This allows getting the thread IDs for service worker threads, then later
// posting tasks back to them.
class CONTENT_EXPORT WorkerThread {
 public:
  constexpr static int kInvalidWorkerThreadId = -1;

  // Observes worker thread lifetime.
  class CONTENT_EXPORT Observer {
   public:
    virtual ~Observer() {}

    // Notifies the observer that the current worker thread is about to be
    // stopped.
    //
    // The worker state may have already been destroyed. To observe that, use
    // ContentRendererClient::WillDestroyServiceWorkerContextOnWorkerThread.
    virtual void WillStopCurrentWorkerThread() {}
  };

  WorkerThread(const WorkerThread&) = delete;
  WorkerThread& operator=(const WorkerThread&) = delete;

  // Adds/removes an Observer. Observers are stored per-thread, so it is only
  // valid to call these from a worker thread, and events will be dispatched on
  // that worker's thread.
  static void AddObserver(Observer* observer);
  static void RemoveObserver(Observer* observer);

  // Returns the worker thread ID for the current worker thread, or 0 if this is
  // not a worker thread (for example, the render thread). Worker thread IDs
  // will always be > 0.
  static int GetCurrentId();

  // Posts a task to the worker thread with ID |id|. ID must be > 0.
  static void PostTask(int id, base::OnceClosure task);

 private:
  WorkerThread() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_WORKER_THREAD_H_
