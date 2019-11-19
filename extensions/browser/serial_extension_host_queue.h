// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SERIAL_EXTENSION_HOST_QUEUE_H_
#define EXTENSIONS_BROWSER_SERIAL_EXTENSION_HOST_QUEUE_H_

#include <list>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/extension_host_queue.h"

namespace extensions {

class DeferredStartRenderHost;

// An ExtensionHostQueue which initializes DeferredStartRenderHosts in the order
// they're Add()ed, with simple rate limiting logic that re-posts each task to
// the UI thread, to avoid clogging it for a long period of time.
class SerialExtensionHostQueue : public ExtensionHostQueue {
 public:
  SerialExtensionHostQueue();
  ~SerialExtensionHostQueue() override;

 private:
  // ExtensionHostQueue:
  void Add(DeferredStartRenderHost* host) override;
  void Remove(DeferredStartRenderHost* host) override;

  // Queues up a delayed task to process the next DeferredStartRenderHost in
  // the queue.
  void PostTask();

  // Creates the RenderView for the next host in the queue.
  void ProcessOneHost();

  // True if this queue is currently in the process of starting an
  // DeferredStartRenderHost.
  bool pending_create_;

  // The list of DeferredStartRenderHosts waiting to be started.
  std::list<DeferredStartRenderHost*> queue_;

  base::WeakPtrFactory<SerialExtensionHostQueue> ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SerialExtensionHostQueue);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SERIAL_EXTENSION_HOST_QUEUE_H_
