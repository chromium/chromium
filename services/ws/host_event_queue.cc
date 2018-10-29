// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/host_event_queue.h"

#include "services/ws/event_queue.h"
#include "services/ws/host_event_dispatcher.h"

namespace ws {

HostEventQueue::HostEventQueue(base::WeakPtr<EventQueue> event_queue,
                               aura::WindowTreeHost* window_tree_host,
                               HostEventDispatcher* host_event_dispatcher)
    : event_queue_(event_queue),
      window_tree_host_(window_tree_host),
      host_event_dispatcher_(host_event_dispatcher) {
  event_queue_->OnHostEventQueueCreated(this);
}

HostEventQueue::~HostEventQueue() {
  if (event_queue_)
    event_queue_->OnHostEventQueueDestroyed(this);
}

void HostEventQueue::DispatchOrQueueEvent(ui::Event* event) {
  if (event_queue_ && event_queue_->ShouldQueueEvent(this, *event))
    event_queue_->QueueEvent(this, *event);
  else
    host_event_dispatcher_->DispatchEventFromQueue(event);
}

}  // namespace ws
