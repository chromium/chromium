// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/event_observer_helper.h"

#include "services/ws/window_service.h"
#include "services/ws/window_tree.h"
#include "ui/aura/env.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"

namespace ws {

EventObserverHelper::EventObserverHelper(WindowTree* tree) : tree_(tree) {
  tree->window_service()->env()->AddWindowEventDispatcherObserver(this);
}

EventObserverHelper::~EventObserverHelper() {
  tree_->window_service()->env()->RemoveWindowEventDispatcherObserver(this);
}

bool EventObserverHelper::DoesEventMatch(const ui::Event& event) const {
  return types_.find(event.type()) != types_.end();
}

void EventObserverHelper::ClearPendingEvent() {
  pending_event_.reset();
}

void EventObserverHelper::OnWindowEventDispatcherStartedProcessing(
    aura::WindowEventDispatcher* dispatcher,
    const ui::Event& event) {
  // See |pending_event_|'s comment for why the event isn't sent immediately.
  if (DoesEventMatch(event))
    pending_event_ = ui::Event::Clone(event);
}

void EventObserverHelper::OnWindowEventDispatcherFinishedProcessingEvent(
    aura::WindowEventDispatcher* dispatcher) {
  if (pending_event_) {
    tree_->SendObservedEventToClient(dispatcher->host()->GetDisplayId(),
                                     std::move(pending_event_));
  }
}

}  // namespace ws
