// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_HOST_QUEUE_H_
#define EXTENSIONS_BROWSER_EXTENSION_HOST_QUEUE_H_

#include <list>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

namespace extensions {
class DeferredStartRenderHost;

// A queue of ExtensionHosts waiting for initialization. This initializes
// DeferredStartRenderHosts in the order they're Add()ed, with simple rate
// limiting logic that re-posts each task to the UI thread, to avoid clogging it
// for a long period of time.
class ExtensionHostQueue {
 public:
  ExtensionHostQueue();
  ~ExtensionHostQueue();

  ExtensionHostQueue(const ExtensionHostQueue& queue) = delete;
  ExtensionHostQueue& operator=(const ExtensionHostQueue& queue) = delete;

  // Returns the single global instance of the ExtensionHostQueue.
  static ExtensionHostQueue& GetInstance();

  // Adds a host to the queue for RenderView creation.
  void Add(DeferredStartRenderHost* host);

  // Removes a host from the queue (for example, it may be deleted before
  // having a chance to start)
  void Remove(DeferredStartRenderHost* host);

  // Adds a delay before starting the next ExtensionHost. This can be used for
  // testing purposes to help flush out flakes.
  void SetCustomDelayForTesting(base::TimeDelta delay) { delay_ = delay; }

 private:
  // Queues up a delayed task to process the next DeferredStartRenderHost in
  // the queue.
  void PostTask();

  // Creates the RenderView for the next host in the queue.
  void ProcessOneHost();

  // True if this queue is currently in the process of starting an
  // DeferredStartRenderHost.
  bool pending_create_;

  // The delay before starting the next host. By default, this is 0, meaning we
  // just wait until the event loop yields.
  base::TimeDelta delay_;

  // The list of DeferredStartRenderHosts waiting to be started.
  std::list<raw_ptr<DeferredStartRenderHost, CtnExperimental>> queue_;

  base::WeakPtrFactory<ExtensionHostQueue> ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_HOST_QUEUE_H_
