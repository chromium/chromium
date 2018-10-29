// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_EVENT_QUEUE_H_
#define SERVICES_WS_EVENT_QUEUE_H_

#include <stdint.h>

#include <memory>
#include <set>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "services/ws/ids.h"
#include "services/ws/window_service_observer.h"

namespace aura {
class WindowTreeHost;
}

namespace ui {
class Event;
}

namespace ws {

class HostEventDispatcher;
class HostEventQueue;
class WindowService;

// EventQueue handles the actual queuing, waiting and dispatching of events.
// HostEventQueues calls into this. EventQueue tracks the dispatch by way of
// being a WindowServiceObserver. EventQueue also supports adding a callback
// that is notified when the queue is empty.
class COMPONENT_EXPORT(WINDOW_SERVICE) EventQueue
    : public WindowServiceObserver {
 public:
  explicit EventQueue(WindowService* service);
  ~EventQueue() override;

  // Creates a new HostEventQueue.
  std::unique_ptr<HostEventQueue> RegisterHostEventDispatcher(
      aura::WindowTreeHost* window_tree_host,
      HostEventDispatcher* dispatcher);

  // Convenience to locate the HostEventDispatcher for |window_tree_host|
  // and call DispatchOrQueueEvent() on it with |event|.
  static void DispatchOrQueueEvent(WindowService* service,
                                   aura::WindowTreeHost* window_tree_host,
                                   ui::Event* event);

  // Returns true if |event| should be queued at this time.
  bool ShouldQueueEvent(HostEventQueue* host, const ui::Event& event);

  // Notifies |closure| when this EventQueue is ready to dispatch an event,
  // which may be immediately.
  void NotifyWhenReadyToDispatch(base::OnceClosure closure);

  // Returns the HostEventQueue associated with |host|.
  HostEventQueue* FindHostEventQueueForWindowTreeHost(
      aura::WindowTreeHost* host);

 private:
  friend class EventQueueTestHelper;
  friend class HostEventQueue;

  struct QueuedEvent;

  // Tracks an event that was sent to a client.
  struct InFlightEvent {
    ClientSpecificId client_id = 0u;
    uint32_t event_id = 0u;
  };

  // Called when a HostEventQueue is created/deleted.
  void OnHostEventQueueCreated(HostEventQueue* host);
  void OnHostEventQueueDestroyed(HostEventQueue* host);

  // Adds |event| to |queued_events_|.
  void QueueEvent(HostEventQueue* host, const ui::Event& event);

  // Processes QueuedEvents until |queued_events_| is empty, or there is an
  // event sent to a client.
  void DispatchNextQueuedEvent();

  // WindowServiceObserver:
  void OnWillSendEventToClient(ClientSpecificId client_id,
                               uint32_t event_id,
                               const ui::Event& event) override;
  void OnClientAckedEvent(ClientSpecificId client_id,
                          uint32_t event_id) override;
  void OnWillDestroyClient(ClientSpecificId client_id) override;

  WindowService* window_service_;

  base::circular_deque<std::unique_ptr<QueuedEvent>> queued_events_;

  // Set when an event has been sent to a client.
  base::Optional<InFlightEvent> in_flight_event_;

  // Set of HostEventQueues.
  std::set<HostEventQueue*> host_event_queues_;

  // Because of destruction order HostEventQueues may outlive this.
  base::WeakPtrFactory<EventQueue> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EventQueue);
};

}  // namespace ws

#endif  // SERVICES_WS_EVENT_QUEUE_H_
