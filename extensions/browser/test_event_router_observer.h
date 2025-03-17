// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_TEST_EVENT_ROUTER_OBSERVER_H_
#define EXTENSIONS_BROWSER_TEST_EVENT_ROUTER_OBSERVER_H_

#include <map>
#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "extensions/browser/event_router.h"

namespace extensions {

class TestEventRouterObserver : public EventRouter::TestObserver {
 public:
  using EventMap = std::map<std::string, std::unique_ptr<Event>>;

  explicit TestEventRouterObserver(EventRouter* event_router);

  TestEventRouterObserver(const TestEventRouterObserver&) = delete;
  TestEventRouterObserver& operator=(const TestEventRouterObserver&) = delete;

  ~TestEventRouterObserver() override;

  // Clears all recorded events.
  void ClearEvents();

  // Methods returning at most one event of each type, not preserving the order
  // they were dispatched across event types.
  const EventMap& events() const { return events_; }
  const EventMap& dispatched_events() const { return dispatched_events_; }

  // Methods returning all events, in dispatch order.
  const std::vector<std::unique_ptr<Event>>& all_events() const {
    return all_events_;
  }
  const std::vector<std::unique_ptr<Event>>& all_dispatched_events() const {
    return all_dispatched_events_;
  }

  // Waits until `events()` contains an event with `name`.
  void WaitForEventWithName(const std::string& name);

 private:
  // EventRouter::TestObserver:
  void OnWillDispatchEvent(const Event& event) override;
  void OnDidDispatchEventToProcess(const Event& event, int process_id) override;

  EventMap events_;
  EventMap dispatched_events_;
  std::vector<std::unique_ptr<Event>> all_events_;
  std::vector<std::unique_ptr<Event>> all_dispatched_events_;
  base::ScopedObservation<EventRouter, EventRouter::TestObserver> observation_{
      this};
  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_TEST_EVENT_ROUTER_OBSERVER_H_
