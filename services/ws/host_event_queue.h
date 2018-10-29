// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_HOST_EVENT_QUEUE_H_
#define SERVICES_WS_HOST_EVENT_QUEUE_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace aura {
class WindowTreeHost;
}

namespace ui {
class Event;
}

namespace ws {

class EventQueue;
class HostEventDispatcher;

// HostEventQueue is associated with a single WindowTreeHost and used to queue
// events (if necessary), before the WindowTreeHost does processing.
// HostEventQueue is created by way of
// WindowService::RegisterHostEventDispatcher(). The expectation is
// WindowTreeHost calls to HostEventQueue::DispatchOrQueueEvent() on every
// event. When necessary, HostEventQueue calls back to HostEventDispatcher to
// handle the actual dispatch.
class COMPONENT_EXPORT(WINDOW_SERVICE) HostEventQueue {
 public:
  HostEventQueue(base::WeakPtr<EventQueue> event_queue,
                 aura::WindowTreeHost* window_tree_host,
                 HostEventDispatcher* host_event_dispatcher);
  ~HostEventQueue();

  // If necessary, queues the event. If the event need not be queued,
  // HostEventDispatcher::DispatchEventFromQueue() is called synchronously.
  void DispatchOrQueueEvent(ui::Event* event);

  aura::WindowTreeHost* window_tree_host() { return window_tree_host_; }

  HostEventDispatcher* host_event_dispatcher() {
    return host_event_dispatcher_;
  }

 private:
  // Because of shutdown ordering, HostEventQueue may be deleted *after*
  // EventQueue.
  base::WeakPtr<EventQueue> event_queue_;
  aura::WindowTreeHost* window_tree_host_;
  HostEventDispatcher* host_event_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(HostEventQueue);
};

}  // namespace ws

#endif  // SERVICES_WS_HOST_EVENT_QUEUE_H_
