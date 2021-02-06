// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_TEST_EVENT_ROUTER_OBSERVER_H_
#define EXTENSIONS_BROWSER_TEST_EVENT_ROUTER_OBSERVER_H_

#include <map>
#include <memory>
#include <string>

#include "extensions/browser/event_router.h"

namespace extensions {

class TestEventRouterObserver : public EventRouter::TestObserver {
 public:
  using EventMap = std::map<std::string, std::unique_ptr<Event>>;

  explicit TestEventRouterObserver(EventRouter* event_router);
  ~TestEventRouterObserver() override;

  // Clears all recorded events.
  void ClearEvents();

  const EventMap& events() const { return events_; }
  const EventMap& dispatched_events() const { return dispatched_events_; }

 private:
  // EventRouter::TestObserver:
  void OnWillDispatchEvent(const Event& event) override;
  void OnDidDispatchEventToProcess(const Event& event) override;

  EventMap events_;
  EventMap dispatched_events_;
  EventRouter* event_router_;

  DISALLOW_COPY_AND_ASSIGN(TestEventRouterObserver);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_TEST_EVENT_ROUTER_OBSERVER_H_
