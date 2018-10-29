// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_EVENT_OBSERVER_HELPER_H_
#define SERVICES_WS_EVENT_OBSERVER_HELPER_H_

#include <memory>
#include <set>

#include "base/macros.h"
#include "ui/aura/window_event_dispatcher_observer.h"
#include "ui/events/event_constants.h"

namespace ws {

class WindowTree;

// EventObserverHelper is used when a client has requested to observe events
// that the client would not normally receive. This class observes events by way
// of aura::WindowEventDispatcherObserver and forwards them to the client.
// See mojom::WindowTree::ObserveEventTypes() for more information.
class EventObserverHelper : public aura::WindowEventDispatcherObserver {
 public:
  explicit EventObserverHelper(WindowTree* tree);
  ~EventObserverHelper() override;

  // Returns true if |event| is listed in the set of requested |types_|.
  bool DoesEventMatch(const ui::Event& event) const;

  void set_types(const std::set<ui::EventType>& types) { types_ = types; }

  // See comment above |pending_event_| for details.
  void ClearPendingEvent();

 private:
  // aura::WindowEventDispatcherObserver:
  void OnWindowEventDispatcherStartedProcessing(
      aura::WindowEventDispatcher* dispatcher,
      const ui::Event& event) override;
  void OnWindowEventDispatcherFinishedProcessingEvent(
      aura::WindowEventDispatcher* dispatcher) override;

  // The requested types of events to be observed.
  std::set<ui::EventType> types_;

  WindowTree* tree_;

  // Events matching the requested |types_| are processed in two phases:
  // . In OnWindowEventDispatcherStartedProcessing() if the event should be
  //   sent to the client, it's stored in |pending_event_|.
  // . In OnWindowEventDispatcherFinishedProcessingEvent() if |pending_event_|
  //   is non-null, |pending_event_| is sent to the client.
  // During event processing if the event targets the client, then
  // |pending_event_| is reset. This is done to avoid sending the event twice.
  // WindowTreeClient::OnWindowInputEvent() indicates if the event matched an
  // event observer.
  std::unique_ptr<ui::Event> pending_event_;

  DISALLOW_COPY_AND_ASSIGN(EventObserverHelper);
};

}  // namespace ws

#endif  // SERVICES_WS_EVENT_OBSERVER_HELPER_H_
