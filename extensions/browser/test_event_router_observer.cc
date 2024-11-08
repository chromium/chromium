// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/test_event_router_observer.h"

#include <memory>

#include "base/run_loop.h"

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

void TestEventRouterObserver::WaitForEventWithName(const std::string& name) {
  while (!base::Contains(events_, name)) {
    // Create a new `RunLoop` since reuse is not supported.
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }
}

void TestEventRouterObserver::OnWillDispatchEvent(const Event& event) {
  CHECK(!event.event_name.empty());
  events_[event.event_name] = event.DeepCopy();
  if (run_loop_) {
    run_loop_->Quit();
  }
}

void TestEventRouterObserver::OnDidDispatchEventToProcess(const Event& event,
                                                          int process_id) {
  CHECK(!event.event_name.empty());
  dispatched_events_[event.event_name] = event.DeepCopy();
}

}  // namespace extensions
