// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/test_event_router_observer.h"

namespace extensions {

TestEventRouterObserver::TestEventRouterObserver(EventRouter* event_router)
    : event_router_(event_router) {
  event_router_->AddObserverForTesting(this);
}

TestEventRouterObserver::~TestEventRouterObserver() {
  // Note: can't use ScopedObserver<> here because the method is
  // RemoveObserverForTesting() instead of RemoveObserver().
  event_router_->RemoveObserverForTesting(this);
}

void TestEventRouterObserver::ClearEvents() {
  events_.clear();
  dispatched_events_.clear();
}

void TestEventRouterObserver::OnWillDispatchEvent(const Event& event) {
  DCHECK(!event.event_name.empty());
  events_[event.event_name] = event.DeepCopy();
}

void TestEventRouterObserver::OnDidDispatchEventToProcess(const Event& event,
                                                          int process_id) {
  DCHECK(!event.event_name.empty());
  dispatched_events_[event.event_name] = event.DeepCopy();
}

}  // namespace extensions
